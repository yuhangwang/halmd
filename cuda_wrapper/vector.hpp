/* cuda_wrapper/vector.hpp
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

#ifndef CUDA_VECTOR_HPP
#define CUDA_VECTOR_HPP

#include <cuda_runtime.h>
#include <cuda_wrapper/allocator.hpp>
#include <cuda_wrapper/symbol.hpp>
#include <cuda_wrapper/host/vector.hpp>
#include <cuda_wrapper/stream.hpp>
#include <algorithm>
#include <assert.h>

namespace cuda
{

namespace host
{

template <typename T, typename Alloc>
class vector;

}

template <typename T>
class symbol;


/**
 * auto-pointer for linear global device memory
 */
template <typename T, typename Alloc>
class auto_ptr
{
public:
    typedef Alloc _Alloc;
    typedef T value_type;
    typedef size_t size_type;

protected:
    value_type* _ptr;
    size_type _size;

public:
    /**
     * allocate global device memory
     */
    auto_ptr(size_type size) : _ptr(_Alloc().allocate(size)), _size(size) { }

    /**
     * deallocate global device memory
     */
    ~auto_ptr()
    {
	_Alloc().deallocate(_ptr, _size);
    }

    /**
     * returns device pointer to allocated device memory
     */
    value_type* get() const
    {
	return _ptr;
    }

    /**
     * returns size
     */
    size_type size() const
    {
	return _size;
    }
};


/**
 * vector pseudo-container for linear global device memory
 */
template <typename T>
class vector
{
public:
    typedef allocator<T> _Alloc;
    typedef vector<T> vector_type;
    typedef T value_type;
    typedef size_t size_type;

protected:
    auto_ptr<value_type, _Alloc> _ptr;

public:
    /**
     * initialize device vector of given size
     */
    vector(size_type size): _ptr(size) { }

    /**
     * initialize device vector with content of device vector
     */
    vector(const vector_type& v): _ptr(v.size())
    {
	memcpy(v);
    }

    /**
     * initialize device vector with content of host vector
     */
    template <typename Alloc>
    vector(const host::vector<value_type, Alloc>& v): _ptr(v.size())
    {
	memcpy(v);
    }

    /**
     * initialize device vector with content of device symbol
     */
    vector(const symbol<value_type> &v): _ptr(v.size())
    {
	memcpy(v);
    }

    /**
     * copy from device memory area to device memory area
     */
    void memcpy(const vector_type& v)
    {
	assert(v.size() == size());
	CUDA_CALL(cudaMemcpy(get(), v.get(), v.size() * sizeof(value_type), cudaMemcpyDeviceToDevice));
    }

    /**
     * copy from host memory area to device memory area
     */
    template <typename Alloc>
    void memcpy(const host::vector<value_type, Alloc>& v)
    {
	assert(v.size() == size());
	CUDA_CALL(cudaMemcpy(get(), &v.front(), v.size() * sizeof(value_type), cudaMemcpyHostToDevice));
    }

#ifdef CUDA_WRAPPER_ASYNC_API

    /**
     * asynchronous copy from device memory area to device memory area
     */
    void memcpy(const vector_type& v, const stream& stream)
    {
	assert(v.size() == size());
	CUDA_CALL(cudaMemcpyAsync(get(), v.get(), v.size() * sizeof(value_type), cudaMemcpyDeviceToDevice, stream._stream));
    }

    /**
     * asynchronous copy from host memory area to device memory area
     *
     * requires page-locked host memory (default host vector allocator)
     */
    void memcpy(const host::vector<value_type, host::allocator<value_type> >& v, const stream& stream)
    {
	assert(v.size() == size());
	CUDA_CALL(cudaMemcpyAsync(get(), &v.front(), v.size() * sizeof(value_type), cudaMemcpyHostToDevice, stream._stream));
    }

#endif /* CUDA_WRAPPER_ASYNC_API */

    /*
     * copy from device symbol to device memory area
     */
    void memcpy(const symbol<value_type>& symbol)
    {
	assert(symbol.size() == size());
	CUDA_CALL(cudaMemcpyFromSymbol(get(), reinterpret_cast<const char *>(symbol.get()), symbol.size() * sizeof(value_type), 0, cudaMemcpyDeviceToDevice));
    }

    /**
     * assign content of device vector to device vector
     */
    vector_type& operator=(const vector_type& v)
    {
	if (this != &v) {
	    memcpy(v);
	}
	return *this;
    }

    /**
     * assign content of host vector to device vector
     */
    template <typename Alloc>
    vector_type& operator=(const host::vector<value_type, Alloc>& v)
    {
	memcpy(v);
	return *this;
    }

    /**
     * assign content of device symbol to device vector
     */
    vector_type& operator=(const symbol<value_type>& symbol)
    {
	memcpy(symbol);
	return *this;
    }

    /**
     * assign copies of value to device vector
     */
    vector_type& operator=(const value_type& value)
    {
	host::vector<value_type, host::allocator<value_type> > v(size(), value);
	memcpy(v);
	return *this;
    }

    /**
     * swap device memory areas with another device vector
     */
    static void swap(vector_type& a, vector_type& b)
    {
	std::swap(a._size, b._size);
	std::swap(a._ptr, b._ptr);
    }

    /**
     * returns element count of device vector
     */
    size_type size() const
    {
	return _ptr.size();
    }

    /**
     * returns device pointer to allocated device memory
     */
    value_type* get() const
    {
	return _ptr.get();
    }
};

}

#endif /* CUDA_VECTOR_HPP */
