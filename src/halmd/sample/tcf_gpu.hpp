/* Time correlation functions for CUDA
 *
 * Copyright © 2008-2010  Peter Colberg
 *                        Felix Höfling
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

#ifndef HALMD_SAMPLE_TCF_GPU_HPP
#define HALMD_SAMPLE_TCF_GPU_HPP

#include <algorithm>
#include <boost/mpl/vector.hpp>
#include <boost/variant.hpp>
#include <vector>

#include <cuda_wrapper.hpp>
#include <halmd/math/vector2d.hpp>
#include <halmd/math/vector3d.hpp>
#include <halmd/mdsim/sample.hpp>
#include <halmd/mdsim/traits.hpp>
#include <halmd/sample/gpu/tcf.hpp>
#include <halmd/sample/tcf_base.hpp>

namespace halmd {

/**
 * Phase space sample for evaluating correlation functions
 */
template <int dimension>
struct tcf_gpu_sample : public tcf_sample<dimension>
{
    typedef tcf_sample<dimension> _Base;
    typedef typename _Base::vector_type vector_type;
    typedef typename _Base::q_value_vector q_value_vector;
    typedef typename _Base::q_vector_vector q_vector_vector;
    typedef typename _Base::isf_vector_vector isf_vector_vector;
    typedef typename _Base::density_pair density_pair;
    typedef typename _Base::density_vector density_vector;
    typedef typename _Base::density_vector_vector density_vector_vector;
    typedef std::vector<vector_type> sample_vector;

    typedef mdsim_traits<ljfluid_impl_gpu_base, dimension> traits_type;
    typedef typename traits_type::gpu_vector_type gpu_vector_type;
    typedef cuda::vector<gpu_vector_type> gpu_sample_vector;
    typedef gpu::tcf<dimension> _gpu;

    tcf_gpu_sample() {}
    tcf_gpu_sample(boost::shared_ptr<gpu_sample_vector> r, boost::shared_ptr<gpu_sample_vector> v): r(r), v(v) {}

    using _Base::rho;
    using _Base::isf;

    /**
     * initialise phase space sample
     */
    void operator()(q_vector_vector const& q)
    {
        // CUDA execution dimensions for accumulator
        enum { BLOCKS = _gpu::BLOCKS };
        enum { THREADS = _gpu::THREADS };

        // q-vector iterators
        typename q_vector_vector::const_iterator q0;
        typename q_vector_vector::value_type::const_iterator q1;
        // Fourier-transformed density iterators
        typename density_vector_vector::iterator rho0;
        typename density_vector_vector::value_type::iterator rho1;
        // self-intermediate scattering function iterator
        typename isf_vector_vector::iterator isf0;
        // accumulator iterator
        dsfloat* sum0;

        // allocate memory for Fourier-transformed densities and self-intermediate scattering function
        rho.reset(new density_vector_vector(q.size()));
        isf.reset(new isf_vector_vector(q.size()));
        size_t size = 0;
        for (q0 = q.begin(), rho0 = rho->begin(), isf0 = isf->begin(); q0 != q.end(); ++q0, ++rho0, ++isf0) {
            rho0->assign(q0->size(), density_pair(0, 0));
            isf0->resize(q0->size());
            size += q0->size();
        }
        // allocate device and host memory for accumulators
        cuda::vector<dsfloat> g_sum(2 * size * BLOCKS);
        cuda::host::vector<dsfloat> h_sum(g_sum.size());

        // compute Fourier-transformed densities on GPU
        for (q0 = q.begin(), sum0 = g_sum.data(); q0 != q.end(); ++q0) {
            for (q1 = q0->begin(); q1 != q0->end(); ++q1, sum0 += 2 * BLOCKS) {
                cuda::configure(BLOCKS, THREADS);
                _gpu::coherent_scattering_function(*r, *q1, sum0, sum0 + BLOCKS, r->size());
            }
        }
        // copy accumulator block results from GPU to host
        cuda::copy(g_sum, h_sum);
        // accumulate Fourier-transformed densities on host
        for (rho0 = rho->begin(), sum0 = h_sum.data(); rho0 != rho->end(); ++rho0) {
            for (rho1 = rho0->begin(); rho1 != rho0->end(); ++rho1, sum0 += 2 * BLOCKS) {
                rho1->first = std::accumulate(sum0, sum0 + BLOCKS, 0.);
                rho1->second = std::accumulate(sum0 + BLOCKS, sum0 + 2 * BLOCKS, 0.);
            }
        }
    }

