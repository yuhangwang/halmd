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

#include <halmd/mdsim/samples/host/trajectory.hpp>
#include <halmd/utility/lua_wrapper/lua_wrapper.hpp>

using namespace boost;
using namespace std;

namespace halmd
{
namespace mdsim { namespace samples { namespace host
{

template <int dimension, typename float_type>
trajectory<dimension, float_type>::trajectory(
    shared_ptr<particle_type> particle
)
  // dependency injection
  : particle(particle)
  // allocate sample pointers
  , r(particle->ntype)
  , v(particle->ntype)
{
    for (size_t i = 0; i < particle->ntype; ++i) {
        r[i].reset(new sample_vector(particle->ntypes[i]));
        v[i].reset(new sample_vector(particle->ntypes[i]));
    }
}

template <int dimension, typename float_type>
void trajectory<dimension, float_type>::luaopen(lua_State* L)
{
    using namespace luabind;
    string class_name("trajectory_" + lexical_cast<string>(dimension) + "_");
    module(L)
    [
        namespace_("halmd_wrapper")
        [
            namespace_("mdsim")
            [
                namespace_("samples")
                [
                    namespace_("host")
                    [
                        class_<trajectory, shared_ptr<trajectory> >(class_name.c_str())
                            .def("acquire", &trajectory::acquire)
                    ]
                ]
            ]
        ]
    ];
}

static __attribute__((constructor)) void register_lua()
{
    lua_wrapper::register_(0) //< distance of derived to base class
#ifndef USE_HOST_SINGLE_PRECISION
    [
        &trajectory<3, double>::luaopen
    ]
    [
        &trajectory<2, double>::luaopen
    ]
#endif
    [
        &trajectory<3, float>::luaopen
    ]
    [
        &trajectory<2, float>::luaopen
    ];
}

#ifndef USE_HOST_SINGLE_PRECISION
template class trajectory<3, double>;
template class trajectory<2, double>;
#endif
template class trajectory<3, float>;
template class trajectory<2, float>;

}}} // namespace mdsim::samples::host

} // namespace halmd