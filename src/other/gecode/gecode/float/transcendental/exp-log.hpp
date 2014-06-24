/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
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

namespace Gecode { namespace Float { namespace Transcendental {

  /*
   * Bounds consistent exponential operator
   *
   */

  template<class A, class B>
  forceinline
  Exp<A,B>::Exp(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1) {}

  template<class A, class B>
  ExecStatus
  Exp<A,B>::post(Home home, A x0, B x1) {
    if (same(x0,x1)) {
      return ES_FAILED;
    } else {
      GECODE_ME_CHECK(x1.gq(home,0.0));
    }
    
    (void) new (home) Exp<A,B>(home,x0,x1);
    return ES_OK;
  }


  template<class A, class B>
  forceinline
  Exp<A,B>::Exp(Space& home, bool share, Exp<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  Exp<A,B>::copy(Space& home, bool share) {
    return new (home) Exp<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  Exp<A,B>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x1.eq(home,exp(x0.domain())));
    if (x1.max() == 0.0)
      return ES_FAILED;
    GECODE_ME_CHECK(x0.eq(home,log(x1.domain())));
    return x0.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }


  /*
   * Bounds consistent logarithm operator with base
   *
   */

  template<class A, class B>
  forceinline
  Pow<A,B>::Pow(Home home, FloatNum base0, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1),
      base(base0) {}

  template<class A, class B>
  ExecStatus
  Pow<A,B>::post(Home home, FloatNum base, A x0, B x1) {
    if (base <= 0) return ES_FAILED;
    if (same(x0,x1)) {
      GECODE_ME_CHECK(x0.eq(home,0.0));
    } else {
      GECODE_ME_CHECK(x1.gq(home,0.0));
      (void) new (home) Pow<A,B>(home,base,x0,x1);
    }
    return ES_OK;
  }

  template<class A, class B>
  forceinline
  Pow<A,B>::Pow(Space& home, bool share, Pow<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p),
      base(p.base) {}

  template<class A, class B>
  Actor*
  Pow<A,B>::copy(Space& home, bool share) {
    return new (home) Pow<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  Pow<A,B>::propagate(Space& home, const ModEventDelta&) {
    if (x1.max() == 0.0)
      return ES_FAILED;
    GECODE_ME_CHECK(x0.eq(home,log(x1.domain())/log(base)));
    GECODE_ME_CHECK(x1.eq(home,exp(x0.domain()*log(base))));
    return x0.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

}}}

// STATISTICS: float-prop

