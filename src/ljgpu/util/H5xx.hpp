/* HDF5 C++ extensions
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

#ifndef LJGPU_UTIL_H5XX_HPP
#define LJGPU_UTIL_H5XX_HPP

#include <H5Cpp.h>
#include <boost/array.hpp>
#include <boost/multi_array.hpp>
#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>
#include <vector>

namespace H5xx
{

/*
 * fundamental type to HDF5 native data type translation
 */
template <typename T>
struct ctype
{
    static H5::PredType const& type;
};

template <typename T>
struct is_boost_array : public boost::false_type {};

template <typename T, size_t size>
struct is_boost_array<boost::array<T, size> >: public boost::true_type {};

template <typename T>
struct is_boost_multi_array : public boost::false_type {};

template <typename T, size_t dimension>
struct is_boost_multi_array<boost::multi_array<T, dimension> >: public boost::true_type {};

template <typename T>
struct is_vector : public boost::false_type {};

template <typename T>
struct is_vector<std::vector<T> >: public boost::true_type {};


/**
 * HDF5 attribute
 */
class attribute
{
public:
    attribute(H5::Group const& node, std::string const& name)
	: m_node(node), m_name(name) {}

    template <typename T>
    typename boost::enable_if<boost::is_fundamental<T>, attribute&>::type
    operator=(T const& value);
    template <typename T>
    typename boost::enable_if<boost::is_fundamental<T>, T>::type
    as();

    template <typename T>
    typename boost::enable_if<boost::is_same<T, std::string>, attribute&>::type
    operator=(T const& value);
    template <typename T>
    typename boost::enable_if<boost::is_same<T, std::string>, T>::type
    as();

    template <typename T>
    typename boost::enable_if<boost::is_same<T, char const*>, attribute&>::type
    operator=(T value);

    template <typename T>
    typename boost::enable_if<is_boost_array<T>, attribute&>::type
    operator=(T const& value);
    template <typename T>
    typename boost::enable_if<is_boost_array<T>, T>::type
    as();

    template <typename T>
    typename boost::enable_if<is_boost_multi_array<T>, attribute&>::type
    operator=(T const& value);
    template <typename T>
    typename boost::enable_if<is_boost_multi_array<T>, T>::type
    as();

    template <typename T>
    typename boost::enable_if<is_vector<T>, attribute&>::type
    operator=(T const& value);
    template <typename T>
    typename boost::enable_if<is_vector<T>, T>::type
    as();

private:
    /** group which attribute belongs to */
    H5::Group m_node;
    /** attribute name */
    std::string m_name;
};

/**
 * HDF5 group
 */
class group : public H5::Group
{
public:
    group() {}
    group(H5::Group const& node) : H5::Group(node) {}

    /**
     * returns existing or creates attribute
     */
    attribute operator[](char const* name) const
    {
	return attribute(*this, name);
    }
};

template <typename Exception>
class no_autoprint : public Exception
{
public:
    no_autoprint()
    {
	Exception::getAutoPrint(func, &client_data);
	Exception::dontPrint();
    }

    ~no_autoprint()
    {
	Exception::setAutoPrint(func, client_data);
    }

private:
    H5E_auto_t func;
    void* client_data;
};

#define H5XX_NO_AUTO_PRINT(exception) H5xx::no_autoprint<exception> __no_autoprint;

/*
 * create and write fundamental type attribute
 */
template <typename T>
typename boost::enable_if<boost::is_fundamental<T>, attribute&>::type
attribute::operator=(T const& value)
{
    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	attr = m_node.createAttribute(m_name, ctype<T>::type, H5S_SCALAR);
    }
    attr.write(ctype<T>::type, &value);
    return *this;
}

/**
 * read fundamental type attribute
 */
template <typename T>
typename boost::enable_if<boost::is_fundamental<T>, T>::type
attribute::as()
{
    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	throw;
    }
    T value;
    attr.read(ctype<T>::type, &value);
    return value;
}

/**
 * create and write string attribute
 */
template <typename T>
typename boost::enable_if<boost::is_same<T, std::string>, attribute&>::type
attribute::operator=(T const& value)
{
    H5::StrType tid(H5::PredType::C_S1, 256);
    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	attr = m_node.createAttribute(m_name, tid, H5S_SCALAR);
    }
    attr.write(tid, value.c_str());
    return *this;
}

/**
 * read string attribute
 */
template <typename T>
typename boost::enable_if<boost::is_same<T, std::string>, T>::type
attribute::as()
{
    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	throw;
    }
    // fixed string length includes terminating NULL character
    char value[256];
    attr.read(H5::StrType(H5::PredType::C_S1, 256), value);
    return value;
}

/**
 * create and write C string attribute
 */
