/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

namespace Gecode { namespace Int { namespace Member {

  template<class View>
  forceinline
  Prop<View>::Prop(Home home, ValSet& vs0, ViewArray<View>& x, View y)
    : NaryOnePropagator<View,PC_INT_DOM>(home,x,y),
      vs(vs0) {}

  template<class View>
  forceinline void
  Prop<View>::add(Space& home, ValSet& vs, ViewArray<View>& x) {
    int n=x.size();
    for (int i=n; i--; )
      if (x[i].assigned()) {
        vs.add(home, x[i].val());
        x[i] = x[--n];
      }
    x.size(n);
  }

  template<class View>
  forceinline void
  Prop<View>::eliminate(Space& home) {
    int n=x.size();
    for (int i=n; i--; )
      if ((rtest_eq_dom(x[i],y) == RT_FALSE) || vs.subset(x[i])) {
        // x[i] is different from y or values are contained in vs
        x[i].cancel(home, *this, PC_INT_DOM);
        x[i] = x[--n]; 
      }
    x.size(n);
  }

  template<class View>
  inline ExecStatus
  Prop<View>::post(Home home, ViewArray<View>& x, View y) {
    if (x.size() == 0)
      return ES_FAILED;

    x.unique(home);

    if (x.size() == 1)
      return Rel::EqDom<View,View>::post(home,x[0],y);
    
    if (x.same(home,y))
      return ES_OK;

    // Eliminate assigned views and store them into the value set
    ValSet vs;
    add(home, vs, x);

    if (x.size() == 0) {
      ValSet::Ranges vsr(vs);
      GECODE_ME_CHECK(y.inter_r(home,vsr,false));
      return ES_OK;
    }

    (void) new (home) Prop<View>(home, vs, x, y);
    return ES_OK;
  }
    
  template<class View>
  forceinline ExecStatus
  Prop<View>::post(Home home, ValSet& vs, ViewArray<View>& x, View y) {
    (void) new (home) Prop<View>(home, vs, x, y);
    return ES_OK;
  }
    
  template<class View>
  forceinline
  Prop<View>::Prop(Space& home, bool share, Prop<View>& p)
    : NaryOnePropagator<View,PC_INT_DOM>(home, share, p) {
    vs.update(home, share, p.vs);
  }

  template<class View>
  Propagator*
  Prop<View>::copy(Space& home, bool share) {
    return new (home) Prop<View>(home, share, *this);
  }

  template<class View>
  forceinline size_t
  Prop<View>::dispose(Space& home) {
    vs.dispose(home);
    (void) NaryOnePropagator<View,PC_INT_DOM>::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  PropCost 
  Prop<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::HI, x.size()+1);
  }

  template<class View>
  ExecStatus
  Prop<View>::propagate(Space& home, const ModEventDelta& med) {
    // Add assigned views to value set
    if (View::me(med) == ME_INT_VAL)
      add(home,vs,x);

    // Eliminate views from x
    eliminate(home);
    
    if (x.size() == 0) {
      // y must have values in the value set
      ValSet::Ranges vsr(vs);
      GECODE_ME_CHECK(y.inter_r(home,vsr,false));
      return home.ES_SUBSUMED(*this);
    }

    // Constrain y to union of x and value set
    Region r(home);

    assert(x.size() > 0);
    ValSet::Ranges vsr(vs);
    ViewRanges<View> xsr(x[x.size()-1]);
    Iter::Ranges::NaryUnion  u(r,vsr,xsr);
    for (int i=x.size()-1; i--; ) {
      ViewRanges<View> xir(x[i]);
      u |= xir;
    }

    GECODE_ME_CHECK(y.inter_r(home,u,false));

    // Check whether all values in y are already in the value set
    if (vs.subset(y))
      return home.ES_SUBSUMED(*this);

    return ES_FIX;
  }

}}}

// STATISTICS: int-prop
