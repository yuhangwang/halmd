/*
 * Copyright © 2011-2013 Felix Höfling
 * Copyright © 2011-2012 Peter Colberg
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

#include <halmd/config.hpp>

#define BOOST_TEST_MODULE ssf
#include <boost/test/unit_test.hpp>

#include <boost/bind.hpp>
#include <boost/numeric/ublas/banded.hpp>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>

#include <halmd/mdsim/box.hpp>
#include <halmd/mdsim/host/particle.hpp>
#include <halmd/mdsim/host/particle_groups/all.hpp>
#include <halmd/mdsim/host/positions/lattice.hpp>
#include <halmd/numeric/accumulator.hpp>
#include <halmd/observables/host/density_mode.hpp>
#include <halmd/observables/ssf.hpp>
#include <halmd/observables/utility/wavevector.hpp>
#ifdef HALMD_WITH_GPU
# include <halmd/mdsim/gpu/particle.hpp>
# include <halmd/mdsim/gpu/particle_groups/all.hpp>
# include <halmd/mdsim/gpu/positions/lattice.hpp>
# include <halmd/observables/gpu/density_mode.hpp>
# include <halmd/utility/gpu/device.hpp>
#endif
#include <test/tools/ctest.hpp>

using namespace halmd;
using namespace std;

// FIXME for some reason the namespace is polluted by boost::make_shared

/**
 * test computation of static structure factor
 *
 * The test is analogous to the one for mdsim/positions/lattice, where the structure
 * factor was computed manually to check the generation of an fcc lattice.
 */

template <typename modules_type>
struct lattice
{
    typedef typename modules_type::box_type box_type;
    typedef typename modules_type::particle_type particle_type;
    typedef typename modules_type::particle_group_type particle_group_type;
    typedef typename modules_type::position_type position_type;
    typedef typename modules_type::density_mode_type density_mode_type;
    static bool const gpu = modules_type::gpu;
    typedef typename particle_type::vector_type vector_type;
    typedef typename vector_type::value_type float_type;
    static unsigned int const dimension = vector_type::static_size;
    typedef observables::utility::wavevector<dimension> wavevector_type;
    typedef observables::ssf<dimension> ssf_type;
    typedef typename ssf_type::result_type ssf_result_type;

    fixed_vector<unsigned, dimension> ncell;
    unsigned nunit_cell;
    unsigned npart;
    float density;
    float lattice_constant;
    fixed_vector<double, dimension> slab;

    shared_ptr<box_type> box;
    shared_ptr<particle_type> particle;
    shared_ptr<position_type> position;
    shared_ptr<wavevector_type> wavevector;
    shared_ptr<density_mode_type> density_mode;
    shared_ptr<ssf_type> ssf;

    void test();
    lattice();
};