    /** particle positions */
    boost::shared_ptr<gpu_sample_vector> r;
    /** particle velocities */
    boost::shared_ptr<gpu_sample_vector> v;
};

/**
 * mean-square displacement
 */
template <>
struct mean_square_displacement<tcf_gpu_sample> : correlation_function<tcf_gpu_sample>
{
    /** block sample results */
    tcf_unary_result_type result;

    /** device and host memory for accumulators */
    cuda::vector<unsigned int> g_count;
    cuda::host::vector<unsigned int> h_count;
    cuda::vector<dsfloat> g_mean;
    cuda::host::vector<dsfloat> h_mean;
    cuda::vector<dsfloat> g_var;
    cuda::host::vector<dsfloat> h_var;

    char const* name() const { return "MSD"; }

    /**
     * autocorrelate samples in block
     */
    template <typename input_iterator, typename output_iterator>
    void operator()(input_iterator const& first, input_iterator const& last, output_iterator result)
    {
        typedef typename input_iterator::first_type sample_iterator;
        typedef typename sample_iterator::value_type::value_type sample_type;
        typedef typename sample_type::_gpu _gpu;
        typedef typename output_iterator::value_type accumulator_type;
        enum { BLOCKS = _gpu::BLOCKS };
        enum { THREADS = _gpu::THREADS };

        sample_iterator sample;
        unsigned int *count, *count0;
        dsfloat *mean, *var;

        // allocate device and host memory for accumulators, if necessary
        g_count.resize((last.first - first.first) * BLOCKS);
        h_count.resize(g_count.size());
        g_mean.resize((last.first - first.first) * BLOCKS);
        h_mean.resize(g_mean.size());
        g_var.resize((last.first - first.first) * BLOCKS);
        h_var.resize(g_var.size());

        // compute mean square displacements on GPU
        for (sample = first.first, count = g_count.data(), mean = g_mean.data(), var = g_var.data(); sample != last.first; ++sample, count += BLOCKS, mean += BLOCKS, var += BLOCKS) {
            cuda::configure(BLOCKS, THREADS);
            _gpu::mean_square_displacement(*(*sample)[type].r, *(*first.first)[type].r, count, mean, var, (*sample)[type].r->size());
        }
        // copy accumulator block results from GPU to host
        cuda::copy(g_count, h_count);
        cuda::copy(g_mean, h_mean);
        cuda::copy(g_var, h_var);
        // accumulate mean square displacements on host
        for (sample = first.first, count0 = h_count.data(), mean = h_mean.data(), var = h_var.data(); sample != last.first; ++sample, ++result, count0 += BLOCKS) {
            for (count = count0; count != count0 + BLOCKS; ++count, ++mean, ++var) {
                *result += accumulator_type(*count, *mean, *var);
            }
        }
    }
};

/**
 * mean-quartic displacement
 */
template <>
struct mean_quartic_displacement<tcf_gpu_sample> : correlation_function<tcf_gpu_sample>
{
    /** block sample results */
    tcf_unary_result_type result;

    /** device and host memory for accumulators */
    cuda::vector<unsigned int> g_count;
    cuda::host::vector<unsigned int> h_count;
    cuda::vector<dsfloat> g_mean;
    cuda::host::vector<dsfloat> h_mean;
    cuda::vector<dsfloat> g_var;
    cuda::host::vector<dsfloat> h_var;

    char const* name() const { return "MQD"; }

