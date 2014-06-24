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

#include <algorithm>

namespace Gecode { namespace Int { namespace Rel {

  /*
   * Disequality
   *
   */
  template<class View>
  forceinline
  Nq<View>::Nq(Home home, View x0, View x1)
    : BinaryPropagator<View,PC_INT_VAL>(home,x0,x1) {}

  template<class View>
  ExecStatus
  Nq<View>::post(Home home, View x0, View x1){
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.nq(home,x0.val()));
    } else if (x1.assigned()) {
      GECODE_ME_CHECK(x0.nq(home,x1.val()));
    } else if (same(x0,x1)) {
      return ES_FAILED;
    } else {
      (void) new (home) Nq<View>(home,x0,x1);
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  Nq<View>::Nq(Space& home, bool share, Nq<View>& p)
    : BinaryPropagator<View,PC_INT_VAL>(home,share,p) {}

  template<class View>
  Actor*
  Nq<View>::copy(Space& home, bool share) {
    return new (home) Nq<View>(home,share,*this);
  }

  template<class View>
  PropCost
  Nq<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::LO);
  }

  template<class View>
  ExecStatus
  Nq<View>::propagate(Space& home, const ModEventDelta&) {
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.nq(home,x0.val()));
    } else {
      GECODE_ME_CHECK(x0.nq(home,x1.val()));
    }
    return home.ES_SUBSUMED(*this);
  }


  /*
   * Nary disequality propagator
   */
  template<class View>
  forceinline
  NaryNq<View>::NaryNq(Home home, ViewArray<View>& x)
    : NaryPropagator<View,PC_INT_VAL>(home,x) {}

  template<class View>
  PropCost
  NaryNq<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO,x.size());
  }

  template<class View>
  forceinline
  NaryNq<View>::NaryNq(Space& home, bool share, NaryNq<View>& p)
    : NaryPropagator<View,PC_INT_VAL>(home,share,p) {}

  template<class View>
  Actor*
  NaryNq<View>::copy(Space& home, bool share) {
    return new (home) NaryNq<View>(home,share,*this);
  }

  template<class View>
  inline ExecStatus
  NaryNq<View>::post(Home home, ViewArray<View>& x) {
    x.unique(home);
    // Try to find an assigned view
    int n = x.size();
    if (n <= 1)
      return ES_FAILED;
    for (int i=n; i--; )
      if (x[i].assigned()) {
        std::swap(x[0],x[i]);
        break;
      }
    if (x[0].assigned()) {
      int v = x[0].val();
      // Eliminate all equal views and possibly find disequal view
      for (int i=n-1; i>0; i--)
        if (!x[i].in(v)) {
          return ES_OK;
        } else if (x[i].assigned()) {
          assert(x[i].val() == v);
          x[i]=x[--n];
        }
      x.size(n);
    }
    if (n == 1)
      return ES_FAILED;
    if (n == 2)
      return Nq<View>::post(home,x[0],x[1]);
    (void) new (home) NaryNq(home,x);
    return ES_OK;
  }

  template<class View>
  forceinline size_t
  NaryNq<View>::dispose(Space& home) {
    (void) NaryPropagator<View,PC_INT_VAL>::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  ExecStatus
  NaryNq<View>::propagate(Space& home, const ModEventDelta&) {
    // Make sure that x[0] is assigned
    if (!x[0].assigned()) {
      // Note that there is at least one assigned view
      for (int i=1; true; i++)
        if (x[i].assigned()) {
          std::swap(x[0],x[i]);
          break;
        }
    }
    int v = x[0].val();
    int n = x.size();
    for (int i=n-1; i>0; i--)
      if (!x[i].in(v)) {
        x.size(n);
        return home.ES_SUBSUMED(*this);
      } else if (x[i].assigned()) {
        assert(x[i].val() == v);
        x[i] = x[--n];
      }
    x.size(n);
    if (n == 1)
      return ES_FAILED;
    if (n == 2) {
      GECODE_ME_CHECK(x[1].nq(home,v));
      return home.ES_SUBSUMED(*this);        
    }
    return ES_FIX;
  }



}}}

// STATISTICS: int-prop
