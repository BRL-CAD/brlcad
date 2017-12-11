# Copyright (c) 2016-     Facebook, Inc            (Adam Paszke)
# Copyright (c) 2014-     Facebook, Inc            (Soumith Chintala)
# Copyright (c) 2011-2014 Idiap Research Institute (Ronan Collobert)
# Copyright (c) 2012-2014 Deepmind Technologies    (Koray Kavukcuoglu)
# Copyright (c) 2011-2012 NEC Laboratories America (Koray Kavukcuoglu)
# Copyright (c) 2011-2013 NYU                      (Clement Farabet)
# Copyright (c) 2006-2010 NEC Laboratories America (Ronan Collobert, Leon Bottou, Iain Melvin, Jason Weston)
# Copyright (c) 2006      Idiap Research Institute (Samy Bengio)
# Copyright (c) 2001-2004 Idiap Research Institute (Ronan Collobert, Samy Bengio, Johnny Mariethoz)
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# 3. Neither the names of Facebook, Deepmind Technologies, NYU, NEC Laboratories America
#    and IDIAP Research Institute nor the names of its contributors may be
#    used to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

include(CheckCSourceRuns)
include(CheckCXXSourceRuns)

set(SSE1_CODE "
  #include <xmmintrin.h>

  int main()
  {
    __m128 a;
    float vals[4] = {0,0,0,0};
    a = _mm_loadu_ps(vals);
    return 0;
  }")

set(SSE2_CODE "
  #include <emmintrin.h>

  int main()
  {
    __m128d a;
    double vals[2] = {0,0};
    a = _mm_loadu_pd(vals);
    return 0;
  }")

set(SSE3_CODE "
  #include <pmmintrin.h>

  int main( )
  {
    const int vals[4] = {0,0,0,0};
    __m128i a;
    a = _mm_lddqu_si128( (const __m128i*)vals );
    return 0;
  }")

set(SSE4_1_CODE "
  #include <smmintrin.h>

  int main ()
  {
    __m128i a = {0,0,0,0}, b = {0,0,0,0};
    __m128i res = _mm_max_epi8(a, b);

    return 0;
  }
")

set(SSE4_2_CODE "
  #include <nmmintrin.h>

  int main()
  {
    __m128i a = {0,0,0,0}, b = {0,0,0,0}, c = {0,0,0,0};
    c = _mm_cmpgt_epi64(a, b);
    return 0;
  }
")

set(AVX_CODE "
  #include <immintrin.h>

  int main()
  {
    __m256 a;
    a = _mm256_set1_ps(0);
    return 0;
  }
")

set(AVX2_CODE "
  #include <immintrin.h>

  int main()
  {
    __m256i a = {0};
    a = _mm256_abs_epi16(a);
    return 0;
  }
")

macro(CHECK_SSE lang type flags)
  set(__FLAG_I 1)
  set(CMAKE_REQUIRED_FLAGS_SAVE ${CMAKE_REQUIRED_FLAGS})
  foreach(__FLAG ${flags})
    if(NOT ${lang}_${type}_FOUND)
      set(CMAKE_REQUIRED_FLAGS ${__FLAG})
      if("${lang}" STREQUAL "CXX")
        CHECK_CXX_SOURCE_RUNS("${${type}_CODE}" "${lang}_HAS_${type}_${__FLAG_I}")
      else()
        CHECK_C_SOURCE_RUNS("${${type}_CODE}" "${lang}_HAS_${type}_${__FLAG_I}")
      endif("${lang}" STREQUAL "CXX")
      if(${lang}_HAS_${type}_${__FLAG_I})
        set(${lang}_${type}_FOUND TRUE CACHE BOOL "${lang} ${type} support")
        set(${lang}_${type}_FLAGS "${__FLAG}" CACHE STRING "${lang} ${type} flags")
      endif(${lang}_HAS_${type}_${__FLAG_I})
      math(EXPR __FLAG_I "${__FLAG_I}+1")
    endif(NOT ${lang}_${type}_FOUND)
  endforeach(__FLAG ${flags})
  set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVE})

  if(NOT ${lang}_${type}_FOUND)
    set(${lang}_${type}_FOUND FALSE CACHE BOOL "${lang} ${type} support")
    set(${lang}_${type}_FLAGS "" CACHE STRING "${lang} ${type} flags")
  endif(NOT ${lang}_${type}_FOUND)

  mark_as_advanced(${lang}_${type}_FOUND ${lang}_${type}_FLAGS)

endmacro(CHECK_SSE)

# Examples:

# CHECK_SSE(C "SSE1" " ;-msse;/arch:SSE")
# CHECK_SSE(C "SSE2" " ;-msse2;/arch:SSE2")
# CHECK_SSE(C "SSE3" " ;-msse3;/arch:SSE3")
# CHECK_SSE(C "SSE4_1" " ;-msse4.1;-msse4;/arch:SSE4")
# CHECK_SSE(C "SSE4_2" " ;-msse4.2;-msse4;/arch:SSE4")
# CHECK_SSE(C "AVX" " ;-mavx;/arch:AVX")
# CHECK_SSE(C "AVX2" " ;-mavx2 -mfma;/arch:AVX2")

# CHECK_SSE(CXX "SSE1" " ;-msse;/arch:SSE")
# CHECK_SSE(CXX "SSE2" " ;-msse2;/arch:SSE2")
# CHECK_SSE(CXX "SSE3" " ;-msse3;/arch:SSE3")
# CHECK_SSE(CXX "SSE4_1" " ;-msse4.1;-msse4;/arch:SSE4")
# CHECK_SSE(CXX "SSE4_2" " ;-msse4.2;-msse4;/arch:SSE4")
# CHECK_SSE(CXX "AVX" " ;-mavx;/arch:AVX")
# CHECK_SSE(CXX "AVX2" " ;-mavx2 -mfma;/arch:AVX2")
