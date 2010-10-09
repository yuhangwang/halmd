/*
 * Copyright © 2010  Felix Höfling
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

#include <halmd/io/logger.hpp>
#include <halmd/mdsim/core.hpp>
#include <halmd/sampler.hpp>
#include <halmd/utility/lua_wrapper/lua_wrapper.hpp>
#include <halmd/utility/scoped_timer.hpp>
#include <halmd/utility/timer.hpp>

using namespace boost;
using namespace boost::fusion;
using namespace std;

namespace halmd
{

/**
 * Assemble module options
 */
template <int dimension>
void sampler<dimension>::options(po::options_description& desc)
{
    desc.add_options()
        ("steps,s", po::value<uint64_t>()->default_value(10000),
         "number of simulation steps")
        ("time,t", po::value<double>(),
         "total simulation time")
        ("sampling-state-vars", po::value<unsigned>()->default_value(25),
         "sample macroscopic state variables every given number of integration steps")
        ("sampling-trajectory", po::value<unsigned>()->default_value(0),
         "sample trajectory every given number of integration steps")
        ;
}

/**
 * Register option value types with Lua
 */
static __attribute__((constructor)) void register_option_converters()
{
    using namespace lua_wrapper;
    register_any_converter<uint64_t>();
    register_any_converter<double>();
    register_any_converter<unsigned>();
}

/**
 * Initialize simulation
 */
template <int dimension>
sampler<dimension>::sampler(
    shared_ptr<core_type> core
  , uint64_t steps
  , unsigned int statevars_interval
  , unsigned int trajectory_interval
)
  : core(core)
  , steps_(steps)
  , time_(steps_ * core->integrator->timestep())
  , statevars_interval_(statevars_interval)
  , trajectory_interval_(trajectory_interval)
{
    LOG("number of integration steps: " << steps_);
    LOG("integration time: " << time_);
}

/**
 * register module runtime accumulators
 */
template <int dimension>
void sampler<dimension>::register_runtimes(profiler_type& profiler)
{
    profiler.register_map(runtime_);
}

/**
 * Run simulation
 */
template <int dimension>
void sampler<dimension>::run()
{
    core->prepare();
    sample(true);

    LOG("starting simulation run");

    while (core->step_counter() < steps_) {
        // perform complete MD integration step
        core->mdstep();

        // sample system state and properties,
        // force sampling after last integration step
        sample(core->step_counter() == steps_);
    }

    LOG("finished simulation run");

    for_each(
        profile_writers.begin()
      , profile_writers.end()
      , bind(&profile_writer_type::write, _1)
    );
}

/**
 * Sample system state and system properties
 */
template <int dimension>
void sampler<dimension>::sample(bool force)
{
    uint64_t step = core->step_counter();
    bool is_sampling_step = false;

    if (!(step % statevars_interval_) || force) {
        BOOST_FOREACH (shared_ptr<observable_type> const& observable, observables) {
            observable->sample(core->time());
            is_sampling_step = true;
        }
        if (statevars_writer) {
            scoped_timer<timer> timer_(at_key<msv_output_>(runtime_));
            statevars_writer->write();
        }
    }

    // allow value 0 for trajectory_interval_
    if (((trajectory_interval_ && !(step % trajectory_interval_)) || force)
          && trajectory_writer) {
        trajectory_writer->append();
        is_sampling_step = true;
    }

    if (is_sampling_step)
        LOG_DEBUG("system state sampled at step " << step);
}

template <typename T>
static void register_lua(lua_State* L, char const* class_name)
{
    typedef typename T::core_type core_type;
    typedef typename T::observable_type observable_type;

    using namespace luabind;
    module(L)
    [
        namespace_("halmd_wrapper")
        [
            class_<T, shared_ptr<T> >(class_name)
                .def(constructor<
                    shared_ptr<core_type>
                  , uint64_t
                  , unsigned int
                  , unsigned int
                >())
                .def("run", &T::run)
                .def("register_runtimes", &T::register_runtimes)
                .def_readwrite("observables", &T::observables)
                .def_readwrite("statevars_writer", &T::statevars_writer)
                .def_readwrite("trajectory_writer", &T::trajectory_writer)
                .def_readwrite("profile_writers", &T::profile_writers)
                .property("steps", &T::steps)
                .property("time", &T::time)
                .scope
                [
                    def("options", &T::options)
                ]
        ]
    ];
}

static __attribute__((constructor)) void register_lua()
{
    lua_wrapper::register_(0) //< distance of derived to base class
    [
        bind(&register_lua<sampler<3> >, _1, "sampler_3_")
    ]
    [
        bind(&register_lua<sampler<2> >, _1, "sampler_2_")
    ];
}

// explicit instantiation
template class sampler<3>;
template class sampler<2>;

} // namespace halmd
