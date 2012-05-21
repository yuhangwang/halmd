/*
 * Copyright © 2008-2011  Peter Colberg and Felix Höfling
 *
 * This file is part of HALMD.
 *
 * HALMD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <halmd/config.hpp>

#include <algorithm>
#include <boost/bind.hpp>
#ifdef HALMD_NO_CXX11
# include <boost/lambda/lambda.hpp>
#endif
#include <boost/make_shared.hpp>
#include <cmath>

#include <halmd/mdsim/gpu/integrators/verlet_nvt_hoover.hpp>
#include <halmd/utility/demangle.hpp>
#include <halmd/utility/lua/lua.hpp>

using namespace boost;
using namespace std;

namespace halmd {
namespace mdsim {
namespace gpu {
namespace integrators {

template <int dimension, typename float_type>
verlet_nvt_hoover<dimension, float_type>::verlet_nvt_hoover(
    boost::shared_ptr<particle_type> particle
  , boost::shared_ptr<box_type const> box
  , float_type timestep
  , float_type temperature
  , float_type resonance_frequency
  , boost::shared_ptr<logger_type> logger
)
  // public member initialisation
  : xi(0)
  , v_xi(0)
  // dependency injection
  , particle_(particle)
  , box_(box)
  , logger_(logger)
  // member initialisation
  , en_nhc_(0)
  , resonance_frequency_(resonance_frequency)
{
    set_timestep(timestep);

    LOG("resonance frequency of heat bath: " << resonance_frequency_);
    set_temperature(temperature);
}

/**
 * set integration time-step
 */
template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::set_timestep(double timestep)
{
    timestep_ = timestep;
    timestep_half_ = timestep_ / 2;
    timestep_4_ = timestep_ / 4;
    timestep_8_ = timestep_ / 8;
}

/*
 * set temperature and adjust masses of heat bath variables
 */
template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::set_temperature(double temperature)
{
    temperature_ = static_cast<float_type>(temperature);
    en_kin_target_2_ = dimension * particle_->nparticle() * temperature_;

    // follow Martyna et al. [J. Chem. Phys. 97, 2635 (1992)]
    // for the masses of the heat bath variables
    float_type omega_sq = pow(2 * M_PI * resonance_frequency_, 2);
    unsigned int dof = dimension * particle_->nparticle();
    chain_type mass;
    mass[0] = dof * temperature_ / omega_sq;
    mass[1] = temperature_ / omega_sq;
    set_mass(mass);

    LOG("temperature of heat bath: " << temperature_);
    LOG_DEBUG("target kinetic energy: " << en_kin_target_2_ / particle_->nparticle());
}

template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::set_mass(chain_type const& mass)
{
    mass_xi_ = mass;
    LOG("`mass' of heat bath variables: " << mass_xi_);
}

/**
 * First leapfrog half-step of velocity-Verlet algorithm
 */
template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::integrate()
{
    scoped_timer<timer> timer_(runtime_.integrate);
    float_type scale = propagate_chain();

    try {
        cuda::configure(particle_->dim.grid, particle_->dim.block);
        wrapper_type::kernel.integrate(
            particle_->position()
          , particle_->image()
          , particle_->velocity()
          , particle_->force()
          , timestep_
          , scale
          , static_cast<vector_type>(box_->length())
        );
        cuda::thread::synchronize();
    }
    catch (cuda::error const&) {
        LOG_ERROR("failed to stream first leapfrog step on GPU");
        throw;
    }
}

/**
 * Second leapfrog half-step of velocity-Verlet algorithm
 */
template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::finalize()
{
    scoped_timer_type timer(runtime_.finalize);

    // TODO: possibly a performance critical issue:
    // the old implementation had this loop included in update_forces(),
    // which saves one additional read of the forces plus the additional kernel execution
    // and scheduling
    try {
        cuda::configure(particle_->dim.grid, particle_->dim.block);
        wrapper_type::kernel.finalize(particle_->velocity(), particle_->force(), timestep_);
        cuda::thread::synchronize();

        float_type scale = propagate_chain();

        // rescale velocities
        scoped_timer_type timer2(runtime_.rescale);
        cuda::configure(particle_->dim.grid, particle_->dim.block);
        wrapper_type::kernel.rescale(particle_->velocity(), scale);
        cuda::thread::synchronize();
    }
    catch (cuda::error const&) {
        LOG_ERROR("failed to stream second leapfrog step on GPU");
        throw;
    }

    // compute energy contribution of chain variables
    en_nhc_ = temperature_ * (dimension * particle_->nparticle() * xi[0] + xi[1]);
    for (unsigned int i = 0; i < 2; ++i ) {
        en_nhc_ += mass_xi_[i] * v_xi[i] * v_xi[i] / 2;
    }
    en_nhc_ /= particle_->nparticle();
}

/**
 * propagate Nosé-Hoover chain
 */
