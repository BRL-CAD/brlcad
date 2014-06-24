/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004,2006,2007
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

  template<class SView, class RView>
  forceinline
  ElementUnionConst<SView,RView>::
  ElementUnionConst(Home home, SView y0,
                    const IntSetArgs& iv0,
                    RView y1)
    : Propagator(home), x0(y0), n_iv(iv0.size()), x1(y1) {
    home.notice(*this,AP_DISPOSE);
    x0.subscribe(home,*this, PC_SET_ANY);
    x1.subscribe(home,*this, PC_SET_ANY);
    iv=static_cast<Space&>(home).alloc<IntSet>(n_iv);
    for (unsigned int i=iv0.size(); i--;)
      iv[i]=iv0[i];
  }

  template<class SView, class RView>
  forceinline
  ElementUnionConst<SView,RView>::
  ElementUnionConst(Space& home, bool share,
                     ElementUnionConst<SView,RView>& p)
    : Propagator(home,share,p), n_iv(p.n_iv) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    iv=home.alloc<IntSet>(n_iv);
    for (unsigned int i=n_iv; i--;)
      iv[i].update(home,share,p.iv[i]);
  }

  template<class SView, class RView>
  PropCost
  ElementUnionConst<SView,RView>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::HI, n_iv+2);
  }

  template<class SView, class RView>
  forceinline size_t
  ElementUnionConst<SView,RView>::dispose(Space& home) {
    home.ignore(*this,AP_DISPOSE);
    if (!home.failed()) {
      x0.cancel(home,*this, PC_SET_ANY);
      x1.cancel(home,*this, PC_SET_ANY);
    }
    for (unsigned int i=n_iv; i--;)
      iv[i].~IntSet();
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class SView, class RView>
  ExecStatus
  ElementUnionConst<SView,RView>::
  post(Home home, SView x0, const IntSetArgs& xs,
       RView x1) {
    int n = xs.size();

    // s2 \subseteq {1,...,n}
    Iter::Ranges::Singleton s(0, n-1);
    GECODE_ME_CHECK(x1.intersectI(home,s));
    (void) new (home)
      ElementUnionConst<SView,RView>(home,x0,xs,x1);
    return ES_OK;
  }

  template<class SView, class RView>
  Actor*
  ElementUnionConst<SView,RView>::copy(Space& home, bool share) {
    return new (home) ElementUnionConst<SView,RView>(home,share,*this);
  }

  template<class SView, class RView>
  ExecStatus
  ElementUnionConst<SView,RView>::propagate(Space& home, const ModEventDelta&) {
    Region r(home);

    bool* stillSelected = r.alloc<bool>(n_iv);

    bool loopVar;
    do {
      loopVar = false;
      for (int i=n_iv; i--;)
        stillSelected[i] = false;

      // Cache the upper bound iterator, as we have to
      // modify the upper bound while iterating
      LubRanges<RView> x1ub(x1);
      Iter::Ranges::Cache x1ubc(r,x1ub);
      Iter::Ranges::ToValues<Iter::Ranges::Cache>
        vx1ub(x1ubc);

      GlbRanges<RView> x1lb(x1);
      Iter::Ranges::Cache x1lbc(r,x1lb);
      Iter::Ranges::ToValues<Iter::Ranges::Cache>
        vx1(x1lbc);

      // In the first iteration, compute in before[i] the union
      // of all the upper bounds of the x_i. At the same time,
      // exclude inconsistent x_i from x1.

      GLBndSet sofarBefore(home);
      LUBndSet selectedInter(home, IntSet (Limits::min,
                                   Limits::max));
      GLBndSet* before =
        static_cast<GLBndSet*>(r.ralloc(sizeof(GLBndSet)*n_iv));

      unsigned int maxCard = 0;
      unsigned int minCard = Limits::card;

      while (vx1ub()) {
        int i = vx1ub.val();

        IntSetRanges candCardR(iv[i]);
        unsigned int candidateCard = Iter::Ranges::size(candCardR);

        IntSetRanges candlb(iv[i]);
        LubRanges<SView> x0ub(x0);
        Iter::Ranges::Diff<IntSetRanges,
          LubRanges<SView> > diff(candlb, x0ub);

        bool selectSingleInconsistent = false;
        if (x1.cardMax() <= 1) {
          GlbRanges<SView> x0lb(x0);
          IntSetRanges candub(iv[i]);
          Iter::Ranges::Diff<GlbRanges<SView>,
                             IntSetRanges > diff2(x0lb, candub);
          selectSingleInconsistent = diff2() || candidateCard < x0.cardMin();
        }

        // exclude inconsistent x_i
        // an x_i is inconsistent if
        //  * at most one x_i can be selected and there are
        //    elements in x_0 that can't be in x_i
        //    (selectSingleInconsistent)
        //  * its min cardinality is greater than maxCard of x0
        //  * inter is not empty (there are elements in x_i
        //    that can't be in x_0)
        if (selectSingleInconsistent ||
            candidateCard > x0.cardMax() ||
            diff()) {
          ModEvent me = (x1.exclude(home,i));
          loopVar |= me_modified(me);
          GECODE_ME_CHECK(me);
        } else {
          stillSelected[i] = true;
          // if x_i is consistent, check whether we know
          // that its index is in x1
          if (vx1() && vx1.val()==i) {
            // x0 >= candidate, candidate <= x0
            // GlbRanges<SView> candlb(candidate);
            IntSetRanges candlb(iv[i]);
            ModEvent me = x0.includeI(home,candlb);
            loopVar |= me_modified(me);
            GECODE_ME_CHECK(me);
            ++vx1;
          }
          new (&before[i]) GLBndSet(home);
          before[i].update(home,sofarBefore);
          IntSetRanges cub(iv[i]);
          sofarBefore.includeI(home,cub);
          IntSetRanges clb(iv[i]);
          selectedInter.intersectI(home,clb);
          maxCard = std::max(maxCard, candidateCard);
          minCard = std::min(minCard, candidateCard);
        }

        ++vx1ub;
      }

      if (x1.cardMax()==0) {
        // Selector is empty, hence the result must be empty
        {
          GECODE_ME_CHECK(x0.cardMax(home,0));
        }
        for (int i=n_iv; i--;)
          if (stillSelected[i])
            before[i].dispose(home);
        selectedInter.dispose(home);
        sofarBefore.dispose(home);
        return home.ES_SUBSUMED(*this);
      }

      if (x1.cardMin() > 0) {
        // Selector is not empty, hence the intersection of the
        // possibly selected lower bounds is contained in x0
        BndSetRanges si(selectedInter);
        ModEvent me = x0.includeI(home, si);
        loopVar |= me_modified(me);
        GECODE_ME_CHECK(me);
        me = x0.cardMin(home, minCard);
        loopVar |= me_modified(me);
        GECODE_ME_CHECK(me);
      }
      selectedInter.dispose(home);

      if (x1.cardMax() <= 1) {
        ModEvent me = x0.cardMax(home, maxCard);
        loopVar |= me_modified(me);
        GECODE_ME_CHECK(me);
      }

      {
        // x0 <= sofarBefore
        BndSetRanges sfB(sofarBefore);
        ModEvent me = x0.intersectI(home,sfB);
        loopVar |= me_modified(me);
        GECODE_ME_CHECK(me);
      }

      sofarBefore.dispose(home);

      GLBndSet sofarAfter(home);

      // In the second iteration, this time backwards, compute
      // sofarAfter as the union of all lub(x_j) with j>i
      for (int i=n_iv; i--;) {
        if (!stillSelected[i])
          continue;
        BndSetRanges b(before[i]);
        BndSetRanges s(sofarAfter);
        GlbRanges<SView> x0lb(x0);
        Iter::Ranges::Union<BndSetRanges, BndSetRanges> inter(b,s);
        Iter::Ranges::Diff<GlbRanges<SView>,
          Iter::Ranges::Union<BndSetRanges,BndSetRanges> > diff(x0lb, inter);
        if (diff()) {
          ModEvent me = (x1.include(home,i));
          loopVar |= me_modified(me);
          GECODE_ME_CHECK(me);

          // candidate != extra
          IntSetRanges ivi(iv[i]);
          if (!Iter::Ranges::subset(diff, ivi))
            GECODE_ME_CHECK(ME_SET_FAILED);
        }

        IntSetRanges iviub(iv[i]);
        sofarAfter.includeI(home,iviub);
        before[i].dispose(home);
      }
      sofarAfter.dispose(home);

    } while (loopVar);

    if (x1.assigned()) {
      assert(x0.assigned());
      return home.ES_SUBSUMED(*this);
    }

    return ES_FIX;
  }

}}}

// STATISTICS: set-prop
