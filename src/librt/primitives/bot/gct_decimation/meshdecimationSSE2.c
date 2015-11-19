/* *****************************************************************************
 *
 * Copyright (c) 2014 Alexis Naveros. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * *****************************************************************************
 */


#if defined(__GNUC__) && (__GNUC__ == 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wunused-function"
#endif


#include "common.h"


#if defined(__SSE2__) && (defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(_MSC_VER))

#include "auxiliary/mm.h"

#include "vmath.h"

#include <emmintrin.h>


#define MD_CONFIG_SSE_APPROX


typedef union {
    __m128 v;
    float f[4];
} m128f;

int mdPathSSE2 = 0x1 | 0x2;


#define MD_COMPACTNESS_NORMALIZATION_FACTOR (0.5*4.0*1.732050808)


float mdEdgeCollapsePenaltyTriangleSSE2f(float *newpoint, float *oldpoint, float *leftpoint, float *rightpoint, int *denyflag, float compactnesstarget)
{
    float penalty, compactness, oldcompactness, newcompactness, vectadp, norm;
    __m128 left;
    m128f vecta, oldvectb, oldvectc, newvectb, newvectc, oldnormal, newnormal;

    /* Normal of old triangle */
    left = _mm_load_ps(leftpoint);
    vecta.v = _mm_sub_ps(_mm_load_ps(rightpoint), left);
    oldvectb.v = _mm_sub_ps(_mm_load_ps(oldpoint), left);
    oldnormal.v = _mm_sub_ps(
		      _mm_mul_ps(_mm_shuffle_ps(vecta.v, vecta.v, _MM_SHUFFLE(3, 0, 2, 1)), _mm_shuffle_ps(oldvectb.v, oldvectb.v, _MM_SHUFFLE(3, 1, 0, 2))),
		      _mm_mul_ps(_mm_shuffle_ps(vecta.v, vecta.v, _MM_SHUFFLE(3, 1, 0, 2)), _mm_shuffle_ps(oldvectb.v, oldvectb.v, _MM_SHUFFLE(3, 0, 2, 1)))
		  );

    /* Normal of new triangle */
    newvectb.v = _mm_sub_ps(_mm_load_ps(newpoint), left);
    newnormal.v = _mm_sub_ps(
		      _mm_mul_ps(_mm_shuffle_ps(vecta.v, vecta.v, _MM_SHUFFLE(3, 0, 2, 1)), _mm_shuffle_ps(newvectb.v, newvectb.v, _MM_SHUFFLE(3, 1, 0, 2))),
		      _mm_mul_ps(_mm_shuffle_ps(vecta.v, vecta.v, _MM_SHUFFLE(3, 1, 0, 2)), _mm_shuffle_ps(newvectb.v, newvectb.v, _MM_SHUFFLE(3, 0, 2, 1)))
		  );

    /* Detect normal inversion */
    if (VDOT(oldnormal.f, newnormal.f) < 0.0) {
	*denyflag = 1;
	return 0.0;
    }

    /* Penalize long thin triangles */
    penalty = 0.0;
    vectadp = VDOT(vecta.f, vecta.f);
    newvectc.v = _mm_sub_ps(_mm_load_ps(newpoint), _mm_load_ps(rightpoint));
#ifdef MD_CONFIG_SSE_APPROX
    norm = vectadp + VDOT(newvectb.f, newvectb.f) + VDOT(newvectc.f, newvectc.f);
    newcompactness = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(MD_COMPACTNESS_NORMALIZATION_FACTOR), _mm_rcp_ss(_mm_mul_ss(_mm_rsqrt_ss(_mm_set_ss(VDOT(newnormal.f, newnormal.f))), _mm_set_ss(norm)))));

    if (newcompactness < compactnesstarget) {
#else
    newcompactness = MD_COMPACTNESS_NORMALIZATION_FACTOR * sqrtf(VDOT(newnormal.f, newnormal.f));
    norm = vectadp + VDOT(newvectb.f, newvectb.f) + VDOT(newvectc.f, newvectc.f);

    if (newcompactness < (compactnesstarget * norm)) {
	newcompactness /= norm;
#endif
	oldvectc.v = _mm_sub_ps(_mm_load_ps(oldpoint), _mm_load_ps(rightpoint));
#ifdef MD_CONFIG_SSE_APPROX
	norm = vectadp + VDOT(oldvectb.f, oldvectb.f) + VDOT(oldvectc.f, oldvectc.f);
	oldcompactness = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(MD_COMPACTNESS_NORMALIZATION_FACTOR), _mm_rcp_ss(_mm_mul_ss(_mm_rsqrt_ss(_mm_set_ss(VDOT(oldnormal.f, oldnormal.f))), _mm_set_ss(norm)))));
