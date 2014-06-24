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
  forceinline
  SuperOfInter<View0,View1,View2>::SuperOfInter
  (Home home, View0 y0, View1 y1, View2 y2)
    : MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                             View2,PC_SET_CLUB>(home,y0,y1,y2) {}

  template<class View0, class View1, class View2>
  forceinline
  SuperOfInter<View0,View1,View2>::SuperOfInter
  (Space& home, bool share, SuperOfInter<View0,View1,View2>& p)
    : MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                             View2,PC_SET_CLUB>(home,share,p) {}

  template<class View0, class View1, class View2>
  ExecStatus
  SuperOfInter<View0,View1,View2>::post(Home home,
                                        View0 x0, View1 x1, View2 x2) {
    (void) new (home) SuperOfInter<View0,View1,View2>(home, x0, x1, x2);
    return ES_OK;
  }

  template<class View0, class View1, class View2>
  Actor*
  SuperOfInter<View0,View1,View2>::copy(Space& home, bool share) {
    return new (home) SuperOfInter(home,share,*this);
  }

  template<class View0, class View1, class View2>
  ExecStatus
  SuperOfInter<View0,View1,View2>::propagate(Space& home, const ModEventDelta& med) {

    bool allassigned = x0.assigned() && x1.assigned() && x2.assigned();

    ModEvent me0 = View0::me(med);
    ModEvent me1 = View1::me(med);
    ModEvent me2 = View2::me(med);

    bool modified = false;

    do {
      // glb(x2) >= glb(x0) ^ glb(x1)
      if ( modified || Rel::testSetEventLB(me0,me1)) {
        GlbRanges<View0> lb0(x0);
        GlbRanges<View1> lb1(x1);
        Iter::Ranges::Inter<GlbRanges<View0>,GlbRanges<View1> >
          is(lb0, lb1);

        GECODE_ME_CHECK_MODIFIED(modified,x2.includeI(home,is));
      }

      // lub(x0) -= glb(x1)-lub(x2)
      // lub(x1) -= glb(x0)-lub(x2)
      if ( modified || Rel::testSetEventAnyB(me0,me1,me2)) {
         modified = false;
        GlbRanges<View1> lb12(x1);
        LubRanges<View2> ub22(x2);
        Iter::Ranges::Diff<GlbRanges<View1>, LubRanges<View2> >
          diff1(lb12, ub22);

        GECODE_ME_CHECK_MODIFIED(modified, x0.excludeI(home,diff1));

        GlbRanges<View0> lb01(x0);
        LubRanges<View2> ub23(x2);
        Iter::Ranges::Diff<GlbRanges<View0>, LubRanges<View2> >
          diff2(lb01, ub23);

        GECODE_ME_CHECK_MODIFIED(modified, x1.excludeI(home,diff2));
      } else {
         modified = false;
      }

      // Cardinality propagation
      if ( modified ||
            Rel::testSetEventCard(me0,me1,me2) ||
           Rel::testSetEventUB(me0,me1)
           ) {

        LubRanges<View0> ub0(x0);
        LubRanges<View1> ub1(x1);
        Iter::Ranges::Union<LubRanges<View0>, LubRanges<View1> > u(ub0,ub1);

        unsigned int m = Iter::Ranges::size(u);

        if (m < x0.cardMin() + x1.cardMin()) {
          GECODE_ME_CHECK_MODIFIED(modified,
                            x2.cardMin( home,
                                        x0.cardMin()+x1.cardMin() - m ) );
        }
        if (m + x2.cardMax() > x1.cardMin()) {
          GECODE_ME_CHECK_MODIFIED(modified,
                            x0.cardMax( home,
                                        m+x2.cardMax()-x1.cardMin() )  );
        }
        if (m + x2.cardMax() > x0.cardMin()) {
          GECODE_ME_CHECK_MODIFIED(modified,
                            x1.cardMax( home,
                                        m+x2.cardMax()-x0.cardMin() )  );
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

}}}

// STATISTICS: set-prop
