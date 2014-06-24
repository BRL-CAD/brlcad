/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2004
 *     Guido Tack, 2006
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
   * Positive bounds consistent squaring
   *
   */
  template<class VA, class VB>
  forceinline
  SqrPlus<VA,VB>::SqrPlus(Home home, VA x0, VB x1)
    : MixBinaryPropagator<VA,PC_FLOAT_BND,VB,PC_FLOAT_BND>(home,x0,x1) {}

  template<class VA, class VB>
  forceinline ExecStatus
  SqrPlus<VA,VB>::post(Home home, VA x0, VB x1) {
    if (same(x0,x1)) {
      if (x0.assigned())
        return ((x0.val() == 0) || (x0.val() == 1))? ES_OK : ES_FAILED;
    } else {
      GECODE_ME_CHECK(x0.eq(home,sqrt(x1.val())));
      GECODE_ME_CHECK(x1.eq(home,sqr(x0.val())));
    }

    (void) new (home) SqrPlus<VA,VB>(home,x0,x1);
    return ES_OK;
  }

  template<class VA, class VB>
  forceinline
  SqrPlus<VA,VB>::SqrPlus(Space& home, bool share, SqrPlus<VA,VB>& p)
    : MixBinaryPropagator<VA,PC_FLOAT_BND,VB,PC_FLOAT_BND>(home,share,p) {}

  template<class VA, class VB>
  Actor*
  SqrPlus<VA,VB>::copy(Space& home, bool share) {
    return new (home) SqrPlus<VA,VB>(home,share,*this);
  }

  template<class VA, class VB>
  ExecStatus
  SqrPlus<VA,VB>::propagate(Space& home, const ModEventDelta&) {
    if (same(x0,x1)) {
      if (x0.max() < 1) GECODE_ME_CHECK(x0.eq(home,0));
      else if (x0.min() > 0) GECODE_ME_CHECK(x0.eq(home,1));
      if (x0.assigned())
        return ((x0.val() == 0) || (x0.val() == 1))? home.ES_SUBSUMED(*this) : ES_FAILED;
    } else {
      GECODE_ME_CHECK(x0.eq(home,sqrt(x1.val())));
      GECODE_ME_CHECK(x1.eq(home,sqr(x0.val())));
      if (x0.assigned() || x1.assigned()) return home.ES_SUBSUMED(*this);
    }

    return ES_FIX;
  }


  /*
   * Bounds consistent squaring
   *
   */

  template<class View>
  forceinline
  Sqr<View>::Sqr(Home home, View x0, View x1)
    : BinaryPropagator<View,PC_FLOAT_BND>(home,x0,x1) {}

  template<class View>
  forceinline ExecStatus
  Sqr<View>::post(Home home, View x0, View x1) {
    GECODE_ME_CHECK(x1.gq(home,0));
    if (same(x0,x1)) {
      if (x0.assigned())
        return ((x0.val() == 0) || (x0.val() == 1))? ES_OK : ES_FAILED;
      GECODE_ME_CHECK(x1.lq(home,1));
      return SqrPlus<FloatView,FloatView>::post(home,x0,x1);
    } else {
      if (x0.min() >= 0)
        return SqrPlus<FloatView,FloatView>::post(home,x0,x1);
      if (x0.max() <= 0)
        return SqrPlus<MinusView,FloatView>::post(home,MinusView(x0),x1);
      GECODE_ME_CHECK(x1.eq(home,sqr(x0.val())));
      (void) new (home) Sqr<View>(home,x0,x1);
    }
    return ES_OK;
  }

  template<class View>
  forceinline
  Sqr<View>::Sqr(Space& home, bool share, Sqr<View>& p)
    : BinaryPropagator<View,PC_FLOAT_BND>(home,share,p) {}

  template<class View>
  Actor*
  Sqr<View>::copy(Space& home, bool share) {
    return new (home) Sqr<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  Sqr<View>::propagate(Space& home, const ModEventDelta&) {
    assert(x1.min() >= 0);
    if (x0.min() >= 0)
      GECODE_REWRITE(*this,(SqrPlus<FloatView,FloatView>::post(home(*this),x0,x1)));
    if (x0.max() <= 0)
      GECODE_REWRITE(*this,(SqrPlus<MinusView,FloatView>::post(home(*this),
        MinusView(x0),x1)));

    GECODE_ME_CHECK(x1.eq(home,sqr(x0.val())));
    Rounding r;
    FloatVal z = sqrt(x1.val());
    if (x0.min() > -r.sqrt_up(x1.min()))
      GECODE_ME_CHECK(x0.eq(home,z));
    else if (x0.max() < r.sqrt_down(x1.min()))
      GECODE_ME_CHECK(x0.eq(home,-z));
    else
      GECODE_ME_CHECK(x0.eq(home,hull(z,-z)));

    return ES_NOFIX;
  }


  /*
   * Bounds consistent square root operator
   *
   */

  template<class A, class B>
  forceinline
  Sqrt<A,B>::Sqrt(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1) {}

  template<class A, class B>
  ExecStatus
  Sqrt<A,B>::post(Home home, A x0, B x1) {
    GECODE_ME_CHECK(x0.gq(home,0));    
    if (same(x0,x1)) {
      if (x0.assigned())
        return ((x0.val() == 0) || (x0.val() == 1))? ES_OK : ES_FAILED;
      GECODE_ME_CHECK(x0.lq(home,1));
      (void) new (home) Sqrt<A,B>(home,x0,x1);
    } else {
      GECODE_ME_CHECK(x1.eq(home,sqrt(x0.val())));
      (void) new (home) Sqrt<A,B>(home,x0,x1);
    }
    return ES_OK;
  }

  template<class A, class B>
  forceinline
  Sqrt<A,B>::Sqrt(Space& home, bool share, Sqrt<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  Sqrt<A,B>::copy(Space& home, bool share) {
    return new (home) Sqrt<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  Sqrt<A,B>::propagate(Space& home, const ModEventDelta&) {
    if (same(x0,x1)) {
      if (x0.max() < 1) GECODE_ME_CHECK(x0.eq(home,0));
      else if (x0.min() > 0) GECODE_ME_CHECK(x0.eq(home,1));
      if (x0.assigned())
        return ((x0.val() == 0) || (x0.val() == 1))? home.ES_SUBSUMED(*this) : ES_FAILED;
    } else {
      GECODE_ME_CHECK(x0.eq(home,sqr(x1.val())));
      GECODE_ME_CHECK(x1.eq(home,sqrt(x0.val())));
      if (x0.assigned() || x1.assigned()) return home.ES_SUBSUMED(*this);
    }
    
    return ES_FIX;
  }

  /*
   * Absolute consistent square operator
   *
   */

  template<class A, class B>
  forceinline
  Abs<A,B>::Abs(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1) {}

  template<class A, class B>
  ExecStatus
  Abs<A,B>::post(Home home, A x0, B x1) {
    (void) new (home) Abs<A,B>(home,x0,x1);
    return ES_OK;
  }

  template<class A, class B>
  forceinline
  Abs<A,B>::Abs(Space& home, bool share, Abs<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  Abs<A,B>::copy(Space& home, bool share) {
    return new (home) Abs<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  Abs<A,B>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x1.eq(home,abs(x0.val())));
    if (x0.min() >= 0)
      GECODE_ME_CHECK(x0.eq(home,FloatVal(x1.min(), x1.max())));
    else if (x0.max() <= 0)
      GECODE_ME_CHECK(x0.eq(home,FloatVal(-x1.max(), -x1.min())));
    else
      GECODE_ME_CHECK(x0.eq(home,FloatVal(-x1.max(), x1.max())));
    return (x0.assigned() && x1.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

}}}

// STATISTICS: float-prop

