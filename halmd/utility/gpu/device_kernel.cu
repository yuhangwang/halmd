/*
 * Copyright © 2010  Peter Colberg
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

#include <halmd/utility/gpu/device_kernel.hpp>

namespace halmd
{
namespace utility { namespace gpu
{
namespace device_kernel
{

__global__ void arch(int* g_arch)
{
#if __CUDA_ARCH__ // work around error: identifier "__CUDA_ARCH__" is undefined
    *g_arch = __CUDA_ARCH__;
#endif
}

} // namespace device_kernel

namespace device_wrapper
{

cuda::function<void (int*)> arch(device_kernel::arch);

} // namespace device_wrapper

}} // namespace utility::gpu

} // namespace halmd