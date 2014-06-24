/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

namespace Gecode { namespace Int { namespace Member {

  template<class View, ReifyMode rm>
  forceinline
  ReProp<View,rm>::ReProp(Home home, ValSet& vs, ViewArray<View>& x, View y, 
                          BoolView b0)
    : Prop<View>(home,vs,x,y), b(b0) {
    b.subscribe(home,*this,PC_BOOL_VAL);
  }

  template<class View, ReifyMode rm>
  inline ExecStatus
  ReProp<View,rm>::post(Home home, ViewArray<View>& x, View y, BoolView b) {
    if (x.size() == 0) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(b.zero(home));
      return ES_OK;
    }

    x.unique(home);

    if (x.size() == 1)
      return Rel::ReEqDom<View,BoolView,rm>::post(home,x[0],y,b);

    if (x.same(home,y)) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(b.one(home));
      return ES_OK;
    }
    
    // Eliminate assigned views and store them into the value set
    ValSet vs;
    add(home, vs, x);

    switch (vs.compare(y)) {
    case Iter::Ranges::CS_SUBSET:
      if (rm != RM_IMP)
        GECODE_ME_CHECK(b.one(home));
      return ES_OK;
    case Iter::Ranges::CS_DISJOINT:
      if (x.size() == 0) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        return ES_OK;
      }
      break;
    case Iter::Ranges::CS_NONE:
      break;
    default:
      GECODE_NEVER;
    }

    (void) new (home) ReProp<View,rm>(home, vs, x, y, b);
    return ES_OK;
  }
    
  template<class View, ReifyMode rm>
  forceinline
  ReProp<View,rm>::ReProp(Space& home, bool share, ReProp<View,rm>& p)
    : Prop<View>(home, share, p) {
    b.update(home, share, p.b);
  }

  template<class View, ReifyMode rm>
  Propagator*
  ReProp<View,rm>::copy(Space& home, bool share) {
    return new (home) ReProp<View,rm>(home, share, *this);
  }

  template<class View, ReifyMode rm>
  forceinline size_t
  ReProp<View,rm>::dispose(Space& home) {
    b.cancel(home, *this, PC_BOOL_VAL);
    (void) Prop<View>::dispose(home);
    return sizeof(*this);
  }

  template<class View, ReifyMode rm>
  ExecStatus
  ReProp<View,rm>::propagate(Space& home, const ModEventDelta& med) {
    // Add assigned views to value set
    if (View::me(med) == ME_INT_VAL)
      add(home,vs,x);

    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);
      ValSet vsc(vs);
      vs.flush();
      GECODE_REWRITE(*this,Prop<View>::post(home,vsc,x,y));
    }

    if (b.zero()) {
      if (rm != RM_IMP) {
        ValSet::Ranges vsr(vs);
        GECODE_ME_CHECK(y.minus_r(home,vsr,false));
        for (int i=x.size(); i--; )
          GECODE_ES_CHECK(Rel::Nq<View>::post(home,x[i],y));
      }
      return home.ES_SUBSUMED(*this);
    }

    // Eliminate views from x
    eliminate(home);
    
    switch (vs.compare(y)) {
    case Iter::Ranges::CS_SUBSET:
      if (rm != RM_IMP)
        GECODE_ME_CHECK(b.one(home));
      return home.ES_SUBSUMED(*this);
    case Iter::Ranges::CS_DISJOINT:
      if (x.size() == 0) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        return home.ES_SUBSUMED(*this);
      }
      break;
    case Iter::Ranges::CS_NONE:
      break;
    default:
      GECODE_NEVER;
    }

    // Check whether y is in union of x and value set
    if (x.size() > 0) {
      Region r(home);

      ValSet::Ranges vsr(vs);
      ViewRanges<View> xsr(x[x.size()-1]);
      Iter::Ranges::NaryUnion  u(r,vsr,xsr);
      for (int i=x.size()-1; i--; ) {
        ViewRanges<View> xir(x[i]);
        u |= xir;
      }

      ViewRanges<View> yr(y);
      
      if (Iter::Ranges::disjoint(u,yr)) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        return home.ES_SUBSUMED(*this);
      }
    }

    return ES_FIX;
  }

}}}

// STATISTICS: int-prop
