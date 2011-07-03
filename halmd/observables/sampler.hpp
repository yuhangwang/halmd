/*
 * Copyright © 2010-2011  Felix Höfling and Peter Colberg
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

#ifndef HALMD_OBSERVABLES_SAMPLER_HPP
#define HALMD_OBSERVABLES_SAMPLER_HPP

#include <utility> // pair

#include <halmd/mdsim/clock.hpp>
#include <halmd/mdsim/core.hpp>
#include <halmd/utility/profiler.hpp>
#include <halmd/utility/signal.hpp>

namespace halmd
{
namespace observables
{

/**
 * Sampler to run Molecular Dynamics simulation
 */
class sampler
{
public:
    typedef mdsim::clock clock_type;
    typedef clock_type::step_type step_type;
    typedef clock_type::time_type time_type;
    typedef mdsim::core core_type;
    typedef utility::profiler profiler_type;
    typedef halmd::signal<void (step_type)> signal_type;
    typedef signal_type::slot_function_type slot_function_type;

    struct runtime
    {
        typedef profiler_type::accumulator_type accumulator_type;
        accumulator_type total;
    };

    sampler(
        boost::shared_ptr<clock_type const> clock
      , boost::shared_ptr<core_type> core
      , step_type steps
    );
    virtual void run();
    void register_runtimes(profiler_type& profiler);
    void on_start(slot_function_type const& slot);
    void on_prepare(slot_function_type const& slot, step_type interval);
    void on_sample(slot_function_type const& slot, step_type interval);
    void on_finish(slot_function_type const& slot);

    /** total number of integration steps */
    step_type steps() const
    {
        return steps_;
    }

    /** total integration time in MD units */
    time_type total_time() const
    {
        return total_time_;
    }

private:
    void prepare(slot_function_type const& slot, step_type interval, step_type step) const;
    void sample(slot_function_type const& slot, step_type interval, step_type step) const;

    /** Molecular Dynamics simulation clock */
    boost::shared_ptr<clock_type const> clock_;
    /** Molecular Dynamics simulation core */
    boost::shared_ptr<core_type> core_;
    /** total number of integration steps */
    step_type steps_;
    /** total integration time in MD units */
    time_type total_time_;
    /** profiling runtime accumulators */
    runtime runtime_;
    /** signal emitted before starting simulation run */
    signal_type on_start_;
    /** signal emitted before MD integration step */
    signal_type on_prepare_;
    /** signal emitted after MD integration step */
    signal_type on_sample_;
    /** signal emitted after finishing simulation run */
    signal_type on_finish_;
};

} // namespace observables

} // namespace halmd

#endif /* ! HALMD_OBSERVABLES_SAMPLER_HPP */
