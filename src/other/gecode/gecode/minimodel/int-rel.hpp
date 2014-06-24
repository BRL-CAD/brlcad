/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2005
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

namespace Gecode {

  /*
   * Operations for linear expressions
   *
   */
  forceinline
  LinIntRel::LinIntRel(void) {}

  forceinline
  LinIntRel::LinIntRel(const LinIntExpr& l, IntRelType irt0, const LinIntExpr& r)
    : e(l-r), irt(irt0) {}

  forceinline
  LinIntRel::LinIntRel(const LinIntExpr& l, IntRelType irt0, int r)
    : e(l-r), irt(irt0) {}

  forceinline
  LinIntRel::LinIntRel(int l, IntRelType irt0, const LinIntExpr& r)
    : e(l-r), irt(irt0) {}

  forceinline IntRelType
  LinIntRel::neg(IntRelType irt) {
    switch (irt) {
    case IRT_EQ: return IRT_NQ;
    case IRT_NQ: return IRT_EQ;
    case IRT_LQ: return IRT_GR;
    case IRT_LE: return IRT_GQ;
    case IRT_GQ: return IRT_LE;
    case IRT_GR: return IRT_LQ;
    default: GECODE_NEVER;
    }
    return IRT_LQ;
  }

  forceinline void
  LinIntRel::post(Home home, bool t, IntConLevel icl) const {
    e.post(home,t ? irt : neg(irt),icl);
  }

  forceinline void
  LinIntRel::post(Home home, const BoolVar& b, bool t, IntConLevel icl) const {
    e.post(home,t ? irt : neg(irt),b,icl);
  }

}

// STATISTICS: minimodel-any
