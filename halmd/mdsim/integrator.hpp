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

#ifndef HALMD_MDSIM_INTEGRATOR_HPP
#define HALMD_MDSIM_INTEGRATOR_HPP

#include <lua.hpp>

#include <halmd/options.hpp>

namespace halmd
{
namespace mdsim
{

template <int dimension>
class integrator
{
public:
    static void options(po::options_description& desc);
    static void luaopen(lua_State* L);

    static double const default_timestep;

    integrator() {}
    virtual ~integrator() {}
    virtual void integrate() = 0;
    virtual void finalize() = 0;
    virtual double timestep() const = 0;
    virtual void timestep(double timestep) = 0;
};

} // namespace mdsim

} // namespace halmd

#endif /* ! HALMD_MDSIM_INTEGRATOR_HPP */
