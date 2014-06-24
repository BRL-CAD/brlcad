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

namespace Gecode { namespace Int { namespace Channel {

  forceinline
  LinkMulti::LinkMulti(Home home, ViewArray<BoolView>& x, IntView y, int o0)
    : MixNaryOnePropagator<BoolView,PC_BOOL_NONE,IntView,PC_INT_DOM>
  (home,x,y), c(home), status(S_NONE), o(o0) {
    x.subscribe(home,*new (home) Advisor(home,*this,c));
    // Propagator is scheduled because of the dependency subscription
  }

  forceinline ExecStatus
  LinkMulti::post(Home home, ViewArray<BoolView>& x, IntView y, int o) {
    int n=x.size();
    GECODE_ME_CHECK(y.gq(home,o));
    GECODE_ME_CHECK(y.lq(home,o+n-1));
    assert(n > 0);
    if (n == 1) {
      GECODE_ME_CHECK(x[0].one(home));
      assert(y.val() == o);
    } else if (y.assigned()) {
      int j=y.val()-o;
      GECODE_ME_CHECK(x[j].one(home));
      for (int i=0; i<j; i++)
        GECODE_ME_CHECK(x[i].zero(home));
      for (int i=j+1; i<n; i++)
        GECODE_ME_CHECK(x[i].zero(home));
    } else {
      for (int i=n; i--; )
        if (x[i].one()) {
          for (int j=0; j<i; j++)
            GECODE_ME_CHECK(x[j].zero(home));
          for (int j=i+1; j<n; j++)
            GECODE_ME_CHECK(x[j].zero(home));
          GECODE_ME_CHECK(y.eq(home,o+i));
          return ES_OK;
        } else if (x[i].zero()) {
          GECODE_ME_CHECK(y.nq(home,o+i));
        }
      (void) new (home) LinkMulti(home,x,y,o);
    }
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

