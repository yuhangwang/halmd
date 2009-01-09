/* Molecular Dynamics simulation of a Lennard-Jones fluid
 *
 * Copyright © 2008-2009  Peter Colberg
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

#ifndef LJGPU_MDSIM_HPP
#define LJGPU_MDSIM_HPP

#include <boost/bind.hpp>
#include <fstream>
#include <iostream>
#include <ljgpu/ljfluid/ljfluid.hpp>
#include <ljgpu/options.hpp>
#include <ljgpu/sample/correlation.hpp>
#include <ljgpu/sample/energy.hpp>
#include <ljgpu/sample/perf.hpp>
#include <ljgpu/sample/trajectory.hpp>
#include <ljgpu/util/log.hpp>
#include <ljgpu/util/signal.hpp>
#include <ljgpu/util/timer.hpp>
#include <stdint.h>
#include <unistd.h>

namespace ljgpu
{

/**
 * Molecular Dynamics simulation of a Lennard-Jones fluid
 */
template <typename ljfluid_impl>
class mdsim
{
public:
    typedef typename ljfluid_impl::float_type float_type;
    typedef typename ljfluid_impl::vector_type vector_type;
    static int const dimension = vector_type::static_size;

public:
    enum {
	/** HDF5 buffers flush to disk interval in seconds */
	FLUSH_TO_DISK_INTERVAL = 900,
	/** waiting time in seconds before runtime estimate after block completion */
	TIME_ESTIMATE_WAIT_AFTER_BLOCK = 300,
	/** runtime estimate interval in seconds */
	TIME_ESTIMATE_INTERVAL = 1800,
    };

public:
    /** initialize MD simulation program */
    mdsim(options const& opt);
    /** run MD simulation program */
    void operator()();

private:
    /** program options */
    options const& opt;
    /** Lennard-Jones fluid simulation */
    ljfluid_impl fluid;
    /** block correlations */
    correlation<float_type, dimension> tcf;
    /**  trajectory file writer */
    trajectory<true, float_type, dimension> traj;
    /** thermodynamic equilibrium properties */
    energy<float_type, dimension> tep;
    /** performance data */
    perf prf;
};

/**
 * initialize MD simulation program
 */
template <typename ljfluid_impl>
mdsim<ljfluid_impl>::mdsim(options const& opt) : opt(opt), fluid(opt)
{
    // initialize random number generator with seed
    if (opt["rand-seed"].empty()) {
	LOG("obtaining 32-bit integer seed from /dev/random");
	unsigned int seed;
	try {
	    std::ifstream rand;
	    rand.exceptions(std::ifstream::eofbit|std::ifstream::failbit|std::ifstream::badbit);
	    rand.open("/dev/random");
	    rand.read(reinterpret_cast<char*>(&seed), sizeof(seed));
	    rand.close();
	}
	catch (std::ifstream::failure const& e) {
	    throw std::logic_error(std::string("failed to read from /dev/random: ") + e.what());
	}
	fluid.rng(seed);
    }
    else {
	fluid.rng(opt["rand-seed"].as<unsigned int>());
    }

    if (!opt["trajectory-sample"].empty()) {
	trajectory<false, float_type, dimension> traj;
	// open trajectory input file
	traj.open(opt["trajectory"].as<std::string>());
	// read trajectory sample and restore system state
	fluid.restore(boost::bind(&trajectory<false, float_type, dimension>::read,
				  boost::ref(traj), _1, _2, opt["trajectory-sample"].as<int>()));
	// close trajectory input file
	traj.close();
    }
    else {
	// arrange particles on a face-centered cubic (fcc) lattice
	fluid.lattice();
    }

    if (opt["trajectory-sample"].empty() || !opt["temperature"].defaulted()) {
	// set system temperature according to Maxwell-Boltzmann distribution
	fluid.temperature(opt["temperature"].as<float>());
    }

    if (!opt["device"].empty()) {
	int const dev = cuda::device::get();
	LOG("GPU allocated global device memory: " << cuda::device::mem_get_used(dev) << " bytes");
	LOG("GPU available global device memory: " << cuda::device::mem_get_free(dev) << " bytes");
	LOG("GPU total global device memory: " << cuda::device::mem_get_total(dev) << " bytes");
    }

    if (!opt["disable-correlation"].as<bool>()) {
	if (!opt["time"].empty()) {
	    // set total simulation time
	    tcf.time(opt["time"].as<float>(), fluid.timestep());
	}
	else {
	    // set total number of simulation steps
	    tcf.steps(opt["steps"].as<uint64_t>(), fluid.timestep());
	}
	// set sample rate for lowest block level
	tcf.sample_rate(opt["sample-rate"].as<unsigned int>());
	// set block size
	tcf.block_size(opt["block-size"].as<unsigned int>());
	// set maximum number of samples per block
	tcf.max_samples(opt["max-samples"].as<uint64_t>());
	// set q-vectors for spatial Fourier transformation
	tcf.q_values(opt["q-values"].as<unsigned int>(), fluid.box());
    }
}

/**
 * run MD simulation program
 */
