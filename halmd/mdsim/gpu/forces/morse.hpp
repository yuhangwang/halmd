/*
 * Copyright © 2008-2010  Peter Colberg and Felix Höfling
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

#ifndef HALMD_MDSIM_GPU_FORCES_MORSE_HPP
#define HALMD_MDSIM_GPU_FORCES_MORSE_HPP

#include <boost/numeric/ublas/symmetric.hpp>
#include <cuda_wrapper/cuda_wrapper.hpp>
#include <lua.hpp>

#include <halmd/mdsim/gpu/forces/pair_trunc.hpp>
#include <halmd/mdsim/gpu/forces/morse_kernel.hpp>

namespace halmd
{
namespace mdsim { namespace gpu { namespace forces
{

/**
 * define Morse potential and parameters
 */
template <typename float_type>
class morse_potential
{
public:
    typedef morse_kernel::morse_potential gpu_potential_type;
    typedef boost::numeric::ublas::symmetric_matrix<float_type, boost::numeric::ublas::lower> matrix_type;

    static char const* name() { return "Morse"; }
    static char const* module_name() { return "morse"; }

    static void luaopen(lua_State* L);

    morse_potential(
        unsigned ntype
      , boost::array<float, 3> const& cutoff
      , boost::array<float, 3> const& epsilon
      , boost::array<float, 3> const& sigma
      , boost::array<float, 3> const& r_min
    );

    /** bind textures before kernel invocation */
    void bind_textures() const
    {
        morse_wrapper::param.bind(g_param_);
        morse_wrapper::rr_cut.bind(g_rr_cut_);
    }

    matrix_type const& r_cut() const { return r_cut_; }

    float_type r_cut(unsigned a, unsigned b) const
    {
        return r_cut_(a, b);
    }

    float_type rr_cut(unsigned a, unsigned b) const
    {
        return rr_cut_(a, b);
    }

private:
    /** depths of potential well in MD units */
    matrix_type epsilon_;
    /** width of potential well in MD units */
    matrix_type sigma_;
    /** position of potential well in MD units */
    matrix_type r_min_;
    /** potential energy at cutoff length in MD units */
    matrix_type en_cut_;
    /** cutoff radius in MD units */
    matrix_type r_cut_;
    /** square of cutoff radius */
    matrix_type rr_cut_;
    /** potential parameters at CUDA device */
    cuda::vector<float4> g_param_;
    /** squared cutoff radius at CUDA device */
    cuda::vector<float> g_rr_cut_;
};

}}} // namespace mdsim::gpu::forces

} // namespace halmd

#endif /* ! HALMD_MDSIM_GPU_FORCES_MORSE_HPP */