    /**
     * autocorrelate samples in block
     */
    template <typename input_iterator, typename output_iterator>
    void operator()(input_iterator const& first, input_iterator const& last, output_iterator result)
    {
        typedef typename input_iterator::first_type sample_iterator;
        typedef typename sample_iterator::value_type::value_type sample_type;
        typedef typename sample_type::_gpu _gpu;
        typedef typename output_iterator::value_type accumulator_type;
        enum { BLOCKS = _gpu::BLOCKS };
        enum { THREADS = _gpu::THREADS };

        sample_iterator sample;
        unsigned int *count, *count0;
        dsfloat *mean, *var;

        // allocate device and host memory for accumulators, if necessary
        g_count.resize((last.first - first.first) * BLOCKS);
        h_count.resize(g_count.size());
        g_mean.resize((last.first - first.first) * BLOCKS);
        h_mean.resize(g_mean.size());
        g_var.resize((last.first - first.first) * BLOCKS);
        h_var.resize(g_var.size());

        // compute mean quartic displacements on GPU
        for (sample = first.first, count = g_count.data(), mean = g_mean.data(), var = g_var.data(); sample != last.first; ++sample, count += BLOCKS, mean += BLOCKS, var += BLOCKS) {
            cuda::configure(BLOCKS, THREADS);
            _gpu::mean_quartic_displacement(*(*sample)[type].r, *(*first.first)[type].r, count, mean, var, (*sample)[type].r->size());
        }
        // copy accumulator block results from GPU to host
        cuda::copy(g_count, h_count);
        cuda::copy(g_mean, h_mean);
        cuda::copy(g_var, h_var);
        // accumulate mean quartic displacements on host
        for (sample = first.first, count0 = h_count.data(), mean = h_mean.data(), var = h_var.data(); sample != last.first; ++sample, ++result, count0 += BLOCKS) {
            for (count = count0; count != count0 + BLOCKS; ++count, ++mean, ++var) {
                *result += accumulator_type(*count, *mean, *var);
            }
        }
    }
};

/**
 * velocity autocorrelation
 */
template <>
struct velocity_autocorrelation<tcf_gpu_sample> : correlation_function<tcf_gpu_sample>
{
    /** block sample results */
    tcf_unary_result_type result;

    /** device and host memory for accumulators */
    cuda::vector<unsigned int> g_count;
    cuda::host::vector<unsigned int> h_count;
    cuda::vector<dsfloat> g_mean;
    cuda::host::vector<dsfloat> h_mean;
    cuda::vector<dsfloat> g_var;
    cuda::host::vector<dsfloat> h_var;

    char const* name() const { return "VAC"; }

    /**
     * autocorrelate samples in block
     */
    template <typename input_iterator, typename output_iterator>
    void operator()(input_iterator const& first, input_iterator const& last, output_iterator result)
    {
        typedef typename input_iterator::first_type sample_iterator;
        typedef typename sample_iterator::value_type::value_type sample_type;
        typedef typename sample_type::_gpu _gpu;
        typedef typename output_iterator::value_type accumulator_type;
        enum { BLOCKS = _gpu::BLOCKS };
        enum { THREADS = _gpu::THREADS };

        sample_iterator sample;
        unsigned int *count, *count0;
        dsfloat *mean, *var;

        // allocate device and host memory for accumulators, if necessary
        g_count.resize((last.first - first.first) * BLOCKS);
        h_count.resize(g_count.size());
        g_mean.resize((last.first - first.first) * BLOCKS);
        h_mean.resize(g_mean.size());
        g_var.resize((last.first - first.first) * BLOCKS);
        h_var.resize(g_var.size());

        // compute velocity autocorrelations on GPU
        for (sample = first.first, count = g_count.data(), mean = g_mean.data(), var = g_var.data(); sample != last.first; ++sample, count += BLOCKS, mean += BLOCKS, var += BLOCKS) {
            cuda::configure(BLOCKS, THREADS);
            _gpu::velocity_autocorrelation(*(*sample)[type].v, *(*first.first)[type].v, count, mean, var, (*sample)[type].v->size());
        }
        // copy accumulator block results from GPU to host
        cuda::copy(g_count, h_count);
        cuda::copy(g_mean, h_mean);
        cuda::copy(g_var, h_var);
        // accumulate velocity autocorrelations on host
        for (sample = first.first, count0 = h_count.data(), mean = h_mean.data(), var = h_var.data(); sample != last.first; ++sample, ++result, count0 += BLOCKS) {
            for (count = count0; count != count0 + BLOCKS; ++count, ++mean, ++var) {
                *result += accumulator_type(*count, *mean, *var);
            }
        }
    }
};

