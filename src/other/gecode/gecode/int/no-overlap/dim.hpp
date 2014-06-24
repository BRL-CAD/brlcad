/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

namespace Gecode { namespace Int { namespace NoOverlap {

  /*
   * Dimension with integer size
   *
   */
  forceinline
  FixDim::FixDim(void) 
    : s(0) {}
  forceinline
  FixDim::FixDim(IntView c0, int s0)
    : c(c0), s(s0) {}

  forceinline int 
  FixDim::ssc(void) const {
    return c.min();
  }
  forceinline int 
  FixDim::lsc(void) const {
    return c.max();
  }
  forceinline int 
  FixDim::sec(void) const {
    return c.min() + s;
  }
  forceinline int 
  FixDim::lec(void) const {
    return c.max() + s;
  }

  forceinline ExecStatus
  FixDim::ssc(Space& home, int n) {
    GECODE_ME_CHECK(c.gq(home, n));
    return ES_OK;
  }
  forceinline ExecStatus
  FixDim::lec(Space& home, int n) {
    GECODE_ME_CHECK(c.lq(home, n - s));
    return ES_OK;
  }
  forceinline ExecStatus
  FixDim::nooverlap(Space& home, int n, int m) {
    if (n <= m) {
      Iter::Ranges::Singleton r(n-s+1,m);
      GECODE_ME_CHECK(c.minus_r(home,r,false));
    }
    return ES_OK;
  }
  forceinline ExecStatus
  FixDim::nooverlap(Space& home, FixDim& d) {
    if (d.sec() > lsc()) {
      // Propagate that d must be after this
      GECODE_ES_CHECK(lec(home,d.lsc()));
      GECODE_ES_CHECK(d.ssc(home,sec()));
    } else {
      nooverlap(home, d.lsc(), d.sec()-1);
    }
    return ES_OK;
  }

  forceinline void
  FixDim::update(Space& home, bool share, FixDim& d) {
    c.update(home,share,d.c);
    s = d.s;
  }

  forceinline void
  FixDim::subscribe(Space& home, Propagator& p) {
    c.subscribe(home,p,PC_INT_DOM);
  }
  forceinline void
  FixDim::cancel(Space& home, Propagator& p) {
    c.cancel(home,p,PC_INT_DOM);
  }


  /*
   * Dimension with integer view size
   *
   */
  forceinline
  FlexDim::FlexDim(void) {}
  forceinline
  FlexDim::FlexDim(IntView c00, IntView s0, IntView c10)
    : c0(c00), s(s0), c1(c10) {}

  forceinline int 
  FlexDim::ssc(void) const {
    return c0.min();
  }
  forceinline int 
  FlexDim::lsc(void) const {
    return c0.max();
  }
  forceinline int 
  FlexDim::sec(void) const {
    return c1.min();
  }
  forceinline int 
  FlexDim::lec(void) const {
    return c1.max();
  }

  forceinline ExecStatus
  FlexDim::ssc(Space& home, int n) {
    GECODE_ME_CHECK(c0.gq(home, n));
    return ES_OK;
  }
  forceinline ExecStatus
  FlexDim::lec(Space& home, int n) {
    GECODE_ME_CHECK(c1.lq(home, n));
    return ES_OK;
  }
  forceinline ExecStatus
  FlexDim::nooverlap(Space& home, int n, int m) {
    if (n <= m) {
      Iter::Ranges::Singleton r0(n-s.min()+1,m);
      GECODE_ME_CHECK(c0.minus_r(home,r0,false));
      Iter::Ranges::Singleton r1(n+1,s.min()+m);
      GECODE_ME_CHECK(c1.minus_r(home,r1,false));
    }
    return ES_OK;
  }
  forceinline ExecStatus
  FlexDim::nooverlap(Space& home, FlexDim& d) {
    if (d.sec() > lsc()) {
      // Propagate that d must be after this
      GECODE_ES_CHECK(lec(home,d.lsc()));
      GECODE_ES_CHECK(d.ssc(home,sec()));
    } else {
      nooverlap(home, d.lsc(), d.sec()-1);
    }
    return ES_OK;
  }


  forceinline void
  FlexDim::update(Space& home, bool share, FlexDim& d) {
    c0.update(home,share,d.c0);
    s.update(home,share,d.s);
    c1.update(home,share,d.c1);
  }

  forceinline void
  FlexDim::subscribe(Space& home, Propagator& p) {
    c0.subscribe(home,p,PC_INT_DOM);
    s.subscribe(home,p,PC_INT_BND);
    c1.subscribe(home,p,PC_INT_DOM);
  }
  forceinline void
  FlexDim::cancel(Space& home, Propagator& p) {
    c0.cancel(home,p,PC_INT_DOM);
    s.cancel(home,p,PC_INT_BND);
    c1.cancel(home,p,PC_INT_DOM);
  }

}}}

// STATISTICS: int-prop

