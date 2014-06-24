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

namespace Gecode { namespace Int { namespace Rel {

  /*
   * Binary bounds consistent equality
   *
   */

  template<class View0, class View1>
  forceinline
  EqBnd<View0,View1>::EqBnd(Home home, View0 x0, View1 x1)
    : MixBinaryPropagator<View0,PC_INT_BND,View1,PC_INT_BND>(home,x0,x1) {}

  template<class View0, class View1>
  ExecStatus
  EqBnd<View0,View1>::post(Home home, View0 x0, View1 x1){
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.eq(home,x0.val()));
    } else if (x1.assigned()) {
      GECODE_ME_CHECK(x0.eq(home,x1.val()));
    } else if (!same(x0,x1)) {
      GECODE_ME_CHECK(x0.lq(home,x1.max()));
      GECODE_ME_CHECK(x1.lq(home,x0.max()));
      GECODE_ME_CHECK(x0.gq(home,x1.min()));
      GECODE_ME_CHECK(x1.gq(home,x0.min()));
      (void) new (home) EqBnd<View0,View1>(home,x0,x1);
    }
    return ES_OK;
  }

  template<class View0, class View1>
  forceinline
  EqBnd<View0,View1>::EqBnd(Space& home, bool share, EqBnd<View0,View1>& p)
    : MixBinaryPropagator<View0,PC_INT_BND,View1,PC_INT_BND>(home,share,p) {}

  template<class View0, class View1>
  forceinline
  EqBnd<View0,View1>::EqBnd(Space& home, bool share, Propagator& p,
                            View0 x0, View1 x1)
    : MixBinaryPropagator<View0,PC_INT_BND,View1,PC_INT_BND>(home,share,p,
                                                             x0,x1) {}

  template<class View0, class View1>
  Actor*
  EqBnd<View0,View1>::copy(Space& home, bool share) {
    return new (home) EqBnd<View0,View1>(home,share,*this);
  }

