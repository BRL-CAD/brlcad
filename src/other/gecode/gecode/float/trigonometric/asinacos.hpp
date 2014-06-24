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

namespace Gecode { namespace Float { namespace Trigonometric {

  /*
   * Bounds consistent arc sinus operator
   *
   */

  template<class A, class B>
  forceinline
  ASin<A,B>::ASin(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1) {}

  template<class A, class B>
  ExecStatus
  ASin<A,B>::post(Home home, A x0, B x1) {
    if (same(x0,x1)) {
      GECODE_ME_CHECK(x0.eq(home,0.0));
    } else {
      GECODE_ME_CHECK(x0.gq(home,-1.0));
      GECODE_ME_CHECK(x0.lq(home,1.0));
      (void) new (home) ASin<A,B>(home,x0,x1);
    }
    return ES_OK;
  }


  template<class A, class B>
  forceinline
  ASin<A,B>::ASin(Space& home, bool share, ASin<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  ASin<A,B>::copy(Space& home, bool share) {
    return new (home) ASin<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  ASin<A,B>::propagate(Space& home, const ModEventDelta&) {
    if ((x0.max() < -1) || (x0.min() > 1)) return ES_FAILED;
    GECODE_ME_CHECK(x1.eq(home,asin(x0.domain())));
    GECODE_ME_CHECK(x0.eq(home,sin(x1.domain())));
    return (x0.assigned() || x1.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }


  /*
   * Bounds consistent arc cosinus operator
   *
   */

  template<class A, class B>
  forceinline
  ACos<A,B>::ACos(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1) {}

  template<class A, class B>
  ExecStatus
  ACos<A,B>::post(Home home, A x0, B x1) {
    if (same(x0,x1)) {
      GECODE_ME_CHECK(x0.gq(home,0.7390851332151));
      GECODE_ME_CHECK(x0.lq(home,0.7390851332152));
      bool mod;
      do {
        mod = false;
        GECODE_ME_CHECK_MODIFIED(mod,x0.eq(home,acos(x0.val())));
      } while (mod);
    } else {
      GECODE_ME_CHECK(x0.gq(home,-1.0));
      GECODE_ME_CHECK(x0.lq(home,1.0));
      (void) new (home) ACos<A,B>(home,x0,x1);
    }
    return ES_OK;
  }


  template<class A, class B>
  forceinline
  ACos<A,B>::ACos(Space& home, bool share, ACos<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  ACos<A,B>::copy(Space& home, bool share) {
    return new (home) ACos<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  ACos<A,B>::propagate(Space& home, const ModEventDelta&) {
    if ((x0.max() < -1) || (x0.min() > 1)) return ES_FAILED;
    GECODE_ME_CHECK(x1.eq(home,acos(x0.domain())));
    GECODE_ME_CHECK(x0.eq(home,cos(x1.domain())));
    return (x0.assigned() || x1.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

}}}

// STATISTICS: float-prop

