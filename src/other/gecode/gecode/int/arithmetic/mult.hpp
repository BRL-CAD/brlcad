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

#include <cmath>
#include <climits>

#include <gecode/int/div.hh>
#include <gecode/int/support-values.hh>

namespace Gecode { namespace Int { namespace Arithmetic {

  /*
   * Arithmetic help functions
   *
   */
  /// Multiply \a x and \y
  forceinline long long int
  mll(long long int x, long long int y) {
    return x*y;
  }
  /// Cast \a x into a long long int
  forceinline long long int
  ll(int x) {
    return static_cast<long long int>(x);
  }
  /// Increment \a x by one
  forceinline long long int
  ill(int x) {
    return static_cast<long long int>(x) + 1;
  }
  /// Decrement \a x by one
  forceinline long long int
  dll(int x) {
    return static_cast<long long int>(x) - 1;
  }
    
  /// Test whether \a x is postive
  template<class View>
  forceinline bool
  pos(const View& x) {
    return x.min() > 0;
  }
  /// Test whether \a x is negative
  template<class View>
  forceinline bool
  neg(const View& x) {
    return x.max() < 0;
  }
  /// Test whether \a x is neither positive nor negative
  template<class View>
  forceinline bool
  any(const View& x) {
    return (x.min() <= 0) && (x.max() >= 0);
  }


  /*
   * Propagator for x * y = x
   *
   */

  template<class View, PropCond pc>
  forceinline
  MultZeroOne<View,pc>::MultZeroOne(Home home, View x0, View x1)
    : BinaryPropagator<View,pc>(home,x0,x1) {}

  template<class View, PropCond pc>
  forceinline RelTest
  MultZeroOne<View,pc>::equal(View x, int n) {
    if (pc == PC_INT_DOM) {
      return rtest_eq_dom(x,n);
    } else {
      return rtest_eq_bnd(x,n);
    }
  }

  template<class View, PropCond pc>
  forceinline ExecStatus
  MultZeroOne<View,pc>::post(Home home, View x0, View x1) {
    switch (equal(x0,0)) {
    case RT_FALSE:
      GECODE_ME_CHECK(x1.eq(home,1));
      break;
    case RT_TRUE:
      break;
    case RT_MAYBE:
      switch (equal(x1,1)) {
      case RT_FALSE:
        GECODE_ME_CHECK(x0.eq(home,0));
        break;
      case RT_TRUE:
        break;
      case RT_MAYBE:
        (void) new (home) MultZeroOne<View,pc>(home,x0,x1);
        break;
      default: GECODE_NEVER;
      }
      break;
    default: GECODE_NEVER;
    }
    return ES_OK;
  }

  template<class View, PropCond pc>
  forceinline
  MultZeroOne<View,pc>::MultZeroOne(Space& home, bool share,
                                    MultZeroOne<View,pc>& p)
    : BinaryPropagator<View,pc>(home,share,p) {}

  template<class View, PropCond pc>
  Actor*
  MultZeroOne<View,pc>::copy(Space& home, bool share) {
    return new (home) MultZeroOne<View,pc>(home,share,*this);
  }

  template<class View, PropCond pc>
  ExecStatus
  MultZeroOne<View,pc>::propagate(Space& home, const ModEventDelta&) {
    switch (equal(x0,0)) {
    case RT_FALSE:
      GECODE_ME_CHECK(x1.eq(home,1));
      break;
    case RT_TRUE:
      break;
    case RT_MAYBE:
      switch (equal(x1,1)) {
      case RT_FALSE:
        GECODE_ME_CHECK(x0.eq(home,0));
        break;
      case RT_TRUE:
        break;
      case RT_MAYBE:
        return ES_FIX;
      default: GECODE_NEVER;
      }
      break;
    default: GECODE_NEVER;
    }
    return home.ES_SUBSUMED(*this);
  }


