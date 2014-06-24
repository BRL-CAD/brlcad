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

namespace Gecode { namespace Int { namespace NValues {

  template<class VY>
  forceinline
  LqBool<VY>::LqBool(Home home, int status, ViewArray<BoolView>& x, VY y)
    : BoolBase<VY>(home,status,x,y) {}

  template<class VY>
  forceinline
  LqBool<VY>::LqBool(Space& home, bool share, LqBool<VY>& p)
    : BoolBase<VY>(home,share,p) {}

  template<class VY>
  Actor*
  LqBool<VY>::copy(Space& home, bool share) {
    return new (home) LqBool<VY>(home,share,*this);
  }

  template<class VY>
  inline ExecStatus
  LqBool<VY>::post(Home home, ViewArray<BoolView>& x, VY y) {
    if (x.size() == 0) {
      GECODE_ME_CHECK(y.gq(home,0));
      return ES_OK;
    }

    x.unique(home);

    GECODE_ME_CHECK(y.gq(home,1));

    if (x.size() == 1)
      return ES_OK;

    if (y.max() == 1) {
      assert(y.assigned());
      ViewArray<BoolView> xc(home,x);
      return Bool::NaryEq<BoolView>::post(home,xc);
    }

    if (y.min() >= 2)
      return ES_OK;

    int n = x.size();
    int status = 0;
    for (int i=n; i--; )
      if (x[i].zero()) {
        if (status & VS_ONE) {
          GECODE_ME_CHECK(y.gq(home,2));
          return ES_OK;
        }
        x[i] = x[--n];
        status |= VS_ZERO;
      } else if (x[i].one()) {
        if (status & VS_ZERO) {
          GECODE_ME_CHECK(y.gq(home,2));
          return ES_OK;
        }
        x[i] = x[--n];
        status |= VS_ONE;
      }
    
    assert(status != (VS_ZERO | VS_ONE));
    if (n == 0) {
      assert((status != 0) && (y.min() >= 1));
      return ES_OK;
    }

    x.size(n);

    (void) new (home) LqBool<VY>(home,status,x,y);
    return ES_OK;
  }

  template<class VY>
  ExecStatus
  LqBool<VY>::propagate(Space& home, const ModEventDelta&) {
    if (status == (VS_ZERO | VS_ONE)) {
      GECODE_ME_CHECK(y.gq(home,2));
      return home.ES_SUBSUMED(*this);
    }

    if (c.empty()) {
      assert((status != 0) && (y.min() >= 1));
      return home.ES_SUBSUMED(*this);
    }

    if (y.max() == 1) {
      if (status == VS_ZERO) {
        // Mark that everything is done
        status = VS_ZERO | VS_ONE;
        for (Advisors<ViewAdvisor<BoolView> > as(c); as(); ++as)
          GECODE_ME_CHECK(as.advisor().view().zero(home));
        return home.ES_SUBSUMED(*this);
      }
      if (status == VS_ONE) {
        // Mark that everything is done
        status = VS_ZERO | VS_ONE;
        for (Advisors<ViewAdvisor<BoolView> > as(c); as(); ++as)
          GECODE_ME_CHECK(as.advisor().view().one(home));
        return home.ES_SUBSUMED(*this);
      }
    }

    if (y.min() == 2)
      return home.ES_SUBSUMED(*this);

    return ES_FIX;
  }

}}}

// STATISTICS: int-prop
