/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

#include <climits>

namespace Gecode { namespace Int { namespace Distinct {

  template<class View>
  forceinline
  Dom<View>::Dom(Home home, ViewArray<View>& x)
    : NaryPropagator<View,PC_INT_DOM>(home,x) {}

  template<class View>
  ExecStatus
  Dom<View>::post(Home home, ViewArray<View>& x) {
    if (x.size() == 2)
      return Rel::Nq<View>::post(home,x[0],x[1]);
    if (x.size() == 3)
      return TerDom<View>::post(home,x[0],x[1],x[2]);
    if (x.size() > 3) {
      // Do bounds propagation to make view-value graph smaller
      GECODE_ES_CHECK(prop_bnd<View>(home,x));
      (void) new (home) Dom<View>(home,x);
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  Dom<View>::Dom(Space& home, bool share, Dom<View>& p)
    : NaryPropagator<View,PC_INT_DOM>(home,share,p) {}

  template<class View>
  PropCost
  Dom<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_VAL)
      return PropCost::linear(PropCost::LO, x.size());
    else
      return PropCost::quadratic(PropCost::HI, x.size());
  }

  template<class View>
  Actor*
  Dom<View>::copy(Space& home, bool share) {
    return new (home) Dom<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  Dom<View>::propagate(Space& home, const ModEventDelta& med) {
    if (View::me(med) == ME_INT_VAL) {
      ExecStatus es = prop_val<View,false>(home,x);
      GECODE_ES_CHECK(es);
      if (x.size() < 2)
        return home.ES_SUBSUMED(*this);
      if (es == ES_FIX)
        return home.ES_FIX_PARTIAL(*this,View::med(ME_INT_DOM));
      es = prop_bnd<View>(home,x);
      GECODE_ES_CHECK(es);
      if (x.size() < 2)
        return home.ES_SUBSUMED(*this);
      es = prop_val<View,true>(home,x);
      GECODE_ES_CHECK(es);
      if (x.size() < 2)
        return home.ES_SUBSUMED(*this);
      return home.ES_FIX_PARTIAL(*this,View::med(ME_INT_DOM));
    }

    if (x.size() == 2)
      GECODE_REWRITE(*this,Rel::Nq<View>::post(home(*this),x[0],x[1]));
    if (x.size() == 3)
      GECODE_REWRITE(*this,TerDom<View>::post(home(*this),x[0],x[1],x[2]));

    if (dc.available()) {
      GECODE_ES_CHECK(dc.sync(home));
    } else {
      GECODE_ES_CHECK(dc.init(home,x));
    }

    bool assigned;
    GECODE_ES_CHECK(dc.propagate(home,assigned));

    return ES_FIX;
  }

}}}

// STATISTICS: int-prop

