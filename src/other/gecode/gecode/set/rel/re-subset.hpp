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

namespace Gecode { namespace Set { namespace Rel {

  template<class View0, class View1, ReifyMode rm>
  forceinline
  ReSubset<View0,View1,rm>::ReSubset(Home home, View0 y0,
                                     View1 y1, Gecode::Int::BoolView y2)
    : Propagator(home), x0(y0), x1(y1), b(y2) {
    b.subscribe(home,*this, Gecode::Int::PC_INT_VAL);
    x0.subscribe(home,*this, PC_SET_ANY);
    x1.subscribe(home,*this, PC_SET_ANY);
  }

  template<class View0, class View1, ReifyMode rm>
  forceinline
  ReSubset<View0,View1,rm>::ReSubset(Space& home, bool share, ReSubset& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    b.update(home,share,p.b);
  }

  template<class View0, class View1, ReifyMode rm>
  PropCost
  ReSubset<View0,View1,rm>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::ternary(PropCost::LO);
  }

  template<class View0, class View1, ReifyMode rm>
  forceinline size_t
  ReSubset<View0,View1,rm>::dispose(Space& home) {
    b.cancel(home,*this, Gecode::Int::PC_INT_VAL);
    x0.cancel(home,*this, PC_SET_ANY);
    x1.cancel(home,*this, PC_SET_ANY);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View0, class View1, ReifyMode rm>
  ExecStatus
  ReSubset<View0,View1,rm>::post(Home home, View0 x0, View1 x1,
                              Gecode::Int::BoolView b) {
    (void) new (home) ReSubset<View0,View1,rm>(home,x0,x1,b);
    return ES_OK;
  }

  template<class View0, class View1, ReifyMode rm>
  Actor*
  ReSubset<View0,View1,rm>::copy(Space& home, bool share) {
    return new (home) ReSubset<View0,View1,rm>(home,share,*this);
  }

  template<class View0, class View1, ReifyMode rm>
  ExecStatus
  ReSubset<View0,View1,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this,(Subset<View0,View1>::post(home(*this),x0,x1)));
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return home.ES_SUBSUMED(*this);        
      GECODE_REWRITE(*this,(NoSubset<View0,View1>::post(home(*this),x0,x1)));
    }

    // check whether cardinalities still allow subset
    if (x0.cardMin() > x1.cardMax()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(b.zero_none(home));
      return home.ES_SUBSUMED(*this);
    }

    // check lub(x0) subset glb(x1)
    {
      LubRanges<View0> x0ub(x0);
      GlbRanges<View1> x1lb(x1);
      Iter::Ranges::Diff<LubRanges<View0>,GlbRanges<View1> > d(x0ub,x1lb);
      if (!d()) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
        return home.ES_SUBSUMED(*this);
      }
    }

    // check glb(x0) subset lub(x1)
    {
      GlbRanges<View0> x0lb(x0);
      LubRanges<View1> x1ub(x1);
      Iter::Ranges::Diff<GlbRanges<View0>,LubRanges<View1> > d(x0lb,x1ub);
      if (d()) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
        return home.ES_SUBSUMED(*this);
      } else if (x0.assigned() && x1.assigned()) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
        return home.ES_SUBSUMED(*this);
      }
    }

    if (x0.cardMin() > 0) {
      LubRanges<View0> x0ub(x0);
      LubRanges<View1> x1ub(x1);
      Iter::Ranges::Inter<LubRanges<View0>,LubRanges<View1> >
        i(x0ub,x1ub);
      if (!i()) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
        return home.ES_SUBSUMED(*this);
      }
    }

    return ES_FIX;
  }

}}}

// STATISTICS: set-prop
