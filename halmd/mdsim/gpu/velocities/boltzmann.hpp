/*
 * Copyright © 2010 Felix Höfling
 * Copyright © 2008-2012 Peter Colberg
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

#ifndef HALMD_MDSIM_GPU_VELOCITIES_BOLTZMANN_HPP
#define HALMD_MDSIM_GPU_VELOCITIES_BOLTZMANN_HPP

#include <boost/make_shared.hpp>
#include <lua.hpp>
#include <utility>

#include <halmd/io/logger.hpp>
#include <halmd/mdsim/gpu/particle.hpp>
#include <halmd/mdsim/gpu/velocity.hpp>
#include <halmd/mdsim/gpu/velocities/boltzmann_kernel.hpp>
#include <halmd/numeric/mp/dsfloat.hpp>
#include <halmd/random/gpu/random.hpp>
#include <halmd/utility/profiler.hpp>

namespace halmd {
namespace mdsim {
namespace gpu {
namespace velocities {

template <int dimension, typename float_type, typename RandomNumberGenerator>
class boltzmann
  : public gpu::velocity<dimension, float_type>
{
public:
    typedef gpu::particle<dimension, float_type> particle_type;
    typedef random::gpu::random<RandomNumberGenerator> random_type;
    typedef logger logger_type;

    boltzmann(
        boost::shared_ptr<particle_type> particle
      , boost::shared_ptr<random_type> random
      , double temperature
      , boost::shared_ptr<logger_type> logger = boost::make_shared<logger_type>()
    );

    /**
     * Initialise velocities from Maxwell-Boltzmann distribution
     */
    void set();

    /**
     * Returns temperature.
     */
    float_type temperature() const
    {
        return temp_;
    }

    /**
     * Bind class to Lua.
     */
    static void luaopen(lua_State* L);

private:
    typedef gpu::velocity<dimension, float_type> _Base;
    typedef typename particle_type::vector_type vector_type;
    typedef typename particle_type::gpu_vector_type gpu_vector_type;
    typedef typename random_type::rng_type rng_type;
#ifdef USE_VERLET_DSFUN
    typedef boltzmann_wrapper<dimension, dsfloat, rng_type> wrapper_type;
#else
    typedef boltzmann_wrapper<dimension, float, rng_type> wrapper_type;
#endif
    typedef typename wrapper_type::gaussian_impl_type gaussian_impl_type;

    static gaussian_impl_type get_gaussian_impl(int threads);

    /** system state */
    boost::shared_ptr<particle_type> particle_;
    /** random number generator */
    boost::shared_ptr<random_type> random_;
    /** module logger */
    boost::shared_ptr<logger_type> logger_;
    /** generate Maxwell-Boltzmann distribution */
    gaussian_impl_type const gaussian_impl_;
    /** temperature */
    float_type temp_;
    /** block sum of velocity */
    cuda::vector<gpu_vector_type> g_vcm_;
    /** block sum of squared velocity */
    cuda::vector<dsfloat> g_vv_;

    typedef utility::profiler profiler_type;
    typedef typename profiler_type::accumulator_type accumulator_type;
    typedef typename profiler_type::scoped_timer_type scoped_timer_type;

    struct runtime
    {
        accumulator_type set;
    };

    /** profiling runtime accumulators */
    runtime runtime_;
};

} // namespace velocities
} // namespace gpu
} // namespace mdsim
} // namespace halmd

#endif /* ! HALMD_MDSIM_GPU_VELOCITIES_BOLTZMANN_HPP */
