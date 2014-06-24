/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
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

#include <gecode/int/rel.hh>

namespace Gecode { namespace Int { namespace Dom {

  template<class View, ReifyMode rm>
  forceinline
  ReRange<View,rm>::ReRange(Home home, View x, int min0, int max0, BoolView b)
    : ReUnaryPropagator<View,PC_INT_BND,BoolView>(home,x,b),
      min(min0), max(max0) {}

  template<class View, ReifyMode rm>
  ExecStatus
  ReRange<View,rm>::post(Home home, View x, int min, int max, BoolView b) {
    if (min == max) {
      return Rel::ReEqDomInt<View,BoolView,rm>::post(home,x,min,b);
    } else if ((min > max) || (max < x.min()) || (min > x.max())) {
      if (rm == RM_PMI)
        return ES_OK;
      GECODE_ME_CHECK(b.zero(home));
    } else if ((min <= x.min()) && (x.max() <= max)) {
      if (rm == RM_IMP)
        return ES_OK;
      GECODE_ME_CHECK(b.one(home));
    } else if (b.one()) {
      if (rm == RM_PMI)
        return ES_OK;
      GECODE_ME_CHECK(x.gq(home,min));
      GECODE_ME_CHECK(x.lq(home,max));
    } else if (b.zero()) {
      if (rm == RM_IMP)
        return ES_OK;
      Iter::Ranges::Singleton r(min,max);
      GECODE_ME_CHECK(x.minus_r(home,r,false));
    } else {
      (void) new (home) ReRange<View,rm>(home,x,min,max,b);
    }
    return ES_OK;
  }


  template<class View, ReifyMode rm>
  forceinline
  ReRange<View,rm>::ReRange(Space& home, bool share, ReRange& p)
    : ReUnaryPropagator<View,PC_INT_BND,BoolView>(home,share,p),
      min(p.min), max(p.max) {}

  template<class View, ReifyMode rm>
  Actor*
  ReRange<View,rm>::copy(Space& home, bool share) {
    return new (home) ReRange<View,rm>(home,share,*this);
  }

  template<class View, ReifyMode rm>
  ExecStatus
  ReRange<View,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm != RM_PMI) {
        GECODE_ME_CHECK(x0.gq(home,min));
        GECODE_ME_CHECK(x0.lq(home,max));
      }
    } else if (b.zero()) {
      if (rm != RM_IMP) {
        Iter::Ranges::Singleton r(min,max);
        GECODE_ME_CHECK(x0.minus_r(home,r,false));
      }
    } else if ((x0.max() <= max) && (x0.min() >= min)) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(b.one_none(home));
    } else if ((x0.max() < min) || (x0.min() > max)) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(b.zero_none(home));
    } else {
      return ES_FIX;
    }
    return home.ES_SUBSUMED(*this);
  }


}}}

// STATISTICS: int-prop

