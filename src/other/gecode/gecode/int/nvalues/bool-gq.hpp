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

namespace Gecode { namespace Int { namespace NValues {

  template<class VY>
  forceinline
  GqBool<VY>::GqBool(Home home, int status, ViewArray<BoolView>& x, VY y)
    : BoolBase<VY>(home,status,x,y) {}

  template<class VY>
  forceinline
  GqBool<VY>::GqBool(Space& home, bool share, GqBool<VY>& p)
    : BoolBase<VY>(home,share,p) {}

  template<class VY>
  Actor*
  GqBool<VY>::copy(Space& home, bool share) {
    return new (home) GqBool<VY>(home,share,*this);
  }

  template<class VY>
  inline ExecStatus
  GqBool<VY>::post(Home home, ViewArray<BoolView>& x, VY y) {
    if (x.size() == 0) {
      GECODE_ME_CHECK(y.lq(home,0));
      return ES_OK;
    }

    x.unique(home);

    if (x.size() == 1) {
      GECODE_ME_CHECK(y.lq(home,1));
      return ES_OK;
    }

    GECODE_ME_CHECK(y.lq(home,2));

    if (y.max() <= 1)
      return ES_OK;

    if (y.min() == 2) {
      assert(y.assigned());
      ViewArray<BoolView> xc(home,x);
      return Rel::NaryNq<BoolView>::post(home,xc);
    }

    int n = x.size();
    int status = 0;
    for (int i=n; i--; )
      if (x[i].zero()) {
        if (status & VS_ONE)
          return ES_OK;
        x[i] = x[--n];
        status |= VS_ZERO;
      } else if (x[i].one()) {
        if (status & VS_ZERO)
          return ES_OK;
        x[i] = x[--n];
        status |= VS_ONE;
      }
    
    assert(status != (VS_ZERO | VS_ONE));
    if (n == 0) {
      assert(status != 0);
      GECODE_ME_CHECK(y.lq(home,1));
      return ES_OK;
    }
    x.size(n);

    (void) new (home) GqBool<VY>(home,status,x,y);
    return ES_OK;
  }

  template<class VY>
  ExecStatus
  GqBool<VY>::propagate(Space& home, const ModEventDelta&) {
    if (status == (VS_ZERO | VS_ONE))
      return home.ES_SUBSUMED(*this);

    if (c.empty()) {
      assert(status != 0);
      GECODE_ME_CHECK(y.lq(home,1));
      return home.ES_SUBSUMED(*this);
    }

    if (y.max() <= 1)
      return home.ES_SUBSUMED(*this);

    if (y.min() == 2) {
      Advisors<ViewAdvisor<BoolView> > as(c);
      assert(as());
      ViewAdvisor<BoolView>& a(as.advisor());
      ++as;
      if (!as()) {
        // Only a single view is left
        if (status == VS_ZERO) {
          GECODE_ME_CHECK(a.view().one(home));
        } else if (status == VS_ONE) {
          GECODE_ME_CHECK(a.view().zero(home));
        } else {
          return ES_FAILED;
        }
        return home.ES_SUBSUMED(*this);
      }
    }

    return ES_FIX;
  }

}}}

// STATISTICS: int-prop
