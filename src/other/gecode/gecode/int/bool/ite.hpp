/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2013
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

#include <algorithm>

namespace Gecode { namespace Int { namespace Bool {

  template<class View, PropCond pc>
  forceinline
  IteBase<View,pc>::IteBase(Home home, BoolView b0, View y0, View y1, View y2)
    : Propagator(home), b(b0), x0(y0), x1(y1), x2(y2) {
    b.subscribe(home,*this,PC_BOOL_VAL);
    x0.subscribe(home,*this,pc);
    x1.subscribe(home,*this,pc);
    x2.subscribe(home,*this,pc);
  }

  template<class View, PropCond pc>
  forceinline
  IteBase<View,pc>::IteBase(Space& home, bool share, IteBase<View,pc>& p)
    : Propagator(home,share,p) {
    b.update(home,share,p.b);
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    x2.update(home,share,p.x2);
  }

  template<class View, PropCond pc>
  PropCost
  IteBase<View,pc>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::ternary(PropCost::LO);
  }

  template<class View, PropCond pc>
  forceinline size_t
  IteBase<View,pc>::dispose(Space& home) {
    b.cancel(home,*this,PC_BOOL_VAL);
    x0.cancel(home,*this,pc);
    x1.cancel(home,*this,pc);
    x2.cancel(home,*this,pc);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }



  template<class View>
  forceinline
  IteBnd<View>::IteBnd(Home home, BoolView b, View x0, View x1, View x2)
    : IteBase<View,PC_INT_BND>(home,b,x0,x1,x2) {}

  template<class View>
  forceinline
  IteBnd<View>::IteBnd(Space& home, bool share, IteBnd<View>& p)
    : IteBase<View,PC_INT_BND>(home,share,p) {}

  template<class View>
  Actor*
  IteBnd<View>::copy(Space& home, bool share) {
    return new (home) IteBnd<View>(home,share,*this);
  }

  template<class View>
  inline ExecStatus
  IteBnd<View>::post(Home home, BoolView b, View x0, View x1, View x2) {
    if (same(x0,x1) || b.one())
      return Rel::EqBnd<View,View>::post(home,x2,x0);
    if (b.zero())
      return Rel::EqBnd<View,View>::post(home,x2,x1);
    GECODE_ME_CHECK(x2.lq(home,std::max(x0.max(),x1.max())));
    GECODE_ME_CHECK(x2.gq(home,std::min(x0.min(),x1.min())));
    (void) new (home) IteBnd<View>(home,b,x0,x1,x2);
    return ES_OK;
  }

  template<class View>
  ExecStatus
  IteBnd<View>::propagate(Space& home, const ModEventDelta&) {
    if (b.one())
      GECODE_REWRITE(*this,(Rel::EqBnd<View,View>::post(home(*this),x2,x0)));
    if (b.zero())
      GECODE_REWRITE(*this,(Rel::EqBnd<View,View>::post(home(*this),x2,x1)));

    GECODE_ME_CHECK(x2.lq(home,std::max(x0.max(),x1.max())));
    GECODE_ME_CHECK(x2.gq(home,std::min(x0.min(),x1.min())));
    
    RelTest eq20 = rtest_eq_bnd(x2,x0);
    RelTest eq21 = rtest_eq_bnd(x2,x1);

    if ((eq20 == RT_FALSE) && (eq21 == RT_FALSE))
      return ES_FAILED;

    if (eq20 == RT_FALSE) {
      GECODE_ME_CHECK(b.zero_none(home));
      if (eq21 == RT_TRUE)
        return home.ES_SUBSUMED(*this);
      else
        GECODE_REWRITE(*this,(Rel::EqBnd<View,View>::post(home(*this),x2,x1)));
    }

    if (eq21 == RT_FALSE) {
      GECODE_ME_CHECK(b.one_none(home));
      if (eq20 == RT_TRUE)
        return home.ES_SUBSUMED(*this);
      else
        GECODE_REWRITE(*this,(Rel::EqBnd<View,View>::post(home(*this),x2,x0)));
    }
    
    if ((eq20 == RT_TRUE) && (eq21 == RT_TRUE))
      return home.ES_SUBSUMED(*this);

    return ES_FIX;
  }



  template<class View>
  forceinline
  IteDom<View>::IteDom(Home home, BoolView b, View x0, View x1, View x2)
    : IteBase<View,PC_INT_DOM>(home,b,x0,x1,x2) {}

