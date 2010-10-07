/* Time correlation functions for CUDA
 *
 * Copyright © 2008-2009  Peter Colberg
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

#ifndef HALMD_ALGORITHM_GPU_TCF_HPP
#define HALMD_ALGORITHM_GPU_TCF_HPP

#include <cuda_wrapper.hpp>
#include <halmd/math/gpu/dsfloat.cuh>

namespace halmd { namespace gpu
{

struct tcf_base
{
    enum {
        BLOCKS = 32,
        THREADS = 256,
    };

    struct velocity_autocorrelation_fastest {
        static cuda::symbol<float> min_sq_v;
    };
    struct velocity_autocorrelation_slowest {
        static cuda::symbol<float> max_sq_v;
    };
};

template <int dimension>
struct tcf;

template <>
struct tcf<3> : public tcf_base
{
    static cuda::function<void (float4 const*, float4 const*, uint*, dsfloat*, dsfloat*, uint)>
               mean_square_displacement;
    static cuda::function<void (float4 const*, float4 const*, uint*, dsfloat*, dsfloat*, uint)>
               mean_quartic_displacement;
    static cuda::function<void (float4 const*, float4 const*, uint*, dsfloat*, dsfloat*, uint)>
               velocity_autocorrelation;
    static cuda::function<void (float4 const*, float4 const*, uint*, dsfloat*, dsfloat*, uint)>
               velocity_autocorrelation_fastest;
    static cuda::function<void (float4 const*, float4 const*, uint*, dsfloat*, dsfloat*, uint)>
               velocity_autocorrelation_slowest;
    static cuda::function<void (float4 const*, float4 const*, float3 const, dsfloat*, uint)>
        incoherent_scattering_function;
    static cuda::function<void (float4 const*, float3 const, dsfloat*, dsfloat*, uint)>
        coherent_scattering_function;
};

template <>
struct tcf<2> : public tcf_base
{
    static cuda::function<void (float2 const*, float2 const*, uint*, dsfloat*, dsfloat*, uint)>
               mean_square_displacement;
    static cuda::function<void (float2 const*, float2 const*, uint*, dsfloat*, dsfloat*, uint)>
               mean_quartic_displacement;
    static cuda::function<void (float2 const*, float2 const*, uint*, dsfloat*, dsfloat*, uint)>
               velocity_autocorrelation;
    static cuda::function<void (float2 const*, float2 const*, uint*, dsfloat*, dsfloat*, uint)>
               velocity_autocorrelation_fastest;
    static cuda::function<void (float2 const*, float2 const*, uint*, dsfloat*, dsfloat*, uint)>
               velocity_autocorrelation_slowest;
    static cuda::function<void (float2 const*, float2 const*, float2 const, dsfloat*, uint)>
        incoherent_scattering_function;
    static cuda::function<void (float2 const*, float2 const, dsfloat*, dsfloat*, uint)>
        coherent_scattering_function;
};

}} // namespace halmd::gpu

#endif /* ! HALMD_ALGORITHM_GPU_TCF_HPP */
