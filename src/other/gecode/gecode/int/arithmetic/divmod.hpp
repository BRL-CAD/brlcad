/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2008
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

#include <gecode/int/linear.hh>

namespace Gecode { namespace Int { namespace Arithmetic {

  /*
   * Positive bounds consistent division
   *
   */

  template<class VA, class VB, class VC>
  forceinline
  DivPlusBnd<VA,VB,VC>::DivPlusBnd(Home home, VA x0, VB x1, VC x2)
    : MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>
  (home,x0,x1,x2) {}

  template<class VA, class VB, class VC>
  forceinline
  DivPlusBnd<VA,VB,VC>::DivPlusBnd(Space& home, bool share, 
                                   DivPlusBnd<VA,VB,VC>& p)
    : MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>
  (home,share,p) {}

  template<class VA, class VB, class VC>
  Actor*
  DivPlusBnd<VA,VB,VC>::copy(Space& home, bool share) {
    return new (home) DivPlusBnd<VA,VB,VC>(home,share,*this);
  }

  template<class VA, class VB, class VC>
  ExecStatus
  DivPlusBnd<VA,VB,VC>::propagate(Space& home, const ModEventDelta&) {
    assert(pos(x0) && pos(x1) && !neg(x2));
    bool mod;
    do {
      mod = false;
      GECODE_ME_CHECK_MODIFIED(mod,x2.lq(home,
                                         floor_div_pp(x0.max(),x1.min())));
      GECODE_ME_CHECK_MODIFIED(mod,x2.gq(home,
                                         floor_div_px(x0.min(),x1.max())));
      GECODE_ME_CHECK_MODIFIED(mod,x0.le(home,mll(x1.max(),ill(x2.max()))));
      GECODE_ME_CHECK_MODIFIED(mod,x0.gq(home,mll(x1.min(),x2.min())));
      if (x2.min() > 0) {
          GECODE_ME_CHECK_MODIFIED(mod,
            x1.lq(home,floor_div_pp(x0.max(),x2.min())));
      }
      GECODE_ME_CHECK_MODIFIED(mod,x1.gq(home,ceil_div_pp(ll(x0.min()),
                                                          ill(x2.max()))));
    } while (mod);
    return x0.assigned() && x1.assigned() ?
      home.ES_SUBSUMED(*this) : ES_FIX;
  }

  template<class VA, class VB, class VC>
  forceinline ExecStatus
  DivPlusBnd<VA,VB,VC>::post(Home home, VA x0, VB x1, VC x2) {
    GECODE_ME_CHECK(x0.gr(home,0));
    GECODE_ME_CHECK(x1.gr(home,0));
    GECODE_ME_CHECK(x2.gq(home,floor_div_pp(x0.min(),x1.max())));
    (void) new (home) DivPlusBnd<VA,VB,VC>(home,x0,x1,x2);
    return ES_OK;
  }


  /*
   * Bounds consistent division
   *
   */
  template<class View>
  forceinline
  DivBnd<View>::DivBnd(Home home, View x0, View x1, View x2)
    : TernaryPropagator<View,PC_INT_BND>(home,x0,x1,x2) {}

  template<class View>
  forceinline
  DivBnd<View>::DivBnd(Space& home, bool share, DivBnd<View>& p)
    : TernaryPropagator<View,PC_INT_BND>(home,share,p) {}

