/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

namespace Gecode { namespace Int { namespace Linear {

  /*
   * Ternary linear propagators
   *
   */
  template<class Val, class A, class B, class C, PropCond pc>
  forceinline
  LinTer<Val,A,B,C,pc>::LinTer(Home home, A y0, B y1, C y2, Val c0)
    : Propagator(home), x0(y0), x1(y1), x2(y2), c(c0) {
    x0.subscribe(home,*this,pc);
    x1.subscribe(home,*this,pc);
    x2.subscribe(home,*this,pc);
  }

  template<class Val, class A, class B, class C, PropCond pc>
  forceinline
  LinTer<Val,A,B,C,pc>::LinTer(Space& home, bool share,
                               LinTer<Val,A,B,C,pc>& p)
    : Propagator(home,share,p), c(p.c) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    x2.update(home,share,p.x2);
  }

  template<class Val, class A, class B, class C, PropCond pc>
  forceinline
  LinTer<Val,A,B,C,pc>::LinTer(Space& home, bool share, Propagator& p,
                               A y0, B y1, C y2, Val c0)
    : Propagator(home,share,p), c(c0) {
    x0.update(home,share,y0);
    x1.update(home,share,y1);
    x2.update(home,share,y2);
  }

  template<class Val, class A, class B, class C, PropCond pc>
  PropCost
  LinTer<Val,A,B,C,pc>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::ternary(PropCost::LO);
  }

