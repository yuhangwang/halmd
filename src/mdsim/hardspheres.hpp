/* Hard Spheres simulation
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

#ifndef MDSIM_HARDSPHERES_HPP
#define MDSIM_HARDSPHERES_HPP

#include <algorithm>
#include <boost/array.hpp>
#include <boost/foreach.hpp>
#include <boost/multi_array.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <list>
#include <queue>
#include <sys/times.h>
#include <vector>
#include "H5param.hpp"
#include "H5xx.hpp"
#include "exception.hpp"
#include "gsl_rng.hpp"
#include "log.hpp"
#include "perf.hpp"


#define foreach BOOST_FOREACH

namespace mdsim
{

/**
 * Hard Spheres simulation
 */
template <unsigned dimension, typename T>
class hardspheres
{
    //
    // Details of the implementation are described in
    //
    // S. Miller, S. Luding,
    // Event-driven molecular dynamics in parallel,
    // Journal of Computational Physics 193 (2003) 306-316
    //
    // M. P. Allen, D. Frenkel & J. Talbot,
    // Molecular dynamics simulation using hard particles,
    // Computer Physics Reports 9 (1989) 301-353
    //

public:
    typedef std::list<unsigned int> cell_type;
    typedef boost::array<unsigned int, dimension> cell_index;

    /**
     * particle state
     */
    struct particle
    {
	/** periodically reduced particle position */
	T r;
	/** periodically extended particle position */
	T R;
	/** particle velocity */
	T v;
	/** time of that event */
	double t;
	/** event counter */
	uint64_t count;
	/** cell which particle belongs to */
	cell_index cell;

	/** initialize event counter to zero */
	particle() : count(0) {}
    };

    /**
     * particle event list item
     */
    struct event
    {
	/** time of event */
	double t;
	/** event type */
	enum {
	    /** collision with other particle */
	    COLLISION,
	    /** cell boundary */
	    CELL,
	} type;

	/** collision event partner */
	unsigned int n2;
	/** cell boundary */
	cell_index cell2;
	/** copy of event counter of partner at time of event */
	uint64_t count2;
    };

    /** particle event queue item with event time and particle */
    typedef std::pair<double, unsigned int> event_queue_item;

public:
    /** set number of particles */
    void particles(unsigned int value);
    /** set pair separation at which particle collision occurs */
    void pair_separation(double value);
    /** set particle density */
    void density(double value);
    /** set periodic box length */
    void box(double value);
    /** initialize cells */
    void init_cell();

    /** set system state from phase space sample */
    template <typename V> void restore(V visitor);
    /** initialize random number generator with seed */
    void rng(unsigned int seed);
    /** initialize random number generator from state */
    void rng(rng::gsl::gfsr4::state_type const& state);
    /** place particles on a face-centered cubic (fcc) lattice */
    void lattice();
    /** set system temperature according to Maxwell-Boltzmann distribution */
    void temperature(double value);
    /** initialize event list */
    void init_event_list();

    /** returns number of particles */
    unsigned int const& particles() const { return npart; }
    /** returns pair separation at which particle collision occurs */
    double const& pair_separation() const { return pair_sep_; }
    /** returns particle density */
    double const& density() const { return density_; }
    /** returns periodic box length */
    double const& box() const { return box_; }
    /** returns number of cells per dimension */
    unsigned int const& cells() const { return ncell; }
    /** returns cell length */
    double const& cell_length() { return cell_length_; }
    /** returns and resets CPU tick statistics */
    perf_counters times();

    /** write parameters to HDF5 parameter group */
    void attrs(H5::Group const& param) const;