  template<class View>
  Actor*
  DivBnd<View>::copy(Space& home, bool share) {
    return new (home) DivBnd<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  DivBnd<View>::propagate(Space& home, const ModEventDelta&) {
    if (pos(x1)) {
      if (pos(x2) || pos(x0)) goto rewrite_ppp;
      if (neg(x2) || neg(x0)) goto rewrite_npn;
      goto prop_xpx;
    }
    if (neg(x1)) {
      if (neg(x2) || pos(x0)) goto rewrite_pnn;
      if (pos(x2) || neg(x0)) goto rewrite_nnp;
      goto prop_xnx;
    }
    if (pos(x2)) {
      if (pos(x0)) goto rewrite_ppp;
      if (neg(x0)) goto rewrite_nnp;
      goto prop_xxp;
    }
    if (neg(x2)) {
      if (pos(x0)) goto rewrite_pnn;
      if (neg(x0)) goto rewrite_npn;
      goto prop_xxn;
    }
    assert(any(x1) && any(x2));
    GECODE_ME_CHECK(x0.lq(home,std::max(mll(x1.max(),ill(x2.max()))-1,
                                        mll(x1.min(),dll(x2.min()))-1)));
    GECODE_ME_CHECK(x0.gq(home,std::min(mll(x1.min(),ill(x2.max())),
                                        mll(x1.max(),dll(x2.min())))));
    return ES_NOFIX;

  prop_xxp:
    assert(any(x0) && any(x1) && pos(x2));

    GECODE_ME_CHECK(x0.le(home, mll(x1.max(),ill(x2.max()))));
    GECODE_ME_CHECK(x0.gq(home, mll(x1.min(),ill(x2.max()))));

    if (pos(x0)) goto rewrite_ppp;
    if (neg(x0)) goto rewrite_nnp;

    GECODE_ME_CHECK(x1.lq(home,floor_div_pp(x0.max(),x2.min())));
    GECODE_ME_CHECK(x1.gq(home,ceil_div_xp(x0.min(),x2.min())));

    if (x0.assigned() && x1.assigned())
      goto subsumed;
    return ES_NOFIX;

  prop_xpx:
    assert(any(x0) && pos(x1) && any(x2));

    GECODE_ME_CHECK(x0.le(home, mll(x1.max(),ill(x2.max()))));
    GECODE_ME_CHECK(x0.gq(home, mll(x1.max(),dll(x2.min()))));

    if (pos(x0)) goto rewrite_ppp;
    if (neg(x0)) goto rewrite_npn;

    GECODE_ME_CHECK(x2.lq(home,floor_div_xp(x0.max(),x1.min())));
    GECODE_ME_CHECK(x2.gq(home,floor_div_xp(x0.min(),x1.min())));

    if (x0.assigned() && x1.assigned())
      goto subsumed;
    return ES_NOFIX;

  prop_xxn:
    assert(any(x0) && any(x1) && neg(x2));

    GECODE_ME_CHECK(x0.lq(home, mll(x1.min(),dll(x2.min()))));
    GECODE_ME_CHECK(x0.gq(home, mll(x1.max(),dll(x2.min()))));

    if (pos(x0)) goto rewrite_pnn;
    if (neg(x0)) goto rewrite_npn;

    if (x2.max() != -1)
      GECODE_ME_CHECK(x1.lq(home,ceil_div_xx(ll(x0.min()),ill(x2.max()))));
    if (x2.max() != -1)
      GECODE_ME_CHECK(x1.gq(home,ceil_div_xx(ll(x0.max()),ill(x2.max()))));

    if (x0.assigned() && x1.assigned())
      goto subsumed;
    return ES_NOFIX;

  prop_xnx:
    assert(any(x0) && neg(x1) && any(x2));

    GECODE_ME_CHECK(x0.lq(home, mll(x1.min(),dll(x2.min()))));
    GECODE_ME_CHECK(x0.gq(home, mll(x1.min(),ill(x2.max()))));

    if (pos(x0)) goto rewrite_pnn;
    if (neg(x0)) goto rewrite_nnp;

    GECODE_ME_CHECK(x2.lq(home,floor_div_xx(x0.min(),x1.max())));
    GECODE_ME_CHECK(x2.gq(home,floor_div_xx(x0.max(),x1.max())));

    if (x0.assigned() && x1.assigned())
      goto subsumed;
    return ES_NOFIX;

  rewrite_ppp:
    GECODE_REWRITE(*this,(DivPlusBnd<IntView,IntView,IntView>
                          ::post(home(*this),x0,x1,x2)));
  rewrite_nnp:
    GECODE_REWRITE(*this,(DivPlusBnd<MinusView,MinusView,IntView>
                          ::post(home(*this),MinusView(x0),MinusView(x1),x2)));
  rewrite_pnn:
    GECODE_REWRITE(*this,(DivPlusBnd<IntView,MinusView,MinusView>
                          ::post(home(*this),x0,MinusView(x1),MinusView(x2))));
  rewrite_npn:
    GECODE_REWRITE(*this,(DivPlusBnd<MinusView,IntView,MinusView>
                          ::post(home(*this),MinusView(x0),x1,MinusView(x2))));
  subsumed:
    assert(x0.assigned() && x1.assigned());
    int result = std::abs(x0.val()) / std::abs(x1.val());
    if (x0.val()/x1.val() < 0)
      result = -result;
    GECODE_ME_CHECK(x2.eq(home,result));
    return home.ES_SUBSUMED(*this);
  }

  template<class View>
  ExecStatus
  DivBnd<View>::post(Home home, View x0, View x1, View x2) {
    GECODE_ME_CHECK(x1.nq(home, 0));
    if (pos(x0)) {
      if (pos(x1) || pos(x2)) goto post_ppp;
      if (neg(x1) || neg(x2)) goto post_pnn;
    } else if (neg(x0)) {
      if (neg(x1) || pos(x2)) goto post_nnp;
      if (pos(x1) || neg(x2)) goto post_npn;
    } else if (pos(x1)) {
      if (pos(x2)) goto post_ppp;
      if (neg(x2)) goto post_npn;
    } else if (neg(x1)) {
      if (pos(x2)) goto post_nnp;
      if (neg(x2)) goto post_pnn;
    }
    (void) new (home) DivBnd<View>(home,x0,x1,x2);
    return ES_OK;

  post_ppp:
    return DivPlusBnd<IntView,IntView,IntView>
      ::post(home,x0,x1,x2);
  post_nnp:
    return DivPlusBnd<MinusView,MinusView,IntView>
      ::post(home,MinusView(x0),MinusView(x1),x2);
  post_pnn:
    return DivPlusBnd<IntView,MinusView,MinusView>
      ::post(home,x0,MinusView(x1),MinusView(x2));
  post_npn:
    return DivPlusBnd<MinusView,IntView,MinusView>
      ::post(home,MinusView(x0),x1,MinusView(x2));
  }


  /*
   * Propagator for x0 != 0 /\ (x1 != 0 => x0*x1>0) /\ abs(x1)<abs(x0)
   *
   */

  template<class View>
  forceinline
  DivMod<View>::DivMod(Home home, View x0, View x1, View x2)
    : TernaryPropagator<View,PC_INT_BND>(home,x0,x1,x2) {}

  template<class View>
  forceinline ExecStatus
  DivMod<View>::post(Home home, View x0, View x1, View x2) {
    GECODE_ME_CHECK(x1.nq(home,0));
    (void) new (home) DivMod<View>(home,x0,x1,x2);
    return ES_OK;
  }

  template<class View>
  forceinline
  DivMod<View>::DivMod(Space& home, bool share, DivMod<View>& p)
  : TernaryPropagator<View,PC_INT_BND>(home,share,p) {}

  template<class View>
  Actor*
  DivMod<View>::copy(Space& home, bool share) {
    return new (home) DivMod<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  DivMod<View>::propagate(Space& home, const ModEventDelta&) {
    bool signIsSame;
    do {
      signIsSame = true;
      // The sign of x0 and x2 is the same
      if (x0.min() >= 0) {
        GECODE_ME_CHECK(x2.gq(home, 0));
      } else if (x0.max() <= 0) {
        GECODE_ME_CHECK(x2.lq(home, 0));
      } else if (x2.min() > 0) {
        GECODE_ME_CHECK(x0.gq(home, 0));
      } else if (x2.max() < 0) {
        GECODE_ME_CHECK(x0.lq(home, 0));
      } else {
        signIsSame = false;
      }

      // abs(x2) is less than abs(x1)
      int x1max = std::max(x1.max(),std::max(-x1.max(),
                           std::max(x1.min(),-x1.min())));
      GECODE_ME_CHECK(x2.le(home, x1max));
      GECODE_ME_CHECK(x2.gr(home, -x1max));

      int x2absmin = any(x2) ? 0 : (pos(x2) ? x2.min() : -x2.max());
      Iter::Ranges::Singleton sr(-x2absmin,x2absmin);
      GECODE_ME_CHECK(x1.minus_r(home,sr,false));
    } while (!signIsSame &&
             (x0.min() > 0 || x0.max() < 0 || x2.min() > 0 || x2.max() < 0));

    if (signIsSame) {
      int x2max = std::max(x2.max(),std::max(-x2.max(),
                           std::max(x2.min(),-x2.min())));
      int x1absmin = any(x1) ? 0 : (pos(x1) ? x1.min() : -x1.max());
      if (x2max < x1absmin)
        return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }

}}}

// STATISTICS: int-prop
