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

#define BOOST_TEST_MODULE phase_space
#include <boost/test/unit_test.hpp>

#include <algorithm> // std::max
#include <boost/assign.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <limits>

#include <halmd/mdsim/box.hpp>
#include <halmd/mdsim/clock.hpp>
#include <halmd/mdsim/host/particle.hpp>
#include <halmd/mdsim/host/positions/phase_space.hpp>
#include <halmd/mdsim/host/velocities/phase_space.hpp>
#include <halmd/numeric/accumulator.hpp>
#include <halmd/observables/host/phase_space.hpp>
#include <halmd/observables/host/samples/phase_space.hpp>
#ifdef WITH_CUDA
# include <cuda_wrapper/cuda_wrapper.hpp>
# include <halmd/mdsim/gpu/particle.hpp>
# include <halmd/mdsim/gpu/positions/phase_space.hpp>
# include <halmd/mdsim/gpu/velocities/phase_space.hpp>
# include <halmd/observables/gpu/phase_space.hpp>
# include <halmd/observables/gpu/samples/phase_space.hpp>
# include <halmd/utility/gpu/device.hpp>
#endif
#include <test/tools/ctest.hpp>

using namespace boost;
using namespace boost::assign; // list_of
using namespace halmd;
using namespace std;

/**
 * test acquisition of phase space samples
 */

/**
 * copy GPU sample to host sample, do nothing if input is host sample
 */
#ifdef WITH_CUDA
template <int dimension, typename float_type>
boost::shared_ptr<observables::host::samples::phase_space<dimension, float_type> >
copy_sample(boost::shared_ptr<observables::gpu::samples::phase_space<dimension, float_type> > sample)
{
    typedef observables::host::samples::phase_space<dimension, float_type> host_sample_type;
    typedef typename observables::gpu::samples::phase_space<dimension, float_type>::gpu_vector_type gpu_vector_type;

    // allocate memory
    vector<unsigned int> ntypes;
    for (unsigned int i = 0; i < sample->r.size(); ++i) {
        ntypes.push_back(sample->r[i]->size());
    }
    boost::shared_ptr<host_sample_type> result = make_shared<host_sample_type>(ntypes);
    cuda::host::vector<gpu_vector_type> h_buf;

    // copy from GPU to host via page-locked memory
    for (unsigned int i = 0; i < ntypes.size(); ++i) {
        // positions
        h_buf.resize(sample->r[i]->size());
        cuda::copy(*sample->r[i], h_buf);
        cuda::thread::synchronize();
        std::copy(h_buf.begin(), h_buf.end(), result->r[i]->begin());

        // velocities
        h_buf.resize(sample->v[i]->size());
        cuda::copy(*sample->v[i], h_buf);
        cuda::thread::synchronize();
        std::copy(h_buf.begin(), h_buf.end(), result->v[i]->begin());
    }

    return result;
}
#endif


template <int dimension, typename float_type>
boost::shared_ptr<observables::host::samples::phase_space<dimension, float_type> >
copy_sample(boost::shared_ptr<observables::host::samples::phase_space<dimension, float_type> > sample)
{
    return sample;
}

template <typename modules_type>
struct phase_space
{
    typedef typename modules_type::box_type box_type;
    typedef typename modules_type::particle_type particle_type;
    typedef typename modules_type::position_type position_type;
    typedef typename modules_type::velocity_type velocity_type;
    typedef typename modules_type::phase_space_type phase_space_type;
    typedef typename modules_type::input_sample_type input_sample_type;
    typedef typename modules_type::output_sample_type output_sample_type;
    static bool const gpu = modules_type::gpu;

    typedef mdsim::clock clock_type;
    typedef typename particle_type::vector_type vector_type;
    typedef typename vector_type::value_type float_type;
    enum { dimension = vector_type::static_size };

    vector<unsigned int> npart;
    typename box_type::vector_type box_length;

    boost::shared_ptr<box_type> box;
    boost::shared_ptr<clock_type> clock;
    boost::shared_ptr<particle_type> particle;
    boost::shared_ptr<position_type> position;
    boost::shared_ptr<velocity_type> velocity;
    boost::shared_ptr<input_sample_type> input_sample;
    boost::shared_ptr<output_sample_type> output_sample;

    void test();
    phase_space();
};

