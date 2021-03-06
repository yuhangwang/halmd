/*
 * Copyright © 2010-2012 Felix Höfling
 * Copyright © 2013      Nicolas Höft
 * Copyright © 2008-2012 Peter Colberg
 *
 * This file is part of HALMD.
 *
 * HALMD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <halmd/config.hpp>

#include <halmd/algorithm/gpu/iota.hpp>
#include <halmd/algorithm/gpu/radix_sort.hpp>
#include <halmd/io/logger.hpp>
#include <halmd/mdsim/gpu/particle.hpp>
#include <halmd/mdsim/gpu/particle_kernel.hpp>
#include <halmd/mdsim/gpu/velocity.hpp>
#include <halmd/utility/gpu/device.hpp>
#include <halmd/utility/lua/lua.hpp>
#include <halmd/utility/signal.hpp>

#include <boost/function_output_iterator.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <luaponte/out_value_policy.hpp>

#include <algorithm>
#include <exception>
#include <iterator>
#include <numeric>

namespace halmd {
namespace mdsim {
namespace gpu {

/**
 * Allocate microscopic system state.
 *
 * @param particles number of particles per type or species
 */
template <int dimension, typename float_type>
particle<dimension, float_type>::particle(size_type nparticle, unsigned int nspecies)
  // FIXME default CUDA kernel execution dimensions
  : dim(device::validate(cuda::config((nparticle + 128 - 1) / 128, 128)))
  // allocate global device memory
  , nparticle_(nparticle)
  , nspecies_(std::max(nspecies, 1u))
  , g_position_(nparticle)
  , g_image_(nparticle)
  , g_velocity_(nparticle)
  , g_tag_(nparticle)
  , g_reverse_tag_(nparticle)
  , g_force_(nparticle)
  , g_en_pot_(nparticle)
  , g_stress_pot_(nparticle)
  // enable auxiliary variables by default to allow sampling of initial state
  , force_zero_(true)
  , force_dirty_(true)
  , aux_dirty_(true)
  , aux_enabled_(true)
{
    auto g_position = make_cache_mutable(g_position_);
    auto g_image = make_cache_mutable(g_image_);
    auto g_velocity = make_cache_mutable(g_velocity_);
    auto g_tag = make_cache_mutable(g_tag_);
    auto g_reverse_tag = make_cache_mutable(g_reverse_tag_);
    auto g_force = make_cache_mutable(g_force_);
    auto g_en_pot = make_cache_mutable(g_en_pot_);
    auto g_stress_pot = make_cache_mutable(g_stress_pot_);

    g_force->reserve(dim.threads());
    g_en_pot->reserve(dim.threads());
    //
    // The GPU stores the stress tensor elements in column-major order to
    // optimise access patterns for coalescable access. Increase capacity of
    // GPU array such that there are 4 (6) in 2D (3D) elements per particle
    // available, although stress_pot_->size() still returns the number of
    // particles.
    //
    g_stress_pot->reserve(stress_pot_type::static_size * dim.threads());

    LOG_DEBUG("number of CUDA execution blocks: " << dim.blocks_per_grid());
    LOG_DEBUG("number of CUDA execution threads per block: " << dim.threads_per_block());

    //
    // As the number of threads may exceed the nmber of particles
    // to account for an integer number of threads per block,
    // we need to allocate excess memory for the GPU vectors.
    //
    // The additional memory is allocated using reserve(), which
    // increases the capacity() without changing the size(). The
    // coordinates of these "virtual" particles will be ignored
    // in cuda::copy or cuda::memset calls.
    //
    try {
#ifdef USE_VERLET_DSFUN
        //
        // Double-single precision requires two single precision
        // "words" per coordinate. We use the first part of a GPU
        // vector for the higher (most significant) words of all
        // particle positions or velocities, and the second part for
        // the lower (least significant) words.
        //
        // The additional memory is allocated using reserve(), which
        // increases the capacity() without changing the size().
        //
        // Take care to pass capacity() as an argument to cuda::copy
        // or cuda::memset calls if needed, as the lower words will
        // be ignored in the operation.
        //
        // Particle images remain in single precision as they
        // contain integer values, and otherwise would not matter
        // for the long-time stability of the integrator.
        //
        LOG("integrate using double-single precision");
        g_position->reserve(2 * dim.threads());
        g_velocity->reserve(2 * dim.threads());
#else
        LOG_WARNING("integrate using single precision");
        g_position->reserve(dim.threads());
        g_velocity->reserve(dim.threads());
#endif
        g_image->reserve(dim.threads());
        g_tag->reserve(dim.threads());
        g_reverse_tag->reserve(dim.threads());
    }
    catch (cuda::error const&) {
        LOG_ERROR("failed to allocate particles in global device memory");
        throw;
    }

    // initialise 'ghost' particles to zero
    // this avoids potential nonsense computations resulting in denormalised numbers
    cuda::memset(g_position->begin(), g_position->begin() + g_position->capacity(), 0);
    cuda::memset(g_velocity->begin(), g_velocity->begin() + g_velocity->capacity(), 0);
    cuda::memset(g_image->begin(), g_image->begin() + g_image->capacity(), 0);
    iota(g_tag->begin(), g_tag->begin() + g_tag->capacity(), 0);
    iota(g_reverse_tag->begin(), g_reverse_tag->begin() + g_reverse_tag->capacity(), 0);
    cuda::memset(g_force->begin(), g_force->begin() + g_force->capacity(), 0);
    cuda::memset(g_en_pot->begin(), g_en_pot->begin() + g_en_pot->capacity(), 0);
    cuda::memset(g_stress_pot->begin(), g_stress_pot->begin() + g_stress_pot->capacity(), 0);

    // set particle masses to unit mass
    set_mass(
        *this
      , boost::make_transform_iterator(boost::counting_iterator<tag_type>(0), [](tag_type) {
            return 1;
        })
    );

    try {
        cuda::copy(nparticle_, get_particle_kernel<dimension>().nbox);
        cuda::copy(nspecies_, get_particle_kernel<dimension>().ntype);
    }
    catch (cuda::error const&) {
        LOG_ERROR("failed to copy particle parameters to device symbols");
        throw;
    }

    LOG("number of particles: " << nparticle_);
    LOG("number of particle placeholders: " << dim.threads());
    LOG("number of particle species: " << nspecies_);
}

