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

#ifndef HALMD_MAIN_HPP
#define HALMD_MAIN_HPP

#include <halmd/utility/options.hpp>
#include <halmd/utility/module.hpp>

namespace halmd
{

class main
{
public:
    // module definitions
    typedef main _Self;
    static void options(po::options_description& desc) {}
    static void depends() {}
    static void select(po::options const& vm) {}

    main(modules::factory& factory, po::options const& vm) {}
    virtual ~main() {}
    virtual void load_wrapper() = 0;
    virtual void load_library() = 0;
    virtual void run() = 0;
};

} // namespace halmd

#endif /* ! HALMD_MAIN_HPP */
