/*
 * Copyright © 2008-2010  Peter Colberg
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

#ifndef HALMD_UTILITY_MODULE_WRAPPER_HPP
#define HALMD_UTILITY_MODULE_WRAPPER_HPP

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <string>

#include <halmd/utility/module/builder.hpp>
#include <halmd/utility/module/demangle.hpp>
#include <halmd/utility/module/exception.hpp>
#include <halmd/utility/options.hpp>

namespace halmd
{
namespace utility { namespace module
{

// import into namespace
using boost::shared_ptr;
using boost::weak_ptr;

/**
 * Concrete module
 */
template <typename T>
class wrapper
  : public builder<T>
{
public:
    typedef typename builder<T>::_Module_base _Base;

    /**
     * returns singleton instance
     */
    shared_ptr<_Base> fetch(po::options const& vm)
    {
        // We use an observing weak pointer instead of an owning
        // shared pointer to let the caller decide when the
        // singleton instance and its dependencies are destroyed.
        //
        // Special care has to be taken not to destroy the
        // instance before returning it over to the caller.

        shared_ptr<T> singleton(singleton_.lock());
        if (!singleton) {
            singleton.reset(new T(vm));
            singleton_ = singleton;
        }
        return singleton;
    }

    /**
     * assemble module options
     */
    void options(po::options_description& desc)
    {
        builder<T>::options(desc);
    }

    /**
     * resolve module dependencies
     */
    void resolve(po::options const& vm)
    {
        builder<T>::resolve(vm);
    }

    /**
     * return (demangled) module name
     */
    std::string name()
    {
        return demangled_name<T>();
    }

    /** module instance observer */
    static weak_ptr<T> singleton_;
};

template <typename T> weak_ptr<T> wrapper<T>::singleton_;

}} // namespace utility::module

} // namespace halmd

#endif /* ! HALMD_UTILITY_MODULE_WRAPPER_HPP */
