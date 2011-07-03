/*
 * Copyright © 2008-2010  Peter Colberg
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

#ifndef HALMD_MDSIM_HOST_SORT_HILBERT_HPP
#define HALMD_MDSIM_HOST_SORT_HILBERT_HPP

#include <lua.hpp>

#include <halmd/mdsim/box.hpp>
#include <halmd/mdsim/host/binning.hpp>
#include <halmd/mdsim/host/particle.hpp>
#include <halmd/mdsim/sort.hpp>

namespace halmd {
namespace mdsim {
namespace host {
namespace sorts {

template <int dimension, typename float_type>
class hilbert
  : public mdsim::sort<dimension>
{
public:
    typedef mdsim::sort<dimension> _Base;
    typedef host::particle<dimension, float_type> particle_type;
    typedef typename particle_type::vector_type vector_type;
    typedef mdsim::box<dimension> box_type;
    typedef host::binning<dimension, float_type> binning_type;

    static char const* module_name() { return "hilbert"; }

    static void luaopen(lua_State* L);

    hilbert(
        boost::shared_ptr<particle_type> particle
      , boost::shared_ptr<box_type const> box
      , boost::shared_ptr<binning_type> binning
    );
    virtual void order();

private:
    typedef typename binning_type::cell_size_type cell_size_type;
    typedef typename binning_type::cell_list cell_list;
    typedef typename binning_type::cell_lists cell_lists;

    unsigned int map(vector_type r, unsigned int depth);

    boost::shared_ptr<particle_type> particle_;
    boost::shared_ptr<box_type const> box_;
    boost::shared_ptr<binning_type> binning_;

    /** 1-dimensional Hilbert curve mapping of cell lists */
    std::vector<cell_list const*> map_;
};

} // namespace sorts
} // namespace host
} // namespace mdsim
} // namespace halmd

#endif /* ! HALMD_MDSIM_HOST_SORT_HILBERT_HPP */