  template<class Val, class A, class B, class C, PropCond pc>
  forceinline size_t
  LinTer<Val,A,B,C,pc>::dispose(Space& home) {
    x0.cancel(home,*this,pc);
    x1.cancel(home,*this,pc);
    x2.cancel(home,*this,pc);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  /*
   * Equality propagator
   *
   */

  template<class Val, class A, class B, class C>
  forceinline
  EqTer<Val,A,B,C>::EqTer(Home home, A x0, B x1, C x2, Val c)
    : LinTer<Val,A,B,C,PC_INT_BND>(home,x0,x1,x2,c) {}

  template<class Val, class A, class B, class C>
  ExecStatus
  EqTer<Val,A,B,C>::post(Home home, A x0, B x1, C x2, Val c) {
    (void) new (home) EqTer<Val,A,B,C>(home,x0,x1,x2,c);
    return ES_OK;
  }


  template<class Val, class A, class B, class C>
  forceinline
  EqTer<Val,A,B,C>::EqTer(Space& home, bool share, EqTer<Val,A,B,C>& p)
    : LinTer<Val,A,B,C,PC_INT_BND>(home,share,p) {}

  template<class Val, class A, class B, class C>
  forceinline
  EqTer<Val,A,B,C>::EqTer(Space& home, bool share, Propagator& p,
                          A x0, B x1, C x2, Val c)
    : LinTer<Val,A,B,C,PC_INT_BND>(home,share,p,x0,x1,x2,c) {}

  template<class Val, class A, class B, class C>
  Actor*
  EqTer<Val,A,B,C>::copy(Space& home, bool share) {
    return new (home) EqTer<Val,A,B,C>(home,share,*this);
  }

  /// Describe which view has been modified how
  enum TerMod {
    TM_X0_MIN = 1<<0,
    TM_X0_MAX = 1<<1,
    TM_X1_MIN = 1<<2,
    TM_X1_MAX = 1<<3,
    TM_X2_MIN = 1<<4,
    TM_X2_MAX = 1<<5,
    TM_ALL    = TM_X0_MIN|TM_X0_MAX|TM_X1_MIN|TM_X1_MAX|TM_X2_MIN|TM_X2_MAX
  };

#define GECODE_INT_PV(CASE,TELL,UPDATE)                \
  if (bm & (CASE)) {                                \
    bm -= (CASE); ModEvent me = (TELL);                \
    if (me_failed(me))   return ES_FAILED;        \
    if (me_modified(me)) bm |= (UPDATE);        \
  }

  template<class Val, class A, class B, class C>
  ExecStatus
  EqTer<Val,A,B,C>::propagate(Space& home, const ModEventDelta&) {
    int bm = TM_ALL;
    do {
      GECODE_INT_PV(TM_X0_MIN, x0.gq(home,c-x1.max()-x2.max()),
                    TM_X1_MAX | TM_X2_MAX);
      GECODE_INT_PV(TM_X1_MIN, x1.gq(home,c-x0.max()-x2.max()),
                    TM_X0_MAX | TM_X2_MAX);
      GECODE_INT_PV(TM_X2_MIN, x2.gq(home,c-x0.max()-x1.max()),
                    TM_X0_MAX | TM_X1_MAX);
      GECODE_INT_PV(TM_X0_MAX, x0.lq(home,c-x1.min()-x2.min()),
                    TM_X1_MIN | TM_X2_MIN);
      GECODE_INT_PV(TM_X1_MAX, x1.lq(home,c-x0.min()-x2.min()),
                    TM_X0_MIN | TM_X2_MIN);
      GECODE_INT_PV(TM_X2_MAX, x2.lq(home,c-x0.min()-x1.min()),
                    TM_X0_MIN | TM_X1_MIN);
    } while (bm);
    return (x0.assigned() && x1.assigned()) ?
      home.ES_SUBSUMED(*this) : ES_FIX;
  }

#undef GECODE_INT_PV



  /*
   * Disequality propagator
   *
   */

  template<class Val, class A, class B, class C>
  forceinline
  NqTer<Val,A,B,C>::NqTer(Home home, A x0, B x1, C x2, Val c)
    : LinTer<Val,A,B,C,PC_INT_VAL>(home,x0,x1,x2,c) {}

  template<class Val, class A, class B, class C>
  ExecStatus
  NqTer<Val,A,B,C>::post(Home home, A x0, B x1, C x2, Val c) {
    (void) new (home) NqTer<Val,A,B,C>(home,x0,x1,x2,c);
    return ES_OK;
  }


  template<class Val, class A, class B, class C>
  forceinline
  NqTer<Val,A,B,C>::NqTer(Space& home, bool share, NqTer<Val,A,B,C>& p)
    : LinTer<Val,A,B,C,PC_INT_VAL>(home,share,p) {}

  template<class Val, class A, class B, class C>
  Actor*
  NqTer<Val,A,B,C>::copy(Space& home, bool share) {
    return new (home) NqTer<Val,A,B,C>(home,share,*this);
  }

  template<class Val, class A, class B, class C>
  forceinline
  NqTer<Val,A,B,C>::NqTer(Space& home, bool share, Propagator& p,
                          A x0, B x1, C x2, Val c)
    : LinTer<Val,A,B,C,PC_INT_VAL>(home,share,p,x0,x1,x2,c) {}


  template<class Val, class A, class B, class C>
  ExecStatus
  NqTer<Val,A,B,C>::propagate(Space& home, const ModEventDelta&) {
    if (x0.assigned() && x1.assigned()) {
      GECODE_ME_CHECK(x2.nq(home,c-x0.val()-x1.val()));
      return home.ES_SUBSUMED(*this);
    }
    if (x0.assigned() && x2.assigned()) {
      GECODE_ME_CHECK(x1.nq(home,c-x0.val()-x2.val()));
      return home.ES_SUBSUMED(*this);
    }
    if (x1.assigned() && x2.assigned()) {
      GECODE_ME_CHECK(x0.nq(home,c-x1.val()-x2.val()));
      return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }



  /*
   * Inequality propagator
   *
   */

  template<class Val, class A, class B, class C>
  forceinline
  LqTer<Val,A,B,C>::LqTer(Home home, A x0, B x1, C x2, Val c)
    : LinTer<Val,A,B,C,PC_INT_BND>(home,x0,x1,x2,c) {}

  template<class Val, class A, class B, class C>
  ExecStatus
  LqTer<Val,A,B,C>::post(Home home, A x0, B x1, C x2, Val c) {
    (void) new (home) LqTer<Val,A,B,C>(home,x0,x1,x2,c);
    return ES_OK;
  }


  template<class Val, class A, class B, class C>
  forceinline
  LqTer<Val,A,B,C>::LqTer(Space& home, bool share, LqTer<Val,A,B,C>& p)
    : LinTer<Val,A,B,C,PC_INT_BND>(home,share,p) {}

  template<class Val, class A, class B, class C>
  Actor*
  LqTer<Val,A,B,C>::copy(Space& home, bool share) {
    return new (home) LqTer<Val,A,B,C>(home,share,*this);
  }


  template<class Val, class A, class B, class C>
  forceinline
  LqTer<Val,A,B,C>::LqTer(Space& home, bool share, Propagator& p,
                          A x0, B x1, C x2, Val c)
    : LinTer<Val,A,B,C,PC_INT_BND>(home,share,p,x0,x1,x2,c) {}

  template<class Val, class A, class B, class C>
  ExecStatus
  LqTer<Val,A,B,C>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x0.lq(home,c-x1.min()-x2.min()));
    GECODE_ME_CHECK(x1.lq(home,c-x0.min()-x2.min()));
    GECODE_ME_CHECK(x2.lq(home,c-x0.min()-x1.min()));
    return (x0.max()+x1.max()+x2.max() <= c) ?
      home.ES_SUBSUMED(*this) : ES_FIX;
  }

}}}

// STATISTICS: int-prop

