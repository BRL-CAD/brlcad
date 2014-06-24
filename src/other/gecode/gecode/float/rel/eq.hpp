/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2004
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

namespace Gecode { namespace Float { namespace Rel {

  /*
   * Binary bounds consistent equality
   *
   */

  template<class View0, class View1>
  forceinline
  Eq<View0,View1>::Eq(Home home, View0 x0, View1 x1)
    : MixBinaryPropagator<View0,PC_FLOAT_BND,View1,PC_FLOAT_BND>(home,x0,x1) {}

  template<class View0, class View1>
  ExecStatus
  Eq<View0,View1>::post(Home home, View0 x0, View1 x1){
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.eq(home,x0.val()));
    } else if (x1.assigned()) {
      GECODE_ME_CHECK(x0.eq(home,x1.val()));
    } else if (!same(x0,x1)) {
      GECODE_ME_CHECK(x0.lq(home,x1.max()));
      GECODE_ME_CHECK(x1.lq(home,x0.max()));
      GECODE_ME_CHECK(x0.gq(home,x1.min()));
      GECODE_ME_CHECK(x1.gq(home,x0.min()));
      (void) new (home) Eq<View0,View1>(home,x0,x1);
    }
    return ES_OK;
  }

  template<class View0, class View1>
  forceinline
  Eq<View0,View1>::Eq(Space& home, bool share, Eq<View0,View1>& p)
    : MixBinaryPropagator<View0,PC_FLOAT_BND,View1,PC_FLOAT_BND>(home,share,p) {}

  template<class View0, class View1>
  forceinline
  Eq<View0,View1>::Eq(Space& home, bool share, Propagator& p,
                      View0 x0, View1 x1)
    : MixBinaryPropagator<View0,PC_FLOAT_BND,View1,PC_FLOAT_BND>(home,share,p,
                                                                 x0,x1) {}

  template<class View0, class View1>
  Actor*
  Eq<View0,View1>::copy(Space& home, bool share) {
    return new (home) Eq<View0,View1>(home,share,*this);
  }

