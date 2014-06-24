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

#include <gecode/int/arithmetic.hh>

namespace Gecode { namespace Int { namespace Arithmetic {

  /*
   * Bounds consistent multiplication
   *
   */
  Actor*
  MultBnd::copy(Space& home, bool share) {
    return new (home) MultBnd(home,share,*this);
  }

  ExecStatus
  MultBnd::propagate(Space& home, const ModEventDelta&) {
    if (pos(x0)) {
      if (pos(x1) || pos(x2)) goto rewrite_ppp;
      if (neg(x1) || neg(x2)) goto rewrite_pnn;
      goto prop_pxx;
    }
    if (neg(x0)) {
      if (neg(x1) || pos(x2)) goto rewrite_nnp;
      if (pos(x1) || neg(x2)) goto rewrite_npn;
      goto prop_nxx;
    }
    if (pos(x1)) {
      if (pos(x2)) goto rewrite_ppp;
      if (neg(x2)) goto rewrite_npn;
      goto prop_xpx;
    }
    if (neg(x1)) {
      if (pos(x2)) goto rewrite_nnp;
      if (neg(x2)) goto rewrite_pnn;
      goto prop_xnx;
    }

    assert(any(x0) && any(x1));
    GECODE_ME_CHECK(x2.lq(home,std::max(mll(x0.max(),x1.max()),
                                        mll(x0.min(),x1.min()))));
    GECODE_ME_CHECK(x2.gq(home,std::min(mll(x0.min(),x1.max()),
                                        mll(x0.max(),x1.min()))));

    if (x0.assigned()) {
      assert((x0.val() == 0) && (x2.val() == 0));
      return home.ES_SUBSUMED(*this);
    }

    if (x1.assigned()) {
      assert((x1.val() == 0) && (x2.val() == 0));
      return home.ES_SUBSUMED(*this);
    }

    return ES_NOFIX;

  prop_xpx:
    std::swap(x0,x1);
  prop_pxx:
    assert(pos(x0) && any(x1) && any(x2));

    GECODE_ME_CHECK(x2.lq(home,mll(x0.max(),x1.max())));
    GECODE_ME_CHECK(x2.gq(home,mll(x0.max(),x1.min())));

    if (pos(x2)) goto rewrite_ppp;
    if (neg(x2)) goto rewrite_pnn;

    GECODE_ME_CHECK(x1.lq(home,floor_div_xp(x2.max(),x0.min())));
    GECODE_ME_CHECK(x1.gq(home,ceil_div_xp(x2.min(),x0.min())));

    if (x0.assigned() && x1.assigned()) {
      GECODE_ME_CHECK(x2.eq(home,mll(x0.val(),x1.val())));
      return home.ES_SUBSUMED(*this);
    }

    return ES_NOFIX;

  prop_xnx:
    std::swap(x0,x1);
  prop_nxx:
    assert(neg(x0) && any(x1) && any(x2));

    GECODE_ME_CHECK(x2.lq(home,mll(x0.min(),x1.min())));
    GECODE_ME_CHECK(x2.gq(home,mll(x0.min(),x1.max())));

    if (pos(x2)) goto rewrite_nnp;
    if (neg(x2)) goto rewrite_npn;

    GECODE_ME_CHECK(x1.lq(home,floor_div_xx(x2.min(),x0.max())));
    GECODE_ME_CHECK(x1.gq(home,ceil_div_xx(x2.max(),x0.max())));

    if (x0.assigned() && x1.assigned()) {
      GECODE_ME_CHECK(x2.eq(home,mll(x0.val(),x1.val())));
      return home.ES_SUBSUMED(*this);
    }

    return ES_NOFIX;

  rewrite_ppp:
    GECODE_REWRITE(*this,(MultPlusBnd<IntView,IntView,IntView>
                         ::post(home(*this),x0,x1,x2)));
  rewrite_nnp:
    GECODE_REWRITE(*this,(MultPlusBnd<MinusView,MinusView,IntView>
                         ::post(home(*this),MinusView(x0),MinusView(x1),x2)));
  rewrite_pnn:
    std::swap(x0,x1);
  rewrite_npn:
    GECODE_REWRITE(*this,(MultPlusBnd<MinusView,IntView,MinusView>
                         ::post(home(*this),MinusView(x0),x1,MinusView(x2))));
  }

