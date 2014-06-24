/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2007
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

namespace Gecode { namespace Int { namespace Circuit {

  /*
   * The actual propagator
   *
   */
  template<class View, class Offset>
  forceinline
  Dom<View,Offset>::Dom(Home home, ViewArray<View>& x, Offset& o)
    : Base<View,Offset>(home,x,o) {}

  template<class View, class Offset>
  forceinline
  Dom<View,Offset>::Dom(Space& home, bool share, Dom<View,Offset>& p)
    : Base<View,Offset>(home,share,p) {}

  template<class View, class Offset>
  Actor*
  Dom<View,Offset>::copy(Space& home, bool share) {
    return new (home) Dom<View,Offset>(home,share,*this);
  }

  template<class View, class Offset>
  PropCost
  Dom<View,Offset>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == Int::ME_INT_VAL)
      return PropCost::linear(PropCost::LO, x.size());
    else
      return PropCost::quadratic(PropCost::HI, x.size());
  }

  template<class View, class Offset>
  ExecStatus
  Dom<View,Offset>::propagate(Space& home, const ModEventDelta& med) {
    if (View::me(med) == Int::ME_INT_VAL) {
      GECODE_ES_CHECK((Int::Distinct::prop_val<View,true>(home,y)));
      ExecStatus escv = connected(home);
      if (escv != ES_FIX)
        return escv;
      if (y.size() < 2)
        return home.ES_SUBSUMED(*this);
      return home.ES_FIX_PARTIAL(*this,View::med(Int::ME_INT_DOM));
    }

    if (dc.available()) {
      GECODE_ES_CHECK(dc.sync(home));
    } else {
      GECODE_ES_CHECK(dc.init(home,y));
    }
    bool assigned;
    GECODE_ES_CHECK(dc.propagate(home,assigned));

    ExecStatus esc = connected(home);
    if (esc != ES_FIX)
      return esc;

    // Elminiate assigned views from y, as they have been assigned
    // and propagated by domain consistent propagation. This is required
    // as we need to know how many assigned views actually exist.
    if (assigned)
      for (int i=y.size(); i--; )
        if (y[i].assigned())
          y.move_lst(i);

    return path(home);
  }

  template<class View, class Offset>
  ExecStatus
  Dom<View,Offset>::post(Home home, ViewArray<View>& x, Offset& o) {
    int n = x.size();
    if (n == 1) {
      GECODE_ME_CHECK(o(x[0]).eq(home,0));
    } else if (n == 2) {
      GECODE_ME_CHECK(o(x[0]).eq(home,1));
      GECODE_ME_CHECK(o(x[1]).eq(home,0));
    } else {
      for (int i=n; i--; ) {
        GECODE_ME_CHECK(o(x[i]).gq(home,0));
        GECODE_ME_CHECK(o(x[i]).le(home,n));
        GECODE_ME_CHECK(o(x[i]).nq(home,i));
      }
      (void) new (home) Dom<View,Offset>(home,x,o);
    }
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