/**
 * velocity autocorrelation of fastest particles given lower boundary
 */
template <>
struct velocity_autocorrelation_fastest<tcf_gpu_sample> : correlation_function<tcf_gpu_sample>
{
    /** block sample results */
    tcf_binary_result_type result;

    std::vector<float> v_sq_min; //< set by tcf_add_minimum_velocity_filter

    /** device and host memory for accumulators */
    cuda::vector<unsigned int> g_count;
    cuda::host::vector<unsigned int> h_count;
    cuda::vector<dsfloat> g_mean;
    cuda::host::vector<dsfloat> h_mean;
    cuda::vector<dsfloat> g_var;
    cuda::host::vector<dsfloat> h_var;

    char const* name() const { return "VAC_FASTEST"; }

    /**
     * autocorrelate samples in block
     */
    template <typename input_iterator, typename output_iterator>
    void operator()(input_iterator const& first, input_iterator const& last, output_iterator const& result)
    {
        typedef typename input_iterator::first_type sample_iterator;
        typedef typename sample_iterator::value_type::value_type sample_type;
        typedef typename sample_type::_gpu _gpu;
        typedef typename output_iterator::value_type result_vector;
        typedef typename output_iterator::value_type::value_type accumulator_type;
        enum { BLOCKS = _gpu::BLOCKS };
        enum { THREADS = _gpu::THREADS };

        sample_iterator sample;
        output_iterator result0;
        std::vector<float>::const_iterator vv;
        unsigned int *count, *count0;
        dsfloat *mean, *var;

        // allocate device and host memory for accumulators, if necessary
        g_count.resize((last.first - first.first) * BLOCKS);
        h_count.resize(g_count.size());
        g_mean.resize((last.first - first.first) * BLOCKS);
        h_mean.resize(g_mean.size());
        g_var.resize((last.first - first.first) * BLOCKS);
        h_var.resize(g_var.size());

        for (vv = v_sq_min.begin(); vv != v_sq_min.end(); ++vv) {
            // copy constants to GPU
            cuda::copy(*vv, gpu::tcf_base::velocity_autocorrelation_fastest::min_sq_v);
            // compute velocity autocorrelations on GPU
            for (sample = first.first, count = g_count.data(), mean = g_mean.data(), var = g_var.data(); sample != last.first; ++sample, count += BLOCKS, mean += BLOCKS, var += BLOCKS) {
                cuda::configure(BLOCKS, THREADS);
                _gpu::velocity_autocorrelation_fastest(*(*sample)[type].v, *(*first.first)[type].v, count, mean, var, (*sample)[type].v->size());
            }
            // copy accumulator block results from GPU to host
            cuda::copy(g_count, h_count);
            cuda::copy(g_mean, h_mean);
            cuda::copy(g_var, h_var);
            // accumulate velocity autocorrelations on host
            for (sample = first.first, result0 = result, count0 = h_count.data(), mean = h_mean.data(), var = h_var.data(); sample != last.first; ++sample, ++result0, count0 += BLOCKS) {
                for (count = count0; count != count0 + BLOCKS; ++count, ++mean, ++var) {
                    *(result0->begin() + (vv - v_sq_min.begin())) += accumulator_type(*count, *mean, *var);
                }
            }
        }
    }
};

