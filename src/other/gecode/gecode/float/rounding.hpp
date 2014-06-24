/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2012
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

namespace Gecode { namespace Float {

  forceinline
  Rounding::Rounding(void) { Base::init(); }
  forceinline
  Rounding::~Rounding(void) {}

  forceinline FloatNum
  Rounding::median(FloatNum x, FloatNum y) {
    return Base::median(x,y);
  }

#define GECODE_ROUND_OP(name)                           \
  forceinline FloatNum                                  \
  Rounding::name##_down(FloatNum x, FloatNum y) {   \
    return Base::name##_down(x,y);                      \
  }                                                     \
  forceinline FloatNum                                  \
  Rounding::name##_up(FloatNum x, FloatNum y) {     \
    return Base::name##_up(x,y);                        \
  }

  GECODE_ROUND_OP(add)
  GECODE_ROUND_OP(sub)
  GECODE_ROUND_OP(mul)
  GECODE_ROUND_OP(div)

#undef GECODE_ROUND_OP

#define GECODE_ROUND_FUN(name)                  \
  forceinline FloatNum                          \
  Rounding::name##_down(FloatNum x) {       \
    return Base::name##_down(x);                \
  }                                             \
  forceinline FloatNum                          \
  Rounding::name##_up(FloatNum x) {         \
    return Base::name##_up(x);                  \
  }

  GECODE_ROUND_FUN(sqrt)

#if defined(_M_X64) || defined(_M_IA64)

  // Workaround as MSVC on x64 does not have a rint function
  forceinline FloatNum
  Rounding::int_down(FloatNum x) {
    return floor(x);
  }
  forceinline FloatNum
  Rounding::int_up(FloatNum x) {
    return ceil(x);
  }

#else

  GECODE_ROUND_FUN(int)

#endif

#undef GECODE_ROUND_FUN

}}

// STATISTICS: float-var

