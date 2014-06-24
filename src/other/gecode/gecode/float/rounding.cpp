/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2012
 *     Vincent Barichard, 2012
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <gecode/float.hh>

#ifdef GECODE_HAS_MPFR

#include <gmp.h>
#include <mpfr.h>

namespace Gecode { namespace Float {

  /// Type signatur of mpfr function
  typedef int mpfr_func(mpfr_t, const __mpfr_struct*, mp_rnd_t);

  /// Routine to call mpfr function with proper rounding
  forceinline double 
  invoke_mpfr(FloatNum x, mpfr_func f, mp_rnd_t r) {
    mpfr_t xx;
    mpfr_init_set_d(xx, x, GMP_RNDN);
    f(xx, xx, r);
    FloatNum res = mpfr_get_d(xx, r);
    mpfr_clear(xx);
    return res;
  }

  /// Define mpfr functions with proper rounding
#define GECODE_GENR_FUNC(name) \
  FloatNum Rounding::name##_down(FloatNum x) { \
    return invoke_mpfr(x, mpfr_##name, GMP_RNDD); \
  } \
  FloatNum Rounding::name##_up(FloatNum x) { \
    return invoke_mpfr(x, mpfr_##name, GMP_RNDU); \
  }
  GECODE_GENR_FUNC(exp)
  GECODE_GENR_FUNC(log)
  GECODE_GENR_FUNC(sin)
  GECODE_GENR_FUNC(cos)
  GECODE_GENR_FUNC(tan)
  GECODE_GENR_FUNC(asin)
  GECODE_GENR_FUNC(acos)
  GECODE_GENR_FUNC(atan)
  GECODE_GENR_FUNC(sinh)
  GECODE_GENR_FUNC(cosh)
  GECODE_GENR_FUNC(tanh)
  GECODE_GENR_FUNC(asinh)
  GECODE_GENR_FUNC(acosh)
  GECODE_GENR_FUNC(atanh)

#undef GECODE_GENR_FUNC

}}

#endif

// STATISTICS: float-var

