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
   * "Ternary intersection" propagator
   *
   */

  template<class View0, class View1, class View2> ExecStatus
  Intersection<View0,View1,View2>::post(Home home,
                                        View0 x0,View1 x1,View2 x2) {
    (void) new (home) Intersection<View0,View1,View2>(home,x0,x1,x2);
    return ES_OK;
  }

  template<class View0, class View1, class View2>
  Actor*
  Intersection<View0,View1,View2>::copy(Space& home, bool share) {
    return new (home) Intersection(home,share,*this);
  }

  template<class View0, class View1, class View2>
  ExecStatus
  Intersection<View0,View1,View2>::propagate(Space& home, const ModEventDelta& med) {
    // This propagator implements the constraint
    // x2 = x0 \cap x1

    bool x0ass = x0.assigned();
    bool x1ass = x1.assigned();
    bool x2ass = x2.assigned();

    ModEvent me0 = View0::me(med);
    ModEvent me1 = View1::me(med);
    ModEvent me2 = View2::me(med);

    bool x0lbmod = false;
    bool x1lbmod = false;
    bool modified = false;

    do {

      modified = false;
      do {
        // 4) glb(x2) <= glb(x0) \cap glb(x1)
        {
          GlbRanges<View0> x0lb(x0);
          GlbRanges<View1> x1lb(x1);
          Iter::Ranges::Inter<GlbRanges<View0>, GlbRanges<View1> >
            i2(x0lb,x1lb);
          GECODE_ME_CHECK_MODIFIED(modified, x2.includeI(home,i2) );
        }

        if (modified || Rel::testSetEventLB(me2) )
          {
            modified = false;
            // 5) x2 <= x0
            GlbRanges<View2> x2lb1(x2);
            GECODE_ME_CHECK_MODIFIED(modified, x0.includeI(home,x2lb1) );
            x0lbmod |= modified;

            // 6) x2 <= x1
            bool modified2=false;
            GlbRanges<View2> x2lb2(x2);
            GECODE_ME_CHECK_MODIFIED(modified2, x1.includeI(home,x2lb2) );
            x1lbmod |= modified2;
            modified |= modified2;
          }

      } while (modified);

      modified = false;
      do {
        bool modifiedOld = modified;
        modified = false;

        if (Rel::testSetEventUB(me2) || Rel::testSetEventLB(me0)
             || x0lbmod || modifiedOld)
          {
            // 1) lub(x2) \ glb(x0) => lub(x1)
            GlbRanges<View0> x0lb(x0);
            LubRanges<View2> x2ub(x2);
            Iter::Ranges::Diff<GlbRanges<View0>,LubRanges<View2> >
              diff(x0lb, x2ub);
            GECODE_ME_CHECK_MODIFIED(modified, x1.excludeI(home,diff) );
          }

        if (Rel::testSetEventUB(me2) || Rel::testSetEventLB(me1)
            || x1lbmod || modifiedOld)
          {
            // 2) lub(x2) \ glb(x1) => lub(x0)
            GlbRanges<View1> x1lb(x1);
            LubRanges<View2> x2ub(x2);
            Iter::Ranges::Diff<GlbRanges<View1>, LubRanges<View2> >
              diff(x1lb, x2ub);
            GECODE_ME_CHECK_MODIFIED(modified, x0.excludeI(home,diff) );
          }

         if (Rel::testSetEventUB(me0,me1) || modified)
          {
            //            modified = false;
            // 3) lub(x0) \cap lub(x1) <= lub(x2)
            LubRanges<View0> x0ub(x0);
            LubRanges<View1> x1ub(x1);
            Iter::Ranges::Inter<LubRanges<View0>, LubRanges<View1> >
              i1(x0ub,x1ub);
            GECODE_ME_CHECK_MODIFIED(modified, x2.intersectI(home,i1) );
          }

      } while(modified);

      modified = false;
      {
        // cardinality
        ExecStatus ret = interCard(home,modified, x0, x1, x2);
        GECODE_ES_CHECK(ret);
      }

      if (x2.cardMin() == Set::Limits::card) {
        GECODE_ME_CHECK( x0.cardMin(home, Set::Limits::card) );
        GECODE_ME_CHECK( x1.cardMin(home, Set::Limits::card) );
        return home.ES_SUBSUMED(*this);
      }

      if (x0.cardMin() == Set::Limits::card)
        GECODE_REWRITE(*this,(Rel::Eq<View1,View2>::post(home(*this),x1,x2)));
      if (x1.cardMin() == Set::Limits::card)
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

  template<class View0, class View1, class View2>
  forceinline
  Intersection<View0,View1,View2>::Intersection(Home home,
                                             View0 y0,View1 y1,View2 y2)
    : MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                             View2,PC_SET_ANY>(home,y0,y1,y2) {}

  template<class View0, class View1, class View2>
  forceinline
  Intersection<View0,View1,View2>::Intersection(Space& home, bool share,
                                             Intersection<View0,View1,View2>& p)
    : MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                             View2,PC_SET_ANY>(home,share,p) {}

  /*
   * "Nary intersection" propagator
   *
   */

  template<class View0, class View1>
  forceinline
  IntersectionN<View0,View1>::IntersectionN(Home home, ViewArray<View0>& x,
                                            View1 y)
    : MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>(home,x,y),
      intOfDets(home) {
    shared = x.shared(home) || viewarrayshared(home,x,y);
  }

  template<class View0, class View1>
  forceinline
  IntersectionN<View0,View1>::IntersectionN(Home home, ViewArray<View0>& x,
                                            const IntSet& z, View1 y)
    : MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>(home,x,y),
      intOfDets(home) {
    shared = x.shared(home) || viewarrayshared(home,x,y);
    IntSetRanges rz(z);
    intOfDets.intersectI(home, rz);
  }

  template<class View0, class View1>
  forceinline
  IntersectionN<View0,View1>::IntersectionN(Space& home, bool share,
                                            IntersectionN& p)
    : MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>(home,share,p),
      shared(p.shared),
      intOfDets() {
    intOfDets.update(home, p.intOfDets);
  }

  template<class View0, class View1>
  ExecStatus
  IntersectionN<View0,View1>::post(Home home,
                                   ViewArray<View0>& x, View1 y) {
    switch (x.size()) {
    case 0:
      GECODE_ME_CHECK(y.cardMin(home, Limits::card));
      return ES_OK;
    case 1:
      return Rel::Eq<View0,View1>::post(home, x[0], y);
    case 2:
      return Intersection<View0,View0,View1>::post(home, x[0], x[1], y);
    default:
      (void) new (home) IntersectionN<View0,View1>(home,x,y);
      return ES_OK;
    }
  }

  template<class View0, class View1>
  ExecStatus
  IntersectionN<View0,View1>::post(Home home, ViewArray<View0>& x,
                                   const IntSet& z, View1 y) {
    (void) new (home) IntersectionN<View0,View1>(home,x,z,y);
    return ES_OK;
  }

  template<class View0, class View1>
  Actor*
  IntersectionN<View0,View1>::copy(Space& home, bool share) {
    return new (home) IntersectionN<View0,View1>(home,share,*this);
  }

  template<class View0, class View1>
  PropCost
  IntersectionN<View0,View1>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::HI, x.size()+1);
  }

  template<class View0, class View1>
  ExecStatus
  IntersectionN<View0,View1>::propagate(Space& home, const ModEventDelta&) {
    bool repeat = false;
    do {
      repeat = false;
      int xsize = x.size();
      if (xsize == 0)
        goto size0;
      for (int i = xsize; i--; ) {
        GECODE_ME_CHECK( y.cardMax(home,x[i].cardMax()) );
        GECODE_ME_CHECK( x[i].cardMin(home,y.cardMin()) );
        if (x[i].cardMax()==0) {
          GECODE_ME_CHECK( y.cardMax(home, 0));
          intOfDets.dispose(home);
          return home.ES_SUBSUMED(*this);
        }
      }
      {
        Region r(home);
        GlbRanges<View0>* xLBs = r.alloc<GlbRanges<View0> >(xsize);
        LubRanges<View0>* xUBs = r.alloc<LubRanges<View0> >(xsize);
        for (int i = xsize; i--; ) {
          GlbRanges<View0> lb(x[i]);
          LubRanges<View0> ub(x[i]);
          xLBs[i]=lb;
          xUBs[i]=ub;
        }
        {
          Iter::Ranges::NaryInter lbi(r,xLBs,xsize);
          BndSetRanges dets(intOfDets);
          lbi &= dets;
          GECODE_ME_CHECK(y.includeI(home,lbi));
        }
        {
          Iter::Ranges::NaryInter ubi(r,xUBs,xsize);
          BndSetRanges dets(intOfDets);
          ubi &= dets;
          GECODE_ME_CHECK(y.intersectI(home,ubi));
        }
      }

      for (int i = xsize; i--; ) {
        GlbRanges<View1> ylb(y);
        GECODE_ME_CHECK( x[i].includeI(home,ylb) );
      }

      // xi.exclude (Intersection(xj.lb) - y.ub)
      {
        Region r(home);
        GLBndSet* rightSet =
          static_cast<GLBndSet*>(r.ralloc(sizeof(GLBndSet)*xsize));
        new (&rightSet[xsize-1]) GLBndSet(home);
        rightSet[xsize-1].update(home,intOfDets);
        for (int i=xsize-1;i--;) {
          GlbRanges<View0> xilb(x[i+1]);
          BndSetRanges prev(rightSet[i+1]);
          Iter::Ranges::Inter<GlbRanges<View0>,
            BndSetRanges> inter(xilb,prev);
          new (&rightSet[i]) GLBndSet(home);
          rightSet[i].includeI(home,inter);
        }

        LUBndSet leftAcc(home);

        for (int i=0;i<xsize;i++) {
          BndSetRanges left(leftAcc);
          BndSetRanges right(rightSet[i]);
          Iter::Ranges::Inter<BndSetRanges,
            BndSetRanges> inter(left, right);
          LubRanges<View1> yub(y);
          Iter::Ranges::Diff<Iter::Ranges::Inter<BndSetRanges,
            BndSetRanges>, LubRanges<View1> >
            forbidden(inter, yub);
          GECODE_ME_CHECK( x[i].excludeI(home,forbidden) );
          GlbRanges<View0> xlb(x[i]);
          leftAcc.intersectI(home,xlb);
        }
        for (int i=xsize; i--;)
          rightSet[i].dispose(home);
        leftAcc.dispose(home);
      }


      for(int i=0;i<x.size();i++){
        //Do not reverse! Eats away the end of the array!
        while (i<x.size() && x[i].assigned()) {
          GlbRanges<View0> det(x[i]);
          if (intOfDets.intersectI(home,det)) {repeat = true;}
          x.move_lst(i);
          if (intOfDets.size()==0) {
            GECODE_ME_CHECK( y.cardMax(home,0) );
            intOfDets.dispose(home);
            return home.ES_SUBSUMED(*this);
          }
        }
      }
    size0:
      if (x.size()==0) {
        BndSetRanges all1(intOfDets);
        GECODE_ME_CHECK( y.intersectI(home,all1) );
        BndSetRanges all2(intOfDets);
        GECODE_ME_CHECK( y.includeI(home,all2) );
        intOfDets.dispose(home);
        return home.ES_SUBSUMED(*this);
      }

    } while (repeat);

    return shared ? ES_NOFIX : ES_FIX;
  }

}}}

// STATISTICS: set-prop