  template<class View>
  forceinline
  IteDom<View>::IteDom(Space& home, bool share, IteDom<View>& p)
    : IteBase<View,PC_INT_DOM>(home,share,p) {}

  template<class View>
  Actor*
  IteDom<View>::copy(Space& home, bool share) {
    return new (home) IteDom<View>(home,share,*this);
  }

  template<class View>
  inline ExecStatus
  IteDom<View>::post(Home home, BoolView b, View x0, View x1, View x2) {
    if (same(x0,x1) || b.one())
      return Rel::EqDom<View,View>::post(home,x2,x0);
    if (b.zero())
      return Rel::EqDom<View,View>::post(home,x2,x1);
    GECODE_ME_CHECK(x2.lq(home,std::max(x0.max(),x1.max())));
    GECODE_ME_CHECK(x2.gq(home,std::min(x0.min(),x1.min())));
    (void) new (home) IteDom<View>(home,b,x0,x1,x2);
    return ES_OK;
  }

  template<class View>
  PropCost
  IteDom<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_DOM)
      return PropCost::ternary(PropCost::HI);
    else
      return PropCost::ternary(PropCost::LO);
  }

  template<class View>
  ExecStatus
  IteDom<View>::propagate(Space& home, const ModEventDelta& med) {
    if (b.one())
      GECODE_REWRITE(*this,(Rel::EqDom<View,View>::post(home(*this),x2,x0)));
    if (b.zero())
      GECODE_REWRITE(*this,(Rel::EqDom<View,View>::post(home(*this),x2,x1)));

    GECODE_ME_CHECK(x2.lq(home,std::max(x0.max(),x1.max())));
    GECODE_ME_CHECK(x2.gq(home,std::min(x0.min(),x1.min())));

    if (View::me(med) != ME_INT_DOM) {
      RelTest eq20 = rtest_eq_bnd(x2,x0);
      RelTest eq21 = rtest_eq_bnd(x2,x1);

      if ((eq20 == RT_FALSE) && (eq21 == RT_FALSE))
        return ES_FAILED;

      if (eq20 == RT_FALSE) {
        GECODE_ME_CHECK(b.zero_none(home));
        if (eq21 == RT_TRUE)
          return home.ES_SUBSUMED(*this);
        else
          GECODE_REWRITE(*this,
                         (Rel::EqDom<View,View>::post(home(*this),x2,x1)));
      }
      
      if (eq21 == RT_FALSE) {
        GECODE_ME_CHECK(b.one_none(home));
        if (eq20 == RT_TRUE)
          return home.ES_SUBSUMED(*this);
        else
          GECODE_REWRITE(*this,
                         (Rel::EqDom<View,View>::post(home(*this),x2,x0)));
      }
    
      if ((eq20 == RT_TRUE) && (eq21 == RT_TRUE))
        return home.ES_SUBSUMED(*this);

      return home.ES_FIX_PARTIAL(*this,View::med(ME_INT_DOM));
    }

    RelTest eq20 = rtest_eq_dom(x2,x0);
    RelTest eq21 = rtest_eq_dom(x2,x1);

    if ((eq20 == RT_FALSE) && (eq21 == RT_FALSE))
      return ES_FAILED;

    if (eq20 == RT_FALSE) {
      GECODE_ME_CHECK(b.zero_none(home));
      if (eq21 == RT_TRUE)
        return home.ES_SUBSUMED(*this);
      else
        GECODE_REWRITE(*this,
                       (Rel::EqDom<View,View>::post(home(*this),x2,x1)));
    }
      
    if (eq21 == RT_FALSE) {
      GECODE_ME_CHECK(b.one_none(home));
      if (eq20 == RT_TRUE)
        return home.ES_SUBSUMED(*this);
      else
        GECODE_REWRITE(*this,
                       (Rel::EqDom<View,View>::post(home(*this),x2,x0)));
    }
    
    assert((eq20 != RT_TRUE) || (eq21 != RT_TRUE));

    ViewRanges<View> r0(x0), r1(x1);
    Iter::Ranges::Union<ViewRanges<View>,ViewRanges<View> > u(r0,r1);
    
    if (!same(x0,x2) && !same(x1,x2))
      GECODE_ME_CHECK(x2.inter_r(home,u,false));
    else
      GECODE_ME_CHECK(x2.inter_r(home,u,true));

    return ES_FIX;
  }

}}}

// STATISTICS: int-prop
