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

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/tuple/tuple.hpp>
#include <fstream>
#include <iostream>

#include <halmd/utility/lua_wrapper/lua_wrapper.hpp>
#include <halmd/utility/options_parser.hpp>

using namespace boost;
using namespace std;

namespace halmd
{

/**
 * Add module-independent program options.
 *
 * This function is provided for convenience in main().
 */
po::options_description_easy_init options_parser::add_options()
{
    return desc_.add_options();
}

/**
 * Add general program options description.
 *
 * @param desc options description
 */
void options_parser::add(po::options_description const& desc)
{
    vector<shared_ptr<po::option_description> >::const_iterator i, end = desc.options().end();
    for (i = desc.options().begin(); i != end; ++i) {
        desc_.add(*i);
    }
}

/**
 * Add module program options description.
 *
 * @param desc options description
 * @param section section name
 */
void options_parser::add(po::options_description const& desc, string const& section)
{
    map<string, po::options_description>::iterator m = desc_module_.find(section);
    if (m == desc_module_.end()) {
        bool _;
        po::options_description desc(section);
        tie(m, _) = desc_module_.insert(make_pair(section, desc));
        sections_.push_back(section);
    }
    vector<shared_ptr<po::option_description> >::const_iterator i, end = desc.options().end();
    for (i = desc.options().begin(); i != end; ++i) {
        m->second.add(*i);
    }
}

/**
 * Return general and module options.
 *
 * The result is useful for --help output.
 *
 * @returns options description
 */
po::options_description options_parser::options() const
{
    po::options_description desc(desc_);
    vector<string>::const_iterator i, end = sections_.end();
    for (i = sections_.begin(); i != end; ++i) {
        map<string, po::options_description>::const_iterator m = desc_module_.find(*i);
        desc.add(m->second);
    }
    return desc;
}

/**
 * Parse modular command line options.
 *
 * @param args command line arguments (without program name)
 * @param vm variables map to store option values
 */
void options_parser::parse_command_line(vector<string> const& args, po::variables_map& vm)
{
    // build array of iterators, where each iterator points to
    // command line argument equal to a module namespace
    vector<vector<string>::const_iterator> argp;
    {
        argp.reserve(args.size());
        vector<string>::const_iterator i, ie;
        for (i = args.begin(), ie = args.end(); i != ie; ++i) {
            if (desc_module_.find(*i) != desc_module_.end()) {
                argp.push_back(i);
            }
        }
        argp.push_back(args.end());
    }

    // split command line arguments into module-specific options
    map<string, vector<string> > argm;
    {
        vector<vector<string>::const_iterator>::const_iterator i, ie;
        for (i = argp.begin(), ie = argp.end() - 1; i != ie; ++i) {
            map<string, vector<string> >::iterator m;
            bool _;
            tie(m, _) = argm.insert(make_pair(*(*i), vector<string>()));
            vector<string>& options = m->second;
            options.insert(options.end(), (*i) + 1, *(i + 1));
        }
    }

    // parse module-independent options
    {
        po::command_line_parser parser(vector<string>(args.begin(), argp.front()));
        parser.options(desc_);
        parse_command_line(parser, vm);
    }

    // parse module-specific options for each module
    {
        map<string, po::options_description>::const_iterator i, ie;
        for (i = desc_module_.begin(), ie = desc_module_.end(); i != ie; ++i) {
            po::variable_value vv(po::variables_map(), false);
            po::variables_map::iterator m;
            bool _;
            tie(m, _) = vm.insert(make_pair(i->first, vv));
            po::variables_map& vm_ = m->second.as<po::variables_map>();

            po::command_line_parser parser(argm[i->first]);
            parser.options(i->second);
            parse_command_line(parser, vm_);
        }
    }
}

/**
 * Parse modular command line options.
 *
 * This function is provided for convenience in main().
 *
 * @param argc number of command line arguments
 * @param argv command line arguments (first is program name)
 * @param vm variables map to store option values
 */
void options_parser::parse_command_line(int argc, char** argv, po::variables_map& vm)
{
    parse_command_line(vector<string>(argv + 1, argv + argc), vm);
}

/*
 * Parse command line options.
 *
 * @param parser command line parser containing options description
 * @param vm variables map to store option values
 */
void options_parser::parse_command_line(po::command_line_parser& parser, po::variables_map& vm)
{
    // pass an empty positional options description to the command line
    // parser to warn the user of unintentional positional options
    po::positional_options_description pd;
    parser.positional(pd);

    // disallow abbreviated options, which break forward compatibility of
    // user's scripts as new options are added and create ambiguities
    using namespace po::command_line_style;
    parser.style(default_style & ~allow_guessing);

    po::store(parser.run(), vm);
    po::notify(vm);
}

/**
 * Parse config file options.
 *
 * @param file_name path to configuration file
 * @param vm variables map to store option values
 */
void options_parser::parse_config_file(std::string const& file_name, po::variables_map& vm)
{
    ifstream ifs(file_name.c_str());
    if (ifs.fail()) {
        throw runtime_error("could not open parameter file '" + file_name + "'");
    }
    po::store(po::parse_config_file(ifs, desc_), vm); // FIXME parse module options
    po::notify(vm);
}

/**
 * Register options parser with Lua.
 */
void options_parser::luaopen(lua_State* L)
{
    using namespace luabind;
    module(L, "halmd_wrapper")
    [
        class_<options_parser>("options_parser")
            .def("add", (void (options_parser::*)(po::options_description const&)) &options_parser::add)
            .def("add", (void (options_parser::*)(po::options_description const&, string const&)) &options_parser::add)
    ];
}

static __attribute__((constructor)) void register_lua()
{
    lua_wrapper::register_(0) //< distance of derived to base class
    [
        &options_parser::luaopen
    ];
}

} // namespace halmd