/**
 * velocity autocorrelation of slowest particles given upper boundary
 */
template <>
struct velocity_autocorrelation_slowest<tcf_gpu_sample> : correlation_function<tcf_gpu_sample>
{
    /** block sample results */
    tcf_binary_result_type result;

    std::vector<float> v_sq_max; //< set by tcf_add_maximum_velocity_filter

    /** device and host memory for accumulators */
    cuda::vector<unsigned int> g_count;
    cuda::host::vector<unsigned int> h_count;
    cuda::vector<dsfloat> g_mean;
    cuda::host::vector<dsfloat> h_mean;
    cuda::vector<dsfloat> g_var;
    cuda::host::vector<dsfloat> h_var;

    char const* name() const { return "VAC_SLOWEST"; }

    /**
     * autocorrelate samples in block
     */
    template <typename input_iterator, typename output_iterator>
    void operator()(input_iterator const& first, input_iterator const& last, output_iterator const& result)
    {
        typedef typename input_iterator::first_type sample_iterator;
        typedef typename sample_iterator::value_type::value_type sample_type;
        typedef typename sample_type::_gpu _gpu;
        typedef typename output_iterator::value_type result_vector;
        typedef typename output_iterator::value_type::value_type accumulator_type;
        enum { BLOCKS = _gpu::BLOCKS };
        enum { THREADS = _gpu::THREADS };

        sample_iterator sample;
        output_iterator result0;
        std::vector<float>::const_iterator vv;
        unsigned int *count, *count0;
        dsfloat *mean, *var;

        // allocate device and host memory for accumulators, if necessary
        g_count.resize((last.first - first.first) * BLOCKS);
        h_count.resize(g_count.size());
        g_mean.resize((last.first - first.first) * BLOCKS);
        h_mean.resize(g_mean.size());
        g_var.resize((last.first - first.first) * BLOCKS);
        h_var.resize(g_var.size());

        for (vv = v_sq_max.begin(); vv != v_sq_max.end(); ++vv) {
            // copy constants to GPU
            cuda::copy(*vv, gpu::tcf_base::velocity_autocorrelation_slowest::max_sq_v);
            // compute velocity autocorrelations on GPU
            for (sample = first.first, count = g_count.data(), mean = g_mean.data(), var = g_var.data(); sample != last.first; ++sample, count += BLOCKS, mean += BLOCKS, var += BLOCKS) {
                cuda::configure(BLOCKS, THREADS);
                _gpu::velocity_autocorrelation_slowest(*(*sample)[type].v, *(*first.first)[type].v, count, mean, var, (*sample)[type].v->size());
            }
            // copy accumulator block results from GPU to host
            cuda::copy(g_count, h_count);
            cuda::copy(g_mean, h_mean);
            cuda::copy(g_var, h_var);
            // accumulate velocity autocorrelations on host
            for (sample = first.first, result0 = result, count0 = h_count.data(), mean = h_mean.data(), var = h_var.data(); sample != last.first; ++sample, ++result0, count0 += BLOCKS) {
                for (count = count0; count != count0 + BLOCKS; ++count, ++mean, ++var) {
                    *(result0->begin() + (vv - v_sq_max.begin())) += accumulator_type(*count, *mean, *var);
                }
            }
        }
    }
};

/**
 * self-intermediate scattering function
 */
template <>
struct self_intermediate_scattering_function<tcf_gpu_sample> : correlation_function<tcf_gpu_sample>
{
    /** block sample results */
    tcf_binary_result_type result;

    /** device and host memory for accumulators */
    cuda::vector<dsfloat> g_sum;
    cuda::host::vector<dsfloat> h_sum;

    char const* name() const { return "SISF"; }

