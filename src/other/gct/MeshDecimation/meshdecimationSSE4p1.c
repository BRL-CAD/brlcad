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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>


#include "cpuconfig.h"

#include "cpuinfo.h"

#include "cc.h"
#include "mm.h"
#include "mmhash.h"
#include "math3d.h"

#include "mmbinsort.h"
#include "meshdecimation.h"


#ifdef __SSE__
 #include <xmmintrin.h>
#endif
#ifdef __SSE2__
 #include <emmintrin.h>
#endif
#ifdef __SSE3__
 #include <pmmintrin.h>
#endif
#ifdef __SSSE3__
 #include <tmmintrin.h>
#endif
#ifdef __SSE4A__
 #include <ammintrin.h>
#endif
#ifdef __SSE4_1__
 #include <smmintrin.h>
#endif


/****/


#ifdef __SSE4_1__
 #define MD_CONFIG_SSE4_1_SUPPORT
#else
 #warning "SSE4.1 wasn't enabled to compile this file."
#endif


#if defined(__GNUC__) || defined(__INTEL_COMPILER)
 #define RF_ALIGN16 __attribute__((aligned(16)))
#elif defined(_MSC_VER)
 #define RF_ALIGN16 __declspec(align(16))
#else
 #define RF_ALIGN16
 #warning "SSE Disabled: Unsupported Compiler."
 #ifdef MD_CONFIG_SSE4_1_SUPPORT
  #undef MD_CONFIG_SSE4_1_SUPPORT
 #endif
#endif



#define MD_CONFIG_SSE_APPROX



#ifdef MD_CONFIG_SSE4_1_SUPPORT


int mdPathSSE4p1 = 0x1 | 0x2;


#define MD_COMPACTNESS_NORMALIZATION_FACTOR (0.5*4.0*1.732050808)

float mdEdgeCollapsePenaltyTriangleSSE4p1f( float *newpoint, float *oldpoint, float *leftpoint, float *rightpoint, int *denyflag, float compactnesstarget )
{
  float penalty, compactness, oldcompactness, newcompactness;
  static float zero = 0.0;
  __m128 left, vecta, oldvectb, oldvectc, newvectb, newvectc, oldnormal, newnormal;

  /* Normal of old triangle */
  left = _mm_load_ps( leftpoint );
  vecta = _mm_sub_ps( _mm_load_ps( rightpoint ), left );
  oldvectb = _mm_sub_ps( _mm_load_ps( oldpoint ), left );
  oldnormal = _mm_sub_ps(
    _mm_mul_ps( _mm_shuffle_ps( vecta, vecta, _MM_SHUFFLE(3,0,2,1) ), _mm_shuffle_ps( oldvectb, oldvectb, _MM_SHUFFLE(3,1,0,2) ) ),
    _mm_mul_ps( _mm_shuffle_ps( vecta, vecta, _MM_SHUFFLE(3,1,0,2) ), _mm_shuffle_ps( oldvectb, oldvectb, _MM_SHUFFLE(3,0,2,1) ) )
  );

  /* Normal of new triangle */
  newvectb = _mm_sub_ps( _mm_load_ps( newpoint ), left );
  newnormal = _mm_sub_ps(
    _mm_mul_ps( _mm_shuffle_ps( vecta, vecta, _MM_SHUFFLE(3,0,2,1) ), _mm_shuffle_ps( newvectb, newvectb, _MM_SHUFFLE(3,1,0,2) ) ),
    _mm_mul_ps( _mm_shuffle_ps( vecta, vecta, _MM_SHUFFLE(3,1,0,2) ), _mm_shuffle_ps( newvectb, newvectb, _MM_SHUFFLE(3,0,2,1) ) )
  );

  /* Detect normal inversion */
  if( _mm_comilt_ss( _mm_dp_ps( oldnormal, newnormal, 0x1 | 0x70 ), _mm_load_ss( &zero ) ) )
  {
    *denyflag = 1;
    return 0.0;
  }

  /* Penalize long thin triangles */
  penalty = 0.0;
  vecta = _mm_dp_ps( vecta, vecta, 0x1 | 0x70 );
  newvectc = _mm_sub_ps( _mm_load_ps( newpoint ), _mm_load_ps( rightpoint ) );
  newnormal = _mm_dp_ps( newnormal, newnormal, 0x1 | 0x70 );
  newvectb = _mm_dp_ps( newvectb, newvectb, 0x1 | 0x70 );
  newvectc = _mm_dp_ps( newvectc, newvectc, 0x1 | 0x70 );
#ifdef MD_CONFIG_SSE_APPROX
  newcompactness = _mm_cvtss_f32( _mm_mul_ss( _mm_set_ss( MD_COMPACTNESS_NORMALIZATION_FACTOR ), _mm_rcp_ss( _mm_mul_ss( _mm_rsqrt_ss( newnormal ), _mm_add_ss( _mm_add_ss( vecta, newvectb ), newvectc ) ) ) ) );
#else
  newcompactness = _mm_cvtss_f32( _mm_div_ss( _mm_mul_ss( _mm_set_ss( MD_COMPACTNESS_NORMALIZATION_FACTOR ), _mm_sqrt_ss( newnormal ) ), _mm_add_ss( _mm_add_ss( vecta, newvectb ), newvectc ) ) );
#endif
  if( newcompactness < compactnesstarget )
  {
    oldvectc = _mm_sub_ps( _mm_load_ps( oldpoint ), _mm_load_ps( rightpoint ) );
    oldnormal = _mm_dp_ps( oldnormal, oldnormal, 0x1 | 0x70 );
    oldvectb = _mm_dp_ps( oldvectb, oldvectb, 0x1 | 0x70 );
    oldvectc = _mm_dp_ps( oldvectc, oldvectc, 0x1 | 0x70 );
#ifdef MD_CONFIG_SSE_APPROX
    oldcompactness = _mm_cvtss_f32( _mm_mul_ss( _mm_set_ss( MD_COMPACTNESS_NORMALIZATION_FACTOR ), _mm_rcp_ss( _mm_mul_ss( _mm_rsqrt_ss( oldnormal ), _mm_add_ss( _mm_add_ss( vecta, oldvectb ), oldvectc ) ) ) ) );
#else
    oldcompactness = _mm_cvtss_f32( _mm_div_ss( _mm_mul_ss( _mm_set_ss( MD_COMPACTNESS_NORMALIZATION_FACTOR ), _mm_sqrt_ss( oldnormal ) ), _mm_add_ss( _mm_add_ss( vecta, oldvectb ), oldvectc ) ) );
#endif
    compactness = fmin( compactnesstarget, oldcompactness ) - newcompactness;
    if( compactness > 0.0 )
      penalty = compactness;
  }

  return penalty;
}



