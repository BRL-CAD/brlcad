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

namespace Gecode { namespace Set { namespace RelOp {

  template<class View0, class View1, class View2>
  Actor*
  SubOfUnion<View0,View1,View2>::copy(Space& home, bool share) {
    return new (home) SubOfUnion(home,share,*this);
  }

  template<class View0, class View1, class View2>
  ExecStatus
  SubOfUnion<View0,View1,View2>::propagate(Space& home, const ModEventDelta& med) {

    bool allassigned = x0.assigned() && x1.assigned() && x2.assigned();

    ModEvent me0 = View0::me(med);
    ModEvent me1 = View1::me(med);
    ModEvent me2 = View2::me(med);

    bool modified=false;

    do {
      // lub(x2) <= lub(x0) u lub(x1)
      if (modified || Rel::testSetEventUB(me0,me1))
        {
          LubRanges<View0> ub0(x0);
          LubRanges<View1> ub1(x1);
          Iter::Ranges::Union<LubRanges<View0>, LubRanges<View1> > u(ub0,ub1);
          GECODE_ME_CHECK(x2.intersectI(home,u));
        }

      // x1 <= glb(x2)-lub(x0)
      // x0 <= glb(x2)-lub(x1)
      if (modified || Rel::testSetEventAnyB(me0,me1,me2)) {
        {
          modified = false;
          GlbRanges<View2> lb2(x2);
          LubRanges<View0> ub0(x0);
          Iter::Ranges::Diff<GlbRanges<View2>, LubRanges<View0> >
            diff(lb2,ub0);
          GECODE_ME_CHECK_MODIFIED(modified, x1.includeI(home,diff));
        }
        {
          GlbRanges<View2> lb2(x2);
          LubRanges<View1> ub1(x1);
          Iter::Ranges::Diff<GlbRanges<View2>, LubRanges<View1> >
            diff(lb2,ub1);
          GECODE_ME_CHECK_MODIFIED(modified, x0.includeI(home,diff));
        }
      } else {
        modified = false;
      }

      // cardinality propagation
      if (modified ||
          Rel::testSetEventCard(me0,me1,me2) ||
          Rel::testSetEventLB(me0,me1)
          ) {
        GlbRanges<View0> lb0c(x0);
        GlbRanges<View1> lb1c(x1);
        Iter::Ranges::Inter<GlbRanges<View0>, GlbRanges<View1> >
          inter(lb0c,lb1c);

        unsigned int m = Iter::Ranges::size(inter);

        if (m < x0.cardMax()+x1.cardMax()) {
          GECODE_ME_CHECK_MODIFIED(modified,
                            x2.cardMax( home,
                                        x0.cardMax()+x1.cardMax() - m ) );
        }
        if (m + x2.cardMin() > x1.cardMax()) {
          GECODE_ME_CHECK_MODIFIED(modified,
                            x0.cardMin( home,
                                        m+x2.cardMin()-x1.cardMax() )  );
        }
        if (m + x2.cardMin() > x0.cardMax()) {
          GECODE_ME_CHECK_MODIFIED(modified,
                            x1.cardMin( home,
                                        m+x2.cardMin()-x0.cardMax() )  );
        }
      }

    } while (modified);

    if (shared(x0,x1,x2)) {
      if (allassigned) {
        return home.ES_SUBSUMED(*this);
      } else {
        return ES_NOFIX;
      }
    } else {
      if (x0.assigned() + x1.assigned() + x2.assigned() >= 2) {
         return home.ES_SUBSUMED(*this);
      } else {
        return ES_FIX;
      }
    }

  }

  template<class View0, class View1, class View2>
  forceinline
  SubOfUnion<View0,View1,View2>::SubOfUnion(Home home, View0 y0,
                                         View1 y1, View2 y2)
    : MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                             View2,PC_SET_ANY>(home,y0,y1,y2) {}

  template<class View0, class View1, class View2>
  forceinline
  SubOfUnion<View0,View1,View2>::SubOfUnion
  (Space& home, bool share, SubOfUnion<View0,View1,View2>& p)
    : MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                             View2,PC_SET_ANY>(home,share,p) {}

  template<class View0, class View1, class View2>
  ExecStatus SubOfUnion<View0,View1,View2>::post
  (Home home, View0 x0, View1 x1, View2 x2) {
    (void) new (home) SubOfUnion<View0,View1,View2>(home,x0, x1, x2);
    return ES_OK;
  }

}}}

// STATISTICS: set-prop
