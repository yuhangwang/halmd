/* Lennard-Jones fluid kernel
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

#ifndef LJGPU_MDSIM_GPU_LJFLUID_SQUARE_HPP
#define LJGPU_MDSIM_GPU_LJFLUID_SQUARE_HPP

#include <cuda_wrapper.hpp>
#include <ljgpu/mdsim/impl.hpp>
#include <ljgpu/mdsim/gpu/base.hpp>
#include <ljgpu/rng/gpu/uint48.cuh>

namespace ljgpu { namespace gpu
{

template <>
struct ljfluid_base<ljfluid_impl_gpu_square>
: public ljfluid_base<ljfluid_impl_gpu_base>
{
};

template <>
struct ljfluid<ljfluid_impl_gpu_square<3> >
: public ljfluid_base<ljfluid_impl_gpu_square>, public ljfluid<ljfluid_impl_gpu_base<3> >
{
    static cuda::function<void (float4*, float4*, float4*, float*, float*)> mdstep;
    static cuda::function<void (float4*, float4*, float4*, float*, float*)> mdstep_nvt;
};

template <>
struct ljfluid<ljfluid_impl_gpu_square<2> >
: public ljfluid_base<ljfluid_impl_gpu_square>, public ljfluid<ljfluid_impl_gpu_base<2> >
{
    static cuda::function<void (float2*, float2*, float2*, float*, float*)> mdstep;
    static cuda::function<void (float2*, float2*, float2*, float*, float*)> mdstep_nvt;
};

}} // namespace ljgpu::gpu

#endif /* ! LJGPU_MDSIM_GPU_LJFLUID_SQUARE_HPP */
