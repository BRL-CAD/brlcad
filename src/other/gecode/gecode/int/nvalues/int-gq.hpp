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
#include <gecode/int/distinct.hh>

namespace Gecode { namespace Int { namespace NValues {

  template<class VY>
  forceinline
  GqInt<VY>::GqInt(Home home, ValSet& vs, ViewArray<IntView>& x, VY y)
    : IntBase<VY>(home,vs,x,y) {}

  template<class VY>
  inline ExecStatus
  GqInt<VY>::post(Home home, ViewArray<IntView>& x, VY y) {
    if (x.size() == 0) {
      GECODE_ME_CHECK(y.lq(home,0));
      return ES_OK;
    }

    x.unique(home);

    if (x.size() == 1) {
      GECODE_ME_CHECK(y.lq(home,1));
      return ES_OK;
    }

    GECODE_ME_CHECK(y.lq(home,x.size()));

    if (y.max() <= 1)
      return ES_OK;

    if (y.min() == x.size()) {
      assert(y.assigned());
      return Distinct::Dom<IntView>::post(home,x);
    }
    
    // Eliminate assigned views and store them into the value set
    ValSet vs;
    int n = x.size();
    for (int i=n; i--; )
      if (x[i].assigned()) {
        vs.add(home, x[i].val());
        x[i] = x[--n];
      }

    GECODE_ME_CHECK(y.lq(home,n + vs.size()));

    if (n == 0) {
      assert(vs.size() >= y.max());
      return ES_OK;
    }

    x.size(n);

    (void) new (home) GqInt<VY>(home, vs, x, y);
    return ES_OK;
  }
    
  template<class VY>
  forceinline
  GqInt<VY>::GqInt(Space& home, bool share, GqInt<VY>& p)
    : IntBase<VY>(home, share, p) {}

  template<class VY>
  Propagator*
  GqInt<VY>::copy(Space& home, bool share) {
    return new (home) GqInt<VY>(home, share, *this);
  }

  template<class VY>
  ExecStatus
  GqInt<VY>::propagate(Space& home, const ModEventDelta& med) {
    if (IntView::me(med) == ME_INT_VAL)
      add(home);

    // Eliminate subsumed views
    eliminate(home);

    GECODE_ME_CHECK(y.lq(home, x.size() + vs.size()));

    if (x.size() == 0)
      return home.ES_SUBSUMED(*this);

    if (vs.size() >= y.max())
      return home.ES_SUBSUMED(*this);
      
    GECODE_ES_CHECK(prune_upper(home,g));

    return ES_NOFIX;
  }
  
}}}

// STATISTICS: int-prop