    /**
     * autocorrelate samples in block
     */
    template <typename input_iterator, typename output_iterator>
    void operator()(input_iterator const& first, input_iterator const& last, output_iterator result)
    {
        typedef typename input_iterator::first_type sample_iterator;
        typedef typename sample_iterator::value_type::value_type sample_type;
        typedef typename sample_type::_gpu _gpu;
        typedef typename sample_type::q_vector_vector q_vector_vector;
        typedef typename sample_type::isf_vector_vector isf_vector_vector;
        typedef typename output_iterator::value_type result_vector;
        enum { BLOCKS = _gpu::BLOCKS };
        enum { THREADS = _gpu::THREADS };

        sample_iterator sample;
        typename q_vector_vector::const_iterator q0;
        typename q_vector_vector::value_type::const_iterator q1;
        typename isf_vector_vector::iterator isf0;
        typename isf_vector_vector::value_type::iterator isf;
        typename result_vector::iterator result0;
        dsfloat* sum;

        // allocate device and host memory for accumulators, if necessary
        size_t size = 0;
        for (q0 = first.second; q0 != last.second; ++q0) {
            size += q0->size();
        }
        g_sum.resize((last.first - first.first) * size * BLOCKS);
        h_sum.resize(g_sum.size());

        // compute self-intermediate scattering functions on GPU
        for (sample = first.first, sum = g_sum.data(); sample != last.first; ++sample) {
            for (q0 = first.second; q0 != last.second; ++q0) {
                for (q1 = q0->begin(); q1 != q0->end(); ++q1, sum += BLOCKS) {
                    cuda::configure(BLOCKS, THREADS);
                    _gpu::incoherent_scattering_function(*(*sample)[type].r, *(*first.first)[type].r, *q1, sum, (*sample)[type].r->size());
                }
            }
        }
        // copy accumulator block results from GPU to host
        cuda::copy(g_sum, h_sum);
        // accumulate self-intermediate scattering functions on host
        for (sample = first.first, sum = h_sum.data(); sample != last.first; ++sample, ++result) {
            for (q0 = first.second, isf0 = (*sample)[type].isf->begin(), result0 = result->begin(); q0 != last.second; ++q0, ++isf0, ++result0) {
                for (q1 = q0->begin(), isf = isf0->begin(); q1 != q0->end(); ++q1, ++isf, sum += BLOCKS) {
                    *isf = std::accumulate(sum, sum + BLOCKS, 0.) / (*sample)[type].r->size();
                    *result0 += *isf;
                }
            }
        }
    }
};

/** correlation function types */
typedef boost::mpl::vector<mean_square_displacement<tcf_gpu_sample> > _tcf_gpu_types_0;
typedef boost::mpl::push_back<_tcf_gpu_types_0, mean_quartic_displacement<tcf_gpu_sample> >::type _tcf_gpu_types_1;
typedef boost::mpl::push_back<_tcf_gpu_types_1, velocity_autocorrelation<tcf_gpu_sample> >::type _tcf_gpu_types_2;
typedef boost::mpl::push_back<_tcf_gpu_types_2, intermediate_scattering_function<tcf_gpu_sample> >::type _tcf_gpu_types_3;
typedef boost::mpl::push_back<_tcf_gpu_types_3, self_intermediate_scattering_function<tcf_gpu_sample> >::type _tcf_gpu_types_4;
typedef boost::mpl::push_back<_tcf_gpu_types_4, squared_self_intermediate_scattering_function<tcf_gpu_sample> >::type _tcf_gpu_types_5;
typedef boost::mpl::push_back<_tcf_gpu_types_5, virial_stress<tcf_gpu_sample> >::type _tcf_gpu_types_6;
typedef boost::mpl::push_back<_tcf_gpu_types_6, helfand_moment<tcf_gpu_sample> >::type _tcf_gpu_types_7;
typedef boost::mpl::push_back<_tcf_gpu_types_7, velocity_autocorrelation_fastest<tcf_gpu_sample> >::type _tcf_gpu_types_8;
typedef boost::mpl::push_back<_tcf_gpu_types_8, velocity_autocorrelation_slowest<tcf_gpu_sample> >::type tcf_gpu_types;

} // namespace halmd

#endif /* ! HALMD_SAMPLE_TCF_GPU_HPP */
