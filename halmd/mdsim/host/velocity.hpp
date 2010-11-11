/*
 * Copyright © 2010  Peter Colberg and Felix Höfling
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

#ifndef HALMD_MDSIM_HOST_VELOCITY_HPP
#define HALMD_MDSIM_HOST_VELOCITY_HPP

#include <lua.hpp>

#include <halmd/mdsim/host/particle.hpp>
#include <halmd/mdsim/velocity.hpp>
#include <halmd/utility/program_options/program_options.hpp>

namespace halmd
{
namespace mdsim { namespace host
{

template <int dimension, typename float_type>
class velocity
  : public mdsim::velocity<dimension>
{
public:
    typedef mdsim::velocity<dimension> _Base;
    typedef host::particle<dimension, float_type> particle_type;
    typedef typename _Base::vector_type vector_type;

    boost::shared_ptr<particle_type> particle;

    static void luaopen(lua_State* L);

    velocity(
        boost::shared_ptr<particle_type> particle
    );
    virtual void rescale(double factor);
    virtual void shift(vector_type const& delta);
    virtual void shift_rescale(vector_type const& delta, double factor);
};

}} // namespace mdsim::host

} // namespace halmd

#endif /* ! HALMD_MDSIM_HOST_VELOCITY_HPP */
