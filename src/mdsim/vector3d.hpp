/* 3-dimensional floating-point vector
 *
 * Copyright (C) 2008  Peter Colberg
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

#ifndef MDSIM_VECTOR3D_HPP
#define MDSIM_VECTOR3D_HPP

#include <boost/array.hpp>
#include <cmath>
#include <iostream>


/**
 * 3-dimensional floating-point vector
 */
template <typename T>
class vector3d
{
public:
    typedef T value_type;

public:
    T x, y, z;

public:
    vector3d()
    {
    }

    /**
     * initialization by vector
     */
    template <typename U>
    vector3d(vector3d<U> const& v) : x(v.x), y(v.y), z(v.z)
    {
    }

    /**
     * initialization by scalar
     */
    template <typename U>
    vector3d(U const& s) : x(s), y(s), z(s)
    {
    }

    /**
     * initialization by scalar components
     */
    template <typename U, typename V, typename W>
    vector3d(U const& x, V const& y, W const& z) : x(x), y(y), z(z)
    {
    }

    /**
     * initialization by array
     */
    template <typename U>
    vector3d(boost::array<U, 3> const& v) : x(v[0]), y(v[1]), z(v[2])
    {
    }

    /**
     * dimension of vector space
     */
    static unsigned int dim()
    {
	return 3;
    }

    /**
     * equality comparison
     */
    bool operator==(vector3d<T> const& v) const
    {
        return (v.x == x && v.y == y && v.z == z);
    }

    /**
     * inequality comparison
     */
    bool operator!=(vector3d<T> const& v) const
    {
        return (v.x != x || v.y != y || v.z != z);
    }

    /**
     * componentwise less than comparison
     */
    bool operator<(vector3d<T> const& v) const
    {
	return (v.x < x && v.y < y && v.z < z);
    }

    /**
     * componentwise greater than comparison
     */
    bool operator>(vector3d<T> const& v) const
    {
	return (v.x > x && v.y > y && v.z > z);
    }

    /**
     * componentwise less than or equal to comparison
     */
    bool operator<=(vector3d<T> const& v) const
    {
	return (v.x <= x && v.y <= y && v.z <= z);
    }

    /**
     * componentwise greater than or equal to comparison
     */
    bool operator>=(vector3d<T> const& v) const
    {
	return (v.x >= x && v.y >= y && v.z >= z);
    }

    /**
     * assignment by vector
     */
    vector3d<T>& operator=(vector3d<T> const& v)
    {
	x = v.x;
	y = v.y;
	z = v.z;
	return *this;
    }

    /**
     * assignment by scalar
     */
    vector3d<T>& operator=(T const& s)
    {
	x = s;
	y = s;
	z = s;
	return *this;
    }

    /**
     * assignment by componentwise vector addition
     */
    vector3d<T>& operator+=(vector3d<T> const& v)
    {
	x += v.x;
	y += v.y;
	z += v.z;
	return *this;
    }

    /**
     * assignment by componentwise vector subtraction
     */
    vector3d<T>& operator-=(vector3d<T> const& v)
    {
	x -= v.x;
	y -= v.y;
	z -= v.z;
	return *this;
    }

    /**
     * assignment by scalar multiplication
     */
    vector3d<T>& operator*=(T const& s)
    {
	x *= s;
	y *= s;
	z *= s;
	return *this;
    }

    /**
     * assignment by scalar division
     */
    vector3d<T>& operator/=(T const& s)
    {
	x /= s;
	y /= s;
	z /= s;
	return *this;
    }

    /**
     * componentwise vector addition
     */
    friend vector3d<T> operator+(vector3d<T> v, vector3d<T> const& w)
    {
	v.x += w.x;
	v.y += w.y;
	v.z += w.z;
	return v;
    }

    /**
     * componentwise vector subtraction
     */
    friend vector3d<T> operator-(vector3d<T> v, vector3d<T> const& w)
    {
	v.x -= w.x;
	v.y -= w.y;
	v.z -= w.z;
	return v;
    }

