/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
 *     Guido Tack, 2006
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

#include <algorithm>

namespace Gecode { namespace Int { namespace Arithmetic {

  template<class View, template<class View0,class View1> class Eq>
  ExecStatus
  prop_abs_bnd(Space& home, Propagator& p, View x0, View x1) {
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.eq(home,(x0.val() < 0) ? -x0.val() : x0.val()));
      return home.ES_SUBSUMED(p);
    }

    if (x1.assigned()) {
      if (x0.min() >= 0) {
        GECODE_ME_CHECK(x0.eq(home,x1.val()));
        return home.ES_SUBSUMED(p);
      } else if (x0.max() <= 0) {
        GECODE_ME_CHECK(x0.eq(home,-x1.val()));
        return home.ES_SUBSUMED(p);
      } else if (x1.val() == 0) {
        GECODE_ME_CHECK(x0.eq(home,0));
        return home.ES_SUBSUMED(p);
      } else {
        int mp[2] = {-x1.val(),x1.val()};
        Iter::Values::Array i(mp,2);
        GECODE_ME_CHECK(x0.inter_v(home,i,false));
        return home.ES_SUBSUMED(p);
      }
    }

    if (x0.min() >= 0)
      GECODE_REWRITE(p,(Eq<View,View>::post(home(p),x0,x1)));

    if (x0.max() <= 0)
      GECODE_REWRITE(p,(Eq<MinusView,View>::post(home(p),MinusView(x0),x1)));

    GECODE_ME_CHECK(x1.lq(home,std::max(x0.max(),-x0.min())));
    GECODE_ME_CHECK(x0.lq(home,x1.max()));
    GECODE_ME_CHECK(x0.gq(home,-x1.max()));
    return ES_NOFIX;
  }

  template<class View>
  forceinline
  AbsBnd<View>::AbsBnd(Home home, View x0, View x1)
    : BinaryPropagator<View,PC_INT_BND>(home,x0,x1) {}

  template<class View>
  ExecStatus
  AbsBnd<View>::post(Home home, View x0, View x1) {
    if (x0.min() >= 0) {
      return Rel::EqBnd<View,View>::post(home,x0,x1);
    } else if (x0.max() <= 0) {
      return Rel::EqBnd<MinusView,View>::post(home,MinusView(x0),x1);
    } else {
      assert(!x0.assigned());
      GECODE_ME_CHECK(x1.gq(home,0));
      if (x1.assigned()) {
        int mp[2] = {-x1.val(),x1.val()};
        Iter::Values::Array i(mp,2);
        GECODE_ME_CHECK(x0.inter_v(home,i,false));
      } else if (!same(x0,x1)) {
        GECODE_ME_CHECK(x1.lq(home,std::max(-x0.min(),x0.max())));
        (void) new (home) AbsBnd<View>(home,x0,x1);
      }
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  AbsBnd<View>::AbsBnd(Space& home, bool share, AbsBnd<View>& p)
    : BinaryPropagator<View,PC_INT_BND>(home,share,p) {}

  template<class View>
  Actor*
  AbsBnd<View>::copy(Space& home,bool share) {
    return new (home) AbsBnd<View>(home,share,*this);
  }

  template<class View>
  PropCost
  AbsBnd<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_VAL)
      return PropCost::unary(PropCost::LO);
    else
      return PropCost::binary(PropCost::LO);
  }

  template<class View>
  ExecStatus
  AbsBnd<View>::propagate(Space& home, const ModEventDelta&) {
    return prop_abs_bnd<View,Rel::EqBnd>(home, *this, x0, x1);
  }

  template<class View>
  forceinline
  AbsDom<View>::AbsDom(Home home, View x0, View x1)
    : BinaryPropagator<View,PC_INT_DOM>(home,x0,x1) {}

  template<class View>
  ExecStatus
  AbsDom<View>::post(Home home, View x0, View x1) {
    if (x0.min() >= 0) {
      return Rel::EqDom<View,View>::post(home,x0,x1);
    } else if (x0.max() <= 0) {
      return Rel::EqDom<MinusView,View>::post(home,MinusView(x0),x1);
    } else {
      assert(!x0.assigned());
      GECODE_ME_CHECK(x1.gq(home,0));
      if (x1.assigned()) {
        int mp[2] = {-x1.val(),x1.val()};
        Iter::Values::Array i(mp,2);
        GECODE_ME_CHECK(x0.inter_v(home,i,false));
      } else if (!same(x0,x1)) {
        GECODE_ME_CHECK(x1.lq(home,std::max(-x0.min(),x0.max())));
        (void) new (home) AbsDom<View>(home,x0,x1);
      }
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  AbsDom<View>::AbsDom(Space& home, bool share, AbsDom<View>& p)
    : BinaryPropagator<View,PC_INT_DOM>(home,share,p) {}

  template<class View>
  Actor*
  AbsDom<View>::copy(Space& home,bool share) {
    return new (home) AbsDom<View>(home,share,*this);
  }

  template<class View>
  PropCost
  AbsDom<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_VAL)
      return PropCost::unary(PropCost::LO);
    else
      return PropCost::binary((View::me(med) == ME_INT_DOM) ?
                              PropCost::HI : PropCost::LO);
  }

  template<class View>
  ExecStatus
  AbsDom<View>::propagate(Space& home, const ModEventDelta& med) {
    if (View::me(med) != ME_INT_DOM) {
      GECODE_ES_CHECK((prop_abs_bnd<View,Rel::EqDom>(home, *this, x0, x1)));
      return home.ES_NOFIX_PARTIAL(*this,View::med(ME_INT_DOM));
    }

    Region r(home);

    {
      ViewRanges<View> i(x0), j(x0);

      using namespace Iter::Ranges;
      Positive<ViewRanges<View> > p(i);
      Negative<ViewRanges<View> > n(j);

      Minus m(r,n);

      Union<Positive<ViewRanges<View> >,Minus> u(p,m);

      GECODE_ME_CHECK(x1.inter_r(home,u,false));

    }

    {
      ViewRanges<View> i(x1), j(x1);

      using namespace Iter::Ranges;
      Minus m(r,j);

      Union<ViewRanges<View>,Minus> u(i,m);

      GECODE_ME_CHECK(x0.inter_r(home,u,false));
    }

    if (x1.assigned())
      return home.ES_SUBSUMED(*this);

    if (x0.min() >= 0)
      GECODE_REWRITE(*this,(Rel::EqDom<View,View>::post(home(*this),x0,x1)));

    if (x0.max() <= 0) {
      MinusView mx0(x0);
      GECODE_REWRITE(*this,(Rel::EqDom<MinusView,View>::post(home(*this),mx0,x1)));
    }

    return ES_FIX;
  }

}}}

// STATISTICS: int-prop

