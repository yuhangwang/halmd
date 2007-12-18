/* cuda_host_array.h
 *
 * Copyright (C) 2007  Peter Colberg
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

#ifndef __CUDA_HOST_ARRAY_H__
#define __CUDA_HOST_ARRAY_H__

#include <assert.h>

#include "cuda_base.h"
#include "cuda_error.h"
#include "cuda_array.h"


template <typename T>
class cuda_array;


template <typename T>
class cuda_host_array : public cuda_base
{
  friend class cuda_array<T>;

protected:
  T *host_ptr;
  int n;

public:
  cuda_host_array(int n): n(n)
  {
    CUDA_CALL(cudaMallocHost((void **) &host_ptr, n * sizeof(T)));
  }

  ~cuda_host_array()
  {
    CUDA_CALL(cudaFreeHost(host_ptr));
  }

  cuda_host_array<T>& operator=(const cuda_host_array<T>& array)
  {
    assert(array.n == n);
    memcpy(host_ptr, array.host_ptr, n * sizeof(T));
    return *this;
  }

  cuda_host_array<T>& operator=(const cuda_array<T>& array)
  {
    assert(array.n == n);
    CUDA_CALL(cudaMemcpy(host_ptr, array.dev_ptr, n * sizeof(T), cudaMemcpyDeviceToHost));
    return *this;
  }

  cuda_host_array<T>& operator=(const T& value)
  {
    for (int i = 0; i < n; i++) {
      host_ptr[i] = value;
    }
  }

  T& operator[](const int i)
  {
    assert(i < n);
    return host_ptr[i];
  }

  int dim() const
  {
    return n;
  }
};


#endif /* __CUDA_HOST_ARRAY_H__ */