template <typename modules_type>
void phase_space<modules_type>::test()
{
    float_type const epsilon = numeric_limits<float_type>::epsilon();

    // prepare input sample
    for (unsigned int i = 0; i < npart.size(); ++i) { // iterate over particle species
        BOOST_CHECK(input_sample->r[i]->size() == npart[i]);
        BOOST_CHECK(input_sample->v[i]->size() == npart[i]);
        for (unsigned int j = 0; j < npart[i]; ++j) { // iterate over particles
            vector_type& r = (*input_sample->r[i])[j];
            vector_type& v = (*input_sample->v[i])[j];
            r[0] = float_type(j) + float_type(1) / (i + 1); //< a large, non-integer value
            r[1] = 0;
            r[dimension - 1] = - static_cast<float_type>(j);
            v[0] = static_cast<float_type>(i);
            v[1] = 0;
            v[dimension - 1] = float_type(1) / (j + 1);
        }
    }
    input_sample->step = 0;

    // copy input sample to particle
    position->set();
    velocity->set();

    // randomly permute particles in memory
    // TODO

    // acquire sample from particle, construct temporary sampler module
    clock->advance();
    phase_space_type(output_sample, particle, box, clock).acquire();
    BOOST_CHECK(output_sample->step == 1);

    // compare output and input, copy GPU sample to host before
    boost::shared_ptr<observables::host::samples::phase_space<dimension, float_type> > result
        = copy_sample(output_sample);
    for (unsigned int i = 0; i < npart.size(); ++i) { // iterate over particle species
        // compare positions with a tolerance due to mapping to and from the periodic box
        typename input_sample_type::sample_vector const& result_position = *result->r[i];
        typename input_sample_type::sample_vector const& input_position = *input_sample->r[i];
        BOOST_CHECK_EQUAL(result_position.size(), npart[i]);
        for (unsigned int j = 0; j < npart[i]; ++j) {
            for (unsigned int k = 0; k < dimension; ++k) {
                BOOST_CHECK_CLOSE_FRACTION(result_position[j][k], input_position[j][k], 10 * epsilon);
            }
        }
        // compare velocities directly as they should not have been modified
        BOOST_CHECK_EQUAL_COLLECTIONS(
            result->v[i]->begin(), result->v[i]->end()
          , input_sample->v[i]->begin(), input_sample->v[i]->end()
        );
    }
}

template <typename modules_type>
phase_space<modules_type>::phase_space()
{
    BOOST_TEST_MESSAGE("initialise simulation modules");

    // choose a value smaller than warp size and some limiting values
    npart.push_back(1024);
    npart.push_back(512);
    npart.push_back(30);
    npart.push_back(1);

    // choose a box length with is not an exactly representable as a
    // floating-point number and which is small enough to have some overflow
    // from the periodic box. In addition, some of the coordinates should sit
    // precisely at the edge.
    box_length = fixed_vector<double, dimension>(40./3);

    // create modules
    particle = boost::make_shared<particle_type>(npart);
    box = boost::make_shared<box_type>(particle->nbox, box_length);
    input_sample = boost::make_shared<input_sample_type>(npart);
    output_sample = boost::make_shared<output_sample_type>(npart);
    position = boost::make_shared<position_type>(particle, box, input_sample);
    velocity = boost::make_shared<velocity_type>(particle, input_sample);
    clock = boost::make_shared<clock_type>(0); // bogus time-step

    // set particle tags and types
    particle->set();
}

template <int dimension, typename float_type>
struct host_modules
{
    typedef mdsim::box<dimension> box_type;
    typedef mdsim::host::particle<dimension, float_type> particle_type;
    typedef mdsim::host::positions::phase_space<dimension, float_type> position_type;
    typedef mdsim::host::velocities::phase_space<dimension, float_type> velocity_type;
    typedef observables::host::phase_space<dimension, float_type> phase_space_type;
    typedef observables::host::samples::phase_space<dimension, float_type> input_sample_type;
    typedef input_sample_type output_sample_type;
    static bool const gpu = false;
};

BOOST_AUTO_TEST_CASE( phase_space_host_2d ) {
    phase_space<host_modules<2, double> >().test();
}
BOOST_AUTO_TEST_CASE( phase_space_host_3d ) {
    phase_space<host_modules<3, double> >().test();
}

#ifdef WITH_CUDA
template <int dimension, typename float_type>
struct gpu_host_modules
{
    typedef mdsim::box<dimension> box_type;
    typedef mdsim::gpu::particle<dimension, float_type> particle_type;
    typedef mdsim::gpu::positions::phase_space<dimension, float_type> position_type;
    typedef mdsim::gpu::velocities::phase_space<dimension, float_type> velocity_type;
    typedef observables::host::samples::phase_space<dimension, float_type> input_sample_type;
    typedef input_sample_type output_sample_type;
    typedef observables::gpu::phase_space<output_sample_type> phase_space_type;
    static bool const gpu = true;
};

template <int dimension, typename float_type>
struct gpu_gpu_modules
{
    typedef mdsim::box<dimension> box_type;
    typedef mdsim::gpu::particle<dimension, float_type> particle_type;
    typedef mdsim::gpu::positions::phase_space<dimension, float_type> position_type;
    typedef mdsim::gpu::velocities::phase_space<dimension, float_type> velocity_type;
    typedef observables::host::samples::phase_space<dimension, float_type> input_sample_type;
    typedef observables::gpu::samples::phase_space<dimension, float_type> output_sample_type;
    typedef observables::gpu::phase_space<output_sample_type> phase_space_type;
    static bool const gpu = true;
};

BOOST_FIXTURE_TEST_CASE( phase_space_gpu_host_2d, device ) {
    phase_space<gpu_host_modules<2, float> >().test();
}
BOOST_FIXTURE_TEST_CASE( phase_space_gpu_host_3d, device ) {
    phase_space<gpu_host_modules<3, float> >().test();
}
BOOST_FIXTURE_TEST_CASE( phase_space_gpu_gpu_2d, device ) {
    phase_space<gpu_gpu_modules<2, float> >().test();
}
BOOST_FIXTURE_TEST_CASE( phase_space_gpu_gpu_3d, device ) {
    phase_space<gpu_gpu_modules<3, float> >().test();
}
#endif // WITH_CUDA
