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
  Subset<View0,View1>::Subset(Home home, View0 y0, View1 y1)
    : MixBinaryPropagator<View0,PC_SET_CGLB,
                            View1,PC_SET_CLUB>(home,y0,y1) {}

  template<class View0, class View1>
  forceinline
  Subset<View0,View1>::Subset(Space& home, bool share, Subset& p)
    : MixBinaryPropagator<View0,PC_SET_CGLB,
                            View1,PC_SET_CLUB>(home,share,p) {}

  template<class View0, class View1>
  ExecStatus Subset<View0,View1>::post(Home home, View0 x, View1 y) {
    (void) new (home) Subset(home,x,y);
    return ES_OK;
  }

  template<class View0, class View1>
  Actor*
  Subset<View0,View1>::copy(Space& home, bool share) {
    return new (home) Subset(home,share,*this);
  }

  template<class View0, class View1>
  ExecStatus
  Subset<View0,View1>::propagate(Space& home, const ModEventDelta&) {
    bool oneassigned = x0.assigned() || x1.assigned();
    unsigned int x0glbsize;
    do {
      GlbRanges<View0> x0lb(x0);
      GECODE_ME_CHECK ( x1.includeI(home,x0lb) );
      GECODE_ME_CHECK ( x1.cardMin(home,x0.cardMin()) );
      LubRanges<View1> x1ub(x1);
      x0glbsize = x0.glbSize();
      GECODE_ME_CHECK ( x0.intersectI(home,x1ub) );
      GECODE_ME_CHECK ( x0.cardMax(home,x1.cardMax()) );
    } while (x0.glbSize() > x0glbsize);

    if (x0.cardMin() == x1.cardMax())
      GECODE_REWRITE(*this,(Eq<View0,View1>::post(home(*this),x0,x1)));

    if (shared(x0,x1)) {
      return oneassigned ? home.ES_SUBSUMED(*this) : ES_NOFIX;
    }
    return (x0.assigned() || x1.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

}}}

// STATISTICS: set-prop
