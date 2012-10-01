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

#include <algorithm>
#include <boost/bind.hpp>
#include <functional>
#include <limits>

#include <halmd/observables/host/fields/density.hpp>
#include <halmd/utility/demangle.hpp>
#include <halmd/utility/lua/lua.hpp>

using namespace boost;
using namespace std;

namespace halmd {
namespace observables {
namespace host {
namespace fields {

template <int dimension, typename float_type>
density<dimension, float_type>::density(
    shared_ptr<sample_type const> sample
  , shared_ptr<clock_type const> clock
  , shared_ptr<logger_type> logger
)
  // module dependencies
  : sample_(sample)
  , clock_(clock)
  , logger_(logger)
  // initialise attributes
  , density_(sample_->nbin)
  , step_(numeric_limits<step_type>::max())
{}

/**
 *
 */
template <int dimension, typename float_type>
void density<dimension, float_type>::sample()
{
    if (step_ == clock_->step()) {
        LOG_TRACE("sample is up to date");
        return;
    }

    // trigger update of binned phase space sample
    on_sample_();

    LOG_TRACE("acquire sample");
    scoped_timer_type timer(runtime_.sample);

    // normalise by volume of binning cells
    double volume = accumulate(sample_->cell_length.begin(), sample_->cell_length.end(), 1., multiplies<double>());

    // iterate over all elements of cell_array and assign size() / volume to density_
    transform(
        sample_->cell_array.data(), sample_->cell_array.data() + sample_->cell_array.num_elements()
      , density_.data()
      , bind(multiplies<double>(), bind(&sample_type::cell_type::size, _1), 1 / volume)
    );

    step_ = clock_->step();
}

template <int dimension, typename float_type>
void density<dimension, float_type>::luaopen(lua_State* L)
{
    using namespace luabind;
    static string class_name("density_" + lexical_cast<string>(dimension) + "_" + demangled_name<float_type>() + "_");
    module(L, "libhalmd")
    [
        namespace_("observables")
        [
            namespace_("host")
            [
                namespace_("fields")
                [
                    class_<density, shared_ptr<_Base>, _Base>(class_name.c_str())
                        .scope
                        [
                            class_<runtime>("runtime")
                                .def_readonly("sample", &runtime::sample)
                        ]
                        .def_readonly("runtime", &density::runtime_)
                ]
            ]

          , namespace_("fields")
            [
                def("density", &make_shared<density
                    , shared_ptr<sample_type const>
                    , shared_ptr<clock_type const>
                    , shared_ptr<logger_type>
                >)
            ]
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_observables_host_fields_density(lua_State* L)
{
#ifndef USE_HOST_SINGLE_PRECISION
    density<3, double>::luaopen(L);
    density<2, double>::luaopen(L);
#endif
    density<3, float>::luaopen(L);
    density<2, float>::luaopen(L);
    return 0;
}

// explicit instantiation
#ifndef USE_HOST_SINGLE_PRECISION
template class density<3, double>;
template class density<2, double>;
#endif
template class density<3, float>;
template class density<2, float>;

} // namespace fields
} // namespace host
} // namespace observables
} // namespace halmd
