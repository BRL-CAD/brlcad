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

  /*
   * "Ternary union" propagator
   *
   */

  template<class View0, class View1, class View2>
  forceinline
  Union<View0,View1,View2>::Union(Home home, View0 y0,View1 y1,View2 y2)
    : MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                             View2,PC_SET_ANY>(home,y0,y1,y2) {}

  template<class View0, class View1, class View2>
  forceinline
  Union<View0,View1,View2>::Union(Space& home, bool share,
                               Union<View0,View1,View2>& p)
    : MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                             View2,PC_SET_ANY>(home,share,p) {}

  template<class View0, class View1, class View2>
  ExecStatus Union<View0,View1,View2>::post(Home home, View0 x0,
                                         View1 x1, View2 x2) {
    (void) new (home) Union<View0,View1,View2>(home,x0,x1,x2);
    return ES_OK;
  }

  template<class View0, class View1, class View2>
  Actor*
  Union<View0,View1,View2>::copy(Space& home, bool share) {
    return new (home) Union(home,share,*this);
  }

  template<class View0, class View1, class View2>
  ExecStatus
  Union<View0,View1,View2>::propagate(Space& home, const ModEventDelta& med) {
    // This propagator implements the constraint
    // x2 = x0 u x1

    bool x0ass = x0.assigned();
    bool x1ass = x1.assigned();
    bool x2ass = x2.assigned();

    ModEvent me0 = View0::me(med);
    ModEvent me1 = View1::me(med);
    ModEvent me2 = View2::me(med);

    bool x0ubmod = false;
    bool x1ubmod = false;
    bool modified = false;

    do {

      modified = false;
      do {
        // 4) lub(x2) <= lub(x0) u lub(x1)
        {
          LubRanges<View0> x0ub(x0);
          LubRanges<View1> x1ub(x1);
          Iter::Ranges::Union<LubRanges<View0>, LubRanges<View1> > u2(x0ub,x1ub);
          GECODE_ME_CHECK_MODIFIED(modified, x2.intersectI(home,u2) );
        }

        if (modified || Rel::testSetEventUB(me2) )
          {
            modified = false;
            // 5) x0 <= x2
            LubRanges<View2> x2ub1(x2);
            GECODE_ME_CHECK_MODIFIED(modified, x0.intersectI(home,x2ub1) );
            x0ubmod |= modified;

            // 6) x1 <= x2
            bool modified2=false;
            LubRanges<View2> x2ub2(x2);
            GECODE_ME_CHECK_MODIFIED(modified2, x1.intersectI(home,x2ub2) );
            x1ubmod |= modified2;
            modified |= modified2;
          }

      } while (modified);

      modified = false;
      do {
        bool modifiedOld = modified;
        modified = false;

        if (Rel::testSetEventLB(me2) || Rel::testSetEventUB(me0)
             || x0ubmod || modifiedOld)
          {
            // 1) glb(x0) \ lub(x1) <= glb(x2)
            GlbRanges<View2> x2lb(x2);
            LubRanges<View0> x0ub(x0);
            Iter::Ranges::Diff<GlbRanges<View2>, LubRanges<View0> >
              diff(x2lb, x0ub);
            GECODE_ME_CHECK_MODIFIED(modified, x1.includeI(home,diff) );
          }

        if (Rel::testSetEventLB(me2) || Rel::testSetEventUB(me1)
            || x1ubmod || modifiedOld)
          {
            // 2) glb(x0) \ lub(x2) <= glb(x1)
            GlbRanges<View2> x2lb(x2);
            LubRanges<View1> x1ub(x1);
            Iter::Ranges::Diff<GlbRanges<View2>, LubRanges<View1> >
              diff(x2lb, x1ub);
            GECODE_ME_CHECK_MODIFIED(modified, x0.includeI(home,diff) );
          }

         if (Rel::testSetEventLB(me0,me1) || modified)
          {
            //            modified = false;
            // 3) glb(x1) u glb(x2) <= glb(x0)
            GlbRanges<View0> x0lb(x0);
            GlbRanges<View1> x1lb(x1);
            Iter::Ranges::Union<GlbRanges<View0>, GlbRanges<View1> >
              u1(x0lb,x1lb);
            GECODE_ME_CHECK_MODIFIED(modified, x2.includeI(home,u1) );
          }

      } while(modified);

      modified = false;
      {
        // cardinality
        ExecStatus ret = unionCard(home,modified, x0, x1, x2);
        GECODE_ES_CHECK(ret);
      }

      if (x2.cardMax() == 0) {
        GECODE_ME_CHECK( x0.cardMax(home, 0) );
        GECODE_ME_CHECK( x1.cardMax(home, 0) );
        return home.ES_SUBSUMED(*this);
      }

      if (x0.cardMax() == 0)
        GECODE_REWRITE(*this,(Rel::Eq<View1,View2>::post(home(*this),x1,x2)));
      if (x1.cardMax() == 0)
        GECODE_REWRITE(*this,(Rel::Eq<View0,View2>::post(home(*this),x0,x2)));

    } while(modified);

    if (shared(x0,x1,x2)) {
      return (x0ass && x1ass && x2ass) ? home.ES_SUBSUMED(*this) : ES_NOFIX;
    } else {
      if (x0ass && x1ass && x2ass)
        return home.ES_SUBSUMED(*this);
      if (x0ass != x0.assigned() ||
          x1ass != x1.assigned() ||
          x2ass != x2.assigned()) {
        return ES_NOFIX;
      } else {
         return ES_FIX;
      }
    }
  }


  /*
   * "Nary union" propagator
   *
   */

  template<class View0, class View1>
  forceinline
  UnionN<View0,View1>::UnionN(Home home, ViewArray<View0>& x, View1 y)
    : MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>(home,x,y) {
    shared = x.shared(home) || viewarrayshared(home,x,y);
  }

  template<class View0, class View1>
  forceinline
  UnionN<View0,View1>::UnionN(Home home, ViewArray<View0>& x,
                              const IntSet& z, View1 y)
    : MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>(home,x,y) {
    shared = x.shared(home) || viewarrayshared(home,x,y);
    IntSetRanges rz(z);
    unionOfDets.includeI(home, rz);
  }

  template<class View0, class View1>
  forceinline
  UnionN<View0,View1>::UnionN(Space& home, bool share, UnionN& p)
    : MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>(home,share,p),
      shared(p.shared) {
    unionOfDets.update(home,p.unionOfDets);
  }

  template<class View0, class View1>
  Actor*
  UnionN<View0,View1>::copy(Space& home, bool share) {
    return new (home) UnionN<View0,View1>(home,share,*this);
  }

  template<class View0, class View1>
  ExecStatus
  UnionN<View0,View1>::post(Home home, ViewArray<View0>& x, View1 y) {
    switch (x.size()) {
    case 0:
      GECODE_ME_CHECK(y.cardMax(home, 0));
      return ES_OK;
    case 1:
      return Rel::Eq<View0,View1>::post(home, x[0], y);
    case 2:
      return Union<View0,View0,View1>::post(home, x[0], x[1], y);
    default:
      (void) new (home) UnionN<View0,View1>(home,x,y);
      return ES_OK;
    }
  }

  template<class View0, class View1>
  ExecStatus
  UnionN<View0,View1>::post(Home home, ViewArray<View0>& x,
                            const IntSet& z, View1 y) {
    (void) new (home) UnionN<View0,View1>(home,x,z,y);
    return ES_OK;
  }

  template<class View0, class View1>
  PropCost
  UnionN<View0,View1>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::LO, x.size()+1);
  }

  template<class View0, class View1>
  ExecStatus
  UnionN<View0,View1>::propagate(Space& home, const ModEventDelta& med) {
    ModEvent me0 = View0::me(med);
    ModEvent me1 = View1::me(med);
    bool ubevent = Rel::testSetEventUB(me0, me1);
    bool lbevent = Rel::testSetEventLB(me0, me1);
    bool anybevent = Rel::testSetEventAnyB(me0, me1);
    bool cardevent = Rel::testSetEventCard(me0, me1);

    bool modified = false;
    bool oldModified = false;

    do {
      oldModified = modified;
      modified = false;
      if (modified || oldModified || ubevent)
        GECODE_ES_CHECK(unionNXiUB(home, modified, x, y,unionOfDets));
      if (modified || oldModified || ubevent)
        GECODE_ES_CHECK(partitionNYUB(home, modified, x, y,unionOfDets));
      if (modified || oldModified || anybevent)
        GECODE_ES_CHECK(partitionNXiLB(home, modified, x, y,unionOfDets));
      if (modified || oldModified || lbevent)
        GECODE_ES_CHECK(partitionNYLB(home, modified, x, y,unionOfDets));
      if (modified || oldModified || cardevent || ubevent)
        GECODE_ES_CHECK(unionNCard(home, modified, x, y, unionOfDets));
    } while (modified);

    for(int i=0;i<x.size();i++){
      //Do not reverse! Eats away the end of the array!
      while (i<x.size() && x[i].assigned()) {
        GlbRanges<View0> det(x[i]);
        unionOfDets.includeI(home,det);
        x.move_lst(i);
      }
    }
    // When we run out of variables, make a final check and disolve:
    if (x.size()==0) {
      BndSetRanges all1(unionOfDets);
      GECODE_ME_CHECK( y.intersectI(home,all1) );
      BndSetRanges all2(unionOfDets);
      GECODE_ME_CHECK( y.includeI(home,all2) );
      unionOfDets.dispose(home);
      return home.ES_SUBSUMED(*this);
    }

    return shared ? ES_NOFIX : ES_FIX;
  }

}}}

// STATISTICS: set-prop
