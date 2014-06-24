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

#include <gecode/int/channel.hh>

namespace Gecode { namespace Int { namespace Channel {

  /// Iterates the values to be removed as defined by an array of Boolean views
  class BoolIter {
  private:
    /// The array of Boolean views
    const ViewArray<BoolView>& x;
    /// The offset
    const int o;
    /// Current position in the array
    int i;
  public:
    /// Initialize iterator
    BoolIter(const ViewArray<BoolView>& x0, int o0);
    /// Test whether further values available
    bool operator ()(void) const;
    /// Return value
    int val(void) const;
    /// Move to the next value
    void operator ++(void);
  };

  forceinline
  BoolIter::BoolIter(const ViewArray<BoolView>& x0, int o0) :
    x(x0), o(o0), i(0) {
    while ((i<x.size()) && !x[i].zero())
      i++;
  }
  forceinline bool
  BoolIter::operator ()(void) const {
    return i<x.size();
  }
  forceinline int
  BoolIter::val(void) const {
    assert(x[i].zero());
    return i+o;
  }
  forceinline void
  BoolIter::operator ++(void) {
    do {
      i++;
    } while ((i<x.size()) && !x[i].zero());
  }


  forceinline
  LinkMulti::LinkMulti(Space& home, bool share, LinkMulti& p)
    : MixNaryOnePropagator<BoolView,PC_BOOL_NONE,IntView,PC_INT_DOM>
  (home,share,p), status(S_NONE), o(p.o) {
    assert(p.status == S_NONE);
    c.update(home,share,p.c);
  }

  Actor*
  LinkMulti::copy(Space& home, bool share) {
    return new (home) LinkMulti(home,share,*this);
  }

  forceinline size_t
  LinkMulti::dispose(Space& home) {
    Advisors<Advisor> as(c);
    x.cancel(home,as.advisor());
    c.dispose(home);
    (void) MixNaryOnePropagator<BoolView,PC_BOOL_NONE,IntView,PC_INT_DOM>
      ::dispose(home);
    return sizeof(*this);
  }

  PropCost
  LinkMulti::cost(const Space&, const ModEventDelta& med) const {
    if ((status == S_ONE) || (IntView::me(med) == ME_INT_VAL))
      return PropCost::unary(PropCost::LO);
    else
      return PropCost::linear(PropCost::LO, x.size());
  }

  ExecStatus
  LinkMulti::advise(Space&, Advisor&, const Delta& d) {
    if (status == S_RUN)
      return ES_FIX;
    // Detect a one
    if (BoolView::one(d))
      status = S_ONE;
    return ES_NOFIX;
  }

  ExecStatus
  LinkMulti::propagate(Space& home, const ModEventDelta& med) {
    int n = x.size();

    // Always maintain the invariant that y lies inside the x-array
    assert((y.min()-o >= 0) && (y.max()-o < n));

    if (y.assigned()) {
      status = S_RUN;
      int j=y.val()-o;
      GECODE_ME_CHECK(x[j].one(home));
      for (int i=0; i<j; i++)
        GECODE_ME_CHECK(x[i].zero(home));
      for (int i=j+1; i<n; i++)
        GECODE_ME_CHECK(x[i].zero(home));
      return home.ES_SUBSUMED(*this);
    }

    // Check whether there is a one
    if (status == S_ONE) {
      status = S_RUN;
      for (int i=0; true; i++)
        if (x[i].one()) {
          for (int j=0; j<i; j++)
            GECODE_ME_CHECK(x[j].zero(home));
          for (int j=i+1; j<n; j++)
            GECODE_ME_CHECK(x[j].zero(home));
          GECODE_ME_CHECK(y.eq(home,i+o));
          return home.ES_SUBSUMED(*this);
        }
      GECODE_NEVER;
    }

    status = S_RUN;

  redo:

    // Assign all Boolean views to zero that are outside bounds
    {
      int min=y.min()-o;
      for (int i=0; i<min; i++)
        GECODE_ME_CHECK(x[i].zero(home));
      x.drop_fst(min); o += min; n = x.size();

      int max=y.max()-o;
      for (int i=max+1; i<n; i++)
        GECODE_ME_CHECK(x[i].zero(home));
      x.drop_lst(max); n = x.size();
    }

    {
      // Remove zeros on the left
      int i=0;
      while ((i < n) && x[i].zero()) i++;
      if (i >= n)
        return ES_FAILED;
      x.drop_fst(i); o += i; n = x.size();
    }

    {
      // Remove zeros on the right
      int i=n-1;
      while ((i >= 0) && x[i].zero()) i--;
      assert(i >= 0);
      x.drop_lst(i); n = x.size();
    }

    assert(n >= 1);

    // Is there only one left?
    if (n == 1) {
      GECODE_ME_CHECK(x[0].one(home));
      GECODE_ME_CHECK(y.eq(home,o));
      return home.ES_SUBSUMED(*this);
    }

    // Update bounds
    GECODE_ME_CHECK(y.gq(home,o));
    GECODE_ME_CHECK(y.lq(home,o+n-1));
    if ((y.min() > o) || (y.max() < o+n-1))
      goto redo;

    assert((n >= 2) && x[0].none() && x[n-1].none());
    assert((y.min()-o == 0) && (y.max()-o == n-1));

    // Propagate from y to Boolean views
    if ((IntView::me(med) == ME_INT_BND) || (IntView::me(med) == ME_INT_DOM)) {
      ViewValues<IntView> v(y);
      int i=0;
      do {
        int j = v.val()-o;
        if (i < j) {
          GECODE_ME_CHECK(x[i].zero(home));
          ++i;
        } else if (i > j) {
          ++v;
        } else {
          assert(i == j);
          ++i; ++v;
        }
      } while (v() && (i < n));
    }

    // Propagate from Boolean views to y
    if (BoolView::me(med) == ME_BOOL_VAL) {
      BoolIter bv(x,o);
      GECODE_ME_CHECK(y.minus_v(home,bv,false));
    }
    status = S_NONE;
    return ES_FIX;
  }

}}}

// STATISTICS: int-prop