template <typename ljfluid_impl>
void mdsim<ljfluid_impl>::operator()()
{
    if (opt["dry-run"].as<bool>()) {
	// test parameters only
	return;
    }
    if (opt["daemon"].as<bool>()) {
	// run program in background
	daemon(0, 0);
    }

    // handle non-lethal POSIX signals to allow for a partial simulation run
    signal_handler signal;
    // measure elapsed realtime
    real_timer timer;

    // performance data
    prf.open(opt["output"].as<std::string>() + ".prf");
    prf.attrs() << fluid << tcf;

    // time correlation functions
    if (!opt["disable-correlation"].as<bool>()) {
	tcf.open(opt["output"].as<std::string>() + ".tcf");
	tcf.attrs() << fluid << tcf;
    }
    // trajectory file writer
    traj.open(opt["output"].as<std::string>() + ".trj", fluid.particles());
    traj.attrs() << fluid << tcf;
    // thermodynamic equilibrium properties
    tep.open(opt["output"].as<std::string>() + ".tep");
    tep.attrs() << fluid << tcf;

    // schedule first disk flush
    alarm(FLUSH_TO_DISK_INTERVAL);

    LOG("starting MD simulation");
    timer.start();

    for (iterator_timer<uint64_t> step = 0; step < tcf.steps(); ++step) {
	// check if sample is acquired for given simulation step
	if (tcf.sample(*step)) {
	    // copy previous MD simulation state from GPU to host
	    fluid.sample();
	}

	// stream next MD simulation program step on GPU
	fluid.mdstep();

	// check if sample is acquired for given simulation step
	if (tcf.sample(*step)) {
	    bool flush = false;
	    // simulation time
	    double time = *step * (double)fluid.timestep();
	    // sample time correlation functions
	    if (!opt["disable-correlation"].as<bool>()) {
		tcf.sample(fluid.trajectory(), *step, flush);
	    }
	    // sample thermodynamic equilibrium properties
	    tep.sample(fluid.trajectory(), fluid.density(), time);
	    // sample trajectory
	    if (opt["enable-trajectory"].as<bool>() || *step == 0) {
		traj.sample(fluid.trajectory(), time);
		if (*step == 0) {
		    traj.flush();
		}
	    }
	    // acquired maximum number of samples for a block level
	    if (flush) {
		// sample performance counters
		prf.sample(fluid.times());
		// write partial results to HDF5 files and flush to disk
		if (!opt["disable-correlation"].as<bool>()) {
		    tcf.flush();
		}
		if (opt["enable-trajectory"].as<bool>())
		    traj.flush();
		tep.flush();
		prf.flush();
		LOG("flushed HDF5 buffers to disk");
		// schedule remaining runtime estimate
		step.clear();
		step.set(TIME_ESTIMATE_WAIT_AFTER_BLOCK);
		// schedule next disk flush
		alarm(FLUSH_TO_DISK_INTERVAL);
	    }
	}
	// synchronize MD simulation program step on GPU
	fluid.synchronize();

	// check whether a runtime estimate has finished
	if (step.elapsed() > 0) {
	    LOG("estimated remaining runtime: " << step);
	    step.clear();
	    // schedule next remaining runtime estimate
	    step.set(TIME_ESTIMATE_INTERVAL);
	}

	// process signal event
	if (*signal) {
	    if (*signal != SIGALRM) {
		LOG_WARNING("trapped signal " << signal << " at simulation step " << *step);
	    }
	    if (*signal == SIGUSR1) {
		// schedule runtime estimate now
		step.set(0);
		signal.clear();
	    }
	    else if (*signal == SIGHUP || *signal == SIGALRM) {
		// sample performance counters
		prf.sample(fluid.times());
		// write partial results to HDF5 files and flush to disk
		if (!opt["disable-correlation"].as<bool>()) {
		    tcf.flush();
		}
		if (opt["enable-trajectory"].as<bool>())
		    traj.flush();
		tep.flush();
		prf.flush();
		LOG("flushed HDF5 buffers to disk");
		// schedule next disk flush
		alarm(FLUSH_TO_DISK_INTERVAL);
	    }
	    else if (*signal == SIGINT || *signal == SIGTERM) {
		LOG_WARNING("aborting simulation");
		signal.clear();
		break;
	    }
	    signal.clear();
	}
    }

    // copy last MD simulation state from GPU to host
    fluid.sample();
    // save last phase space sample
    traj.sample(fluid.trajectory(), tcf.steps() * fluid.timestep());

    // sample performance counters
    prf.sample(fluid.times());
    // commit HDF5 performance datasets
    prf.commit();

    timer.stop();
    LOG("finished MD simulation");
    LOG("total MD simulation runtime: " << timer);

    // cancel previously scheduled disk flush
    alarm(0);
    // close HDF5 output files
    if (!opt["disable-correlation"].as<bool>()) {
	tcf.close();
    }
    traj.close();
    tep.close();
    prf.close();
}

} // namespace ljgpu

#endif /* ! LJGPU_MDSIM_HPP */
