/*
 * Copyright © 2008-2011  Peter Colberg and Felix Höfling
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

#ifndef HALMD_MDSIM_CORE_HPP
#define HALMD_MDSIM_CORE_HPP

#include <boost/shared_ptr.hpp>

#include <halmd/mdsim/clock.hpp>
#include <halmd/utility/profiler.hpp>
#include <halmd/utility/signal.hpp>

/** HAL’s MD package */
namespace halmd
{
/** Molecular Dynamics simulation modules */
namespace mdsim
{

class core
{
public:
    typedef halmd::signal<void ()> signal_type;
    typedef signal_type::slot_function_type slot_function_type;
    typedef mdsim::clock clock_type;
    typedef utility::profiler profiler_type;

    struct runtime_type
    {
        typedef profiler_type::accumulator_type accumulator_type;
        accumulator_type prepare;
        accumulator_type mdstep;
    };

    core(boost::shared_ptr<clock_type> clock);
    void register_runtimes(profiler_type& profiler) const;
    void prepare();
    void mdstep();

    /** profiling runtime accumulators */
    runtime_type const& runtime() const
    {
        return runtime_;
    }

    void on_prepend_prepare(slot_function_type const& slot)
    {
        on_prepend_prepare_.connect(slot);
    }

    void on_prepare(slot_function_type const& slot)
    {
        on_prepare_.connect(slot);
    }

    void on_append_prepare(slot_function_type const& slot)
    {
        on_append_prepare_.connect(slot);
    }

    void on_prepend_integrate(slot_function_type const& slot)
    {
        on_prepend_integrate_.connect(slot);
    }

    void on_integrate(slot_function_type const& slot)
    {
        on_integrate_.connect(slot);
    }

    void on_append_integrate(slot_function_type const& slot)
    {
        on_append_integrate_.connect(slot);
    }

    void on_prepend_force(slot_function_type const& slot)
    {
        on_prepend_force_.connect(slot);
    }

    void on_force(slot_function_type const& slot)
    {
        on_force_.connect(slot);
    }

    void on_append_force(slot_function_type const& slot)
    {
        on_append_force_.connect(slot);
    }

    void on_prepend_finalize(slot_function_type const& slot)
    {
        on_prepend_finalize_.connect(slot);
    }

    void on_finalize(slot_function_type const& slot)
    {
        on_finalize_.connect(slot);
    }

    void on_append_finalize(slot_function_type const& slot)
    {
        on_append_finalize_.connect(slot);
    }

private:
    boost::shared_ptr<clock_type> clock_;
    signal_type on_prepend_prepare_;
    signal_type on_prepare_;
    signal_type on_append_prepare_;
    signal_type on_prepend_integrate_;
    signal_type on_integrate_;
    signal_type on_append_integrate_;
    signal_type on_prepend_force_;
    signal_type on_force_;
    signal_type on_append_force_;
    signal_type on_prepend_finalize_;
    signal_type on_finalize_;
    signal_type on_append_finalize_;
    runtime_type runtime_;
};

} // namespace mdsim

} // namespace halmd

#endif /* ! HALMD_MDSIM_CORE_HPP */
