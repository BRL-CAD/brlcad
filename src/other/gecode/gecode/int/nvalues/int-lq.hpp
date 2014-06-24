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
  LqInt<VY>::LqInt(Home home, ValSet& vs, ViewArray<IntView>& x, VY y)
    : IntBase<VY>(home,vs,x,y) {
    home.notice(*this, AP_WEAKLY);
  }

  template<class VY>
  inline ExecStatus
  LqInt<VY>::post(Home home, ViewArray<IntView>& x, VY y) {
    if (x.size() == 0) {
      GECODE_ME_CHECK(y.eq(home,0));
      return ES_OK;
    }

    x.unique(home);

    GECODE_ME_CHECK(y.gq(home,1));

    if (x.size() == 1)
      return ES_OK;

    if (y.max() == 1) {
      assert(y.assigned());
      return Rel::NaryEqDom<IntView>::post(home,x);
    }

    if (y.min() >= x.size())
      return ES_OK;
    
    // Eliminate assigned views and store them into the value set
    ValSet vs;
    int n = x.size();
    for (int i=n; i--; )
      if (x[i].assigned()) {
        vs.add(home, x[i].val());
        x[i] = x[--n];
      }

    GECODE_ME_CHECK(y.gq(home,vs.size()));

    if (n == 0) {
      assert(y.min() >= vs.size());
      return ES_OK;
    }

    x.size(n);

    (void) new (home) LqInt<VY>(home, vs, x, y);
    return ES_OK;
  }
    
  template<class VY>
  forceinline
  LqInt<VY>::LqInt(Space& home, bool share, LqInt<VY>& p)
    : IntBase<VY>(home, share, p) {}

  template<class VY>
  Propagator*
  LqInt<VY>::copy(Space& home, bool share) {
    return new (home) LqInt<VY>(home, share, *this);
  }

  template<class VY>
  forceinline size_t
  LqInt<VY>::dispose(Space& home) {
    home.ignore(*this, AP_WEAKLY);
    (void) IntBase<VY>::dispose(home);
    return sizeof(*this);
  }

  template<class VY>
  ExecStatus
  LqInt<VY>::propagate(Space& home, const ModEventDelta& med) {
    // Add assigned views to value set
    if (IntView::me(med) == ME_INT_VAL)
      add(home);

    GECODE_ME_CHECK(y.gq(home, vs.size()));

    if (x.size() == 0)
      return home.ES_SUBSUMED(*this);

    // All values must be in the value set
    if (y.max() == vs.size())
      return all_in_valset(home);

    if (x.size() + vs.size() <= y.min())
      return home.ES_SUBSUMED(*this);
      
    // Compute positions of disjoint views
    Region r(home);
    int* dis; int n_dis;
    disjoint(home,r,dis,n_dis);

    // Some views might have been eliminated as they are subsumed
    if (x.size() == 0)
      return home.ES_SUBSUMED(*this);

    // No lower bound pruning possible
    if (n_dis == 0)
      return ES_NOFIX;

    // Do lower bound-based pruning
    GECODE_ES_CHECK(prune_lower(home,dis,n_dis));

    return ES_NOFIX;
  }
  
}}}

// STATISTICS: int-prop
