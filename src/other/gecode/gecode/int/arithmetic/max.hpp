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

namespace Gecode { namespace Int { namespace Arithmetic {

  /*
   * Ternary bounds consistent maximum
   *
   */

  template<class View>
  forceinline ExecStatus
  prop_max_bnd(Space& home, View x0, View x1, View x2) {
    bool mod = false;
    do {
      mod = false;
      {
        ModEvent me = x2.lq(home,std::max(x0.max(),x1.max()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x2.gq(home,std::max(x0.min(),x1.min()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x0.lq(home,x2.max());
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x1.lq(home,x2.max());
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
    } while (mod);
    return ES_OK;
  }

  template<class View>
  forceinline
  MaxBnd<View>::MaxBnd(Home home, View x0, View x1, View x2)
    : TernaryPropagator<View,PC_INT_BND>(home,x0,x1,x2) {}

  template<class View>
  ExecStatus
  MaxBnd<View>::post(Home home, View x0, View x1, View x2) {
    GECODE_ME_CHECK(x2.gq(home,std::max(x0.min(),x1.min())));
    GECODE_ME_CHECK(x2.lq(home,std::max(x0.max(),x1.max())));
    if (same(x0,x1))
      return Rel::EqBnd<View,View>::post(home,x0,x2);
    if (same(x0,x2))
      return Rel::Lq<View>::post(home,x1,x2);
    if (same(x1,x2))
      return Rel::Lq<View>::post(home,x0,x2);
    (void) new (home) MaxBnd<View>(home,x0,x1,x2);
    return ES_OK;
  }

  template<class View>
  forceinline
  MaxBnd<View>::MaxBnd(Space& home, bool share, MaxBnd<View>& p)
    : TernaryPropagator<View,PC_INT_BND>(home,share,p) {}

  template<class View>
  forceinline
  MaxBnd<View>::MaxBnd(Space& home, bool share, Propagator& p,
                 View x0, View x1, View x2)
    : TernaryPropagator<View,PC_INT_BND>(home,share,p,x0,x1,x2) {}

  template<class View>
  Actor*
  MaxBnd<View>::copy(Space& home, bool share) {
    return new (home) MaxBnd<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  MaxBnd<View>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ES_CHECK(prop_max_bnd(home,x0,x1,x2));
    if ((x0.max() <= x1.min()) || (x0.max() < x2.min()))
      GECODE_REWRITE(*this,(Rel::EqBnd<View,View>::post(home(*this),x1,x2)));
    if ((x1.max() <= x0.min()) || (x1.max() < x2.min()))
      GECODE_REWRITE(*this,(Rel::EqBnd<View,View>::post(home(*this),x0,x2)));
    return x0.assigned() && x1.assigned() && x2.assigned() ?
      home.ES_SUBSUMED(*this) : ES_FIX;
  }

  /*
   * Nary bounds consistent maximum
   *
   */

  template<class View>
  forceinline
  NaryMaxBnd<View>::NaryMaxBnd(Home home, ViewArray<View>& x, View y)
    : NaryOnePropagator<View,PC_INT_BND>(home,x,y) {}

  template<class View>
  ExecStatus
  NaryMaxBnd<View>::post(Home home, ViewArray<View>& x, View y) {
    assert(x.size() > 0);
    x.unique(home);
    if (x.size() == 1)
      return Rel::EqBnd<View,View>::post(home,x[0],y);
    if (x.size() == 2)
      return MaxBnd<View>::post(home,x[0],x[1],y);
    int l = Int::Limits::min;
    int u = Int::Limits::min;
    for (int i=x.size(); i--; ) {
      l = std::max(l,x[i].min());
      u = std::max(u,x[i].max());
    }
    GECODE_ME_CHECK(y.gq(home,l));
    GECODE_ME_CHECK(y.lq(home,u));
    if (x.same(home,y)) {
      // Check whether y occurs in x
      for (int i=x.size(); i--; )
        GECODE_ES_CHECK(Rel::Lq<View>::post(home,x[i],y));
    } else {
      (void) new (home) NaryMaxBnd<View>(home,x,y);
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  NaryMaxBnd<View>::NaryMaxBnd(Space& home, bool share, NaryMaxBnd<View>& p)
    : NaryOnePropagator<View,PC_INT_BND>(home,share,p) {}

  template<class View>
  Actor*
  NaryMaxBnd<View>::copy(Space& home, bool share) {
    if (x.size() == 1)
      return new (home) Rel::EqBnd<View,View>(home,share,*this,x[0],y);
    if (x.size() == 2)
      return new (home) MaxBnd<View>(home,share,*this,x[0],x[1],y);
    return new (home) NaryMaxBnd<View>(home,share,*this);
  }

  /// Status of propagation for nary max
  enum MaxPropStatus {
    MPS_ASSIGNED  = 1<<0, ///< All views are assigned
    MPS_REMOVED   = 1<<1, ///< A view is removed
    MPS_NEW_BOUND = 1<<2  ///< Telling has found a new upper bound
  };

  template<class View>
  forceinline ExecStatus
  prop_nary_max_bnd(Space& home, Propagator& p,
                    ViewArray<View>& x, View y, PropCond pc) {
  rerun:
    assert(x.size() > 0);
    int maxmax = x[x.size()-1].max();
    int maxmin = x[x.size()-1].min();
    for (int i = x.size()-1; i--; ) {
      maxmax = std::max(x[i].max(),maxmax);
      maxmin = std::max(x[i].min(),maxmin);
    }
    GECODE_ME_CHECK(y.lq(home,maxmax));
    GECODE_ME_CHECK(y.gq(home,maxmin));
    maxmin = y.min();
    maxmax = y.max();
    int status = MPS_ASSIGNED;
    for (int i = x.size(); i--; ) {
      ModEvent me = x[i].lq(home,maxmax);
      if (me == ME_INT_FAILED)
        return ES_FAILED;
      if (me_modified(me) && (x[i].max() != maxmax))
        status |= MPS_NEW_BOUND;
      if (x[i].max() < maxmin) {
        x.move_lst(i,home,p,pc);
        status |= MPS_REMOVED;
      } else if (!x[i].assigned())
        status &= ~MPS_ASSIGNED;
    }
    if (x.size() == 0)
      return ES_FAILED;
    if ((status & MPS_REMOVED) != 0)
      goto rerun;
    if (((status & MPS_ASSIGNED) != 0) && y.assigned())
      return home.ES_SUBSUMED(p);
    return ((status & MPS_NEW_BOUND) != 0) ? ES_NOFIX : ES_FIX;
  }

  template<class View>
  ExecStatus
  NaryMaxBnd<View>::propagate(Space& home, const ModEventDelta&) {
    ExecStatus es = prop_nary_max_bnd(home,*this,x,y,PC_INT_BND);
    GECODE_ES_CHECK(es);
    if (x.size() == 1)
      GECODE_REWRITE(*this,(Rel::EqBnd<View,View>::post(home(*this),x[0],y)));
    return es;
  }


  /*
   * Ternary domain consistent maximum
   *
   */

  template<class View>
  forceinline
  MaxDom<View>::MaxDom(Home home, View x0, View x1, View x2)
    : TernaryPropagator<View,PC_INT_DOM>(home,x0,x1,x2) {}

  template<class View>
  ExecStatus
  MaxDom<View>::post(Home home, View x0, View x1, View x2) {
    GECODE_ME_CHECK(x2.gq(home,std::max(x0.min(),x1.min())));
    GECODE_ME_CHECK(x2.lq(home,std::max(x0.max(),x1.max())));
    if (same(x0,x1))
      return Rel::EqDom<View,View>::post(home,x0,x2);
    if (same(x0,x2))
      return Rel::Lq<View>::post(home,x1,x2);
    if (same(x1,x2))
      return Rel::Lq<View>::post(home,x0,x2);
    (void) new (home) MaxDom<View>(home,x0,x1,x2);
    return ES_OK;
  }

  template<class View>
  forceinline
  MaxDom<View>::MaxDom(Space& home, bool share, MaxDom<View>& p)
    : TernaryPropagator<View,PC_INT_DOM>(home,share,p) {}

  template<class View>
  forceinline
  MaxDom<View>::MaxDom(Space& home, bool share, Propagator& p,
                       View x0, View x1, View x2)
    : TernaryPropagator<View,PC_INT_DOM>(home,share,p,x0,x1,x2) {}

  template<class View>
  Actor*
  MaxDom<View>::copy(Space& home, bool share) {
    return new (home) MaxDom<View>(home,share,*this);
  }

  template<class View>
  PropCost
  MaxDom<View>::cost(const Space&, const ModEventDelta& med) const {
    return PropCost::ternary((View::me(med) == ME_INT_DOM) ?
                             PropCost::HI : PropCost::LO);
  }

  template<class View>
  ExecStatus
  MaxDom<View>::propagate(Space& home, const ModEventDelta& med) {
    if (View::me(med) != ME_INT_DOM) {
      GECODE_ES_CHECK(prop_max_bnd(home,x0,x1,x2));
      if ((x0.max() <= x1.min()) || (x0.max() < x2.min()))
        GECODE_REWRITE(*this,(Rel::EqDom<View,View>::post(home(*this),x1,x2)));
      if ((x1.max() <= x0.min()) || (x1.max() < x2.min()))
        GECODE_REWRITE(*this,(Rel::EqDom<View,View>::post(home(*this),x0,x2)));
      return x0.assigned() && x1.assigned() && x2.assigned() ?
        home.ES_SUBSUMED(*this) :
        home.ES_NOFIX_PARTIAL(*this,View::med(ME_INT_DOM));
    }
    ViewRanges<View> r0(x0), r1(x1);
    Iter::Ranges::Union<ViewRanges<View>,ViewRanges<View> > u(r0,r1);
    GECODE_ME_CHECK(x2.inter_r(home,u,false));
    if (rtest_nq_dom(x0,x2) == RT_TRUE) {
      GECODE_ES_CHECK(Rel::Lq<View>::post(home,x0,x2));
      GECODE_REWRITE(*this,(Rel::EqDom<View,View>::post(home(*this),x1,x2)));
    }
    if (rtest_nq_dom(x1,x2) == RT_TRUE) {
      GECODE_ES_CHECK(Rel::Lq<View>::post(home,x1,x2));
      GECODE_REWRITE(*this,(Rel::EqDom<View,View>::post(home(*this),x0,x2)));
    }
    return ES_FIX;
  }

  /*
   * Nary domain consistent maximum
   *
   */

  template<class View>
  forceinline
  NaryMaxDom<View>::NaryMaxDom(Home home, ViewArray<View>& x, View y)
    : NaryOnePropagator<View,PC_INT_DOM>(home,x,y) {}

  template<class View>
  ExecStatus
  NaryMaxDom<View>::post(Home home, ViewArray<View>& x, View y) {
    assert(x.size() > 0);
    x.unique(home);
    if (x.size() == 1)
      return Rel::EqDom<View,View>::post(home,x[0],y);
    if (x.size() == 2)
      return MaxDom<View>::post(home,x[0],x[1],y);
    int l = Int::Limits::min;
    int u = Int::Limits::min;
    for (int i=x.size(); i--; ) {
      l = std::max(l,x[i].min());
      u = std::max(u,x[i].max());
    }
    GECODE_ME_CHECK(y.gq(home,l));
    GECODE_ME_CHECK(y.lq(home,u));
    if (x.same(home,y)) {
      // Check whether y occurs in x
      for (int i=x.size(); i--; )
        GECODE_ES_CHECK(Rel::Lq<View>::post(home,x[i],y));
    } else {
      (void) new (home) NaryMaxDom<View>(home,x,y);
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  NaryMaxDom<View>::NaryMaxDom(Space& home, bool share, NaryMaxDom<View>& p)
    : NaryOnePropagator<View,PC_INT_DOM>(home,share,p) {}

  template<class View>
  Actor*
  NaryMaxDom<View>::copy(Space& home, bool share) {
    if (x.size() == 1)
      return new (home) Rel::EqDom<View,View>(home,share,*this,x[0],y);
    if (x.size() == 2)
      return new (home) MaxDom<View>(home,share,*this,x[0],x[1],y);
    return new (home) NaryMaxDom<View>(home,share,*this);
  }

  template<class View>
  PropCost
  NaryMaxDom<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_DOM)
      return PropCost::linear(PropCost::HI, y.size());
    else
      return PropCost::linear(PropCost::LO, x.size());
  }

  template<class View>
  ExecStatus
  NaryMaxDom<View>::propagate(Space& home, const ModEventDelta& med) {
    if (View::me(med) != ME_INT_DOM) {
      ExecStatus es = prop_nary_max_bnd(home,*this,x,y,PC_INT_DOM);
      GECODE_ES_CHECK(es);
      return (es == ES_FIX) ?
        home.ES_FIX_PARTIAL(*this,View::med(ME_INT_DOM)) :
        home.ES_NOFIX_PARTIAL(*this,View::med(ME_INT_DOM));
    }
    Region r(home);
    ViewRanges<View>* i_x = r.alloc<ViewRanges<View> >(x.size());
    for (int i = x.size(); i--; ) {
      ViewRanges<View> i_xi(x[i]); i_x[i]=i_xi;
    }
    Iter::Ranges::NaryUnion u(r, i_x, x.size());
    GECODE_ME_CHECK(y.inter_r(home,u,false));
    for (int i = x.size(); i--; )
      if (rtest_nq_dom(x[i],y) == RT_TRUE) {
        GECODE_ES_CHECK(Rel::Lq<View>::post(home,x[i],y));
        x.move_lst(i,home,*this,PC_INT_DOM);
      }
    assert(x.size() > 0);
    if (x.size() == 1)
      GECODE_REWRITE(*this,(Rel::EqDom<View,View>::post(home(*this),x[0],y)));
    return ES_FIX;
  }

}}}

// STATISTICS: int-prop

