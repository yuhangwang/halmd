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

#define BOOST_TEST_MODULE box
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <boost/assign.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <limits>

#include <halmd/mdsim/box.hpp>
#include <halmd/mdsim/type_traits.hpp>
#include <halmd/numeric/blas/fixed_vector.hpp>
#include <halmd/numeric/mp/dsfloat.hpp>
#ifdef WITH_CUDA
# include <cuda_wrapper/cuda_wrapper.hpp>
# include <halmd/mdsim/gpu/box_kernel.cuh>
# include <test/unit/mdsim/box_kernel.hpp>
# include <halmd/utility/gpu/device.hpp>
#endif
#include <test/tools/ctest.hpp>

using namespace boost;
using namespace boost::assign;
using namespace halmd;
using namespace std;

template <int dimension>
void construction()
{
    typedef mdsim::box<dimension> box_type;
    typedef typename box_type::vector_type vector_type;

    double const epsilon = numeric_limits<double>::epsilon();

    vector_type ratios = (dimension == 2) ? list_of(1.)(1.) : list_of(.001)(1.)(1000.);
    vector_type length = element_prod(vector_type(10), ratios);
    double volume = (dimension == 2) ? 100 : 1000;

    BOOST_TEST_MESSAGE("Construction from edge lengths");
    shared_ptr<box_type> box = make_shared<box_type>(length);
    BOOST_CHECK_EQUAL(box->length(), length);
    BOOST_CHECK_CLOSE_FRACTION(box->volume(), volume, epsilon);
}

template <int dimension>
void periodic_host()
{
    typedef mdsim::box<dimension> box_type;
    typedef typename box_type::vector_type vector_type;

    double const epsilon = numeric_limits<double>::epsilon();

    vector_type length = (dimension == 2) ? list_of(1./3)(1./5) : list_of(.001)(1.)(1000.);
    box_type box(length);

    // set up list of positions that are (half-) multiples of the
    // edge lengths or completely unrelated
    std::vector<vector_type> position;
    position += vector_type(0);
    position += length;
    position += -1.5 * length;
    position += length / 7;
    if (dimension == 2) {
        position += list_of(0.)(-.2);
        position += list_of(1./3)(1./10);
        position += list_of(-1./6)(1./5);
    }
    else if (dimension == 3) {
        position += list_of(-0.001)(1.)(1000.);
        position += list_of(0.001)(-.1)(-500.);
    }

    // perform periodic reduction and extend the reduced vector afterwards
    BOOST_FOREACH (vector_type const& r0, position) {
        vector_type r1 = r0;
        vector_type image = box.reduce_periodic(r1);
        for (unsigned int i = 0; i < dimension; ++i) {
            BOOST_CHECK_MESSAGE(
                // FIXME the epsilon-tolerance appears a bit weird here
                r1[i] >= length[i] * (-.5 - epsilon) && r1[i] < length[i] * (.5 + epsilon)
              , "coordinate " << i << " of (" << r1 << ") is outside of the simulation box"
                << " [(" << -length / 2 << "), (" << length / 2 << ")]"
            );
        }

        box.extend_periodic(r1, image);
        if (r0 != vector_type(0)) {
            BOOST_CHECK_SMALL(norm_2(r0 - r1) / norm_2(r0), epsilon);
        }
        else {
            BOOST_CHECK_EQUAL(r1, r0);
        }
    }
}

#ifdef WITH_CUDA

template <int dimension, typename float_type>
void periodic_gpu()
{
    typedef fixed_vector<float_type, dimension> vector_type;
    typedef typename mdsim::type_traits<dimension, float_type>::gpu::coalesced_vector_type coalesced_vector_type;

    float_type const epsilon = numeric_limits<float_type>::epsilon();

    vector_type length = (dimension == 2) ? list_of(1./3)(1./5) : list_of(.001)(1.)(1000.);
    unsigned int warp_size = 32;

    // set up list of positions that are (half-) multiples of the
    // edge lengths or completely unrelated
    std::vector<vector_type> position;
    position += vector_type(0);
    position += length;
    position += -1.5 * length;
    position += length / 7;
    position += 2 * length;
    position += -2.5 * length;
    position += vector_type(.5);
    position += vector_type(1);
    position += vector_type(1.5);
    if (dimension == 2) {
        position += list_of(0.)(-.2);
        position += list_of(1./3)(1./10);
        position += list_of(-1./6)(1./5);
    }
    else if (dimension == 3) {
        position += list_of(-0.001)(1.)(1000.);
        position += list_of(0.001)(-.1)(-500.);
    }
    unsigned int npos = position.size();

    // allocate device memory and host memory for conversion to GPU type
    cuda::host::vector<coalesced_vector_type> h_position(npos);
    cuda::host::vector<coalesced_vector_type> h_reduced(npos);
    cuda::vector<coalesced_vector_type> g_position(npos);
    cuda::vector<coalesced_vector_type> g_reduced(npos);

    // allocate memory for conversion to GPU type and transfer positions to device
    std::copy(position.begin(), position.end(), h_position.begin());
    cuda::copy(h_position, g_position);

    // call reduce_periodic kernel
    cuda::config config((npos + warp_size - 1) / warp_size, warp_size);
    BOOST_MESSAGE("kernel reduce_periodic: using " << config.blocks_per_grid() << " block with "
        << config.threads_per_block() << " threads"
    );
    cuda::configure(config.grid, config.block);
    box_kernel_wrapper<dimension, float_type>::kernel.reduce_periodic(g_position, g_reduced, length, npos);
    cuda::thread::synchronize();

    // copy results back to host (but don't convert to vector_type)
    cuda::copy(g_position, h_position);
    cuda::copy(g_reduced, h_reduced);

    // perform periodic reduction and extend the reduced vector afterwards
    for (unsigned int i = 0; i < position.size(); ++i) {
        vector_type const& r0 = position[i];
        vector_type r1 = h_reduced[i]; // reduced position
        for (unsigned int j = 0; j < dimension; ++j) {
            BOOST_CHECK_MESSAGE(
                // FIXME the epsilon-tolerance appears a bit weird here
                r1[j] >= length[j] * (-.5 - epsilon) && r1[j] < length[j] * (.5 + epsilon)
              , "coordinate " << j << " of (" << r1 << ") is outside of the simulation box"
                << " [(" << -length / 2 << "), (" << length / 2 << ")]"
            );
        }

        r1 = h_position[i]; // reduced and extended position
        if (r0 != vector_type(0)) {
            BOOST_CHECK_SMALL(norm_2(r0 - r1) / norm_2(r0), epsilon);
        }
        else {
            BOOST_CHECK_EQUAL(r1, r0);
        }
    }
}

#endif

BOOST_AUTO_TEST_CASE(box_construction_2d) {
    construction<2>();
}
BOOST_AUTO_TEST_CASE(box_construction_3d) {
    construction<3>();
}
BOOST_AUTO_TEST_CASE(box_periodic_host_2d) {
    periodic_host<2>();
}
BOOST_AUTO_TEST_CASE(box_periodic_host_3d) {
    periodic_host<3>();
}

#ifdef WITH_CUDA
BOOST_FIXTURE_TEST_CASE(box_periodic_gpu_2d, device) {
    periodic_gpu<2, float>();
}
BOOST_FIXTURE_TEST_CASE(box_periodic_gpu_3d, device) {
    periodic_gpu<3, float>();
}
#endif
