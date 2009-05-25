/* DSFUN division test
 *
 * Copyright (C) 2009  Peter Colberg
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

#ifndef TEST_DSFUN_DIV_HPP
#define TEST_DSFUN_DIV_HPP

#include <cuda_wrapper.hpp>
#include <ljgpu/math/gpu/dsfun.cuh>

extern cuda::function<void (dfloat const*, dfloat const*, dfloat*)> kernel_div;

#endif /* ! TEST_DSFUN_DIV_HPP */