  ExecStatus
  MultBnd::post(Home home, IntView x0, IntView x1, IntView x2) {
    if (same(x0,x1)) {
      SqrOps ops;
      return PowBnd<SqrOps>::post(home,x0,x2,ops);
    }
    if (same(x0,x2))
      return MultZeroOne<IntView,PC_INT_BND>::post(home,x0,x1);
    if (same(x1,x2))
      return MultZeroOne<IntView,PC_INT_BND>::post(home,x1,x0);
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
    {
      long long int a = mll(x0.min(),x1.min());
      long long int b = mll(x0.min(),x1.max());
      long long int c = mll(x0.max(),x1.min());
      long long int d = mll(x0.max(),x1.max());
      GECODE_ME_CHECK(x2.gq(home,std::min(std::min(a,b),std::min(c,d))));
      GECODE_ME_CHECK(x2.lq(home,std::max(std::max(a,b),std::max(c,d))));
      (void) new (home) MultBnd(home,x0,x1,x2);
    }
    return ES_OK;

  post_ppp:
    return MultPlusBnd<IntView,IntView,IntView>
      ::post(home,x0,x1,x2);
  post_nnp:
    return MultPlusBnd<MinusView,MinusView,IntView>
      ::post(home,MinusView(x0),MinusView(x1),x2);
  post_pnn:
    std::swap(x0,x1);
  post_npn:
    return MultPlusBnd<MinusView,IntView,MinusView>
      ::post(home,MinusView(x0),x1,MinusView(x2));
  }



  /*
   * Domain consistent multiplication
   *
   */
  Actor*
  MultDom::copy(Space& home, bool share) {
    return new (home) MultDom(home,share,*this);
  }

  PropCost
  MultDom::cost(const Space&, const ModEventDelta& med) const {
    if (IntView::me(med) == ME_INT_DOM)
      return PropCost::ternary(PropCost::HI);
    else
      return PropCost::ternary(PropCost::LO);
  }

