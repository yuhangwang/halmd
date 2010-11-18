--
-- Copyright © 2010  Peter Colberg
--
-- This file is part of HALMD.
--
-- HALMD is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
--

require("halmd.modules")

-- grab environment
local hooks = require("halmd.hooks")
local assert = assert
local setmetatable = setmetatable

module("halmd.parameter", halmd.modules.register)

--
-- register HDF5 writer as parameter writer
--
-- @param writer HDF5 writer object
--
function register_writer(writer)
    hooks.register_module_hook(function(module, object)
        if module.write_parameters then
            local file = writer:file()
            local globals = file:open_group("param")

            -- If write_parameters only stores global parameters, or stores
            -- no parameters at all, the module's HDF5 parameter group would
            -- remain empty. Therefore we delay creation by creating or opening
            -- the group upon access of its methods.

            local group = setmetatable({}, {
                __index = function(self, name)
                    local namespace = assert(module.namespace)
                    local group = globals:open_group(namespace)

                    local method = group[name]
                    if method then
                        return function(self, ...)
                            method(group, ...)
                        end
                    end
                end
            })

            module.write_parameters(object, group, globals)
        end
    end)
end
