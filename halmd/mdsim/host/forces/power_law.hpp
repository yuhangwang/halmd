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

#ifndef HALMD_MDSIM_HOST_FORCES_POWER_LAW_HPP
#define HALMD_MDSIM_HOST_FORCES_POWER_LAW_HPP

#include <boost/assign.hpp>
#include <boost/shared_ptr.hpp>
#include <lua.hpp>
#include <utility>

#include <halmd/mdsim/box.hpp>
#include <halmd/mdsim/host/force.hpp>
#include <halmd/mdsim/host/forces/pair_short_ranged.hpp>
#include <halmd/mdsim/host/forces/smooth.hpp>
#include <halmd/mdsim/host/particle.hpp>
#include <halmd/numeric/pow.hpp>
#include <halmd/options.hpp>

namespace halmd
{
namespace mdsim { namespace host { namespace forces
{

/**
 * A power-law potential @f$r^{-n}@f$ is often used for
 * repulsive smooth spheres. A big advantage is
 * its scale invariance (in the absence of a cutoff).
 */

template <int dimension, typename float_type>
class power_law_potential
{
public:
    typedef typename mdsim::host::force<dimension, float_type>::matrix_type matrix_type;

    power_law_potential(
        unsigned ntype
      , int index
      , boost::array<float, 3> const& cutoff
      , boost::array<float, 3> const& epsilon
      , boost::array<float, 3> const& sigma
    );

    /** 
     * Compute potential and its derivative at squared distance 'rr'
     * for particles of type 'a' and 'b'
     *
     * Call index-dependent template implementations
     * for efficiency of fixed_pow() function.
     */
    std::pair<float_type, float_type> operator() (float_type rr, unsigned a, unsigned b)
    {
        switch (index_) {
            case 6:  return impl_<6>(rr, a, b);
            case 12: return impl_<12>(rr, a, b);
            case 24: return impl_<24>(rr, a, b);
            case 48: return impl_<48>(rr, a, b);
            default:
                LOG_WARNING_ONCE("Using non-optimised force routine for index " << index_);
                return impl_<0>(rr, a, b);
        }
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
    /** optimise pow() function by providing the index at compile time */
    template <int index>
    std::pair<float_type, float_type> impl_(float_type rr, unsigned a, unsigned b)
    {
        // choose arbitrary index_ if template parameter index = 0
        float_type rni;
        if (index > 0) {
            rni = fixed_pow<index>(sigma_(a, b) / std::sqrt(rr));
        }
        else {
            rni = std::pow(sigma_(a, b) / std::sqrt(rr), index_);
        }
        float_type en_pot = epsilon_(a, b) * rni;      // U(r)
        float_type fval = (index > 0 ? index : index_) * en_pot / rr;
                                                       // F(r) / r
        en_pot -= en_cut_(a, b);                       // shift potential

        return std::make_pair(fval, en_pot);
    }

    /** power law index */
    int index_;
    /** interaction strength in MD units */
    matrix_type epsilon_;
    /** interaction range in MD units */
    matrix_type sigma_;
    /** cutoff length in MD units */
    matrix_type r_cut_;
    /** square of cutoff length */
    matrix_type rr_cut_;
    /** potential energy at cutoff in MD units */
    matrix_type en_cut_;
};

template <int dimension, typename float_type>
class power_law
  : public pair_short_ranged<dimension, float_type, power_law_potential<dimension, float_type> >
{
public:
    static void options(po::options_description& desc);

    typedef power_law_potential<dimension, float_type> potential_type;
    typedef mdsim::host::forces::pair_short_ranged<dimension, float_type, potential_type> _Base;
    typedef typename _Base::particle_type particle_type;
    typedef typename _Base::box_type box_type;

    static void luaopen(lua_State* L);

    power_law(
        boost::shared_ptr<particle_type> particle
      , boost::shared_ptr<box_type> box
      , int index
      , boost::array<float, 3> const& cutoff
      , boost::array<float, 3> const& epsilon
      , boost::array<float, 3> const& sigma
    );

    static int default_index()
    {
        return 12;
    }

    // FIXME define sensible default values
    static boost::array<float, 3> default_cutoff()
    {
        return boost::assign::list_of(2.5f)(2.5f)(2.5f);
    }

    static boost::array<float, 3> default_epsilon()
    {
        return boost::assign::list_of(1.0f)(1.5f)(0.5f);
    }

    static boost::array<float, 3> default_sigma()
    {
        return boost::assign::list_of(1.0f)(0.8f)(0.88f);
    }
};

}}} // namespace mdsim::host::forces

} // namespace halmd

#endif /* ! HALMD_MDSIM_HOST_FORCES_POWER_LAW_HPP */