  template<class View0, class View1>
  ExecStatus
  EqBnd<View0,View1>::propagate(Space& home, const ModEventDelta&) {
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
   * Binary domain consistent equality
   *
   */

  template<class View0, class View1>
  forceinline
  EqDom<View0,View1>::EqDom(Home home, View0 x0, View1 x1)
    : MixBinaryPropagator<View0,PC_INT_DOM,View1,PC_INT_DOM>(home,x0,x1) {}

  template<class View0, class View1>
  ExecStatus
  EqDom<View0,View1>::post(Home home, View0 x0, View1 x1){
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.eq(home,x0.val()));
    } else if (x1.assigned()) {
      GECODE_ME_CHECK(x0.eq(home,x1.val()));
    } else if (!same(x0,x1)) {
      GECODE_ME_CHECK(x0.lq(home,x1.max()));
      GECODE_ME_CHECK(x1.lq(home,x0.max()));
      GECODE_ME_CHECK(x0.gq(home,x1.min()));
      GECODE_ME_CHECK(x1.gq(home,x0.min()));
      (void) new (home) EqDom<View0,View1>(home,x0,x1);
    }
    return ES_OK;
  }


  template<class View0, class View1>
  forceinline
  EqDom<View0,View1>::EqDom(Space& home, bool share, EqDom<View0,View1>& p)
    : MixBinaryPropagator<View0,PC_INT_DOM,View1,PC_INT_DOM>(home,share,p) {}

  template<class View0, class View1>
  forceinline
  EqDom<View0,View1>::EqDom(Space& home, bool share, Propagator& p,
                            View0 x0, View1 x1)
    : MixBinaryPropagator<View0,PC_INT_DOM,View1,PC_INT_DOM>(home,share,p,
                                                             x0,x1) {}

  template<class View0, class View1>
  Actor*
  EqDom<View0,View1>::copy(Space& home, bool share) {
    return new (home) EqDom<View0,View1>(home,share,*this);
  }

  template<class View0, class View1>
  PropCost
  EqDom<View0,View1>::cost(const Space&, const ModEventDelta& med) const {
    if ((View0::me(med) == ME_INT_VAL) || (View1::me(med) == ME_INT_VAL))
      return PropCost::unary(PropCost::LO);
    else if ((View0::me(med) == ME_INT_DOM) || (View1::me(med) == ME_INT_DOM))
      return PropCost::binary(PropCost::LO);
    else
      return PropCost::binary(PropCost::HI);
  }

  template<class View0, class View1>
  ExecStatus
  EqDom<View0,View1>::propagate(Space& home, const ModEventDelta& med) {
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.eq(home,x0.val()));
      return home.ES_SUBSUMED(*this);
    }
    if (x1.assigned()) {
      GECODE_ME_CHECK(x0.eq(home,x1.val()));
      return home.ES_SUBSUMED(*this);
    }
    if ((View0::me(med) != ME_INT_DOM) && (View1::me(med) != ME_INT_DOM)) {
      do {
        GECODE_ME_CHECK(x0.gq(home,x1.min()));
        GECODE_ME_CHECK(x1.gq(home,x0.min()));
      } while (x0.min() != x1.min());
      do {
        GECODE_ME_CHECK(x0.lq(home,x1.max()));
        GECODE_ME_CHECK(x1.lq(home,x0.max()));
      } while (x0.max() != x1.max());
      if (x0.assigned())
        return home.ES_SUBSUMED(*this);
      if (x0.range() && x1.range())
        return ES_FIX;
      return home.ES_FIX_PARTIAL(*this,View0::med(ME_INT_DOM));
    }
    ViewRanges<View0> r0(x0);
    GECODE_ME_CHECK(x1.inter_r(home,r0,shared(x0,x1)));
    ViewRanges<View1> r1(x1);
    GECODE_ME_CHECK(x0.narrow_r(home,r1,shared(x0,x1)));
    if (x0.assigned())
      return home.ES_SUBSUMED(*this);
    return ES_FIX;
  }



  /*
   * Nary domain consistent equality
   *
   */

  template<class View>
  forceinline
  NaryEqDom<View>::NaryEqDom(Home home, ViewArray<View>& x)
    : NaryPropagator<View,PC_INT_DOM>(home,x) {}

  template<class View>
  ExecStatus
  NaryEqDom<View>::post(Home home, ViewArray<View>& x) {
    x.unique(home);
    if (x.size() == 2) {
      return EqDom<View,View>::post(home,x[0],x[1]);
    } else if (x.size() > 2) {
      int l = x[0].min();
      int u = x[0].max();
      for (int i=x.size(); i-- > 1; ) {
        l = std::max(l,x[i].min());
        u = std::min(u,x[i].max());
      }
      for (int i=x.size(); i--; ) {
        GECODE_ME_CHECK(x[i].gq(home,l));
        GECODE_ME_CHECK(x[i].lq(home,u));
      }
      (void) new (home) NaryEqDom<View>(home,x);
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  NaryEqDom<View>::NaryEqDom(Space& home, bool share, NaryEqDom<View>& p)
    : NaryPropagator<View,PC_INT_DOM>(home,share,p) {}

  template<class View>
  Actor*
  NaryEqDom<View>::copy(Space& home, bool share) {
    return new (home) NaryEqDom<View>(home,share,*this);
  }

  template<class View>
  PropCost
  NaryEqDom<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_VAL)
      return PropCost::unary(PropCost::LO);
    else
      return PropCost::linear((View::me(med) == ME_INT_DOM) ?
                              PropCost::LO : PropCost::HI, x.size());
  }

  template<class View>
  ExecStatus
  NaryEqDom<View>::propagate(Space& home, const ModEventDelta& med) {
    assert(x.size() > 2);

    ModEvent me = View::me(med);
    if (me == ME_INT_VAL) {
      // One of the variables is assigned
      for (int i = 0; ; i++)
        if (x[i].assigned()) {
          int n = x[i].val();
          x.move_lst(i);
          for (int j = x.size(); j--; )
            GECODE_ME_CHECK(x[j].eq(home,n));
          return home.ES_SUBSUMED(*this);
        }
      GECODE_NEVER;
    }

    if (me == ME_INT_BND) {
      {
        // One of the mins has changed
        int mn = x[0].min();
      restart_min:
        for (int i = x.size(); i--; ) {
          GECODE_ME_CHECK(x[i].gq(home,mn));
          if (mn < x[i].min()) {
            mn = x[i].min();
            goto restart_min;
          }
        }
      }
      {
        // One of the maxs has changed
        int mx = x[0].max();
      restart_max:
        for (int i = x.size(); i--; ) {
          GECODE_ME_CHECK(x[i].lq(home,mx));
          if (mx > x[i].max()) {
            mx = x[i].max();
            goto restart_max;
          }
        }
      }
      if (x[0].assigned())
        return home.ES_SUBSUMED(*this);
      return home.ES_FIX_PARTIAL(*this,View::med(ME_INT_DOM));
    }

    int n = x.size();

    Region re(home);
    ViewRanges<View>* i_x = re.alloc<ViewRanges<View> >(n);
    for (int i = n; i--; ) {
      ViewRanges<View> i_xi(x[i]);
      i_x[i] = i_xi;
    }
    Iter::Ranges::NaryInter r(re,i_x,n);

    if (!r())
      return ES_FAILED;
    ++r;
    if (!r()) {
      r.reset();
      for (int i = n; i--; ) {
        GECODE_ME_CHECK(x[i].gq(home,r.min()));
        GECODE_ME_CHECK(x[i].lq(home,r.max()));
      }
    } else {
      for (int i = n; i--; ) {
        r.reset();
        GECODE_ME_CHECK(x[i].narrow_r(home,r,false));
      }
    }
    return ES_FIX;
  }



  /*
   * Nary bound consistent equality
   *
   */

  template<class View>
  forceinline
  NaryEqBnd<View>::NaryEqBnd(Home home, ViewArray<View>& x)
    : NaryPropagator<View,PC_INT_BND>(home,x) {}

  template<class View>
  ExecStatus
  NaryEqBnd<View>::post(Home home, ViewArray<View>& x) {
    x.unique(home);
    if (x.size() == 2) {
      return EqBnd<View,View>::post(home,x[0],x[1]);
    } else if (x.size() > 2) {
      int l = x[0].min();
      int u = x[0].max();
      for (int i=x.size(); i-- > 1; ) {
        l = std::max(l,x[i].min());
        u = std::min(u,x[i].max());
      }
      for (int i=x.size(); i--; ) {
        GECODE_ME_CHECK(x[i].gq(home,l));
        GECODE_ME_CHECK(x[i].lq(home,u));
      }
      (void) new (home) NaryEqBnd<View>(home,x);
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  NaryEqBnd<View>::NaryEqBnd(Space& home, bool share, NaryEqBnd<View>& p)
    : NaryPropagator<View,PC_INT_BND>(home,share,p) {}

  template<class View>
  Actor*
  NaryEqBnd<View>::copy(Space& home, bool share) {
    return new (home) NaryEqBnd<View>(home,share,*this);
  }

  template<class View>
  PropCost
  NaryEqBnd<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_VAL)
      return PropCost::unary(PropCost::LO);
    else
      return PropCost::linear(PropCost::LO, x.size());
  }

  template<class View>
  ExecStatus
  NaryEqBnd<View>::propagate(Space& home, const ModEventDelta& med) {
    assert(x.size() > 2);
    if (View::me(med) == ME_INT_VAL) {
      // One of the variables is assigned
      for (int i = 0; ; i++)
        if (x[i].assigned()) {
          int n = x[i].val();
          x.move_lst(i);
          for (int j = x.size(); j--; )
            GECODE_ME_CHECK(x[j].eq(home,n));
          return home.ES_SUBSUMED(*this);
        }
      GECODE_NEVER;
    }

    int mn = x[0].min();
  restart_min:
    for (int i = x.size(); i--; ) {
      GECODE_ME_CHECK(x[i].gq(home,mn));
      if (mn < x[i].min()) {
        mn = x[i].min();
        goto restart_min;
      }
    }
    int mx = x[0].max();
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
   * Reified domain consistent equality
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqDom<View,CtrlView,rm>::ReEqDom(Home home, View x0, View x1, CtrlView b)
    : ReBinaryPropagator<View,PC_INT_DOM,CtrlView>(home,x0,x1,b) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqDom<View,CtrlView,rm>::post(Home home, View x0, View x1, CtrlView b) {
    if (b.one()) {
      if (rm == RM_PMI)
        return ES_OK;
      return EqDom<View,View>::post(home,x0,x1);
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return ES_OK;
      return Nq<View>::post(home,x0,x1);
    }
    if (!same(x0,x1)) {
      (void) new (home) ReEqDom(home,x0,x1,b);
    } else if (rm != RM_IMP) {
      GECODE_ME_CHECK(b.one(home));
    }
    return ES_OK;
  }


  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqDom<View,CtrlView,rm>::ReEqDom(Space& home, bool share, ReEqDom& p)
    : ReBinaryPropagator<View,PC_INT_DOM,CtrlView>(home,share,p) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReEqDom<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReEqDom<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqDom<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this,(EqDom<View,View>::post(home(*this),x0,x1)));
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return home.ES_SUBSUMED(*this);        
      GECODE_REWRITE(*this,Nq<View>::post(home(*this),x0,x1));
    }
    switch (rtest_eq_dom(x0,x1)) {
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
   * Reified bounds consistent equality
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqBnd<View,CtrlView,rm>::ReEqBnd(Home home, View x0, View x1, CtrlView b)
    : ReBinaryPropagator<View,PC_INT_BND,CtrlView>(home,x0,x1,b) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqBnd<View,CtrlView,rm>::post(Home home, View x0, View x1, CtrlView b){
    if (b.one()) {
      if (rm == RM_PMI)
        return ES_OK;
      return EqBnd<View,View>::post(home,x0,x1);
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return ES_OK;
      return Nq<View>::post(home,x0,x1);
    }
    if (!same(x0,x1)) {
      (void) new (home) ReEqBnd(home,x0,x1,b);
    } else if (rm != RM_IMP) {
      GECODE_ME_CHECK(b.one(home));
    }
    return ES_OK;
  }


  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqBnd<View,CtrlView,rm>::ReEqBnd(Space& home, bool share, ReEqBnd& p)
    : ReBinaryPropagator<View,PC_INT_BND,CtrlView>(home,share,p) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReEqBnd<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReEqBnd<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqBnd<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this,(EqBnd<View,View>::post(home(*this),x0,x1)));
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this,Nq<View>::post(home(*this),x0,x1));
    }
    switch (rtest_eq_bnd(x0,x1)) {
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
   * Reified domain consistent equality (one variable)
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqDomInt<View,CtrlView,rm>::ReEqDomInt
  (Home home, View x, int c0, CtrlView b)
    : ReUnaryPropagator<View,PC_INT_DOM,CtrlView>(home,x,b), c(c0) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqDomInt<View,CtrlView,rm>::post(Home home, View x, int c, CtrlView b) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x.eq(home,c));
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(x.nq(home,c));
    } else if (x.assigned()) {
      assert(b.none());
      if (x.val() == c) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
      } else {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
      }
    } else {
      (void) new (home) ReEqDomInt(home,x,c,b);
    }
    return ES_OK;
  }


  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqDomInt<View,CtrlView,rm>::ReEqDomInt(Space& home, bool share,
                                           ReEqDomInt& p)
    : ReUnaryPropagator<View,PC_INT_DOM,CtrlView>(home,share,p), c(p.c) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReEqDomInt<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReEqDomInt<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqDomInt<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x0.eq(home,c));
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(x0.nq(home,c));
    } else {
      switch (rtest_eq_dom(x0,c)) {
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
    }
    return home.ES_SUBSUMED(*this);
  }




  /*
   * Reified bounds consistent equality (one variable)
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqBndInt<View,CtrlView,rm>::ReEqBndInt
  (Home home, View x, int c0, CtrlView b)
    : ReUnaryPropagator<View,PC_INT_BND,CtrlView>(home,x,b), c(c0) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqBndInt<View,CtrlView,rm>::post(Home home, View x, int c, CtrlView b) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x.eq(home,c));
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(x.nq(home,c));
    } else if (x.assigned()) {
      assert(b.none());
      if (x.val() == c) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
      } else {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
      }
    } else {
      (void) new (home) ReEqBndInt(home,x,c,b);
    }
    return ES_OK;
  }


  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReEqBndInt<View,CtrlView,rm>::ReEqBndInt(Space& home, bool share, ReEqBndInt& p)
    : ReUnaryPropagator<View,PC_INT_BND,CtrlView>(home,share,p), c(p.c) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReEqBndInt<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReEqBndInt<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReEqBndInt<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x0.eq(home,c));
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(x0.nq(home,c));
    } else {
      switch (rtest_eq_bnd(x0,c)) {
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
    }
    return home.ES_SUBSUMED(*this);
  }

}}}

// STATISTICS: int-prop
