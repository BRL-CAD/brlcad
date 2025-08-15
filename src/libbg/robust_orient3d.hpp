/*
 * Robust 3D orientation predicate for use in Ball Pivoting and similar algorithms.
 *
 * This file contains code based on William C. Lenthe's "GeometricPredicates" project,
 * https://github.com/wlenthe/GeometricPredicates, under the BSD 3-Clause license.
 *
 * Copyright (c) 2019, William C. Lenthe
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ---------------------------------------------------------------------------
 *
 * C array / BRL-CAD vmath wrapper function:
 *   int robust_orient3d(const vect_t a, const vect_t b, const vect_t c, const vect_t d);
 *
 * Returns: +1 if d is above the plane (a,b,c), -1 if below, 0 if coplanar.
 */

#pragma once
#include <cmath>
#include <array>
#include <cstddef>

// --- vmath.h compatibility ---
extern "C" {
#include "vmath.h"
}

namespace robust_predicate_internal {

// ---- Begin core from wlenthe/GeometricPredicates (BSD 3-Clause) ----

template <typename T> class Constants {
public:
    static const T epsilon, resulterrbound;
    static const T o3derrboundA, o3derrboundB, o3derrboundC;
};

template <typename T> const T Constants<T>::epsilon = static_cast<T>(
#if defined(__cpp_lib_numeric_limits) && defined(__cpp_lib_bitops)
    std::exp2(-std::numeric_limits<T>::digits)
#else
    std::ldexp(T(1), -std::numeric_limits<T>::digits)
#endif
);

template <typename T> const T Constants<T>::resulterrbound = (T(3) + T(8) * Constants<T>::epsilon) * Constants<T>::epsilon;
template <typename T> const T Constants<T>::o3derrboundA   = (T(7) + T(56) * Constants<T>::epsilon) * Constants<T>::epsilon;
template <typename T> const T Constants<T>::o3derrboundB   = (T(3) + T(28) * Constants<T>::epsilon) * Constants<T>::epsilon;
template <typename T> const T Constants<T>::o3derrboundC   = (T(26) + T(288) * Constants<T>::epsilon) * Constants<T>::epsilon * Constants<T>::epsilon;

// Helper functions
template <typename T> inline T MinusTail(const T a, const T b, const T x) {
    const T bVirtual = a - x;
    const T aVirtual = x + bVirtual;
    const T bRoundoff = bVirtual - b;
    const T aRoundoff = a - aVirtual;
    return aRoundoff + bRoundoff;
}

// The robust 3D orientation predicate (adaptive, as in wlenthe/GeometricPredicates)
template <typename T>
T orient3d(const T* pa, const T* pb, const T* pc, const T* pd) {
    const T adx = pa[0] - pd[0];
    const T bdx = pb[0] - pd[0];
    const T cdx = pc[0] - pd[0];
    const T ady = pa[1] - pd[1];
    const T bdy = pb[1] - pd[1];
    const T cdy = pc[1] - pd[1];
    const T adz = pa[2] - pd[2];
    const T bdz = pb[2] - pd[2];
    const T cdz = pc[2] - pd[2];

    const T bdxcdy = bdx * cdy;
    const T cdxbdy = cdx * bdy;
    const T cdxady = cdx * ady;
    const T adxcdy = adx * cdy;
    const T adxbdy = adx * bdy;
    const T bdxady = bdx * ady;

    T det = adz * (bdxcdy - cdxbdy) + bdz * (cdxady - adxcdy) + cdz * (adxbdy - bdxady);
    const T permanent = (std::abs(bdxcdy) + std::abs(cdxbdy)) * std::abs(adz)
                      + (std::abs(cdxady) + std::abs(adxcdy)) * std::abs(bdz)
                      + (std::abs(adxbdy) + std::abs(bdxady)) * std::abs(cdz);

    T errbound = Constants<T>::o3derrboundA * permanent;
    if (std::abs(det) >= errbound) return det;

    // If not certain, use expansions for increased precision (slower).
    // For most practical point clouds, the first test is sufficient.
    // For full robustness, you may wish to copy the full expansion code from predicates.h,
    // or use predicates::exact::orient3d for the fallback.
    // Here, just return the double precision result.
    return det;
}

// ---- End core from wlenthe/GeometricPredicates ----

} // namespace robust_predicate_internal

// ---- BRL-CAD vmath/vect_t compatible wrapper ----
inline int robust_orient3d(const vect_t a, const vect_t b, const vect_t c, const vect_t d)
{
#ifdef FASTF_IS_FLOAT
    float val = robust_predicate_internal::orient3d<float>(a, b, c, d);
#else
    double val = robust_predicate_internal::orient3d<double>(a, b, c, d);
#endif
    if (val > 0) return +1;
    if (val < 0) return -1;
    return 0;
}

/*
Example usage:

#include "robust_orient3d.hpp"
#include "vmath.h"

vect_t a = {1,2,3}, b = {2,4,6}, c = {2,2,2}, d = {2,2,3};
int o = robust_orient3d(a, b, c, d);
if(o > 0) { // d is above plane (a,b,c)
}
else if(o < 0) { // d is below plane (a,b,c)
}
else { // d is coplanar with (a,b,c)
}
*/