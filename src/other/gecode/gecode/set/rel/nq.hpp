/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
 *     Gabor Szokoli, 2004
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

namespace Gecode { namespace Set { namespace Rel {

  template<class View0, class View1>
  forceinline
  Distinct<View0,View1>::Distinct(Home home, View0 x, View1 y)
    : MixBinaryPropagator<View0, PC_SET_VAL, View1, PC_SET_VAL>(home,x,y) {}

  template<class View0, class View1>
  forceinline
  Distinct<View0,View1>::Distinct(Space& home, bool share, Distinct& p)
    : MixBinaryPropagator<View0, PC_SET_VAL, View1, PC_SET_VAL>
        (home,share,p) {}

  template<class View0, class View1>
  ExecStatus
  Distinct<View0,View1>::post(Home home, View0 x, View1 y) {
    if (x.assigned()) {
      GlbRanges<View0> xr(x);
      IntSet xs(xr);
      ConstSetView cv(home, xs);
      GECODE_ES_CHECK((DistinctDoit<View1>::post(home,y,cv)));
    }
    if (y.assigned()) {
      GlbRanges<View1> yr(y);
      IntSet ys(yr);
      ConstSetView cv(home, ys);
      GECODE_ES_CHECK((DistinctDoit<View0>::post(home,x,cv)));
    }
    (void) new (home) Distinct<View0,View1>(home,x,y);
    return ES_OK;
  }

  template<class View0, class View1>
  Actor*
  Distinct<View0,View1>::copy(Space& home, bool share) {
    return new (home) Distinct<View0,View1>(home,share,*this);
  }

  template<class View0, class View1>
  ExecStatus
  Distinct<View0,View1>::propagate(Space& home, const ModEventDelta&) {
    assert(x0.assigned()||x1.assigned());
    if (x0.assigned()) {
      GlbRanges<View0> xr(x0);
      IntSet xs(xr);
      ConstSetView cv(home, xs);
      GECODE_REWRITE(*this,(DistinctDoit<View1>::post(home(*this),x1,cv)));
    } else {
      GlbRanges<View1> yr(x1);
      IntSet ys(yr);
      ConstSetView cv(home, ys);
      GECODE_REWRITE(*this,(DistinctDoit<View0>::post(home(*this),x0,cv)));
    }
  }

  template<class View0>
  ExecStatus
  DistinctDoit<View0>::post(Home home, View0 x, ConstSetView y) {
    (void) new (home) DistinctDoit<View0>(home,x,y);
    return ES_OK;
  }

  template<class View0>
  Actor*
  DistinctDoit<View0>::copy(Space& home, bool share) {
    return new (home) DistinctDoit<View0>(home,share,*this);
  }

  template<class View0>
  ExecStatus
  DistinctDoit<View0>::propagate(Space& home, const ModEventDelta&) {
    if (x0.assigned()) {
      GlbRanges<View0> xi(x0);
      GlbRanges<ConstSetView> yi(y);
      if (Iter::Ranges::equal(xi,yi)) { return ES_FAILED; }
      else { return home.ES_SUBSUMED(*this); }
    }
    assert(x0.lubSize()-x0.glbSize() >0);
    if (x0.cardMin()>y.cardMax()) { return home.ES_SUBSUMED(*this); }
    if (x0.cardMax()<y.cardMin()) { return home.ES_SUBSUMED(*this); }
    //These tests are too expensive, we should only do them
    //in the 1 unknown left case.
    GlbRanges<View0> xi1(x0);
    LubRanges<ConstSetView> yi1(y);
    if (!Iter::Ranges::subset(xi1,yi1)){ return home.ES_SUBSUMED(*this); }
    LubRanges<View0> xi2(x0);
    GlbRanges<ConstSetView> yi2(y);
    if (!Iter::Ranges::subset(yi2,xi2)){ return home.ES_SUBSUMED(*this); }
    // from here, we know y\subseteq lub(x) and glb(x)\subseteq y

    if (x0.lubSize() == y.cardMin() && x0.lubSize() > 0) {
      GECODE_ME_CHECK(x0.cardMax(home, x0.lubSize() - 1));
      return home.ES_SUBSUMED(*this);
    }
    if (x0.glbSize() == y.cardMin()) {
      GECODE_ME_CHECK(x0.cardMin(home, x0.glbSize() + 1));
      return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }

  template<class View0>
  forceinline
  DistinctDoit<View0>::DistinctDoit(Home home, View0 _x, ConstSetView _y)
    : UnaryPropagator<View0, PC_SET_ANY>(home,_x), y(_y)  {}

  template<class View0>
  forceinline
  DistinctDoit<View0>::DistinctDoit(Space& home, bool share,
                                    DistinctDoit<View0>& p)
    : UnaryPropagator<View0, PC_SET_ANY>(home,share,p) {
    y.update(home,share,p.y);
  }

}}}

// STATISTICS: set-prop
