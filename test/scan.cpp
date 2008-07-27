/* Parallel exclusive prefix sum
 *
 * Copyright (C) 2008  Peter Colberg
 *
 * This program is free software: you can redistribute it and/or modify
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
#include <boost/array.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <cmath>
#include <gpu/scan_glue.hpp>
#include <iomanip>
#include <iostream>
#include <libgen.h>
#include <stdexcept>
#include <stdio.h>
#include <timer.hpp>
#include <vector>
namespace po = boost::program_options;
#define foreach BOOST_FOREACH

#define PROGRAM_NAME basename(argv[0])

int main(int argc, char **argv)
{
    // program options
    uint count, threads;
    unsigned short device;
    bool verbose;

    try {
	// parse command line options
	po::options_description opts("Program options");
	opts.add_options()
	    ("count,N", po::value<uint>(&count)->default_value(10000),
	     "number of elements")
	    ("device,D", po::value<unsigned short>(&device)->default_value(0),
	     "CUDA device")
	    ("threads,T", po::value<uint>(&threads)->default_value(256),
	     "number of threads per block")
	    ("verbose,v", po::bool_switch(&verbose),
	     "print results")
	    ("help,h", "display this help and exit");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, opts), vm);
	po::notify(vm);

	if (count < 2) {
	    throw std::logic_error("number of elements must be greater than 1");
	}
	if (threads == 0 || threads & (threads - 1)) {
	    throw std::logic_error("number of threads must be a power of 2");
	}

	// print help
	if (vm.count("help")) {
	    std::cerr << "Usage: " << PROGRAM_NAME << " [OPTION]...\n\n" << opts << "\n";
	    return EXIT_SUCCESS;
	}
    }
    catch (std::exception const& e) {
	std::cerr << PROGRAM_NAME << ": " << e.what() << "\n";
	std::cerr << "Try `" << PROGRAM_NAME << " --help' for more information.\n";
	return EXIT_FAILURE;
    }

    try {
	using namespace mdsim::gpu::scan;

	// set CUDA device
	cuda::device::set(device);
	// asynchroneous GPU operations
	cuda::stream stream;
	cuda::event start, stop;

	// generate array of ascending integers
	cuda::host::vector<uint> h_array(count);
	for (uint i = 0; i < count; ++i)
	    h_array[i] = i + 1;

	// compute block recursion depth
	std::vector<uint> dim, blocks;
	for (uint n = count; n > 1; ) {
	    // number of elements at this recursion depth
	    dim.push_back(n);
	    // number of blocks
	    n = (n + (2 * threads) - 1) / (2 * threads);
	    blocks.push_back(n);
	}

	// obey maximum reasonable recursion depth
	boost::array<cuda::vector<uint>, 10> g_sum, g_sum2;
	if (dim.size() >= g_sum.size()) {
	    throw std::logic_error("maximum recursion depth exceeded");
	}
	// allocate prefix sum arrays in global device memory
	for (uint i = 0; i < dim.size(); ++i) {
	    g_sum[i].resize(dim[i]);
	    g_sum2[i].resize(dim[i]);
	}
	// block sum array for last blockwise parallel prefix sum
	g_sum[dim.size()].resize(1);

	// recursive blockwise parallel prefix sum
	cuda::copy(h_array, g_sum[0], stream);
	start.record(stream);
	for (uint i = 0; i < dim.size(); ++i) {
	    cuda::configure(blocks[i], threads, boff(2 * threads * sizeof(uint)), stream);
	    block_prefix_sum(g_sum[i], g_sum2[i], g_sum[i + 1], dim[i]);
	}

	// add block prefix sum to partial prefix sums
	cuda::copy(g_sum2[dim.size() - 1], g_sum[dim.size() - 1], stream);
	for (uint i = dim.size() - 1; i > 0; --i) {
	    cuda::configure(blocks[i - 1], threads, stream);
	    add_block_sums(g_sum[i], g_sum2[i - 1], g_sum[i - 1], count);
	}
	stop.record(stream);

	cuda::host::vector<uint> h_array2(count);
	cuda::copy(g_sum[0], h_array2, stream);

	// serial prefix sum
	std::vector<uint> h_array3(count);
	mdsim::real_timer timer;
	timer.start();
	h_array3[0] = 0;
	for (uint i = 1; i < count; ++i)
	    h_array3[i] = h_array[i - 1] + h_array3[i - 1];
	timer.stop();

	// wait for GPU to finish
	stream.synchronize();

	if (verbose) {
	    // write results to stdout
	    for (uint i = 0; i < count; ++i) {
		std::cout << "a[" << std::setw(6) << i << "] = " << std::setw(6)
		          << h_array[i] << ",\t[GPU] " << std::setw(10) << h_array2[i]
			  << ",\t[CPU] " << std::setw(10) << h_array3[i]
			  << ((h_array2[i] != h_array3[i]) ? " << MISMATCH\n" : "\n");
	    }
	}

	std::cout << "GPU time: " << std::fixed << std::setprecision(3)
	          << (stop - start) * 1e3 << " ms\n"
		  << "CPU time: " << std::fixed << std::setprecision(3)
		  << timer.elapsed() * 1e3 << " ms\n";

	// verify results
	if (!std::equal(h_array2.begin(), h_array2.end(), h_array3.begin())) {
	    throw std::logic_error("GPU and CPU prefix sum mismatch");
	}
    }
    catch (cuda::error const& e) {
	std::cerr << PROGRAM_NAME << ": CUDA ERROR: " << e.what() << "\n";
	return EXIT_FAILURE;
    }
    catch (std::exception const& e) {
	std::cerr << PROGRAM_NAME << ": ERROR: " << e.what() << "\n";
	return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