template <typename modules_type>
void lattice<modules_type>::test()
{
    BOOST_TEST_MESSAGE("#particles: " << npart << ", #unit cells: " << ncell <<
                       ", lattice constant: " << lattice_constant);

    // define wavenumbers: q = norm_2((2π/a) × (h, k, l))
    //
    // entries must be unique and ascendingly ordered
    //
    vector<double> wavenumber;
    double q_lat = 2 * M_PI / lattice_constant;
    if (dimension == 2) {
        wavenumber.push_back(q_lat / ncell[0]);
        wavenumber.push_back(q_lat / ncell[1]);
        wavenumber.push_back(sqrt( 1.) * q_lat);
        wavenumber.push_back(sqrt( 4.) * q_lat);
        wavenumber.push_back(sqrt( 9.) * q_lat);
        wavenumber.push_back(sqrt(16.) * q_lat);
        wavenumber.push_back(sqrt(64.) * q_lat);
    }
    else if (dimension == 3) {
        wavenumber.push_back(q_lat / ncell[0]);
        wavenumber.push_back(q_lat / ncell[1]);
        wavenumber.push_back(q_lat / ncell[2]);
        wavenumber.push_back(sqrt( 1.) * q_lat);
        wavenumber.push_back(sqrt( 2.) * q_lat);
        wavenumber.push_back(sqrt( 3.) * q_lat);
        wavenumber.push_back(sqrt( 4.) * q_lat);
        wavenumber.push_back(sqrt( 5.) * q_lat);
        wavenumber.push_back(sqrt( 6.) * q_lat);
        wavenumber.push_back(sqrt( 8.) * q_lat);
        wavenumber.push_back(sqrt( 9.) * q_lat);
        wavenumber.push_back(sqrt(10.) * q_lat);
        wavenumber.push_back(sqrt(12.) * q_lat);
    }
    sort(wavenumber.begin(), wavenumber.end());
    wavenumber.erase(
        unique(wavenumber.begin(), wavenumber.end())
      , wavenumber.end()
    );

    // setup wavevectors
    wavevector = std::make_shared<wavevector_type>(wavenumber, box->length(), 1e-3, 2 * dimension);

    // construct modules for density modes and static structure factor
    density_mode = std::make_shared<density_mode_type>(
        particle
      , std::make_shared<particle_group_type>(particle)
      , wavevector
    );
    ssf = std::make_shared<ssf_type>(
        density_mode_type::acquisitor(density_mode)
      , density_mode_type::acquisitor(density_mode)
      , wavevector
      , particle->nparticle()
    );

    // generate lattices
    BOOST_TEST_MESSAGE("generate fcc lattice");
    position->set();

    // explicitly trigger computation of density modes
    BOOST_TEST_MESSAGE("compute density modes");
    density_mode->acquire();

    // compute static structure factor
    BOOST_TEST_MESSAGE("compute static structure factor");
    ssf_result_type const& result = ssf->sample();
    BOOST_CHECK(result.size() == wavenumber.size());

    // compare with expected result
    auto shell = wavevector->shell().begin();
    for (unsigned i = 0; i < result.size(); ++i, ++shell) {
        // range with wavevectors of magnitude q
        unsigned nq = shell->second - shell->first;
        BOOST_CHECK(nq > 0);

        auto q_begin = wavevector->value().begin() + shell->first;
        auto q_end = q_begin + nq;

        // determine (h,k,l) of each wavevector and its contribution to the
        // structure factor
        //
        // S_q = {npart if h,k,l all even or odd; 0 if h,k,l mixed parity}
        // see e.g., http://en.wikipedia.org/wiki/Structure_factor
        //
        // Each fcc unit cell contributes for
        //  d=2: 1 + (-1)^(h+k)
        //  d=3: 1 + (-1)^(h+k) + (-1)^(k+l) + (-1)^(h+l)
        //
        // which is nunit_cell if all h,k,l have the same parity
        // and zero otherwise. Thus, S_q = ncell * nunit_cell = npart
        // for matching wavevectors.
        double S_q = 0;
        for (auto q_it = q_begin; q_it != q_end; ++q_it) {
            // compute elementwise product (h,k,l) × ncell
            // since fractions cannot be represented by double
            typedef fixed_vector<unsigned, dimension> index_type; // caveat: we assume q[i] ≥ 0
            index_type hkl_ncell = static_cast<index_type>(
                round(element_prod(*q_it, box->length() / (2 * M_PI)))
            );
            // check whether all entries are multiples of ncell
            // and if yes, count number of odd multiples
            if (element_mod(hkl_ncell, ncell) == index_type(0)) {
                index_type hkl_mod = element_div(hkl_ncell, ncell) % 2;
                unsigned n_odd = accumulate(hkl_mod.begin(), hkl_mod.end(), 0u, plus<unsigned>());
                if (n_odd == 0 || n_odd == dimension) {
                    S_q += 1;
                }
            }
        }
        S_q *= npart / nq;

        // compare to results

        // The error from accumulation is proportional to the number of terms.
        //
        // Additional errors from the computation of exp(iqr) are ignored, they
        // can be estimated by exp(iqr)[1 + ε(qr)].
        //
        // For wavevectors with very asymmetric hkl-values, errors are large as
        // well, which are accounted for phenomenologically by the factors 2
        // and 4 below.
        //
        const double epsilon = numeric_limits<typename vector_type::value_type>::epsilon();
        double tolerance = nq * epsilon;

        // check accumulator count, i.e., number of wavevectors
        BOOST_CHECK_EQUAL(nq, result[i][2]);
        // check structure factor
        BOOST_CHECK_SMALL(fabs(result[i][0] - S_q), npart * 2 * tolerance);

        // check error estimate on structure factor
        //
        // From N contributions assume n times the value npart, zero otherwise:
        // S_q = npart × n / N
        // variance(S_q) = npart² × (n/N) × (1-n/N) = S_q × (npart - S_q)
        // S_q_err = sqrt(variance(S_q) / (N-1))
        if (nq > 1) {
            double S_q_err = sqrt(S_q * (npart - S_q) / (nq - 1));
            BOOST_CHECK_SMALL(fabs(result[i][1] - S_q_err), max(S_q_err, 1.) * 4 * tolerance);
        }
        else {
            BOOST_CHECK(result[i][1] == 0);
        }
    }
}

