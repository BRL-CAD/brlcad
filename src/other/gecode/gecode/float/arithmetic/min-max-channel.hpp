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

namespace Gecode { namespace Float { namespace Arithmetic {

#include <cmath>
  /*
   * Bounds consistent min operator
   *
   */
  template<class A, class B, class C>
  forceinline
  Min<A,B,C>::Min(Home home, A x0, B x1, C x2)
    : MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>(home,x0,x1,x2) {}

  template<class A, class B, class C>
  forceinline
  Min<A,B,C>::Min(Space& home, bool share, Min<A,B,C>& p)
    : MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B, class C>
  forceinline
  Min<A,B,C>::Min(Space& home, bool share, Propagator& p,
                  A x0, B x1, C x2)
    : MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>(home,share,p,x0,x1,x2) {}

  template<class A, class B, class C>
  Actor*
  Min<A,B,C>::copy(Space& home, bool share) {
    return new (home) Min<A,B,C>(home,share,*this);
  }

  template<class A, class B, class C>
  ExecStatus
  Min<A,B,C>::post(Home home, A x0, B x1, C x2) {
    (void) new (home) Min<A,B,C>(home,x0,x1,x2);
    return ES_OK;
  }

  template<class A, class B, class C>
  ExecStatus
  Min<A,B,C>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x2.eq(home,min(x0.domain(),x1.domain())));
    GECODE_ME_CHECK(x0.gq(home,x2.min()));
    GECODE_ME_CHECK(x1.gq(home,x2.min()));
    if (same(x0,x1)) {
      GECODE_ME_CHECK(x0.lq(home,x2.max()));
    } else {
      if (!overlap(x1.val(),x2.val())) GECODE_ME_CHECK(x0.lq(home,x2.max()));
      if (!overlap(x0.val(),x2.val())) GECODE_ME_CHECK(x1.lq(home,x2.max()));
    }
    return (x0.assigned() && x1.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

  /*
   * Bounds consistent max operator
   *
   */

  template<class A, class B, class C>
  forceinline
  Max<A,B,C>::Max(Home home, A x0, B x1, C x2)
    : MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>(home,x0,x1,x2) {}

  template<class A, class B, class C>
  forceinline
  Max<A,B,C>::Max(Space& home, bool share, Max<A,B,C>& p)
    : MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B, class C>
  forceinline
  Max<A,B,C>::Max(Space& home, bool share, Propagator& p,
                  A x0, B x1, C x2)
    : MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>(home,share,p,x0,x1,x2) {}

  template<class A, class B, class C>
  Actor*
  Max<A,B,C>::copy(Space& home, bool share) {
    return new (home) Max<A,B,C>(home,share,*this);
  }

  template<class A, class B, class C>
  ExecStatus
  Max<A,B,C>::post(Home home, A x0, B x1, C x2) {
    (void) new (home) Max<A,B,C>(home,x0,x1,x2);
    return ES_OK;
  }

  template<class A, class B, class C>
  ExecStatus
  Max<A,B,C>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x2.eq(home,max(x0.domain(),x1.domain())));
    GECODE_ME_CHECK(x0.lq(home,x2.max()));
    GECODE_ME_CHECK(x1.lq(home,x2.max()));
    if (same(x0,x1)) {
      GECODE_ME_CHECK(x0.gq(home,x2.min()));
    } else {
      if (!overlap(x1.val(),x2.val())) GECODE_ME_CHECK(x0.gq(home,x2.min()));
      if (!overlap(x0.val(),x2.val())) GECODE_ME_CHECK(x1.gq(home,x2.min()));
    }
    return (x0.assigned() && x1.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

  /*
   * Nary bounds consistent maximum
   *
   */

  template<class View>
  forceinline
  NaryMax<View>::NaryMax(Home home, ViewArray<View>& x, View y)
    : NaryOnePropagator<View,PC_FLOAT_BND>(home,x,y) {}

  template<class View>
  ExecStatus
  NaryMax<View>::post(Home home, ViewArray<View>& x, View y) {
    assert(x.size() > 0);
    x.unique(home);
    if (x.size() == 1)
      return Rel::Eq<View,View>::post(home,x[0],y);
    if (x.size() == 2)
      return Max<View,View,View>::post(home,x[0],x[1],y);
    FloatNum l = Float::Limits::min;
    FloatNum u = Float::Limits::min;
    for (int i=x.size(); i--; ) {
      l = std::max(l,x[i].min());
      u = std::max(u,x[i].max());
    }
    GECODE_ME_CHECK(y.gq(home,l));
    GECODE_ME_CHECK(y.lq(home,u));
    if (x.same(home,y)) {
      // Check whether y occurs in x
      for (int i=x.size(); i--; )
        GECODE_ES_CHECK((Rel::Lq<View>::post(home,x[i],y)));
    } else {
      (void) new (home) NaryMax<View>(home,x,y);
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  NaryMax<View>::NaryMax(Space& home, bool share, NaryMax<View>& p)
    : NaryOnePropagator<View,PC_FLOAT_BND>(home,share,p) {}

  template<class View>
  Actor*
  NaryMax<View>::copy(Space& home, bool share) {
    if (x.size() == 1)
      return new (home) Rel::Eq<View,View>(home,share,*this,x[0],y);
    if (x.size() == 2)
      return new (home) Max<View,View,View>(home,share,*this,x[0],x[1],y);
    return new (home) NaryMax<View>(home,share,*this);
  }

  /// Status of propagation for nary max
  enum MaxPropStatus {
    MPS_ASSIGNED  = 1<<0, ///< All views are assigned
    MPS_REMOVED   = 1<<1, ///< A view is removed
    MPS_NEW_BOUND = 1<<2  ///< Telling has found a new upper bound
  };

  template<class View>
  forceinline ExecStatus
  prop_nary_max(Space& home, Propagator& p,
                ViewArray<View>& x, View y, PropCond pc) {
  rerun:
    assert(x.size() > 0);
    FloatNum maxmax = x[x.size()-1].max();
    FloatNum maxmin = x[x.size()-1].min();
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
      if (me == ME_FLOAT_FAILED)
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
  NaryMax<View>::propagate(Space& home, const ModEventDelta&) {
    return prop_nary_max(home,*this,x,y,PC_FLOAT_BND);
  }


  /*
   * Bounds consistent interger part operator
   *
   */

  template<class A, class B>
  forceinline
  Channel<A,B>::Channel(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,Int::PC_INT_BND>(home,x0,x1) {}

  template<class A, class B>
  forceinline
  Channel<A,B>::Channel(Space& home, bool share, Channel<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,Int::PC_INT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  Channel<A,B>::copy(Space& home, bool share) {
    return new (home) Channel<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  Channel<A,B>::post(Home home, A x0, B x1) {
    GECODE_ME_CHECK(x0.eq(home,FloatVal(Int::Limits::min,
                                        Int::Limits::max)));
    (void) new (home) Channel<A,B>(home,x0,x1);
    return ES_OK;
  }

  template<class A, class B>
  ExecStatus
  Channel<A,B>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x1.gq(home,static_cast<int>(std::ceil(x0.min()))));
    GECODE_ME_CHECK(x1.lq(home,static_cast<int>(std::floor(x0.max()))));
    GECODE_ME_CHECK(x0.eq(home,FloatVal(x1.min(),x1.max())));
    return x0.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

}}}

// STATISTICS: float-prop

