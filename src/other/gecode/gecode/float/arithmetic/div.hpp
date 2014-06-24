/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Guido Tack, 2008
 *     Vincent Barichard, 2012
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

namespace Gecode { namespace Float { namespace Arithmetic {

  /*
   * Bounds consistent division operator
   *
   */
  template<class A, class B, class C>
  forceinline
  Div<A,B,C>::Div(Home home, A x0, B x1, C x2)
    : MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>(home,x0,x1,x2) {}

  template<class A, class B, class C>
  forceinline
  Div<A,B,C>::Div(Space& home, bool share, Div<A,B,C>& p)
    : MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B, class C>
  Actor*
  Div<A,B,C>::copy(Space& home, bool share) {
    return new (home) Div<A,B,C>(home,share,*this);
  }

  template<class A, class B, class C>
  ExecStatus
  Div<A,B,C>::post(Home home, A x0, B x1, C x2) {
    (void) new (home) Div<A,B,C>(home,x0,x1,x2);
    return ES_OK;
  }

  template<class A, class B, class C>
  ExecStatus
  Div<A,B,C>::propagate(Space& home, const ModEventDelta&) {
    if (x1.assigned() && (x1.val() == 0)) return ES_FAILED;
    GECODE_ME_CHECK(x2.eq(home,x0.domain() / x1.domain()));
    GECODE_ME_CHECK(x0.eq(home,x2.domain() * x1.domain()));
    if (!x2.assigned() || (x2.val() != 0)) GECODE_ME_CHECK(x1.eq(home,x0.domain() / x2.domain()));
    return (x0.assigned() && x1.assigned()) ? home.ES_SUBSUMED(*this) : ES_NOFIX;
  }

}}}

// STATISTICS: float-prop