template <int dimension, typename float_type>
void particle<dimension, float_type>::aux_enable()
{
    LOG_TRACE("enable computation of auxiliary variables");
    aux_enabled_ = true;
}

/**
 * rearrange particles by permutation
 */
template <int dimension, typename float_type>
void particle<dimension, float_type>::rearrange(cuda::vector<unsigned int> const& g_index)
{
    auto g_position = make_cache_mutable(g_position_);
    auto g_image = make_cache_mutable(g_image_);
    auto g_velocity = make_cache_mutable(g_velocity_);
    auto g_tag = make_cache_mutable(g_tag_);
    auto g_reverse_tag = make_cache_mutable(g_reverse_tag_);

    scoped_timer_type timer(runtime_.rearrange);

    cuda::vector<float4> position(nparticle_);
    cuda::vector<gpu_vector_type> image(nparticle_);
    cuda::vector<float4> velocity(nparticle_);
    cuda::vector<unsigned int> tag(nparticle_);

    position.reserve(g_position->capacity());
    image.reserve(g_image->capacity());
    velocity.reserve(g_velocity->capacity());
    tag.reserve(g_reverse_tag->capacity());

    cuda::configure(dim.grid, dim.block);
    get_particle_kernel<dimension>().r.bind(*g_position);
    get_particle_kernel<dimension>().image.bind(*g_image);
    get_particle_kernel<dimension>().v.bind(*g_velocity);
    get_particle_kernel<dimension>().tag.bind(*g_tag);
    get_particle_kernel<dimension>().rearrange(g_index, position, image, velocity, tag);

    position.swap(*g_position);
    image.swap(*g_image);
    velocity.swap(*g_velocity);
    cuda::copy(tag.begin(), tag.begin() + tag.capacity(), g_tag->begin());

    iota(g_reverse_tag->begin(), g_reverse_tag->begin() + g_reverse_tag->capacity(), 0);
    radix_sort(tag.begin(), tag.end(), g_reverse_tag->begin());
}

template <int dimension, typename float_type>
void particle<dimension, float_type>::update_force_(bool with_aux)
{
    on_prepend_force_();          // ask force modules whether force/aux cache is dirty

    if (force_dirty_ || (with_aux && aux_dirty_)) {
        if (with_aux && aux_dirty_) {
            if (!force_dirty_) {
                LOG_WARNING_ONCE("auxiliary variables inactive in prior force computation, use aux_enable()");
            }
            aux_enabled_ = true;  // turn on computation of aux variables
        }
        LOG_TRACE("request force" << (aux_enabled_ ? " and auxiliary variables" : ""));

        force_zero_ = true;       // tell first force module to reset the force
        on_force_();              // compute forces
        force_dirty_ = false;     // mark force cache as clean
        if (aux_enabled_) {
            aux_dirty_ = false;   // aux cache is clean only if requested
        }
        aux_enabled_ = false;     // disable aux variables for next call
    }
    on_append_force_();
}

template <typename particle_type>
static std::vector<typename particle_type::position_type>
wrap_get_position(particle_type const& self)
{
    std::vector<typename particle_type::position_type> output;
    output.reserve(self.nparticle());
    get_position(self, back_inserter(output));
    return std::move(output);
}

