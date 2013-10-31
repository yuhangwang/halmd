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

#include <halmd/mdsim/core.hpp>
#include <halmd/utility/lua/lua.hpp>

namespace halmd {
namespace mdsim {

/**
 * Perform a single MD integration step
 */
void core::mdstep()
{
    scoped_timer_type timer(runtime_.mdstep);

    on_prepend_integrate_();
    on_integrate_();
    on_append_integrate_();
    on_prepend_finalize_();
    on_finalize_();
    on_append_finalize_();
}

void core::luaopen(lua_State* L)
{
    using namespace luaponte;
    module(L, "libhalmd")
    [
        namespace_("mdsim")
        [
            class_<core, std::shared_ptr<core> >("core")
                .def(constructor<>())
                .def("mdstep", &core::mdstep)
                .def("on_prepend_integrate", &core::on_prepend_integrate)
                .def("on_integrate", &core::on_integrate)
                .def("on_append_integrate", &core::on_append_integrate)
                .def("on_prepend_finalize", &core::on_prepend_finalize)
                .def("on_finalize", &core::on_finalize)
                .def("on_append_finalize", &core::on_append_finalize)
                .scope
                [
                    class_<runtime>("runtime")
                        .def_readonly("mdstep", &runtime::mdstep)
                ]
                .def_readonly("runtime", &core::runtime_)
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_mdsim_core(lua_State* L)
{
    core::luaopen(L);
    return 0;
}

} // namespace mdsim
} // namespace halmd