    /** advance phase space state to given sample time */
    void mdstep(double sample_time);
    /** sample trajectory */
    template <typename V> void sample(V visitor) const;

private:
    /** schedule next particle event starting at given time */
    void schedule_event(unsigned int n);
    /** process particle collision event */
    void process_collision_event(unsigned int n);
    /** process cell boundary event */
    void process_cell_event(unsigned int n);
    /** returns cell which a particle belongs to */
    cell_index compute_cell(T const& r);
    /** compute next collision event with particles of given cell starting at given time within given time interval */
    void compute_collision_event(unsigned int n, cell_type const& cell);
    /** compute next cell boundary event starting at given time within given time interval */
    void compute_cell_event(unsigned int n);

private:
    /** number of particles */
    unsigned int npart;
    /** pair separation at which particle collision occurs */
    double pair_sep_;
    /** particle density */
    double density_;
    /** periodic box length */
    double box_;
    /** number of cells per dimension */
    unsigned int ncell;
    /** cell length */
    double cell_length_;

    /** particle states */
    std::vector<particle> part;
    /** cells */
    boost::multi_array<cell_type, dimension> cell_;
    /** particle event list with next event for each particle */
    std::vector<event> event_list;
    /** time-ordered particle event queue */
    std::priority_queue<event_queue_item, std::vector<event_queue_item>, std::greater<event_queue_item> > event_queue;

    /** periodically reduced particle positions at sample time */
    std::vector<T> r_;
    /** periodically extended particle positions at sample time */
    std::vector<T> R_;
    /** particle velocities at sample time */
    std::vector<T> v_;
    /** impulsive limit of the virial expression sum */
    double virial_;

    /** random number generator */
    rng::gsl::gfsr4 rng_;
    /** squared pair separation */
    double pair_sep_sq;

    /** CPU tick statistics */
    perf_counters m_times;
};

/**
 * set number of particles in system
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::particles(unsigned int value)
{
    if (value < 1) {
	throw exception("number of particles must be non-zero");
    }
    npart = value;
    LOG("number of particles: " << npart);

    try {
	part.resize(npart);
	r_.resize(npart);
	R_.resize(npart);
	v_.resize(npart);
    }
    catch (std::bad_alloc const&) {
	throw exception("failed to allocate particle states");
    }
}

/**
 * set pair separation at which particle collision occurs
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::pair_separation(double value)
{
    if (value <= 0.) {
	throw exception("pair separation must be greater than zero");
    }
    pair_sep_ = value;
    LOG("pair separation: " << pair_sep_);

    // squared pair separation
    pair_sep_sq = pair_sep_ * pair_sep_;
}

/**
 * set particle density
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::density(double value)
{
    density_ = value;
    LOG("particle density: " << density_);

    // derive periodic box length
    box_ = std::pow(npart / density_, 1. / dimension);
    LOG("periodic box length: " << box_);
}

/**
 * set periodic box length
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::box(double value)
{
    box_ = value;
    LOG("periodic box length: " << box_);

    // derive particle density
    density_ = npart / std::pow(box_, 1. * dimension);
    LOG("particle density: " << density_);
}

/**
 * initialize cells
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::init_cell()
{
#ifdef DIM_3D
    // FIXME optimal number of cells
    ncell = std::min(cbrt(npart * 8.), std::floor(box_ / pair_sep_));
#else
    ncell = std::min(sqrt(npart * 1.5), std::floor(box_ / pair_sep_));
#endif
    LOG("number of cells per dimension: " << ncell);

    if (ncell < 3) {
	throw exception("number of cells per dimension must be at least 3");
    }

    try {
#ifdef DIM_3D
	cell_.resize(boost::extents[ncell][ncell][ncell]);
#else
	cell_.resize(boost::extents[ncell][ncell]);
#endif
    }
    catch (std::bad_alloc const&) {
	throw exception("failed to allocate cells");
    }

    // derive cell length
    cell_length_ = box_ / ncell;
    LOG("cell length: " << cell_length_);
}

/**
 * set system state from phase space sample
 */