template <typename modules_type>
lattice<modules_type>::lattice()
{
    BOOST_TEST_MESSAGE("initialise simulation modules");
    typedef fixed_vector<unsigned, dimension> cell_vector;

    ncell = (dimension == 3) ? cell_vector{6, 12, 12} : cell_vector{4, 1024};
    if (dimension == 3 && gpu) {
        ncell[0] *= 19; // prime
    }
    nunit_cell = (dimension == 3) ? 4 : 2;  //< number of particles per unit cell
    npart = nunit_cell * accumulate(ncell.begin(), ncell.end(), 1u, multiplies<unsigned>());
    density = 0.3;
    lattice_constant = pow(nunit_cell / density, 1.f / dimension);
    typename box_type::vector_type box_ratios(ncell);
    boost::numeric::ublas::diagonal_matrix<typename box_type::matrix_type::value_type> edges(dimension);
    for (unsigned int i = 0; i < dimension; ++i) {
        edges(i, i) = lattice_constant * box_ratios[i];
    }
    slab = 1;

    particle = std::make_shared<particle_type>(npart, 1);
    box = std::make_shared<box_type>(edges);
    position = std::make_shared<position_type>(particle, box, slab);
}

template <int dimension, typename float_type>
struct host_modules
{
    typedef mdsim::box<dimension> box_type;
    typedef mdsim::host::particle<dimension, float_type> particle_type;
    typedef mdsim::host::particle_groups::all<particle_type> particle_group_type;
    typedef mdsim::host::positions::lattice<dimension, float_type> position_type;
    typedef observables::host::density_mode<dimension, float_type> density_mode_type;
    static bool const gpu = false;
};

BOOST_AUTO_TEST_CASE( ssf_host_2d ) {
    lattice<host_modules<2, double> >().test();
}
BOOST_AUTO_TEST_CASE( ssf_host_3d ) {
    lattice<host_modules<3, double> >().test();
}

#ifdef HALMD_WITH_GPU
template <int dimension, typename float_type>
struct gpu_modules
{
    typedef mdsim::box<dimension> box_type;
    typedef mdsim::gpu::particle<dimension, float_type> particle_type;
    typedef mdsim::gpu::particle_groups::all<particle_type> particle_group_type;
    typedef mdsim::gpu::positions::lattice<dimension, float_type> position_type;
    typedef observables::gpu::density_mode<dimension, float_type> density_mode_type;
    static bool const gpu = true;
};

BOOST_FIXTURE_TEST_CASE( ssf_gpu_2d, device ) {
    lattice<gpu_modules<2, float> >().test();
}
BOOST_FIXTURE_TEST_CASE( ssf_gpu_3d, device ) {
    lattice<gpu_modules<3, float> >().test();
}
#endif // HALMD_WITH_GPU
