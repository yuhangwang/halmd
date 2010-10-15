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
local mdsim = {
  core = require("halmd.mdsim.core")
}
local box_wrapper = {
    [2] = halmd_wrapper.mdsim.box_2_
  , [3] = halmd_wrapper.mdsim.box_3_
}
local args = require("halmd.options")
local assert = assert

module("halmd.mdsim.box", halmd.modules.register)

options = box_wrapper[2].options

--
-- construct box module
--
function new()
    local dimension = assert(args.dimension)
    local density = assert(args.density)
    local box_length = args.box_length -- optional

    local core = mdsim.core()
    local particle = assert(core.particle)

    local box = box_wrapper[dimension]
    if box_length then
        for i = #box_length + 1, dimension do
            box_length[i] = box_length[#box_length]
        end
        return box(particle, box_length)
    else
        local unit_cube = {}
        for i = 1, dimension do
            unit_cube[i] = 1 -- cubic aspect ratio
        end
        return box(particle, density, unit_cube)
    end
end