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

#include <boost/bind.hpp>

#include <test/performance/callbacks_extern.hpp>

using namespace boost;
using namespace std;

/**
 * To protect against link-time optimization across translation units
 * (GCC 4.5, XL C++ 11), all functions have the noinline attribute.
 */

/**
 * Make function to noop using boost::bind
 */
__attribute__((noinline)) function<void (double)> bind_noop()
{
    return bind(&noop, _1);
}

/**
 * Add noop function to signal
 */
__attribute__((noinline)) void bind_noop(halmd::signal<void (double)>& sig)
{
    sig.connect(bind_noop());
}

/**
 * An empty function
 */
__attribute__((noinline)) void noop(double) {}