  template<class View0, class View1>
  ExecStatus
  Eq<View0,View1>::propagate(Space& home, const ModEventDelta&) {
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.eq(home,x0.val()));
    } else if (x1.assigned()) {
      GECODE_ME_CHECK(x0.eq(home,x1.val()));
    } else {
      do {
        GECODE_ME_CHECK(x0.gq(home,x1.min()));
        GECODE_ME_CHECK(x1.gq(home,x0.min()));
      } while (x0.min() != x1.min());
      do {
        GECODE_ME_CHECK(x0.lq(home,x1.max()));
        GECODE_ME_CHECK(x1.lq(home,x0.max()));
      } while (x0.max() != x1.max());
      if (!x0.assigned())
        return ES_FIX;
    }
    assert(x0.assigned() && x1.assigned());
    return home.ES_SUBSUMED(*this);
  }


  /*
   * Nary bound consistent equality
   *
   */

  template<class View>
  forceinline
  NaryEq<View>::NaryEq(Home home, ViewArray<View>& x)
    : NaryPropagator<View,PC_FLOAT_BND>(home,x) {}

  template<class View>
  ExecStatus
  NaryEq<View>::post(Home home, ViewArray<View>& x) {
    x.unique(home);
    if (x.size() == 2) {
      return Eq<View,View>::post(home,x[0],x[1]);
    } else if (x.size() > 2) {
      FloatNum l = x[0].min();
      FloatNum u = x[0].max();
      for (int i=x.size(); i-- > 1; ) {
        l = std::max(l,x[i].min());
        u = std::min(u,x[i].max());
      }
      for (int i=x.size(); i--; ) {
        GECODE_ME_CHECK(x[i].gq(home,l));
        GECODE_ME_CHECK(x[i].lq(home,u));
      }
      (void) new (home) NaryEq<View>(home,x);
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  NaryEq<View>::NaryEq(Space& home, bool share, NaryEq<View>& p)
    : NaryPropagator<View,PC_FLOAT_BND>(home,share,p) {}

  template<class View>
  Actor*
  NaryEq<View>::copy(Space& home, bool share) {
    return new (home) NaryEq<View>(home,share,*this);
  }

  template<class View>
  PropCost
  NaryEq<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_FLOAT_VAL)
      return PropCost::unary(PropCost::LO);
    else
      return PropCost::linear(PropCost::LO, x.size());
  }

  template<class View>
  ExecStatus
  NaryEq<View>::propagate(Space& home, const ModEventDelta& med) {
    assert(x.size() > 2);
    if (View::me(med) == ME_FLOAT_VAL) {
      // One of the variables is assigned
      for (int i = 0; ; i++)
        if (x[i].assigned()) {
          FloatVal n = x[i].val();
          x.move_lst(i);
          for (int j = x.size(); j--; )
            GECODE_ME_CHECK(x[j].eq(home,n));
          return home.ES_SUBSUMED(*this);
        }
      GECODE_NEVER;
    }

    FloatNum mn = x[0].min();
  restart_min:
    for (int i = x.size(); i--; ) {
      GECODE_ME_CHECK(x[i].gq(home,mn));
      if (mn < x[i].min()) {
        mn = x[i].min();
        goto restart_min;
      }
    }
    FloatNum mx = x[0].max();
  restart_max:
    for (int i = x.size(); i--; ) {
      GECODE_ME_CHECK(x[i].lq(home,mx));
      if (mx > x[i].max()) {
        mx = x[i].max();
        goto restart_max;
      }
    }
    return x[0].assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }



  /*
   * Reified bounds consistent equality
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEq<View,CtrlView,rm>::ReEq(Home home, View x0, View x1, CtrlView b)
    : Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,x0,x1,b) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEq<View,CtrlView,rm>::post(Home home, View x0, View x1, CtrlView b){
    if (b.one()) {
      if (rm == RM_PMI)
        return ES_OK;
      return Eq<View,View>::post(home,x0,x1);
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return ES_OK;
      return Nq<View,View>::post(home,x0,x1);
    }
    if (!same(x0,x1)) {
      (void) new (home) ReEq(home,x0,x1,b);
    } else if (rm != RM_IMP) {
      GECODE_ME_CHECK(b.one(home));
    }
    return ES_OK;
  }


  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEq<View,CtrlView,rm>::ReEq(Space& home, bool share, ReEq& p)
    : Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,share,p) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReEq<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReEq<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEq<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this,(Eq<View,View>::post(home(*this),x0,x1)));
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this,(Nq<View,View>::post(home(*this),x0,x1)));
    }
    switch (rtest_eq(x0,x1)) {
    case RT_TRUE:
      if (rm != RM_IMP)
        GECODE_ME_CHECK(b.one_none(home));
      break;
    case RT_FALSE:
      if (rm != RM_PMI)
        GECODE_ME_CHECK(b.zero_none(home));
      break;
    case RT_MAYBE:
      return ES_FIX;
    default: GECODE_NEVER;
    }
    return home.ES_SUBSUMED(*this);
  }


  /*
   * Reified bounds consistent equality (one variable)
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqFloat<View,CtrlView,rm>::ReEqFloat
  (Home home, View x, FloatVal c0, CtrlView b)
    : Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,x,b), c(c0) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqFloat<View,CtrlView,rm>::post(Home home, View x, FloatVal c, CtrlView b) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x.eq(home,c));      
    } else if (x.assigned()) {
      if (overlap(x.val(),c)) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one(home));
      } else {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
      }
    } else {
      (void) new (home) ReEqFloat(home,x,c,b);
    }
    return ES_OK;
  }


  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqFloat<View,CtrlView,rm>::ReEqFloat(Space& home, bool share, ReEqFloat& p)
    : Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,share,p), c(p.c) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReEqFloat<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReEqFloat<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqFloat<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x0.eq(home,c));
    } else {
      switch (rtest_eq(x0,c)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one(home)); 
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home)); 
        break;
      case RT_MAYBE:
        return ES_FIX;
      default: GECODE_NEVER;
      }
    }
    return home.ES_SUBSUMED(*this);
  }


}}}

// STATISTICS: float-prop