#else
	oldcompactness = (MD_COMPACTNESS_NORMALIZATION_FACTOR * sqrtf(VDOT(oldnormal.f, oldnormal.f))) / (vectadp + VDOT(oldvectb.f, oldvectb.f) + VDOT(oldvectc.f, oldvectc.f));
#endif
	compactness = fmin(compactnesstarget, oldcompactness) - newcompactness;

	if (compactness > 0.0)
	    penalty = compactness;
    }

    return penalty;
}


double mdEdgeCollapsePenaltyTriangleSSE2d(double *newpoint, double *oldpoint, double *leftpoint, double *rightpoint, int *denyflag, double compactnesstarget)
{
    __m128d vecta0, vecta1, oldvectb0, oldvectb1, oldvectc0, oldvectc1, newvectb0, newvectb1, newvectc0, newvectc1;
    __m128d oldnormal0, oldnormal1, newnormal0, newnormal1;
    __m128d dotprsum;
    __m128d left0, left1;
    double newcompactness, oldcompactness, compactness, penalty, norm;
    static double zero = 0.0;

    /* Normal of old triangle */
    left0 = _mm_load_pd(leftpoint + 0);
    left1 = _mm_load_sd(leftpoint + 2);
    vecta0 = _mm_sub_pd(_mm_load_pd(rightpoint + 0), left0);
    vecta1 = _mm_sub_pd(_mm_load_pd(rightpoint + 2), left1);
    oldvectb0 = _mm_sub_pd(_mm_load_pd(oldpoint + 0), left0);
    oldvectb1 = _mm_sub_pd(_mm_load_pd(oldpoint + 2), left1);
    oldnormal0 = _mm_sub_pd(
		     _mm_mul_pd(_mm_shuffle_pd(vecta0, vecta1, _MM_SHUFFLE2(0, 1)), _mm_unpacklo_pd(oldvectb1, oldvectb0)),
		     _mm_mul_pd(_mm_unpacklo_pd(vecta1, vecta0), _mm_shuffle_pd(oldvectb0, oldvectb1, _MM_SHUFFLE2(0, 1)))
		 );
    oldnormal1 = _mm_sub_sd(
		     _mm_mul_sd(vecta0, _mm_unpackhi_pd(oldvectb0, oldvectb0)),
		     _mm_mul_sd(_mm_unpackhi_pd(vecta0, vecta0), oldvectb0)
		 );

    /* Normal of new triangle */
    newvectb0 = _mm_sub_pd(_mm_load_pd(newpoint + 0), left0);
    newvectb1 = _mm_sub_pd(_mm_load_pd(newpoint + 2), left1);
    newnormal0 = _mm_sub_pd(
		     _mm_mul_pd(_mm_shuffle_pd(vecta0, vecta1, _MM_SHUFFLE2(0, 1)), _mm_unpacklo_pd(newvectb1, newvectb0)),
		     _mm_mul_pd(_mm_unpacklo_pd(vecta1, vecta0), _mm_shuffle_pd(newvectb0, newvectb1, _MM_SHUFFLE2(0, 1)))
		 );
    newnormal1 = _mm_sub_sd(
		     _mm_mul_sd(vecta0, _mm_unpackhi_pd(newvectb0, newvectb0)),
		     _mm_mul_sd(_mm_unpackhi_pd(vecta0, vecta0), newvectb0)
		 );

    /* Detect normal inversion */
    dotprsum = _mm_mul_pd(oldnormal0, newnormal0);
    dotprsum = _mm_add_sd(_mm_add_sd(dotprsum, _mm_unpackhi_pd(dotprsum, dotprsum)), _mm_mul_sd(oldnormal1, newnormal1));

    if (_mm_comilt_sd(dotprsum, _mm_load_sd(&zero))) {
	*denyflag = 1;
	return 0.0;
    }

    /* Penalize long thin triangles */
    penalty = 0.0;
    vecta0 = _mm_mul_pd(vecta0, vecta0);
    vecta0 = _mm_add_sd(_mm_add_sd(vecta0, _mm_unpackhi_pd(vecta0, vecta0)), _mm_mul_sd(vecta1, vecta1));
    newvectc0 = _mm_sub_pd(_mm_load_pd(newpoint + 0), _mm_load_pd(rightpoint + 0));
    newvectc1 = _mm_sub_sd(_mm_load_sd(newpoint + 2), _mm_load_sd(rightpoint + 2));
    newnormal0 = _mm_mul_pd(newnormal0, newnormal0);
    newnormal0 = _mm_add_sd(_mm_add_sd(newnormal0, _mm_unpackhi_pd(newnormal0, newnormal0)), _mm_mul_sd(newnormal1, newnormal1));
    newvectb0 = _mm_mul_pd(newvectb0, newvectb0);
    newvectb0 = _mm_add_sd(_mm_add_sd(newvectb0, _mm_unpackhi_pd(newvectb0, newvectb0)), _mm_mul_sd(newvectb1, newvectb1));
    newvectc0 = _mm_mul_pd(newvectc0, newvectc0);
    newvectc0 = _mm_add_sd(_mm_add_sd(newvectc0, _mm_unpackhi_pd(newvectc0, newvectc0)), _mm_mul_sd(newvectc1, newvectc1));
    norm = _mm_cvtsd_f64(_mm_add_sd(vecta0, _mm_add_sd(newvectb0, newvectc0)));
    newcompactness = _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(MD_COMPACTNESS_NORMALIZATION_FACTOR), _mm_sqrt_sd(newnormal0, newnormal0)));

    if (newcompactness < (compactnesstarget * norm)) {
	newcompactness /= norm;
	oldvectc0 = _mm_sub_pd(_mm_load_pd(oldpoint + 0), _mm_load_pd(rightpoint + 0));
	oldvectc1 = _mm_sub_sd(_mm_load_sd(oldpoint + 2), _mm_load_sd(rightpoint + 2));
	oldnormal0 = _mm_mul_pd(oldnormal0, oldnormal0);
	oldnormal0 = _mm_add_sd(_mm_add_sd(oldnormal0, _mm_unpackhi_pd(oldnormal0, oldnormal0)), _mm_mul_sd(oldnormal1, oldnormal1));
	oldvectb0 = _mm_mul_pd(oldvectb0, oldvectb0);
	oldvectb0 = _mm_add_sd(_mm_add_sd(oldvectb0, _mm_unpackhi_pd(oldvectb0, oldvectb0)), _mm_mul_sd(oldvectb1, oldvectb1));
	oldvectc0 = _mm_mul_pd(oldvectc0, oldvectc0);
	oldvectc0 = _mm_add_sd(_mm_add_sd(oldvectc0, _mm_unpackhi_pd(oldvectc0, oldvectc0)), _mm_mul_sd(oldvectc1, oldvectc1));
	oldcompactness = _mm_cvtsd_f64(_mm_div_sd(_mm_mul_sd(_mm_set_sd(MD_COMPACTNESS_NORMALIZATION_FACTOR), _mm_sqrt_sd(oldnormal0, oldnormal0)), _mm_add_sd(vecta0, _mm_add_sd(oldvectb0, oldvectc0))));
	compactness = fmin(compactnesstarget, oldcompactness) - newcompactness;

	if (compactness > 0.0)
	    penalty = compactness;
    }

    return penalty;
}


#else


int mdPathSSE2 = 0x0;


float mdEdgeCollapsePenaltyTriangleSSE2f(float *UNUSED(newpoint), float *UNUSED(oldpoint), float *UNUSED(leftpoint), float *UNUSED(rightpoint), int *UNUSED(denyflag))
{
    return 0.0;
}


double mdEdgeCollapsePenaltyTriangleSSE2d(double *UNUSED(newpoint), double *UNUSED(oldpoint), double *UNUSED(leftpoint), double *UNUSED(rightpoint), int *UNUSED(denyflag))
{
    return 0.0;
}


#endif