template <typename particle_type>
static void
wrap_set_position(particle_type& self, std::vector<typename particle_type::position_type> const& input)
{
    if (input.size() != self.nparticle()) {
        throw std::invalid_argument("input array size not equal to number of particles");
    }
    set_position(self, input.begin());
}

template <typename particle_type>
static std::vector<typename particle_type::image_type>
wrap_get_image(particle_type const& self)
{
    std::vector<typename particle_type::image_type> output;
    output.reserve(self.nparticle());
    get_image(self, back_inserter(output));
    return std::move(output);
}

template <typename particle_type>
static void
wrap_set_image(particle_type& self, std::vector<typename particle_type::image_type> const& input)
{
    if (input.size() != self.nparticle()) {
        throw std::invalid_argument("input array size not equal to number of particles");
    }
    set_image(self, input.begin());
}

template <typename particle_type>
static std::vector<typename particle_type::velocity_type>
wrap_get_velocity(particle_type const& self)
{
    std::vector<typename particle_type::velocity_type> output;
    output.reserve(self.nparticle());
    get_velocity(self, back_inserter(output));
    return std::move(output);
}

template <typename particle_type>
static void
wrap_set_velocity(particle_type& self, std::vector<typename particle_type::velocity_type> const& input)
{
    if (input.size() != self.nparticle()) {
        throw std::invalid_argument("input array size not equal to number of particles");
    }
    set_velocity(self, input.begin());
}

template <typename particle_type>
static std::vector<typename particle_type::tag_type>
wrap_get_tag(particle_type const& self)
{
    std::vector<typename particle_type::tag_type> output;
    output.reserve(self.nparticle());
    get_tag(self, back_inserter(output));
    return std::move(output);
}

template <typename particle_type>
static void
wrap_set_tag(particle_type& self, std::vector<typename particle_type::tag_type> const& input)
{
    typedef typename particle_type::tag_type tag_type;
    if (input.size() != self.nparticle()) {
        throw std::invalid_argument("input array size not equal to number of particles");
    }
    tag_type nparticle = self.nparticle();
    set_tag(
        self
      , boost::make_transform_iterator(input.begin(), [&](tag_type t) -> tag_type {
            if (t >= nparticle) {
                throw std::invalid_argument("invalid particle tag");
            }
            return t;
        })
    );
}

template <typename particle_type>
static std::vector<typename particle_type::reverse_tag_type>
wrap_get_reverse_tag(particle_type const& self)
{
    std::vector<typename particle_type::reverse_tag_type> output;
    output.reserve(self.nparticle());
    get_reverse_tag(self, back_inserter(output));
    return std::move(output);
}

template <typename particle_type>
static void
wrap_set_reverse_tag(particle_type& self, std::vector<typename particle_type::reverse_tag_type> const& input)
{
    typedef typename particle_type::reverse_tag_type reverse_tag_type;
    if (input.size() != self.nparticle()) {
        throw std::invalid_argument("input array size not equal to number of particles");
    }
    reverse_tag_type nparticle = self.nparticle();
    set_reverse_tag(
        self
      , boost::make_transform_iterator(input.begin(), [&](reverse_tag_type i) -> reverse_tag_type {
            if (i >= nparticle) {
                throw std::invalid_argument("invalid particle reverse tag");
            }
            return i;
        })
    );
}

template <typename particle_type>
static std::vector<typename particle_type::species_type>
wrap_get_species(particle_type const& self)
{
    std::vector<typename particle_type::species_type> output;
    output.reserve(self.nparticle());
    get_species(self, back_inserter(output));
    return std::move(output);
}

template <typename particle_type>
static void
wrap_set_species(particle_type& self, std::vector<typename particle_type::species_type> const& input)
{
    typedef typename particle_type::species_type species_type;
    if (input.size() != self.nparticle()) {
        throw std::invalid_argument("input array size not equal to number of particles");
    }
    species_type nspecies = self.nspecies();
    set_species(
        self
      , boost::make_transform_iterator(input.begin(), [&](species_type s) -> species_type {
            if (s >= nspecies) {
                throw std::invalid_argument("invalid particle species");
            }
            return s;
        })
    );
}

template <typename particle_type>
static std::vector<typename particle_type::mass_type>
wrap_get_mass(particle_type const& self)
{
    std::vector<typename particle_type::mass_type> output;
    output.reserve(self.nparticle());
    get_mass(self, back_inserter(output));
    return std::move(output);
}

template <typename particle_type>
static void
wrap_set_mass(particle_type& self, std::vector<typename particle_type::mass_type> const& input)
{
    if (input.size() != self.nparticle()) {
        throw std::invalid_argument("input array size not equal to number of particles");
    }
    set_mass(self, input.begin());
}

