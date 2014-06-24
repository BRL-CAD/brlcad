/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2011
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

  template<class View0, class View1, ReifyMode rm, bool strict>
  forceinline
  ReLq<View0,View1,rm,strict>::ReLq(Home home, View0 y0, View1 y1,
                                    Gecode::Int::BoolView y2)
    : Propagator(home), x0(y0), x1(y1), b(y2) {
    b.subscribe(home,*this, Gecode::Int::PC_INT_VAL);
    x0.subscribe(home,*this, PC_SET_ANY);
    x1.subscribe(home,*this, PC_SET_ANY);
  }

  template<class View0, class View1, ReifyMode rm, bool strict>
  forceinline
  ReLq<View0,View1,rm,strict>::ReLq(Space& home, bool share, ReLq& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    b.update(home,share,p.b);
  }

  template<class View0, class View1, ReifyMode rm, bool strict>
  PropCost
  ReLq<View0,View1,rm,strict>::cost(const Space&, const ModEventDelta&) const 
  {
    return PropCost::ternary(PropCost::LO);
  }

  template<class View0, class View1, ReifyMode rm, bool strict>
  forceinline size_t
  ReLq<View0,View1,rm,strict>::dispose(Space& home) {
    b.cancel(home,*this, Gecode::Int::PC_INT_VAL);
    x0.cancel(home,*this, PC_SET_ANY);
    x1.cancel(home,*this, PC_SET_ANY);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View0, class View1, ReifyMode rm, bool strict>
  ExecStatus
  ReLq<View0,View1,rm,strict>::post(Home home, View0 x0, View1 x1,
                            Gecode::Int::BoolView b) {
    (void) new (home) ReLq<View0,View1,rm,strict>(home,x0,x1,b);
    return ES_OK;
  }

  template<class View0, class View1, ReifyMode rm, bool strict>
  Actor*
  ReLq<View0,View1,rm,strict>::copy(Space& home, bool share) {
    return new (home) ReLq<View0,View1,rm,strict>(home,share,*this);
  }

  template<class View0, class View1, ReifyMode rm, bool strict>
  ExecStatus
  ReLq<View0,View1,rm,strict>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this,(Lq<View0,View1,strict>::post(home(*this),x0,x1)));
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this,
        (Lq<View1,View0,!strict>::post(home(*this),x1,x0)));
    }
    if (x0.cardMax() == 0) {
      if ( (!strict) || x1.cardMin() > 0) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
        return home.ES_SUBSUMED(*this);
      }
      if (strict && x1.cardMax() == 0) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
        return home.ES_SUBSUMED(*this);
      }
    }

    if (x0.assigned() && x1.assigned()) {
      // directly test x0<=x1
      int min01;
      {
        GlbRanges<View0> x0l(x0);
        GlbRanges<View1> x1l(x1);
        Iter::Ranges::Diff<GlbRanges<View1>,GlbRanges<View0> > d(x1l,x0l);
        if (!d()) {
          if ((!strict) && x0.cardMax() == x1.cardMax()) {
            // equal
            if (rm != RM_IMP)
              GECODE_ME_CHECK(b.one_none(home));
          } else {
            // subset
            if (rm != RM_PMI)
              GECODE_ME_CHECK(b.zero_none(home));
          }
          return home.ES_SUBSUMED(*this);
        }
        min01 = d.min();
      }
      int min10;
      {
        GlbRanges<View0> x0l(x0);
        GlbRanges<View1> x1l(x1);
        Iter::Ranges::Diff<GlbRanges<View0>,GlbRanges<View1> > d(x0l,x1l);
        if (!d()) {
          if (strict && x0.cardMax() == x1.cardMax()) {
            // equal
            if (rm != RM_PMI)
              GECODE_ME_CHECK(b.zero_none(home));
          } else {
            // subset
            if (rm != RM_IMP)
              GECODE_ME_CHECK(b.one_none(home));
          }
          return home.ES_SUBSUMED(*this);
        }
        min10 = d.min();
      }

      assert(min01 != min10);
      if (min01<min10) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
      } else {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
      }
      return home.ES_SUBSUMED(*this);
    }

    // min(x0lb - x1ub) < min(x1ub) -> b=0
    if (x1.cardMax() > 0) {
      GlbRanges<View0> x0l(x0);
      LubRanges<View1> x1u(x1);
      int x1umin=x1u.min();
      Iter::Ranges::Diff<GlbRanges<View0>,LubRanges<View1> > d(x0l,x1u);
      if (d() && d.min() < x1umin) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
        return home.ES_SUBSUMED(*this);
      }
    }
    // min(x1lb - x0ub) < min(x0ub) -> b=1
    if (x0.cardMax() > 0) {
      LubRanges<View0> x0u(x0);
      GlbRanges<View1> x1l(x1);
      int x0umin=x0u.min();
      Iter::Ranges::Diff<GlbRanges<View1>,LubRanges<View0> > d(x1l,x0u);
      if (d() && d.min() < x0umin) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
        return home.ES_SUBSUMED(*this);
      }
    }

    return ES_FIX;
  }

}}}

// STATISTICS: set-prop