template <int dimension, typename float_type>
float_type verlet_nvt_hoover<dimension, float_type>::propagate_chain()
{
    scoped_timer_type timer(runtime_.propagate);

    // compute total kinetic energy multiplied by 2
    float_type en_kin_2 = 2 * compute_en_kin_(particle_->velocity())();

    // head of the chain
    v_xi[1] += (mass_xi_[0] * v_xi[0] * v_xi[0] - temperature_) / mass_xi_[1] * timestep_4_;
    float_type t = exp(-v_xi[1] * timestep_8_);
    v_xi[0] *= t;
    v_xi[0] += (en_kin_2 - en_kin_target_2_) / mass_xi_[0] * timestep_4_;
    v_xi[0] *= t;

    // propagate heat bath variables
    for (unsigned int i = 0; i < 2; ++i ) {
        xi[i] += v_xi[i] * timestep_half_;
    }

    // rescale velocities and kinetic energy
    // we only compute the factor, the rescaling is done elsewhere
    float_type s = exp(-v_xi[0] * timestep_half_);
    en_kin_2 *= s * s;

    // tail of the chain, mirrors the head
    v_xi[0] *= t;
    v_xi[0] += (en_kin_2 - en_kin_target_2_) / mass_xi_[0] * timestep_4_;
    v_xi[0] *= t;
    v_xi[1] += (mass_xi_[0] * v_xi[0] * v_xi[0] - temperature_) / mass_xi_[1] * timestep_4_;

    // return scaling factor for CUDA kernels
    return s;
}

template <typename integrator_type>
static boost::function<typename integrator_type::chain_type& ()>
wrap_xi(boost::shared_ptr<integrator_type> integrator)
{
#ifdef HALMD_NO_CXX11
    return lambda::var(integrator->xi);
#else
    return [=]() -> typename integrator_type::chain_type& {
        return integrator->xi;
    };
#endif
}

template <typename integrator_type>
static boost::function<typename integrator_type::chain_type& ()>
wrap_v_xi(boost::shared_ptr<integrator_type> integrator)
{
#ifdef HALMD_NO_CXX11
    return lambda::var(integrator->v_xi);
#else
    return [=]() -> typename integrator_type::chain_type& {
        return integrator->v_xi;
    };
#endif
}

template <typename integrator_type>
static boost::function<double ()>
wrap_en_nhc(boost::shared_ptr<integrator_type> integrator)
{
    return bind(&integrator_type::en_nhc, integrator);
}

template <typename integrator_type>
static boost::function<void ()>
wrap_integrate(boost::shared_ptr<integrator_type> self)
{
    return bind(&integrator_type::integrate, self);
}

template <typename integrator_type>
static boost::function<void ()>
wrap_finalize(boost::shared_ptr<integrator_type> self)
{
    return bind(&integrator_type::finalize, self);
}

template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::luaopen(lua_State* L)
{
    using namespace luabind;
    module(L, "libhalmd")
    [
        namespace_("mdsim")
        [
            namespace_("integrators")
            [
                class_<verlet_nvt_hoover>()
                    .property("integrate", &wrap_integrate<verlet_nvt_hoover>)
                    .property("finalize", &wrap_finalize<verlet_nvt_hoover>)
                    .property("timestep", &verlet_nvt_hoover::timestep)
                    .property("temperature", &verlet_nvt_hoover::temperature)
                    .property("xi", &wrap_xi<verlet_nvt_hoover>)
                    .property("v_xi", &wrap_v_xi<verlet_nvt_hoover>)
                    .property("en_nhc", &wrap_en_nhc<verlet_nvt_hoover>)
                    .property("mass", &verlet_nvt_hoover::mass)
                    .property("resonance_frequency", &verlet_nvt_hoover::resonance_frequency)
                    .def("set_timestep", &verlet_nvt_hoover::set_timestep)
                    .def("set_temperature", &verlet_nvt_hoover::set_temperature)
                    .def("set_mass", &verlet_nvt_hoover::set_mass)
                    .scope
                    [
                        class_<runtime>("runtime")
                            .def_readonly("integrate", &runtime::integrate)
                            .def_readonly("finalize", &runtime::finalize)
                            .def_readonly("propagate", &runtime::propagate)
                            .def_readonly("rescale", &runtime::rescale)
                    ]
                    .def_readonly("runtime", &verlet_nvt_hoover::runtime_)

              , def("verlet_nvt_hoover", &boost::make_shared<verlet_nvt_hoover
                  , boost::shared_ptr<particle_type>
                  , boost::shared_ptr<box_type const>
                  , float_type
                  , float_type
                  , float_type
                  , boost::shared_ptr<logger_type>
                >)
            ]
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_mdsim_gpu_integrators_verlet_nvt_hoover(lua_State* L)
{
#ifdef USE_VERLET_DSFUN
    verlet_nvt_hoover<3, double>::luaopen(L);
    verlet_nvt_hoover<2, double>::luaopen(L);
#else
    verlet_nvt_hoover<3, float>::luaopen(L);
    verlet_nvt_hoover<2, float>::luaopen(L);
#endif
    return 0;
}

// explicit instantiation
#ifdef USE_VERLET_DSFUN
template class verlet_nvt_hoover<3, double>;
template class verlet_nvt_hoover<2, double>;
#else
template class verlet_nvt_hoover<3, float>;
template class verlet_nvt_hoover<2, float>;
#endif

} // namespace integrators
} // namespace gpu
} // namespace mdsim
} // namespace halmd