    /**
     * scalar product
     */
    T operator*(vector3d<T> const& v) const
    {
	return x * v.x + y * v.y + z * v.z;
    }

    /**
     * scalar multiplication
     */
    friend vector3d<T> operator*(vector3d<T> v, T const& s)
    {
	v.x *= s;
	v.y *= s;
	v.z *= s;
	return v;
    }

    /**
     * scalar multiplication
     */
    friend vector3d<T> operator*(T const& s, vector3d<T> v)
    {
	v.x *= s;
	v.y *= s;
	v.z *= s;
	return v;
    }

    /**
     * scalar division
     */
    friend vector3d<T> operator/(vector3d<T> v, T const& s)
    {
	v.x /= s;
	v.y /= s;
	v.z /= s;
	return v;
    }

    /**
     * write vector components to output stream
     */
    friend std::ostream& operator<<(std::ostream& os, vector3d<T> const& v)
    {
	os << v.x << "\t" << v.y << "\t" << v.z;
	return os;
    }

    /**
     * read vector components from input stream
     */
    friend std::istream& operator>>(std::istream& is, vector3d<T>& v)
    {
	is >> v.x >> v.y >> v.z;
	return is;
    }
};

/**
 * componentwise round to nearest integer
 */
template <typename T>
vector3d<T> rint(vector3d<T> v);

template <>
vector3d<float> rint(vector3d<float> v)
{
    v.x = rintf(v.x);
    v.y = rintf(v.y);
    v.z = rintf(v.z);
    return v;
}

template <>
vector3d<double> rint(vector3d<double> v)
{
    v.x = rint(v.x);
    v.y = rint(v.y);
    v.z = rint(v.z);
    return v;
}

/**
 * componentwise round to nearest integer, away from zero
 */
template <typename T>
vector3d<T> round(vector3d<T> v);

template <>
vector3d<float> round(vector3d<float> v)
{
    v.x = roundf(v.x);
    v.y = roundf(v.y);
    v.z = roundf(v.z);
    return v;
}

template <>
vector3d<double> round(vector3d<double> v)
{
    v.x = round(v.x);
    v.y = round(v.y);
    v.z = round(v.z);
    return v;
}

/**
 * componentwise round to nearest integer not greater than argument
 */
template <typename T>
vector3d<T> floor(vector3d<T> v)
{
    v.x = std::floor(v.x);
    v.y = std::floor(v.y);
    v.z = std::floor(v.z);
    return v;
}

/**
 * componentwise round to nearest integer not less argument
 */
template <typename T>
vector3d<T> ceil(vector3d<T> v)
{
    v.x = std::ceil(v.x);
    v.y = std::ceil(v.y);
    v.z = std::ceil(v.z);
    return v;
}

/**
 * componentwise round to integer towards zero
 */
template <typename T>
vector3d<T> trunc(vector3d<T> v);

template <>
vector3d<float> trunc(vector3d<float> v)
{
    v.x = truncf(v.x);
    v.y = truncf(v.y);
    v.z = truncf(v.z);
    return v;
}

template <>
vector3d<double> trunc(vector3d<double> v)
{
    v.x = trunc(v.x);
    v.y = trunc(v.y);
    v.z = trunc(v.z);
    return v;
}

/**
 * componentwise square root function
 */
template <typename T>
vector3d<T> sqrt(vector3d<T> v)
{
    v.x = std::sqrt(v.x);
    v.y = std::sqrt(v.y);
    v.z = std::sqrt(v.z);
    return v;
}

/**
 * componentwise cosine function
 */
template <typename T>
vector3d<T> cos(vector3d<T> v)
{
    v.x = std::cos(v.x);
    v.y = std::cos(v.y);
    v.z = std::cos(v.z);
    return v;
}

/**
 * componentwise sine function
 */
template <typename T>
vector3d<T> sin(vector3d<T> v)
{
    v.x = std::sin(v.x);
    v.y = std::sin(v.y);
    v.z = std::sin(v.z);
    return v;
}

#endif /* ! MDSIM_VECTOR3D_HPP */
