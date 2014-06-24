/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
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

namespace Gecode { namespace Set { namespace Element {

  template<class View, class View0, class View1>
  forceinline
  ElementIntersection<View,View0,View1>::
  ElementIntersection(Home home, IdxViewArray& iv0, View0 y0, View1 y1,
                      const IntSet& theUniverse)
    : Propagator(home), universe(theUniverse), iv(iv0), x0(y0), x1(y1) {
    home.notice(*this,AP_DISPOSE);
    x0.subscribe(home,*this, PC_SET_ANY);
    x1.subscribe(home,*this, PC_SET_ANY);
    iv.subscribe(home,*this, PC_SET_ANY);
  }

  template<class View, class View0, class View1>
  forceinline
  ElementIntersection<View,View0,View1>::
  ElementIntersection(Space& home, bool share,
                      ElementIntersection<View,View0,View1>& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    iv.update(home,share,p.iv);
    universe.update(home,share,p.universe);
  }

  template<class View, class View0, class View1>
  PropCost
  ElementIntersection<View,View0,View1>::cost(const Space&,
                                              const ModEventDelta&) const {
    return PropCost::linear(PropCost::HI, iv.size()+2);
  }

  template<class View, class View0, class View1>
  forceinline size_t
  ElementIntersection<View,View0,View1>::dispose(Space& home) {
    home.ignore(*this,AP_DISPOSE);
    if (!home.failed()) {
      x0.cancel(home,*this, PC_SET_ANY);
      x1.cancel(home,*this, PC_SET_ANY);
      iv.cancel(home,*this,PC_SET_ANY);
    }
    universe.~IntSet();
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View, class View0, class View1>
  ExecStatus
  ElementIntersection<View,View0,View1>::
  post(Home home, IdxViewArray& xs, View0 x0, View1 x1,
       const IntSet& universe) {
    int n = xs.size();

    // x0 \subseteq {1,...,n}
    Iter::Ranges::Singleton s(0, n-1);
    GECODE_ME_CHECK(x0.intersectI(home,s));
    (void) new (home)
      ElementIntersection<View,View0,View1>(home,xs,x0,x1,universe);
    return ES_OK;
  }

  template<class View, class View0, class View1>
  Actor*
  ElementIntersection<View,View0,View1>::copy(Space& home, bool share) {
    return new (home) ElementIntersection<View,View0,View1>(home,share,*this);
  }

  template<class View, class View0, class View1>
  ExecStatus
  ElementIntersection<View,View0,View1>::propagate(Space& home,
                                                   const ModEventDelta&) {
    Region r(home);
    int n = iv.size();

    bool loopVar;
    do {
      loopVar = false;

      // Cache the upper bound iterator, as we have to
      // modify the upper bound while iterating
      LubRanges<View0> x0ub(x0);
      Iter::Ranges::Cache x0ubc(r,x0ub);
      Iter::Ranges::ToValues<Iter::Ranges::Cache> vx0ub(x0ubc);

      GlbRanges<View0> x0lb(x0);
      Iter::Ranges::Cache x0lbc(r,x0lb);
      Iter::Ranges::ToValues<Iter::Ranges::Cache> vx0(x0lbc);

      // In the first iteration, compute in before[i] the intersection
      // of all the lower bounds of the x_i. At the same time,
      // exclude inconsistent x_i from x0 and remove them from
      // the list, cancel their dependencies.

      LUBndSet sofarBefore(home,universe);
      LUBndSet* before = r.alloc<LUBndSet>(n);

      int j = 0;
      int i = 0;
      while ( vx0ub() ) {

        // Remove vars at indices not in the upper bound
        if (iv[i].idx < vx0ub.val()) {
          iv[i].view.cancel(home,*this, PC_SET_ANY);
          ++i;
          continue;
        }
        assert(iv[i].idx == vx0ub.val());
        iv[j] = iv[i];

        View candidate = iv[j].view;
        int candidateInd = iv[j].idx;

        // inter = glb(x1) & complement(lub(candidate))
        GlbRanges<View1> x1lb(x1);
        LubRanges<View> candub(candidate);
        Iter::Ranges::Diff<GlbRanges<View1>,LubRanges<View> >
          inter(x1lb, candub);

        // exclude inconsistent x_i
        // an x_i is inconsistent if
        //  * its max cardinality is less than minCard of x1
        //  * inter is not empty (there are elements in x_0
        //    that can't be in x_i)
        if (candidate.cardMax() < x1.cardMin() ||
            inter()) {
          ModEvent me = (x0.exclude(home,candidateInd));
          loopVar |= me_modified(me);
          GECODE_ME_CHECK(me);

          iv[j].view.cancel(home,*this, PC_SET_ANY);
          ++i;
          ++vx0ub;
          continue;
        } else {
          // if x_i is consistent, check whether we know
          // that its index is in x0
          if (vx0() && vx0.val()==candidateInd) {
            // x1 <= candidate, candidate >= x1
            GlbRanges<View1> x1lb(x1);
            ModEvent me = candidate.includeI(home,x1lb);
            loopVar |= me_modified(me);
            GECODE_ME_CHECK(me);

            LubRanges<View> candub(candidate);
            me = x1.intersectI(home,candub);
            loopVar |= me_modified(me);
            GECODE_ME_CHECK(me);
            ++vx0;
          }
          new (&before[j]) LUBndSet(home);
          before[j].update(home,sofarBefore);
          GlbRanges<View> clb(candidate);
          sofarBefore.intersectI(home,clb);
        }

        ++vx0ub;
        ++i; ++j;
      }

      // cancel the variables with index greater than
      // max of lub(x0)
      for (int k=i; k<n; k++) {
        iv[k].view.cancel(home,*this, PC_SET_ANY);
      }
      n = j;
      iv.size(n);

      if (x0.cardMax()==0) {
        // Elementor is empty, hence the result must be universe
        {
          IntSetRanges uniI(universe);
          GECODE_ME_CHECK(x1.includeI(home,uniI));
        }
        {
          IntSetRanges uniI(universe);
          GECODE_ME_CHECK(x1.intersectI(home,uniI));
        }
        for (int i=n; i--;)
          before[i].dispose(home);
        return home.ES_SUBSUMED(*this);
      }

      {
        // x1 >= sofarBefore
        BndSetRanges sfB(sofarBefore);
        ModEvent me = x1.includeI(home,sfB);
        loopVar |= me_modified(me);
        GECODE_ME_CHECK(me);
      }

      sofarBefore.dispose(home);

      LUBndSet sofarAfter(home, universe);

      // In the second iteration, this time backwards, compute
      // sofarAfter as the intersection of all glb(x_j) with j>i
      for (int i=n; i--;) {
        if (sofarAfter.size() == 0) break;

        // extra = inter(before[i], sofarAfter) - lub(x1)
        BndSetRanges b(before[i]);
        BndSetRanges s(sofarAfter);
        LubRanges<View1> x1ub(x1);
        Iter::Ranges::Inter<BndSetRanges, BndSetRanges> inter(b,s);
        Iter::Ranges::Diff<Iter::Ranges::Inter<BndSetRanges,
          BndSetRanges>, LubRanges<View1> > diff(inter, x1ub);
        if (diff()) {
          ModEvent me = (x0.include(home,iv[i].idx));
          loopVar |= me_modified(me);
          GECODE_ME_CHECK(me);

          // candidate != extra
          me = iv[i].view.excludeI(home,diff);
          loopVar |= me_modified(me);
          GECODE_ME_CHECK(me);
        }

        GlbRanges<View> ivilb(iv[i].view);
        sofarAfter.intersectI(home,ivilb);
        before[i].dispose(home);
      }
      sofarAfter.dispose(home);

    } while (loopVar);

    // Test whether we determined x0 without determining x1
    if (x0.assigned() && !x1.assigned()) {
      int ubsize = static_cast<int>(x0.lubSize());
      if (ubsize > 2) {
        assert(ubsize==n);
        ViewArray<View> is(home,ubsize);
        for (int i=n; i--;)
          is[i]=iv[i].view;
        GECODE_REWRITE(*this,(RelOp::IntersectionN<View, View1>
                        ::post(home(*this),is,x1)));
      } else if (ubsize == 2) {
        assert(n==2);
        View a = iv[0].view;
        View b = iv[1].view;
        GECODE_REWRITE(*this,(RelOp::Intersection<View,View,View1>
                       ::post(home(*this),a,b,x1)));
      } else if (ubsize == 1) {
        assert(n==1);
        GECODE_REWRITE(*this,
          (Rel::Eq<View1,View>::post(home(*this),x1,iv[0].view)));
      } else {
        GECODE_ME_CHECK(x1.cardMax(home, 0));
        return home.ES_SUBSUMED(*this);
      }
    }

    bool allAssigned = true;
    for (int i=iv.size(); i--;) {
      if (!iv[i].view.assigned()) {
        allAssigned = false;
        break;
      }
    }
    if (x1.assigned() && x0.assigned() && allAssigned) {
      return home.ES_SUBSUMED(*this);
    }

    return ES_FIX;
  }

}}}

// STATISTICS: set-prop
