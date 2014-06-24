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

#include <gecode/int/bool.hh>

namespace Gecode { namespace Int { namespace Bool {

  /*
   * N-ary Boolean equivalence propagator
   *
   */

  PropCost
  NaryEqv::cost(const Space&, const ModEventDelta&) const {
    return PropCost::binary(PropCost::LO);
  }

  Actor*
  NaryEqv::copy(Space& home, bool share) {
    return new (home) NaryEqv(home,share,*this);
  }

  ExecStatus
  NaryEqv::post(Home home, ViewArray<BoolView>& x, int pm2) {
    int n = x.size();
    for (int i=n; i--; )
      if (x[i].assigned()) {
        pm2 ^= x[i].val();
        x[i] = x[--n];
      }
    if (n == 0)
      return (pm2 == 1) ? ES_OK : ES_FAILED;
    if (n == 1) {
      GECODE_ME_CHECK(x[0].eq(home,1^pm2));
      return ES_OK;
    }
    if (n == 2) {
      if (pm2 == 1) {
        return Bool::Eq<BoolView,BoolView>::post(home,x[0],x[1]);
      } else {
        NegBoolView n(x[1]);
        return Bool::Eq<BoolView,NegBoolView>::post(home,x[0],n);
      }
    }
    x.size(n);
    (void) new (home) NaryEqv(home,x,pm2);
    return ES_OK;
  }

  ExecStatus
  NaryEqv::propagate(Space& home, const ModEventDelta&) {
    resubscribe(home,x0);
    resubscribe(home,x1);
    if (x.size() == 0) {
      if (x0.assigned() && x1.assigned()) {
        return (pm2 == 1) ? home.ES_SUBSUMED(*this) : ES_FAILED;
      } else if (x0.assigned()) {
        GECODE_ME_CHECK(x1.eq(home,1^pm2));
        return home.ES_SUBSUMED(*this);
      } else if (x1.assigned()) {
        GECODE_ME_CHECK(x0.eq(home,1^pm2));
        return home.ES_SUBSUMED(*this);
      }
    }
    return ES_FIX;
  }

}}}

// STATISTICS: int-prop
