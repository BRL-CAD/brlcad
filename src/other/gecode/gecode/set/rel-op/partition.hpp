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
   * "Nary partition" propagator
   *
   */

  template<class View0, class View1>
  forceinline
  PartitionN<View0,View1>::PartitionN(Home home, ViewArray<View0>& x, View1 y)
    : MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>(home, x, y) {
    shared = x.shared(home) || viewarrayshared(home,x,y);
  }

  template<class View0, class View1>
  forceinline
  PartitionN<View0,View1>::PartitionN(Home home, ViewArray<View0>& x,
                                      const IntSet& z, View1 y)
    : MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>(home, x, y) {
    shared = x.shared(home) || viewarrayshared(home,x,y);
    IntSetRanges rz(z);
    unionOfDets.includeI(home, rz);
  }

  template<class View0, class View1>
  forceinline
  PartitionN<View0,View1>::PartitionN(Space& home, bool share, PartitionN& p)
    : MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>(home,share,p),
      shared(p.shared) {
    unionOfDets.update(home,p.unionOfDets);
  }

  template<class View0, class View1>
  Actor*
  PartitionN<View0,View1>::copy(Space& home, bool share) {
    return new (home) PartitionN(home,share,*this);
  }

  template<class View0, class View1>
  ExecStatus PartitionN<View0,View1>::post(Home home, ViewArray<View0>& x,
                                           View1 y) {
    switch (x.size()) {
    case 0:
      GECODE_ME_CHECK(y.cardMax(home, 0));
      return ES_OK;
    case 1:
      return Rel::Eq<View0,View1>::post(home, x[0], y);
    default:
      (void) new (home) PartitionN<View0,View1>(home,x,y);
      return ES_OK;
    }
  }

  template<class View0, class View1>
  ExecStatus PartitionN<View0,View1>::post(Home home, ViewArray<View0>& x,
                                           const IntSet& z, View1 y) {
    (void) new (home) PartitionN<View0,View1>(home,x,z,y);
    return ES_OK;
  }

  template<class View0, class View1>
  PropCost PartitionN<View0,View1>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::LO, x.size()+1);
  }

  template<class View0, class View1>
  ExecStatus
  PartitionN<View0,View1>::propagate(Space& home, const ModEventDelta& med) {

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
      if (oldModified || anybevent)
        GECODE_ES_CHECK(partitionNXiUB(home,modified, x, y,unionOfDets));
      if (modified || oldModified || anybevent)
        GECODE_ES_CHECK(partitionNXiLB(home,modified, x, y,unionOfDets));
      if (modified || oldModified || ubevent)
        GECODE_ES_CHECK(partitionNYUB(home,modified, x, y,unionOfDets));
      if (modified || oldModified || lbevent)
        GECODE_ES_CHECK(partitionNYLB(home,modified, x, y,unionOfDets));
      if (modified || oldModified || ubevent)
        GECODE_ES_CHECK(unionNXiUB(home,modified, x, y,unionOfDets));
      if (modified || oldModified || cardevent)
        GECODE_ES_CHECK(partitionNCard(home,modified, x, y,unionOfDets));
    } while (modified);

    //removing assigned sets from x, accumulating the value:
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
