/*
 * Copyright © 2015 Nicolas Höft
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

#include <halmd/utility/gpu/caching_array.cuh>

namespace halmd {
namespace detail {

// instantiation of the cub allocator, the boolean parameter
// determines whether this is a object in global scope or not (it is)
// and then will skip a call to CachingDeviceAllocator::FreeAllCached()
// in the d'tor
// This allocator will be instatiated during the nvcc host pass only
// (a call from device code would fail anyways)
#ifndef __CUDA_ARCH__
cub::CachingDeviceAllocator caching_allocator_(true);
#endif

} // namespace detail
} // namespace halmd
