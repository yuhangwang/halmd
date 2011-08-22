/*
 * Copyright © 2008-2010  Peter Colberg
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

#ifndef HALMD_MDSIM_HOST_PARTICLE_HPP
#define HALMD_MDSIM_HOST_PARTICLE_HPP

#include <lua.hpp>
#include <vector>

#include <halmd/mdsim/particle.hpp>
#include <halmd/mdsim/type_traits.hpp>
#include <halmd/utility/profiler.hpp>

namespace halmd {
namespace mdsim {
namespace host {

template <unsigned int dimension, typename float_type>
class particle
  : public mdsim::particle<dimension>
{
public:
    typedef mdsim::particle<dimension> _Base;
    typedef typename type_traits<dimension, float_type>::vector_type vector_type;

    static void luaopen(lua_State* L);

    particle(std::vector<unsigned int> const& particles);
    virtual void set();
    void rearrange(std::vector<unsigned int> const& index);

    /** positions, reduced to extended domain box */
    std::vector<vector_type> r;
    /** minimum image vectors */
    std::vector<vector_type> image;
    /** velocities */
    std::vector<vector_type> v;
    /** forces */
    std::vector<vector_type> f;
    /** globally unique particle numbers */
    std::vector<unsigned int> tag;
    /** types */
    std::vector<unsigned int> type;

    /** number of particles in simulation box */
    using _Base::nbox;
    /** number of particle types */
    using _Base::ntype;
    /** number of particles per type */
    using _Base::ntypes;

private:
    typedef utility::profiler profiler_type;
    typedef typename profiler_type::accumulator_type accumulator_type;
    typedef typename profiler_type::scoped_timer_type scoped_timer_type;

    struct runtime
    {
        accumulator_type rearrange;
    };

    /** profiling runtime accumulators */
    runtime runtime_;
};

} // namespace mdsim
} // namespace host
} // namespace halmd

#endif /* ! HALMD_MDSIM_HOST_PARTICLE_HPP */
