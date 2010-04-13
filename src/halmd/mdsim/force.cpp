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

#include <halmd/mdsim/force.hpp>
#include <halmd/mdsim/host/forces/lj.hpp>
#include <halmd/util/logger.hpp>

using namespace boost;
using namespace std;

namespace halmd { namespace mdsim
{

template <int dimension>
force<dimension>::force(options const& vm)
    // dependency injection
    : particle(dynamic_pointer_cast<particle_type>(module<mdsim::particle<dimension> >::fetch(vm)))
    // allocate result variables
    , virial_(particle->ntype)
{
}

// explicit instantiation
template class force<3>;
template class force<2>;

template <int dimension>
typename module<force<dimension> >::pointer
module<force<dimension> >::fetch(options const& vm)
{
    if (!singleton_) {
#ifdef USE_HOST_SINGLE_PRECISION
        singleton_.reset(new host::forces::lj<dimension, float>(vm));
#else
        singleton_.reset(new host::forces::lj<dimension, double>(vm));
#endif
    }
    return singleton_;
}

template <> module<force<3> >::pointer module<force<3> >::singleton_ = pointer();
template class module<force<3> >;
template <> module<force<2> >::pointer module<force<2> >::singleton_ = pointer();
template class module<force<2> >;

}} // namespace halmd::mdsim
