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

#include <boost/foreach.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <cmath>

#include <halmd/mdsim/backend/exception.hpp>
#include <halmd/mdsim/host/forces/lj.hpp>
#include <halmd/util/logger.hpp>

using namespace boost;
using namespace boost::assign;
using namespace boost::numeric::ublas;

namespace halmd
{
namespace mdsim { namespace host { namespace forces
{

/**
 * Assemble module options
 */
template <int dimension, typename float_type>
void lj<dimension, float_type>::options(po::options_description& desc)
{
    desc.add_options()
        ("cutoff", po::value<boost::array<float, 3> >()->default_value(list_of(2.5f)(2.5f)(2.5f)),
         "truncate potential at cutoff radius")
        ("epsilon", po::value<boost::array<float, 3> >()->default_value(list_of(1.0f)(1.5f)(0.5f)),
         "potential well depths AA,AB,BB")
        ("sigma", po::value<boost::array<float, 3> >()->default_value(list_of(1.0f)(0.8f)(0.88f)),
         "collision diameters AA,AB,BB")
        ;
}

/**
 * Resolve module dependencies
 */
template <int dimension, typename float_type>
void lj<dimension, float_type>::resolve(po::options const& vm)
{
    module<particle_type>::required(vm);
    module<box_type>::required(vm);
}

/**
 * Initialize Lennard-Jones potential parameters
 */
template <int dimension, typename float_type>
lj<dimension, float_type>::lj(po::options const& vm)
  : _Base(vm)
  // dependency injection
  , particle(module<particle_type>::fetch(vm))
  , box(module<box_type>::fetch(vm))
  // allocate potential parameters
  , epsilon_(scalar_matrix<float_type>(particle->ntype, particle->ntype, 1))
  , sigma_(scalar_matrix<float_type>(particle->ntype, particle->ntype, 1))
  , r_cut_sigma_(particle->ntype, particle->ntype)
  , r_cut_(particle->ntype, particle->ntype)
  , rr_cut_(particle->ntype, particle->ntype)
  , sigma2_(particle->ntype, particle->ntype)
  , en_cut_(particle->ntype, particle->ntype)
{
    // parse deprecated options
    boost::array<float, 3> epsilon = vm["epsilon"].as<boost::array<float, 3> >();
    boost::array<float, 3> sigma = vm["sigma"].as<boost::array<float, 3> >();
    boost::array<float, 3> r_cut_sigma;
    try {
        r_cut_sigma = vm["cutoff"].as<boost::array<float, 3> >();
    }
    catch (boost::bad_any_cast const&) {
        // backwards compatibility
        std::fill(r_cut_sigma.begin(), r_cut_sigma.end(), vm["cutoff"].as<float>());
    }
    for (size_t i = 0; i < std::min(particle->ntype, 2U); ++i) {
        for (size_t j = i; j < std::min(particle->ntype, 2U); ++j) {
            epsilon_(i, j) = epsilon[i + j];
            sigma_(i, j) = sigma[i + j];
            r_cut_sigma_(i, j) = r_cut_sigma[i + j];
        }
    }

    // precalculate derived parameters
    for (size_t i = 0; i < particle->ntype; ++i) {
        for (size_t j = i; j < particle->ntype; ++j) {
            r_cut_(i, j) = r_cut_sigma_(i, j) * sigma_(i, j);
            rr_cut_(i, j) = std::pow(r_cut_(i, j), 2);
            sigma2_(i, j) = std::pow(sigma_(i, j), 2);
            // energy shift due to truncation at cutoff length
            float_type rri_cut = std::pow(r_cut_sigma_(i, j), -2);
            float_type r6i_cut = rri_cut * rri_cut * rri_cut;
            en_cut_(i, j) = 4 * epsilon_(i, j) * r6i_cut * (r6i_cut - 1);
        }
    }

    LOG("potential well depths: ε = " << epsilon_);
    LOG("potential pair separation: σ = " << sigma_);
    LOG("potential cutoff length: r = " << r_cut_sigma_);
    LOG("potential cutoff energy: U = " << en_cut_);
}

/**
 * Compute Lennard-Jones forces
 */
template <int dimension, typename float_type>
void lj<dimension, float_type>::compute()
{
    // initialize particle forces to zero
    std::fill(particle->f.begin(), particle->f.end(), 0);

    // potential energy
    en_pot_ = 0;
    // virial equation sum
    std::fill(virial_.begin(), virial_.end(), 0);

    for (size_t i = 0; i < particle->nbox; ++i) {
        // calculate pairwise Lennard-Jones force with neighbor particles
        BOOST_FOREACH (size_t j, particle->neighbor[i]) {
            // particle distance vector
            vector_type r = particle->r[i] - particle->r[j];
            box->reduce_periodic(r);
            // particle types
            size_t a = particle->type[i];
            size_t b = particle->type[j];
            // squared particle distance
            float_type rr = inner_prod(r, r);

            // truncate potential at cutoff length
            if (rr >= rr_cut_(a, b))
                continue;

            // compute Lennard-Jones force in reduced units
            float_type sigma2 = sigma2_(a, b);
            float_type rri = sigma2 / rr;
            float_type r6i = rri * rri * rri;
            float_type epsilon = epsilon_(a, b);
            float_type fval = 48 * rri * r6i * (r6i - 0.5) * (epsilon / sigma2);
            float_type en_pot = 4 * epsilon * r6i * (r6i - 1) - en_cut_(a, b);

            // add force contribution to both particles
            particle->f[i] += r * fval;
            particle->f[j] -= r * fval;

            // add contribution to potential energy
            en_pot_ += en_pot;

            // add contribution to virial
            float_type virial = 0.5 * rr * fval;
            virial_[a][0] += virial;
            virial_[b][0] += virial;

            // compute off-diagonal virial stress tensor elements
            if (dimension == 3) {
                virial = 0.5 * r[1] * r[2] * fval;
                virial_[a][1] += virial;
                virial_[b][1] += virial;

                virial = 0.5 * r[2] * r[0] * fval;
                virial_[a][2] += virial;
                virial_[b][2] += virial;

                virial = 0.5 * r[0] * r[1] * fval;
                virial_[a][3] += virial;
                virial_[b][3] += virial;
            }
            else {
                virial = 0.5 * r[0] * r[1] * fval;
                virial_[a][1] += virial;
                virial_[b][1] += virial;
            }
        }
    }

    en_pot_ /= particle->nbox;

    // ensure that system is still in valid state
    if (std::isinf(en_pot_)) {
        throw potential_energy_divergence();
    }
}

// explicit instantiation
#ifndef USE_HOST_SINGLE_PRECISION
template class lj<3, double>;
template class lj<2, double>;
#else
template class lj<3, float>;
template class lj<2, float>;
#endif

}}} // namespace mdsim::host::forces

#ifndef USE_HOST_SINGLE_PRECISION
template class module<mdsim::host::forces::lj<3, double> >;
template class module<mdsim::host::forces::lj<2, double> >;
#else
template class module<mdsim::host::forces::lj<3, float> >;
template class module<mdsim::host::forces::lj<2, float> >;
#endif

} // namespace halmd
