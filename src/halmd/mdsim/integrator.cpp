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

#include <halmd/mdsim/host/integrator/verlet.hpp>
#include <halmd/mdsim/integrator.hpp>
#include <halmd/util/log.hpp>

namespace halmd { namespace mdsim
{

template <int dimension>
boost::shared_ptr<integrator<dimension> >
factory<integrator<dimension> >::fetch(options const& vm)
{
    if (!integrator_) {
#ifdef USE_HOST_SINGLE_PRECISION
        integrator_.reset(new host::integrator::verlet<dimension, float>(vm));
#else
        integrator_.reset(new host::integrator::verlet<dimension, double>(vm));
#endif
    }
    return integrator_;
}

template <> factory<integrator<3> >::integrator_ptr factory<integrator<3> >::integrator_ = integrator_ptr();
template class factory<integrator<3> >;
template <> factory<integrator<2> >::integrator_ptr factory<integrator<2> >::integrator_ = integrator_ptr();
template class factory<integrator<2> >;

}} // namespace halmd::mdsim
