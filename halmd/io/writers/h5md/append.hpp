/*
 * Copyright © 2014      Felix Höfling
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

#ifndef HALMD_IO_WRITERS_H5MD_APPEND_HPP
#define HALMD_IO_WRITERS_H5MD_APPEND_HPP

#include <boost/multi_array.hpp>
#include <functional>
#include <lua.hpp>

#include <h5xx/h5xx.hpp>
#include <halmd/mdsim/clock.hpp>
#include <halmd/utility/signal.hpp>

namespace halmd {
namespace io {
namespace writers {
namespace h5md {

/**
 * H5MD dataset writer (append mode)
 *
 * This module implements collective writing to one or multiple H5MD
 * datasets, where each dataset is a time series. Upon initialisation,
 * the writer is assigned a collective H5MD group. A dataset within this
 * group is created by connecting a data slot to the on_write signal.
 * All datasets share common step and time datasets, which are linked
 * into each dataset group upon connection.
 *
 * The writer provides a common write slot, which may be connected to
 * the sampler to write to the datasets at a fixed interval. Further
 * signals on_prepend_write and on_append_write are provided to call
 * arbitrary slots before and after writing.
 */
class append
{
private:
    typedef signal<void ()> signal_type;
public:
    typedef mdsim::clock clock_type;
    typedef clock_type::step_type step_type;
    typedef clock_type::time_type time_type;
    typedef signal_type::slot_function_type slot_function_type;
    /**
     * For the truncate reader/writer, a subgroup is defined as the dataset
     * which contains the data to be read or written.
     * For the append reader/writer, a subgroup is defined as the group
     * contains the data to be read or written. Additional attributes
     * should always be attached to the subgroup, never the sample
     * dataset.
     * To give both writers the same API for convenient use in template
     * functions in C++ unit tests, we define a subgroup type.
     */
    typedef H5::Group subgroup_type;

    /** open writer group and create time and step datasets */
    append(
        H5::Group const& root
      , std::vector<std::string> const& location
      , std::shared_ptr<clock_type const> clock
    );
    /** connect data slot for writing dataset, return created HDF5 group by reference */
    template <typename T>
    connection on_write(
        subgroup_type& group
      , std::function<T ()> const& slot
      , std::vector<std::string> const& location
    );
    /** connect data slot for writing an accumulated dataset, return created HDF5 group by reference */
    template <typename T>
    connection on_write_averaged(
        subgroup_type& group
      , std::function<T ()> const& value_slot
      , std::function<T ()> const& error_slot
      , std::function<uint64_t ()> const& count_slot
      , std::vector<std::string> const& location
    );
    /** connect slot called before writing */
    connection on_prepend_write(slot_function_type const& slot);
    /** connect slot called after writing */
    connection on_append_write(slot_function_type const& slot);
    /** append datasets */
    void write();
    /** Lua bindings */
    static void luaopen(lua_State* L);

    /**
     * returns writer group
     */
    H5::Group const& group() const
    {
        return group_;
    }

private:
    /** append shared step and time datasets */
    void write_step_time();

    /** writer group */
    H5::Group group_;
    /** signal emitted for writing datasets */
    signal_type on_write_;
    /** signal emitted before writing datasets */
    signal_type on_prepend_write_;
    /** signal emitted before after datasets */
    signal_type on_append_write_;
    /** simulation step and time */
    std::shared_ptr<clock_type const> clock_;
    /** shared step dataset */
    H5::DataSet step_dataset_;
    /** shared time dataset */
    H5::DataSet time_dataset_;
    /** last simulation step written */
    int64_t last_step_;
    /** last simulation time written */
    time_type last_time_;
};

} // namespace h5md
} // namespace writers
} // namespace io
} // namespace halmd

#endif /* ! HALMD_IO_WRITERS_H5MD_APPEND_HPP */
