/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

namespace Gecode { namespace Int { namespace Distinct {


  /*
   * Ternary domain consistent distinct
   *
   */

  template<class View>
  forceinline
  TerDom<View>::TerDom(Home home, View x0, View x1, View x2)
    : TernaryPropagator<View,PC_INT_DOM>(home,x0,x1,x2) {}

  template<class View>
  ExecStatus
  TerDom<View>::post(Home home, View x0, View x1, View x2) {
    (void) new (home) TerDom<View>(home,x0,x1,x2);
    return ES_OK;
  }

  template<class View>
  forceinline
  TerDom<View>::TerDom(Space& home, bool share, TerDom<View>& p)
    : TernaryPropagator<View,PC_INT_DOM>(home,share,p) {}

  template<class View>
  Actor*
  TerDom<View>::copy(Space& home, bool share) {
    return new (home) TerDom<View>(home,share,*this);
  }

  /// Check whether x0 forms a Hall set of cardinality one
#define GECODE_INT_HALL_ONE(x0,x1,x2)           \
  if (x0.assigned()) {                          \
    GECODE_ME_CHECK(x1.nq(home,x0.val()));      \
    GECODE_ME_CHECK(x2.nq(home,x0.val()));      \
    if (x1.assigned()) {                        \
      GECODE_ME_CHECK(x2.nq(home,x1.val()));    \
      return home.ES_SUBSUMED(*this);            \
    }                                           \
    if (x2.assigned()) {                        \
      GECODE_ME_CHECK(x1.nq(home,x2.val()));    \
      return home.ES_SUBSUMED(*this);            \
    }                                           \
    return ES_FIX;                              \
  }


  /// Check whether x0 and x1 form a Hall set of cardinality two
#define GECODE_INT_HALL_TWO(x0,x1,x2)                           \
  if ((x0.size() == 2) && (x1.size() == 2) &&                   \
      (x0.min() == x1.min()) && (x0.max() == x1.max())) {       \
    GECODE_ME_CHECK(x2.nq(home,x0.min()));                      \
    GECODE_ME_CHECK(x2.nq(home,x0.max()));                      \
    return ES_FIX;                                              \
  }

  template<class View>
  ExecStatus
  TerDom<View>::propagate(Space& home, const ModEventDelta&) {
    GECODE_INT_HALL_ONE(x0,x1,x2);
    GECODE_INT_HALL_ONE(x1,x0,x2);
    GECODE_INT_HALL_ONE(x2,x0,x1);
    GECODE_INT_HALL_TWO(x0,x1,x2);
    GECODE_INT_HALL_TWO(x0,x2,x1);
    GECODE_INT_HALL_TWO(x1,x2,x0);
    return ES_FIX;
  }

#undef GECODE_INT_HALL_ONE
#undef GECODE_INT_HALL_TWO

}}}

// STATISTICS: int-prop