  /*
   * Positive bounds consistent multiplication
   *
   */
  template<class VA, class VB, class VC>
  forceinline ExecStatus
  prop_mult_plus_bnd(Space& home, Propagator& p, VA x0, VB x1, VC x2) {
    assert(pos(x0) && pos(x1) && pos(x2));
    bool mod;
    do {
      mod = false;
      {
        ModEvent me = x2.lq(home,mll(x0.max(),x1.max()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x2.gq(home,mll(x0.min(),x1.min()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x0.lq(home,floor_div_pp(x2.max(),x1.min()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x0.gq(home,ceil_div_pp(x2.min(),x1.max()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x1.lq(home,floor_div_pp(x2.max(),x0.min()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x1.gq(home,ceil_div_pp(x2.min(),x0.max()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
    } while (mod);
    return x0.assigned() && x1.assigned() ?
      home.ES_SUBSUMED(p) : ES_FIX;
  }

  template<class VA, class VB, class VC>
  forceinline
  MultPlusBnd<VA,VB,VC>::MultPlusBnd(Home home, VA x0, VB x1, VC x2)
    : MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>
  (home,x0,x1,x2) {}

  template<class VA, class VB, class VC>
  forceinline
  MultPlusBnd<VA,VB,VC>::MultPlusBnd(Space& home, bool share,
                                     MultPlusBnd<VA,VB,VC>& p)
    : MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>
  (home,share,p) {}

  template<class VA, class VB, class VC>
  Actor*
  MultPlusBnd<VA,VB,VC>::copy(Space& home, bool share) {
    return new (home) MultPlusBnd<VA,VB,VC>(home,share,*this);
  }

  template<class VA, class VB, class VC>
  ExecStatus
  MultPlusBnd<VA,VB,VC>::propagate(Space& home, const ModEventDelta&) {
    return prop_mult_plus_bnd<VA,VB,VC>(home,*this,x0,x1,x2);
  }

  template<class VA, class VB, class VC>
  forceinline ExecStatus
  MultPlusBnd<VA,VB,VC>::post(Home home, VA x0, VB x1, VC x2) {
    GECODE_ME_CHECK(x0.gr(home,0));
    GECODE_ME_CHECK(x1.gr(home,0));
    GECODE_ME_CHECK(x2.gq(home,mll(x0.min(),x1.min())));
    GECODE_ME_CHECK(x2.lq(home,mll(x0.max(),x1.max())));
    (void) new (home) MultPlusBnd<VA,VB,VC>(home,x0,x1,x2);
    return ES_OK;
  }


  /*
   * Bounds consistent multiplication
   *
   */
  forceinline
  MultBnd::MultBnd(Home home, IntView x0, IntView x1, IntView x2)
    : TernaryPropagator<IntView,PC_INT_BND>(home,x0,x1,x2) {}

  forceinline
  MultBnd::MultBnd(Space& home, bool share, MultBnd& p)
    : TernaryPropagator<IntView,PC_INT_BND>(home,share,p) {}

  /*
   * Positive domain consistent multiplication
   *
   */
  template<class View>
  forceinline ExecStatus
  prop_mult_dom(Space& home, Propagator& p, View x0, View x1, View x2) {
    Region r(home);
    SupportValues<View,Region> s0(r,x0), s1(r,x1), s2(r,x2);
    while (s0()) {
      while (s1()) {
        if (s2.support(mll(s0.val(),s1.val()))) {
          s0.support(); s1.support();
        }
        ++s1;
      }
      s1.reset(); ++s0;
    }
    GECODE_ME_CHECK(s0.tell(home));
    GECODE_ME_CHECK(s1.tell(home));
    GECODE_ME_CHECK(s2.tell(home));
    return x0.assigned() && x1.assigned() ? home.ES_SUBSUMED(p) : ES_FIX;
  }

  template<class VA, class VB, class VC>
  forceinline
  MultPlusDom<VA,VB,VC>::MultPlusDom(Home home, VA x0, VB x1, VC x2)
    : MixTernaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM,VC,PC_INT_DOM>
  (home,x0,x1,x2) {}

  template<class VA, class VB, class VC>
  forceinline
  MultPlusDom<VA,VB,VC>::MultPlusDom(Space& home, bool share,
                                         MultPlusDom<VA,VB,VC>& p)
    : MixTernaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM,VC,PC_INT_DOM>
      (home,share,p) {}

  template<class VA, class VB, class VC>
  Actor*
  MultPlusDom<VA,VB,VC>::copy(Space& home, bool share) {
    return new (home) MultPlusDom<VA,VB,VC>(home,share,*this);
  }

  template<class VA, class VB, class VC>
  PropCost
  MultPlusDom<VA,VB,VC>::cost(const Space&,
                                  const ModEventDelta& med) const {
    if (VA::me(med) == ME_INT_DOM)
      return PropCost::ternary(PropCost::HI);
    else
      return PropCost::ternary(PropCost::LO);
  }

  template<class VA, class VB, class VC>
  ExecStatus
  MultPlusDom<VA,VB,VC>::propagate(Space& home, const ModEventDelta& med) {
    if (VA::me(med) != ME_INT_DOM) {
      GECODE_ES_CHECK((prop_mult_plus_bnd<VA,VB,VC>(home,*this,x0,x1,x2)));
      return home.ES_FIX_PARTIAL(*this,VA::med(ME_INT_DOM));
    }
    IntView y0(x0.varimp()), y1(x1.varimp()), y2(x2.varimp());
    return prop_mult_dom<IntView>(home,*this,y0,y1,y2);
  }

  template<class VA, class VB, class VC>
  forceinline ExecStatus
  MultPlusDom<VA,VB,VC>::post(Home home, VA x0, VB x1, VC x2) {
    GECODE_ME_CHECK(x0.gr(home,0));
    GECODE_ME_CHECK(x1.gr(home,0));
    GECODE_ME_CHECK(x2.gq(home,mll(x0.min(),x1.min())));
    GECODE_ME_CHECK(x2.lq(home,mll(x0.max(),x1.max())));
    (void) new (home) MultPlusDom<VA,VB,VC>(home,x0,x1,x2);
    return ES_OK;
  }


  /*
   * Domain consistent multiplication
   *
   */
  forceinline
  MultDom::MultDom(Home home, IntView x0, IntView x1, IntView x2)
    : TernaryPropagator<IntView,PC_INT_DOM>(home,x0,x1,x2) {}

  forceinline
  MultDom::MultDom(Space& home, bool share, MultDom& p)
    : TernaryPropagator<IntView,PC_INT_DOM>(home,share,p) {}

}}}

// STATISTICS: int-prop

