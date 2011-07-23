/*
 * Copyright © 2011  Peter Colberg
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

#ifndef HALMD_IO_WRITERS_H5MD_TRUNCATE_HPP
#define HALMD_IO_WRITERS_H5MD_TRUNCATE_HPP

#include <lua.hpp>

#include <h5xx/h5xx.hpp>
#include <halmd/utility/signal.hpp>

namespace halmd {
namespace io {
namespace writers {
namespace h5md {

/**
 * H5MD dataset writer (truncate mode)
 *
 * This module implements collective writing to one or multiple H5MD
 * datasets. Upon initialisation, the writer is assigned a collective
 * H5MD group. A dataset within this group is created by connecting a
 * data slot to the on_write signal.
 *
 * The writer provides a common write slot, which may be connected to
 * the sampler to write to the datasets at a fixed interval. Further
 * signals on_prepend_write and on_append_write are provided to call
 * arbitrary slots before and after writing.
 */
class truncate
{
public:
    typedef signal<void ()> signal_type;
    typedef signal_type::slot_function_type slot_function_type;

    /** open writer group */
    truncate(
        H5::Group const& root
      , std::vector<std::string> const& location
    );
    /** write datasets */
    void write(uint64_t step);
    /** connect data slot for writing */
    template <typename slot_type>
    void on_write(
        H5::DataSet& dataset
      , slot_type const& slot
      , std::vector<std::string> const& location
    );
    /** connect slot called before writing */
    void on_prepend_write(signal<void (uint64_t)>::slot_function_type const& slot);
    /** connect slot called after writing */
    void on_append_write(signal<void (uint64_t)>::slot_function_type const& slot);
    /** Lua bindings */
    static void luaopen(lua_State* L);

private:
    /** writer group */
    H5::Group group_;
    /** signal emitted for writing datasets */
    signal_type on_write_;
    /** signal emitted before writing datasets */
    signal<void (uint64_t)> on_prepend_write_;
    /** signal emitted before after datasets */
    signal<void (uint64_t)> on_append_write_;
};

} // namespace h5md
} // namespace writers
} // namespace io
} // namespace halmd

#endif /* ! HALMD_IO_WRITERS_H5MD_TRUNCATE_HPP */
