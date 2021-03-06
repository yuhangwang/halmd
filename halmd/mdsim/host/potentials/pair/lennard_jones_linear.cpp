/*
 * Copyright © 2010-2013 Felix Höfling
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

#include <halmd/mdsim/host/forces/pair_full.hpp>
#include <halmd/mdsim/host/forces/pair_trunc.hpp>
#include <halmd/mdsim/host/potentials/pair/lennard_jones_linear.hpp>
#include <halmd/utility/lua/lua.hpp>

#include <boost/numeric/ublas/io.hpp>

#include <cmath>
#include <stdexcept>
#include <string>

namespace halmd {
namespace mdsim {
namespace host {
namespace potentials {
namespace pair {

template <typename matrix_type>
static matrix_type const&
check_shape(matrix_type const& m1, matrix_type const& m2)
{
    if (m1.size1() != m2.size1() || m1.size2() != m2.size2()) {
        throw std::invalid_argument("parameter matrix has invalid shape");
    }
    return m1;
}

/**
 * Initialise Lennard-Jones potential parameters
 */
template <typename float_type>
lennard_jones_linear<float_type>::lennard_jones_linear(
    matrix_type const& cutoff
  , matrix_type const& epsilon
  , matrix_type const& sigma
  , std::shared_ptr<logger> logger
)
  // allocate and pre-compute potential parameters
  : epsilon_(epsilon)
  , sigma_(check_shape(sigma, epsilon))
  , r_cut_sigma_(check_shape(cutoff, epsilon))
  , r_cut_(element_prod(sigma_, r_cut_sigma_))
  , rr_cut_(element_prod(r_cut_, r_cut_))
  , sigma2_(element_prod(sigma_, sigma_))
  , en_cut_(size1(), size2())
  , force_cut_(size1(), size2())
  , logger_(logger)
{
    // energy and force shift due to truncation at cutoff distance
    for (unsigned i = 0; i < en_cut_.size1(); ++i) {
        for (unsigned j = 0; j < en_cut_.size2(); ++j) {
            en_cut_(i, j) = 0;
            force_cut_(i, j) = 0;
            float_type fval;
            std::tie(fval, en_cut_(i, j)) = (*this)(rr_cut_(i, j), i, j);
            force_cut_(i, j) = fval * r_cut_(i, j);
        }
    }

    LOG("potential well depths: ε = " << epsilon_);
    LOG("potential core width: σ = " << sigma_);
    LOG("potential cutoff length: r_c = " << r_cut_sigma_);
    LOG("potential cutoff energy: U_c = " << en_cut_);
    LOG("potential cutoff force: F_c = " << force_cut_);
}

template <typename float_type>
void lennard_jones_linear<float_type>::luaopen(lua_State* L)
{
    using namespace luaponte;
    module(L, "libhalmd")
    [
        namespace_("mdsim")
        [
            namespace_("host")
            [
                namespace_("potentials")
                [
                    namespace_("pair")
                    [
                        class_<lennard_jones_linear, std::shared_ptr<lennard_jones_linear> >("lennard_jones_linear")
                            .def(constructor<
                                matrix_type const&
                              , matrix_type const&
                              , matrix_type const&
                              , std::shared_ptr<logger>
                            >())
                            .property("r_cut", (matrix_type const& (lennard_jones_linear::*)() const) &lennard_jones_linear::r_cut)
                            .property("r_cut_sigma", &lennard_jones_linear::r_cut_sigma)
                            .property("epsilon", &lennard_jones_linear::epsilon)
                            .property("sigma", &lennard_jones_linear::sigma)
                    ]
                ]
            ]
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_mdsim_host_potentials_pair_lennard_jones_linear(lua_State* L)
{
#ifndef USE_HOST_SINGLE_PRECISION
    lennard_jones_linear<double>::luaopen(L);
    forces::pair_full<3, double, lennard_jones_linear<double> >::luaopen(L);
    forces::pair_full<2, double, lennard_jones_linear<double> >::luaopen(L);
    forces::pair_trunc<3, double, lennard_jones_linear<double> >::luaopen(L);
    forces::pair_trunc<2, double, lennard_jones_linear<double> >::luaopen(L);
#else
    lennard_jones_linear<float>::luaopen(L);
    forces::pair_full<3, float, lennard_jones_linear<float> >::luaopen(L);
    forces::pair_full<2, float, lennard_jones_linear<float> >::luaopen(L);
    forces::pair_trunc<3, float, lennard_jones_linear<float> >::luaopen(L);
    forces::pair_trunc<2, float, lennard_jones_linear<float> >::luaopen(L);
#endif
    return 0;
}

// explicit instantiation
#ifndef USE_HOST_SINGLE_PRECISION
template class lennard_jones_linear<double>;
#else
template class lennard_jones_linear<float>;
#endif

} // namespace pair
} // namespace potentials

namespace forces {

// explicit instantiation of force modules
#ifndef USE_HOST_SINGLE_PRECISION
template class pair_full<3, double, potentials::pair::lennard_jones_linear<double> >;
template class pair_full<2, double, potentials::pair::lennard_jones_linear<double> >;
template class pair_trunc<3, double, potentials::pair::lennard_jones_linear<double> >;
template class pair_trunc<2, double, potentials::pair::lennard_jones_linear<double> >;
#else
template class pair_full<3, float, potentials::pair::lennard_jones_linear<float> >;
template class pair_full<2, float, potentials::pair::lennard_jones_linear<float> >;
template class pair_trunc<3, float, potentials::pair::lennard_jones_linear<float> >;
template class pair_trunc<2, float, potentials::pair::lennard_jones_linear<float> >;
#endif

} // namespace forces
} // namespace host
} // namespace mdsim
} // namespace halmd
