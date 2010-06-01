/*
 * Copyright © 2008-2010  Peter Colberg and Felix Höfling
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

#include <halmd/io/logger.hpp>
#include <halmd/mdsim/host/forces/smooth.hpp>

using namespace boost;

namespace halmd
{
namespace mdsim { namespace host { namespace forces
{

/**
 * Assemble module options
 */
template <int dimension, typename float_type>
void smooth<dimension, float_type>::options(po::options_description& desc)
{
#if 0
    // FIXME This is a module-specific option, however we do not (yet)
    // parse module options before calling the resolve function of a
    // module, which is needed to make a decision on its suitability.
    desc.add_options()
        ("smooth", po::value<float>(),
         "C²-potential smoothing factor")
        ;
#endif
}

/**
 * Resolve module dependencies
 */
template <int dimension, typename float_type>
void smooth<dimension, float_type>::resolve(po::options const& vm)
{
    if (!vm.count("smooth")) {
        throw unsuitable_module<smooth>("missing option '--smooth'");
    }
    if (vm["smooth"].as<float>() == 0.) {
        throw unsuitable_module<smooth>("potential smoothing disabled");
    }
}

/**
 * Initialize Lennard-Jones potential parameters
 */
template <int dimension, typename float_type>
smooth<dimension, float_type>::smooth(po::options const& vm)
  // allocate potential parameters
  : r_smooth_(vm["smooth"].as<float>())
  , rri_smooth_(std::pow(r_smooth_, -2))
{
    LOG("scale parameter for potential smoothing function: " << r_smooth_);
}

// explicit instantiation
#ifndef USE_HOST_SINGLE_PRECISION
template class smooth<3, double>;
template class smooth<2, double>;
#else
template class smooth<3, float>;
template class smooth<2, float>;
#endif

}}} // namespace mdsim::host::forces

#ifndef USE_HOST_SINGLE_PRECISION
template class module<mdsim::host::forces::smooth<3, double> >;
template class module<mdsim::host::forces::smooth<2, double> >;
#else
template class module<mdsim::host::forces::smooth<3, float> >;
template class module<mdsim::host::forces::smooth<2, float> >;
#endif

} // namespace halmd
