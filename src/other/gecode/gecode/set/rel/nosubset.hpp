/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
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

  /*
   * "No Subset" propagator
   *
   */

  template<class View0, class View1>
  forceinline
  NoSubset<View0,View1>::NoSubset(Home home, View0 y0, View1 y1)
    : MixBinaryPropagator<View0,PC_SET_CLUB,
                            View1,PC_SET_CGLB>(home,y0,y1) {}

  template<class View0, class View1>
  forceinline
  NoSubset<View0,View1>::NoSubset(Space& home, bool share,
                                  NoSubset<View0,View1>& p)
    : MixBinaryPropagator<View0,PC_SET_CLUB,
                            View1,PC_SET_CGLB>(home,share,p) {}

  template<class View0, class View1>
  ExecStatus
  NoSubset<View0,View1>::post(Home home, View0 x, View1 y) {
    if (me_failed(x.cardMin(home,1)))
      return ES_FAILED;
    (void) new (home) NoSubset<View0,View1>(home,x,y);
    return ES_OK;
  }

  template<class View0, class View1>
  Actor*
  NoSubset<View0,View1>::copy(Space& home, bool share) {
    return new (home) NoSubset<View0,View1>(home,share,*this);
  }

  template<class View0, class View1>
  ExecStatus
  NoSubset<View0,View1>::propagate(Space& home, const ModEventDelta&) {
    GlbRanges<View0> x0lb(x0);
    LubRanges<View1> x1ub(x1);
    if (!Iter::Ranges::subset(x0lb, x1ub))
      return home.ES_SUBSUMED(*this);
    if (x0.cardMin()>x1.cardMax()) { return home.ES_SUBSUMED(*this); }
    LubRanges<View0> x0ub(x0);
    GlbRanges<View1> x1lb(x1);
    Iter::Ranges::Diff<LubRanges<View0>,GlbRanges<View1> >
      breakers(x0ub,x1lb);
    if (!breakers()) { return ES_FAILED; }
    if (breakers.min() == breakers.max()) {
      int b1 = breakers.min();
      ++breakers;
      if (breakers()) { return ES_FIX; }
      //Only one subsetness-breaker element left:
      GECODE_ME_CHECK( x0.include(home,b1) );
      GECODE_ME_CHECK( x1.exclude(home,b1) );
      return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }

}}}

// STATISTICS: set-prop