double mdEdgeCollapsePenaltyTriangleSSE4p1d( double *newpoint, double *oldpoint, double *leftpoint, double *rightpoint, int *denyflag, double compactnesstarget )
{
  __m128d vecta0, vecta1, oldvectb0, oldvectb1, oldvectc0, oldvectc1, newvectb0, newvectb1, newvectc0, newvectc1;
  __m128d oldnormal0, oldnormal1, newnormal0, newnormal1;
  __m128d dotprsum;
  __m128d left0, left1;
  double newcompactness, oldcompactness, compactness, penalty, norm;
  static double zero = 0.0;

  /* Normal of old triangle */
  left0 = _mm_load_pd( leftpoint+0 );
  left1 = _mm_load_sd( leftpoint+2 );
  vecta0 = _mm_sub_pd( _mm_load_pd( rightpoint+0 ), left0 );
  vecta1 = _mm_sub_pd( _mm_load_pd( rightpoint+2 ), left1 );
  oldvectb0 = _mm_sub_pd( _mm_load_pd( oldpoint+0 ), left0 );
  oldvectb1 = _mm_sub_pd( _mm_load_pd( oldpoint+2 ), left1 );
  oldnormal0 = _mm_sub_pd(
    _mm_mul_pd( _mm_shuffle_pd( vecta0, vecta1, _MM_SHUFFLE2(0,1) ), _mm_unpacklo_pd( oldvectb1, oldvectb0 ) ),
    _mm_mul_pd( _mm_unpacklo_pd( vecta1, vecta0 ), _mm_shuffle_pd( oldvectb0, oldvectb1, _MM_SHUFFLE2(0,1) ) )
  );
  oldnormal1 = _mm_sub_sd(
    _mm_mul_sd( vecta0, _mm_unpackhi_pd( oldvectb0, oldvectb0 ) ),
    _mm_mul_sd( _mm_unpackhi_pd( vecta0, vecta0 ), oldvectb0 )
  );

  /* Normal of new triangle */
  newvectb0 = _mm_sub_pd( _mm_load_pd( newpoint+0 ), left0 );
  newvectb1 = _mm_sub_pd( _mm_load_pd( newpoint+2 ), left1 );
  newnormal0 = _mm_sub_pd(
    _mm_mul_pd( _mm_shuffle_pd( vecta0, vecta1, _MM_SHUFFLE2(0,1) ), _mm_unpacklo_pd( newvectb1, newvectb0 ) ),
    _mm_mul_pd( _mm_unpacklo_pd( vecta1, vecta0 ), _mm_shuffle_pd( newvectb0, newvectb1, _MM_SHUFFLE2(0,1) ) )
  );
  newnormal1 = _mm_sub_sd(
    _mm_mul_sd( vecta0, _mm_unpackhi_pd( newvectb0, newvectb0 ) ),
    _mm_mul_sd( _mm_unpackhi_pd( vecta0, vecta0 ), newvectb0 )
  );

  /* Detect normal inversion */
  dotprsum = _mm_add_sd( _mm_dp_pd( oldnormal0, oldnormal0, 0x1 | 0x30 ), _mm_mul_sd( oldnormal1, newnormal1 ) );
  if( _mm_comilt_sd( dotprsum, _mm_load_sd( &zero ) ) )
  {
    *denyflag = 1;
    return 0.0;
  }

  /* Penalize long thin triangles */
  penalty = 0.0;
  vecta0 = _mm_add_sd( _mm_dp_pd( vecta0, vecta0, 0x1 | 0x30 ), _mm_mul_sd( vecta1, vecta1 ) );
  newvectc0 = _mm_sub_pd( _mm_load_pd( newpoint+0 ), _mm_load_pd( rightpoint+0 ) );
  newvectc1 = _mm_sub_sd( _mm_load_sd( newpoint+2 ), _mm_load_sd( rightpoint+2 ) );
  newnormal0 = _mm_add_sd( _mm_dp_pd( newnormal0, newnormal0, 0x1 | 0x30 ), _mm_mul_sd( newnormal1, newnormal1 ) );
  newvectb0 = _mm_add_sd( _mm_dp_pd( newvectb0, newvectb0, 0x1 | 0x30 ), _mm_mul_sd( newvectb1, newvectb1 ) );
  newvectc0 = _mm_add_sd( _mm_dp_pd( newvectc0, newvectc0, 0x1 | 0x30 ), _mm_mul_sd( newvectc1, newvectc1 ) );
  norm = _mm_cvtsd_f64( _mm_add_sd( vecta0, _mm_add_sd( newvectb0, newvectc0 ) ) );
  newcompactness = _mm_cvtsd_f64( _mm_mul_sd( _mm_set_sd( MD_COMPACTNESS_NORMALIZATION_FACTOR ), _mm_sqrt_sd( newnormal0, newnormal0 ) ) );
  if( newcompactness < ( compactnesstarget * norm ) )
  {
    newcompactness /= norm;
    oldvectc0 = _mm_sub_pd( _mm_load_pd( oldpoint+0 ), _mm_load_pd( rightpoint+0 ) );
    oldvectc1 = _mm_sub_sd( _mm_load_sd( oldpoint+2 ), _mm_load_sd( rightpoint+2 ) );
    oldnormal0 = _mm_add_sd( _mm_dp_pd( oldnormal0, oldnormal0, 0x1 | 0x30 ), _mm_mul_sd( oldnormal1, oldnormal1 ) );
    oldvectb0 = _mm_add_sd( _mm_dp_pd( oldvectb0, oldvectb0, 0x1 | 0x30 ), _mm_mul_sd( oldvectb1, oldvectb1 ) );
    oldvectc0 = _mm_add_sd( _mm_dp_pd( oldvectc0, oldvectc0, 0x1 | 0x30 ), _mm_mul_sd( oldvectc1, oldvectc1 ) );
    oldcompactness = _mm_cvtsd_f64( _mm_div_sd( _mm_mul_sd( _mm_set_sd( MD_COMPACTNESS_NORMALIZATION_FACTOR ), _mm_sqrt_sd( oldnormal0, oldnormal0 ) ), _mm_add_sd( vecta0, _mm_add_sd( oldvectb0, oldvectc0 ) ) ) );
    compactness = fmin( compactnesstarget, oldcompactness ) - newcompactness;
    if( compactness > 0.0 )
      penalty = compactness;
  }

  return penalty;
}


#else


int mdPathSSE4p1 = 0x0;

float mdEdgeCollapsePenaltyTriangleSSE4p1f( float *newpoint, float *oldpoint, float *leftpoint, float *rightpoint, int *denyflag )
{
  return 0.0;
}

double mdEdgeCollapsePenaltyTriangleSSE4p1d( double *newpoint, double *oldpoint, double *leftpoint, double *rightpoint, int *denyflag )
{
  return 0.0;
}


#endif