template <typename T>
typename boost::enable_if<boost::is_same<T, char const*>, attribute&>::type
attribute::operator=(T value)
{
    H5::StrType tid(H5::PredType::C_S1, 256);
    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	attr = m_node.createAttribute(m_name, tid, H5S_SCALAR);
    }
    attr.write(tid, value);
    return *this;
}

/*
 * create and write fixed-size array type attribute
 */
template <typename T>
typename boost::enable_if<is_boost_array<T>, attribute&>::type
attribute::operator=(T const& value)
{
    typedef typename T::value_type value_type;
    enum { size = T::static_size };

    hsize_t dim[1] = { size };
    H5::DataSpace ds(1, dim);
    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	attr = m_node.createAttribute(m_name, ctype<value_type>::type, ds);
    }
    attr.write(ctype<value_type>::type, value.data());
    return *this;
}

/**
 * read fixed-size array type attribute
 */
template <typename T>
typename boost::enable_if<is_boost_array<T>, T>::type
attribute::as()
{
    typedef typename T::value_type value_type;
    enum { size = T::static_size };

    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	throw;
    }

    H5::DataSpace ds(attr.getSpace());
    if (!ds.isSimple()) {
	throw H5::AttributeIException("H5xx::attribute::as", "attribute dataspace is not simple");
    }
    if (ds.getSimpleExtentNdims() != 1) {
	throw H5::AttributeIException("H5xx::attribute::as", "attribute dataspace is not one-dimensional");
    }
    hsize_t dim[1];
    ds.getSimpleExtentDims(dim);
    if (dim[0] != size) {
	throw H5::AttributeIException("H5xx::attribute::as", "attribute dataspace does not match array size");
    }

    boost::array<value_type, size> value;
    attr.read(ctype<value_type>::type, value.data());
    return value;
}

/*
 * create and write multi-dimensional array type attribute
 */
template <typename T>
typename boost::enable_if<is_boost_multi_array<T>, attribute&>::type
attribute::operator=(T const& value)
{
    typedef typename T::value_type value_type;
    enum { dimension = T::dimensionality };

    hsize_t dim[dimension];
    std::copy(value.shape(), value.shape() + dimension, dim);
    H5::DataSpace ds(dimension, dim);
    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	attr = m_node.createAttribute(m_name, ctype<value_type>::type, ds);
    }
    attr.write(ctype<value_type>::type, value.data());
    return *this;
}

/**
 * read multi-dimensional array type attribute
 */
template <typename T>
typename boost::enable_if<is_boost_multi_array<T>, T>::type
attribute::as()
{
    typedef typename T::value_type value_type;
    enum { dimension = T::dimensionality };

    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	throw;
    }

    H5::DataSpace ds(attr.getSpace());
    if (!ds.isSimple()) {
	throw H5::AttributeIException("H5xx::attribute::as", "attribute dataspace is not simple");
    }
    if (ds.getSimpleExtentNdims() != dimension) {
	throw H5::AttributeIException("H5xx::attribute::as", "attribute dataspace dimensionality mismatch");
    }
    hsize_t dim[dimension];
    ds.getSimpleExtentDims(dim);
    boost::array<size_t, dimension> shape;
    std::copy(dim, dim + dimension, shape.begin());
    boost::multi_array<value_type, dimension> value(shape);
    attr.read(ctype<value_type>::type, value.data());
    return value;
}

/*
 * create and write vector type attribute
 */
template <typename T>
typename boost::enable_if<is_vector<T>, attribute&>::type
attribute::operator=(T const& value)
{
    typedef typename T::value_type value_type;

    hsize_t dim[1] = { value.size() };
    H5::DataSpace ds(1, dim);
    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	attr = m_node.createAttribute(m_name, ctype<value_type>::type, ds);
    }
    attr.write(ctype<value_type>::type, value.data());
    return *this;
}

/**
 * read vector type attribute
 */
template <typename T>
typename boost::enable_if<is_vector<T>, T>::type
attribute::as()
{
    typedef typename T::value_type value_type;

    H5::Attribute attr;
    try {
	H5XX_NO_AUTO_PRINT(H5::AttributeIException);
	attr = m_node.openAttribute(m_name);
    }
    catch (H5::AttributeIException const&) {
	throw;
    }

    H5::DataSpace ds(attr.getSpace());
    if (!ds.isSimple()) {
	throw H5::AttributeIException("H5xx::attribute::as", "attribute dataspace is not simple");
    }
    if (ds.getSimpleExtentNdims() != 1) {
	throw H5::AttributeIException("H5xx::attribute::as", "attribute dataspace is not one-dimensional");
    }
    hsize_t dim[1];
    ds.getSimpleExtentDims(dim);
    std::vector<value_type> value(dim[0]);
    attr.read(ctype<value_type>::type, value.data());
    return value;
}

} // namespace H5xx

#endif /* ! LJGPU_UTIL_H5XX_HPP */