template <typename particle_type>
static std::function<std::vector<typename particle_type::force_type> ()>
wrap_get_force(std::shared_ptr<particle_type> self)
{
    return [=]() -> std::vector<typename particle_type::force_type> {
        std::vector<typename particle_type::force_type> output;
        {
            output.reserve(self->force()->size());
        }
        get_force(*self, std::back_inserter(output));
        return std::move(output);
    };
}

template <typename particle_type>
static std::function<std::vector<typename particle_type::en_pot_type> ()>
wrap_get_potential_energy(std::shared_ptr<particle_type> self)
{
    return [=]() -> std::vector<typename particle_type::en_pot_type> {
        std::vector<typename particle_type::en_pot_type> output;
        {
            output.reserve(self->potential_energy()->size());
        }
        get_potential_energy(*self, std::back_inserter(output));
        return std::move(output);
    };
}

template <typename particle_type>
static std::function<std::vector<typename particle_type::stress_pot_type> ()>
wrap_get_stress_pot(std::shared_ptr<particle_type> self)
{
    return [=]() -> std::vector<typename particle_type::stress_pot_type> {
        std::vector<typename particle_type::stress_pot_type> output;
        {
            output.reserve(self->stress_pot()->size());
        }
        get_stress_pot(*self, std::back_inserter(output));
        return std::move(output);
    };
}

template <int dimension, typename float_type>
static int wrap_dimension(particle<dimension, float_type> const&)
{
    return dimension;
}

template <typename T>
static bool equal(std::shared_ptr<T const> self, std::shared_ptr<T const> other)
{
    // compare pointers of managed objects. I could not get working
    // owner_equal() or owner_before() with shared_ptr's passed from Lua
    return self == other;
}

template <int dimension, typename float_type>
void particle<dimension, float_type>::luaopen(lua_State* L)
{
    using namespace luaponte;
    static std::string class_name = "particle_" + std::to_string(dimension);
    module(L, "libhalmd")
    [
        namespace_("mdsim")
        [
            namespace_("gpu")
            [
                class_<particle, std::shared_ptr<particle>>(class_name.c_str())
                    .def(constructor<size_type, unsigned int>())
                    .property("nparticle", &particle::nparticle)
                    .property("nspecies", &particle::nspecies)
                    .def("get_position", &wrap_get_position<particle>)
                    .def("set_position", &wrap_set_position<particle>)
                    .def("get_image", &wrap_get_image<particle>)
                    .def("set_image", &wrap_set_image<particle>)
                    .def("get_velocity", &wrap_get_velocity<particle>)
                    .def("set_velocity", &wrap_set_velocity<particle>)
                    .def("get_tag", &wrap_get_tag<particle>)
                    .def("set_tag", &wrap_set_tag<particle>)
                    .def("get_reverse_tag", &wrap_get_reverse_tag<particle>)
                    .def("set_reverse_tag", &wrap_set_reverse_tag<particle>)
                    .def("get_species", &wrap_get_species<particle>)
                    .def("set_species", &wrap_set_species<particle>)
                    .def("get_mass", &wrap_get_mass<particle>)
                    .def("set_mass", &wrap_set_mass<particle>)
                    .def("get_force", &wrap_get_force<particle>)
                    .def("get_potential_energy", &wrap_get_potential_energy<particle>)
                    .def("get_stress_pot", &wrap_get_stress_pot<particle>)
                    .def("shift_velocity", &shift_velocity<particle>)
                    .def("shift_velocity_group", &shift_velocity_group<particle>)
                    .def("rescale_velocity", &rescale_velocity<particle>)
                    .def("rescale_velocity_group", &rescale_velocity_group<particle>)
                    .def("shift_rescale_velocity", &shift_rescale_velocity<particle>)
                    .def("shift_rescale_velocity_group", &shift_rescale_velocity_group<particle>)
                    .property("dimension", &wrap_dimension<dimension, float_type>)
                    .def("aux_enable", &particle::aux_enable)
                    .def("on_prepend_force", &particle::on_prepend_force)
                    .def("on_force", &particle::on_force)
                    .def("on_append_force", &particle::on_append_force)
                    .def("__eq", &equal<particle>) // operator= in Lua
                    .scope
                    [
                        class_<runtime>("runtime")
                            .def_readonly("rearrange", &runtime::rearrange)
                    ]
                    .def_readonly("runtime", &particle::runtime_)
            ]
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_mdsim_gpu_particle(lua_State* L)
{
    particle<3, float>::luaopen(L);
    particle<2, float>::luaopen(L);
    return 0;
}

// explicit instantiation
template class particle<3, float>;
template class particle<2, float>;

} // namespace gpu
} // namespace mdsim
} // namespace halmd
