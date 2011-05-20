/*
 * Copyright © 2011  Felix Höfling
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

#include <boost/shared_ptr.hpp>

#include <halmd/observables/dynamics/correlation.hpp>
#include <halmd/utility/lua/lua.hpp>

using namespace boost;

namespace halmd
{
namespace observables { namespace dynamics
{

HALMD_LUA_API int luaopen_libhalmd_observables_dynamics_correlation(lua_State* L)
{
    using namespace luabind;
    module(L, "libhalmd")
    [
        namespace_("observables")
        [
            namespace_("dynamics")
            [
                class_<correlation, shared_ptr<correlation> >("correlation_")
                    .def("compute", &correlation::compute)
            ]
        ]
    ];
    return 0;
}

}} // namespace observables::dynamics

} // namespace halmd
