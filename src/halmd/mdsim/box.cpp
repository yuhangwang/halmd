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

#include <algorithm>
#include <cmath>
#include <numeric>

#include <halmd/mdsim/box.hpp>
#include <halmd/util/logger.hpp>

using namespace boost;
using namespace std;

namespace halmd { namespace mdsim
{

/**
 * Set box edge lengths
 */
template <int dimension>
box<dimension>::box(options const& vm)
    // dependency injection
    : particle(dynamic_pointer_cast<particle_type>(module<mdsim::particle<dimension> >::fetch(vm)))
    // default to cube
    , scale_(1)
{
    // parse options
    if (vm["density"].defaulted() && !vm["box-length"].empty()) {
        length(vm["box-length"].as<float>());
    }
    else {
        density(vm["density"].as<float>());
    }
}

/**
 * Set edge lengths of cuboid
 */
template <int dimension>
void box<dimension>::length(vector_type const& value)
{
    length_ = value;
    scale_ = length_ / *max_element(length_.begin(), length_.end());
    double volume = accumulate(length_.begin(), length_.end(), 1., multiplies<double>());
    density_ = particle->nbox / volume;

    LOG("simulation box edge lengths: " << length_);
    LOG("number density: " << density_);
}

/**
 * Set number density
 */
template <int dimension>
void box<dimension>::density(double value)
{
    density_ = value;
    double volume = particle->nbox / accumulate(scale_.begin(), scale_.end(), density_, multiplies<double>());
    length_ = scale_ * pow(volume, 1. / dimension);

    LOG("simulation box edge lengths: " << length_);
    LOG("number density: " << density_);
}

// explicit instantiation
template class box<3>;
template class box<2>;

template <int dimension>
typename module<box<dimension> >::pointer
module<box<dimension> >::fetch(options const& vm)
{
    if (!singleton_) {
        singleton_.reset(new box<dimension>(vm));
    }
    return singleton_;
}

template <> module<box<3> >::pointer module<box<3> >::singleton_ = pointer();
template class module<box<3> >;
template <> module<box<2> >::pointer module<box<2> >::singleton_ = pointer();
template class module<box<2> >;

}} // namespace halmd::mdsim