template <unsigned dimension, typename T>
template <typename V>
void hardspheres<dimension, T>::restore(V visitor)
{
    // copy particle positions and velocities at sample time zero
    visitor(r_, v_);
    // replicate to periodically extended coordinates
    std::copy(r_.begin(), r_.end(), R_.begin());

    for (unsigned int i = 0; i < npart; ++i) {
	// set periodically reduced particle position at simulation time zero
	part[i].r = r_[i];
	// set periodically extended particle position at simulation time zero
	part[i].R = R_[i];
	// set cell which particle belongs to
	part[i].cell = compute_cell(part[i].r);
	// add particle to cell
	cell_(part[i].cell).push_back(i);
	// set particle velocity at simulation time zero
	part[i].v = v_[i];
	// set particle time
	part[i].t = 0.;
    }
}

/**
 * initialize random number generator with seed
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::rng(unsigned int seed)
{
    rng_.set(seed);
    LOG("initializing random number generator with seed: " << seed);
}

/**
 * initialize random number generator from state
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::rng(rng::gsl::gfsr4::state_type const& state)
{
    rng_.restore(state);
    LOG("restoring random number generator from state");
}

/**
 * place particles on a face-centered cubic (fcc) lattice
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::lattice()
{
    LOG("placing particles on face-centered cubic (fcc) lattice");

    // particles per 2- or 3-dimensional unit cell
    const unsigned int m = 2 * (dimension - 1);
    // lower boundary for number of particles per lattice dimension
    unsigned int n = std::pow(npart / m, 1. / dimension);
    // lower boundary for total number of lattice sites
    unsigned int N = m * std::pow(n, dimension);

    if (N < npart) {
	n += 1;
	N = m * std::pow(n, dimension);
    }
    if (N > npart) {
	LOG_WARNING("lattice not fully occupied (" << N << " sites)");
    }

    // lattice distance
    const double a = box_ / n;
    // minimum distance in 2- or 3-dimensional fcc lattice
    const double dist = a / std::sqrt(2.);
    LOG("minimum lattice distance: " << dist);

    // ensure that particles do not overlap
    if (dist < pair_sep_) {
	throw exception("minimum lattice distance smaller than pair separation");
    }

    for (unsigned int i = 0; i < npart; ++i) {
#ifdef DIM_3D
	// compose primitive vectors from 1-dimensional index
	part[i].r[0] = ((i >> 2) % n) + ((i ^ (i >> 1)) & 1) / 2.;
	part[i].r[1] = ((i >> 2) / n % n) + (i & 1) / 2.;
	part[i].r[2] = ((i >> 2) / n / n) + (i & 2) / 4.;
#else
	// compart[i]ose primitive vectors from 1-dimensional index
	part[i].r[0] = ((i >> 1) % n) + (i & 1) / 2.;
	part[i].r[1] = ((i >> 1) / n) + (i & 1) / 2.;
#endif
	// scale by lattice distance
	part[i].r *= a;
	// set periodically extended particle position
	part[i].R = part[i].r;
	// set cell which particle belongs to
	part[i].cell = compute_cell(part[i].r);
	// add particle to cell
	cell_(part[i].cell).push_back(i);
	// set particle time
	part[i].t = 0.;
	// copy periodically reduced particle position at sample time zero
	r_[i] = part[i].r;
	// copy periodically extended particle position at sample time zero
	R_[i] = part[i].R;
    }
}

/**
 * set system temperature according to Maxwell-Boltzmann distribution
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::temperature(double value)
{
    LOG("initializing velocities from Maxwell-Boltzmann distribution at temperature: " << value);

    // center of mass velocity
    T v_cm = 0.;

    foreach (particle& p, part) {
	// generate random Maxwell-Boltzmann distributed velocity
	rng_.gaussian(p.v[0], p.v[1], value);
#ifdef DIM_3D
	// Box-Muller transformation strictly generates 2 variates at once
	rng_.gaussian(p.v[1], p.v[2], value);
#endif
	v_cm += p.v;
    }

    v_cm /= npart;

    for (unsigned int i = 0; i < npart; ++i) {
	// set center of mass velocity to zero
	part[i].v -= v_cm;
	// copy particle velocity at sample time zero
	v_[i] = part[i].v;
    }
}

/**
 * write parameters to HDF5 parameter group
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::attrs(H5::Group const& param) const
{
    H5xx::group node(param.createGroup("mdsim"));
    node["dimension"] = dimension;
    node["particles"] = npart;
    node["pair_separation"] = pair_sep_;
    node["cells"] = ncell;
    node["cell_length"] = cell_length_;
    node["density"] = density_;
    node["box_length"] = box_;
}

/**
 * initialize event list
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::init_event_list()
{
    try {
	event_list.resize(npart);
    }
    catch (std::bad_alloc const&) {
	throw exception("failed to allocate event list");
    }

    // schedule next event for each particle
    for (unsigned int i = 0; i < npart; ++i) {
	schedule_event(i);
    }
}

/**
 * compute next collision event with particles of given cell
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::compute_collision_event(const unsigned int n, cell_type const& cell)
{
    double dt = std::numeric_limits<double>::max();
    int n2 = -1;

    // iterate over particles in cell
    foreach (unsigned int j, cell) {
	// skip same particle if in same cell
	if (j == n)
	    continue;

	// particle distance vector at time of first particle
	T dr = part[j].r + part[j].v * (part[n].t - part[j].t) - part[n].r;
	// enforce periodic boundary conditions
	dr -= round(dr / box_) * box_;
	// velocity difference at given time
	T dv = part[j].v - part[n].v;

	// check particle collision constraint
	const double drdv = dr * dv;
	if (drdv >= 0.)
	    // no particle collision in future
	    continue;
	const double dvdv = dv * dv;
	const double rad = (drdv * drdv) - dvdv * ((dr * dr) - pair_sep_sq);
	if (rad < 0.)
	    // no particle collision in future
	    continue;
	const double dt_ = (- drdv - std::sqrt(rad)) / dvdv;
	if (dt_ < 0.)
	    // no particle collision in future
	    continue;

	// particles will collide in the future in reference to given time
	if (dt_ < dt) {
	    // set smallest collision time interval
	    dt = dt_;
	    // set partner participating in that collision
	    n2 = j;
	}
    }

    if (n2 < 0)
	// no collision with particles in cell
	return;

    if (dt < event_list[n].t - part[n].t) {
	// generate particle collision event
	event_list[n].type = event::COLLISION;
	event_list[n].t = part[n].t + dt;
	event_list[n].n2 = n2;
	event_list[n].count2 = part[n2].count;
    }
}

/**
 * compute next cell boundary event
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::compute_cell_event(const unsigned int n)
{
    T dt3(std::numeric_limits<double>::max());
    double dt = std::numeric_limits<double>::max();
    cell_index cell2 = part[n].cell;

    for (unsigned int d = 0; d < dimension; ++d) {
	if (part[n].v[d] < 0.) {
	    dt3[d] = (part[n].cell[d] * cell_length_ - part[n].r[d]) / part[n].v[d];
	    cell2[d] = (cell2[d] + ncell - 1) % ncell;
	}
	else if (part[n].v[d] > 0.) {
	    dt3[d] = ((part[n].cell[d] + 1) * cell_length_ - part[n].r[d]) / part[n].v[d];
	    cell2[d] = (cell2[d] + 1) % ncell;
	}
	dt = std::min(dt, dt3[d]);
    }

    if (dt < event_list[n].t - part[n].t) {
	// generate cell boundary event
	event_list[n].t = part[n].t + dt;
	event_list[n].type = event::CELL;
	for (unsigned int d = 0; d < dimension; ++d) {
	    event_list[n].cell2[d] = (dt3[d] == dt) ? cell2[d] : part[n].cell[d];
	}
    }
}

/**
 * schedule next particle event starting at given time
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::schedule_event(const unsigned int n)
{
    // upper boundary for time of next particle event
    event_list[n].t = std::numeric_limits<double>::max();

    // compute next cell boundary event
    compute_cell_event(n);
    // compute next collision event with particles of this cell
    compute_collision_event(n, cell_(part[n].cell));

#ifdef DIM_3D
    // compute next collision event with particles of neighbour cells
    const boost::array<cell_index, 26> neighbour = {{
	{{  0, -1,  0 }},
	{{  0, +1,  0 }},
	{{ -1, -1,  0 }},
	{{ -1,  0,  0 }},
	{{ -1, +1,  0 }},
	{{ +1, -1,  0 }},
	{{ +1,  0,  0 }},
	{{ +1, +1,  0 }},
	{{  0, -1, -1 }},
	{{  0, +1, -1 }},
	{{  0, +1, +1 }},
	{{ -1, -1, -1 }},
	{{ -1,  0, -1 }},
	{{ -1, +1, -1 }},
	{{ +1, -1, -1 }},
	{{ +1,  0, -1 }},
	{{ +1, +1, -1 }},
	{{  0, -1, +1 }},
	{{ -1, -1, +1 }},
	{{ -1,  0, +1 }},
	{{ -1, +1, +1 }},
	{{ +1, -1, +1 }},
	{{ +1,  0, +1 }},
	{{ +1, +1, +1 }},
	{{  0,  0, -1 }},
	{{  0,  0, +1 }},
    }};

    foreach (cell_index const& idx, neighbour) {
	compute_collision_event(n, cell_[(part[n].cell[0] + ncell + idx[0]) % ncell][(part[n].cell[1] + ncell + idx[1]) % ncell][(part[n].cell[2] + ncell + idx[2]) % ncell]);
    }
#else
    // compute next collision event with particles of neighbour cells
    const boost::array<cell_index, 8> neighbour = {{
	{{  0, -1 }},
	{{ -1, -1 }},
	{{ -1,  0 }},
	{{ -1, +1 }},
	{{  0, +1 }},
	{{ +1, -1 }},
	{{ +1,  0 }},
	{{ +1, +1 }},
    }};

    foreach (cell_index const& idx, neighbour) {
	compute_collision_event(n, cell_[(part[n].cell[0] + ncell + idx[0]) % ncell][(part[n].cell[1] + ncell + idx[1]) % ncell]);
    }
#endif

    // schedule particle event
    event_queue.push(event_queue_item(event_list[n].t, n));
}

/*
 * process particle collision event
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::process_collision_event(const unsigned int n1)
{
    const T dr1 = part[n1].v * (event_list[n1].t - part[n1].t);
    // update periodically extended particle position
    part[n1].R += dr1;
    // update periodically reduced particle position to given time
    part[n1].r += dr1;
    // update particle time
    part[n1].t = event_list[n1].t;

    // collision partner particle number
    const unsigned int n2 = event_list[n1].n2;

    // check if partner participated in another collision before this event
    if (part[n2].count != event_list[n1].count2) {
	// schedule next event for this particle
	schedule_event(n1);
	return;
    }

    const T dr2 = part[n2].v * (event_list[n1].t - part[n2].t);
    // update periodically extended particle position
    part[n2].R += dr2;
    // update periodically reduced particle position to given time
    part[n2].r += dr2;
    // update particle time
    part[n2].t = event_list[n1].t;

    // particle distance vector
    T dr = part[n2].r - part[n1].r;
    // enforce periodic boundary conditions
    dr -= round(dr / box_) * box_;
    // velocity difference before collision
    T dv = part[n1].v - part[n2].v;
    // velocity difference after collision without dissipation
    dv = dr * (dr * dv) / (dr * dr);

    // update velocities to current simulation time
    part[n1].v -= dv;
    part[n2].v += dv;

    // add contribution to impulsive limit of the virial expression sum
    virial_ += dr * dv;

    // update particle event counters
    part[n1].count++;
    part[n2].count++;

    // schedule next event for each particle
    schedule_event(n1);
    schedule_event(n2);
}

/*
 * process cell boundary event
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::process_cell_event(const unsigned int n)
{
    const T dr = part[n].v * (event_list[n].t - part[n].t);
    // update periodically extended particle position
    part[n].R += dr;
    // update periodically reduced particle position to given time
    part[n].r += dr;
    // enforce periodic boundary conditions
    for (unsigned int d = 0; d < dimension; ++d) {
	if (part[n].cell[d] == ncell - 1 && event_list[n].cell2[d] == 0)
	    part[n].r[d] -= box_;
	if (part[n].cell[d] == 0 && event_list[n].cell2[d] == ncell - 1)
	    part[n].r[d] += box_;
    }
    // update particle time
    part[n].t = event_list[n].t;

    // remove particle from old cell
    cell_(part[n].cell).remove(n);
    // update particle cell
    part[n].cell = event_list[n].cell2;
    // add particle to cell
    cell_(part[n].cell).push_back(n);

    // schedule next event for particle
    schedule_event(n);
}

/**
 * returns cell which a particle belongs to
 */
