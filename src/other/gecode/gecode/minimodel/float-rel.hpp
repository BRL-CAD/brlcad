/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
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

#ifdef GECODE_HAS_FLOAT_VARS

namespace Gecode {

  /*
   * Operations for linear float relations
   *
   */
  forceinline
  LinFloatRel::LinFloatRel(void) {}

  forceinline
  LinFloatRel::LinFloatRel(const LinFloatExpr& l, FloatRelType frt0, const LinFloatExpr& r)
    : e(l-r), frt(frt0) {}

  forceinline
  LinFloatRel::LinFloatRel(const LinFloatExpr& l, FloatRelType frt0, FloatVal r)
    : e(l-r), frt(frt0) {}

  forceinline
  LinFloatRel::LinFloatRel(FloatVal l, FloatRelType frt0, const LinFloatExpr& r)
    : e(l-r), frt(frt0) {}

  forceinline FloatRelType
  LinFloatRel::neg(FloatRelType frt) {
    switch (frt) {
    case FRT_EQ: return FRT_NQ;
    case FRT_NQ: return FRT_EQ;
    case FRT_LQ: return FRT_GR;
    case FRT_LE: return FRT_GQ;
    case FRT_GQ: return FRT_LE;
    case FRT_GR: return FRT_LQ;
    default: GECODE_NEVER;
    }
    return FRT_LQ;
  }

  forceinline void
  LinFloatRel::post(Home home, bool t) const {
    e.post(home,t ? frt : neg(frt));
  }

  forceinline void
  LinFloatRel::post(Home home, const BoolVar& b, bool t) const {
    e.post(home,t ? frt : neg(frt),b);
  }

}

#endif

// STATISTICS: minimodel-any