  ExecStatus
  MultDom::propagate(Space& home, const ModEventDelta& med) {
    if (IntView::me(med) != ME_INT_DOM) {
      if (pos(x0)) {
        if (pos(x1) || pos(x2)) goto rewrite_ppp;
        if (neg(x1) || neg(x2)) goto rewrite_pnn;
        goto prop_pxx;
      }
      if (neg(x0)) {
        if (neg(x1) || pos(x2)) goto rewrite_nnp;
        if (pos(x1) || neg(x2)) goto rewrite_npn;
        goto prop_nxx;
      }
      if (pos(x1)) {
        if (pos(x2)) goto rewrite_ppp;
        if (neg(x2)) goto rewrite_npn;
        goto prop_xpx;
      }
      if (neg(x1)) {
        if (pos(x2)) goto rewrite_nnp;
        if (neg(x2)) goto rewrite_pnn;
        goto prop_xnx;
      }

      assert(any(x0) && any(x1));
      GECODE_ME_CHECK(x2.lq(home,std::max(mll(x0.max(),x1.max()),
                                          mll(x0.min(),x1.min()))));
      GECODE_ME_CHECK(x2.gq(home,std::min(mll(x0.min(),x1.max()),
                                          mll(x0.max(),x1.min()))));

      if (x0.assigned()) {
        assert((x0.val() == 0) && (x2.val() == 0));
        return home.ES_SUBSUMED(*this);
      }

      if (x1.assigned()) {
        assert((x1.val() == 0) && (x2.val() == 0));
        return home.ES_SUBSUMED(*this);
      }

      return home.ES_NOFIX_PARTIAL(*this,IntView::med(ME_INT_DOM));

    prop_xpx:
      std::swap(x0,x1);
    prop_pxx:
      assert(pos(x0) && any(x1) && any(x2));

      GECODE_ME_CHECK(x2.lq(home,mll(x0.max(),x1.max())));
      GECODE_ME_CHECK(x2.gq(home,mll(x0.max(),x1.min())));

      if (pos(x2)) goto rewrite_ppp;
      if (neg(x2)) goto rewrite_pnn;

      GECODE_ME_CHECK(x1.lq(home,floor_div_xp(x2.max(),x0.min())));
      GECODE_ME_CHECK(x1.gq(home,ceil_div_xp(x2.min(),x0.min())));

      if (x0.assigned() && x1.assigned()) {
        GECODE_ME_CHECK(x2.eq(home,mll(x0.val(),x1.val())));
        return home.ES_SUBSUMED(*this);
      }

      return home.ES_NOFIX_PARTIAL(*this,IntView::med(ME_INT_DOM));

    prop_xnx:
      std::swap(x0,x1);
    prop_nxx:
      assert(neg(x0) && any(x1) && any(x2));

      GECODE_ME_CHECK(x2.lq(home,mll(x0.min(),x1.min())));
      GECODE_ME_CHECK(x2.gq(home,mll(x0.min(),x1.max())));

      if (pos(x2)) goto rewrite_nnp;
      if (neg(x2)) goto rewrite_npn;

      GECODE_ME_CHECK(x1.lq(home,floor_div_xx(x2.min(),x0.max())));
      GECODE_ME_CHECK(x1.gq(home,ceil_div_xx(x2.max(),x0.max())));

      if (x0.assigned() && x1.assigned()) {
        GECODE_ME_CHECK(x2.eq(home,mll(x0.val(),x1.val())));
        return home.ES_SUBSUMED(*this);
      }

      return home.ES_NOFIX_PARTIAL(*this,IntView::med(ME_INT_DOM));

    rewrite_ppp:
      GECODE_REWRITE(*this,(MultPlusDom<IntView,IntView,IntView>
                           ::post(home(*this),x0,x1,x2)));
    rewrite_nnp:
      GECODE_REWRITE(*this,(MultPlusDom<MinusView,MinusView,IntView>
                           ::post(home(*this),
                                  MinusView(x0),MinusView(x1),x2)));
    rewrite_pnn:
      std::swap(x0,x1);
    rewrite_npn:
      GECODE_REWRITE(*this,(MultPlusDom<MinusView,IntView,MinusView>
                           ::post(home(*this),
                                  MinusView(x0),x1,MinusView(x2))));
    }
    return prop_mult_dom<IntView>(home,*this,x0,x1,x2);
  }

  ExecStatus
  MultDom::post(Home home, IntView x0, IntView x1, IntView x2) {
    if (same(x0,x1)) {
      SqrOps ops;
      return PowDom<SqrOps>::post(home,x0,x2,ops);
    }
    if (same(x0,x2))
      return MultZeroOne<IntView,PC_INT_DOM>::post(home,x0,x1);
    if (same(x1,x2))
      return MultZeroOne<IntView,PC_INT_DOM>::post(home,x1,x0);
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
    {
      long long int a = mll(x0.min(),x1.min());
      long long int b = mll(x0.min(),x1.max());
      long long int c = mll(x0.max(),x1.min());
      long long int d = mll(x0.max(),x1.max());
      GECODE_ME_CHECK(x2.gq(home,std::min(std::min(a,b),std::min(c,d))));
      GECODE_ME_CHECK(x2.lq(home,std::max(std::max(a,b),std::max(c,d))));
      (void) new (home) MultDom(home,x0,x1,x2);
    }
    return ES_OK;

  post_ppp:
    return MultPlusDom<IntView,IntView,IntView>
      ::post(home,x0,x1,x2);
  post_nnp:
    return MultPlusDom<MinusView,MinusView,IntView>
      ::post(home,MinusView(x0),MinusView(x1),x2);
  post_pnn:
    std::swap(x0,x1);
  post_npn:
    return MultPlusDom<MinusView,IntView,MinusView>
      ::post(home,MinusView(x0),x1,MinusView(x2));
  }

}}}

// STATISTICS: int-prop