template <unsigned dimension, typename T>
typename hardspheres<dimension, T>::cell_index hardspheres<dimension, T>::compute_cell(T const& r)
{
    T cellf = r / cell_length_;
#ifdef DIM_3D
    cell_index cell = {{ int(cellf[0]), int(cellf[1]), int(cellf[2]) }};
#else
    cell_index cell = {{ int(cellf[0]), int(cellf[1]) }};
#endif
    return cell;
}

/**
 * advance phase space state to given sample time
 */
template <unsigned dimension, typename T>
void hardspheres<dimension, T>::mdstep(const double sample_time)
{
    // CPU cycles in clock ticks
    boost::array<tms, 3> t;
    ::times(&t[0]);

    // impulsive limit of the virial expression sum
    virial_ = 0.;

    // process particle event queue till sample time
    while (event_queue.top().first <= sample_time) {
	if (event_queue.top().first != event_list[event_queue.top().second].t) {
	    // discard invalidated event
	    event_queue.pop();
	    continue;
	}

	switch (event_list[event_queue.top().second].type) {
	  case event::COLLISION:
	    // process particle collision event
	    process_collision_event(event_queue.top().second);
	    break;

	  case event::CELL:
	    // process cell boundary event
	    process_cell_event(event_queue.top().second);
	    break;
	}
	event_queue.pop();
    }

    virial_ /= npart;

    // sample phase space at given time
    ::times(&t[1]);
    for (unsigned int i = 0; i < npart; ++i) {
	const T dr = part[i].v * (sample_time - part[i].t);
	// periodically extended particle position
	R_[i] = part[i].R + dr;
	// periodically reduced particle position
	r_[i] = part[i].r + dr;
	// enforce periodic boundary conditions
	r_[i] -= floor(r_[i] / box_) * box_;
	// particle velocity
	v_[i] = part[i].v;
    }
    ::times(&t[2]);

    // CPU ticks for MD simulation step
    m_times[0] += t[2].tms_utime - t[0].tms_utime;
    // CPU ticks for event queue processing
    m_times[1] += t[1].tms_utime - t[0].tms_utime;
    // CPU ticks for phase space sampling
    m_times[2] += t[2].tms_utime - t[1].tms_utime;
}

/**
 * sample trajectory
 */
template <unsigned dimension, typename T>
template <typename V>
void hardspheres<dimension, T>::sample(V visitor) const
{
    visitor(r_, R_, v_, virial_);
}
 
/*
 * returns and resets CPU tick statistics
 */
template <unsigned dimension, typename T>
perf_counters hardspheres<dimension, T>::times()
{
    perf_counters times(m_times);
    // reset performance counters
    for (unsigned int i = 0; i < m_times.size(); ++i) {
	m_times[i].clear();
    }
    return times;
}

} // namespace mdsim

#undef foreach

#endif /* ! MDSIM_HARDSPHERES_HPP */
