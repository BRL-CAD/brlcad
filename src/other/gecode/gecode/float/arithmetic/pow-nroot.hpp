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

namespace Gecode { namespace Float { namespace Arithmetic {


  /*
   * Bounds consistent square operator
   *
   */

  template<class A, class B>
  forceinline
  Pow<A,B>::Pow(Home home, A x0, B x1, int n)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1), m_n(n) {}

  template<class A, class B>
  ExecStatus
  Pow<A,B>::post(Home home, A x0, B x1, int n) {
    if (n == 0) {
      if ((x0.min() == 0.0) && (x0.max() == 0.0)) return ES_FAILED;
      GECODE_ME_CHECK(x1.eq(home,1.0));
      return ES_OK;
    }

    GECODE_ME_CHECK(x1.eq(home,pow(x0.domain(),n)));
    if ((x1.min() == 0.0) && (x1.max() == 0.0)) {
      GECODE_ME_CHECK(x1.eq(home,0.0));
      return ES_OK;
    }
    
    if ((n % 2) == 0)
    {
      if (x0.min() >= 0)
        GECODE_ME_CHECK(x0.eq(home,nroot(x1.domain(),n)));
      else if (x0.max() <= 0)
        GECODE_ME_CHECK(x0.eq(home,-nroot(x1.domain(),n)));
      else
        GECODE_ME_CHECK(x0.eq(home,
                              hull(
                                  nroot(x1.domain(),n),
                                  -nroot(x1.domain(),n)
                              )
                        ));
    } else
      GECODE_ME_CHECK(x0.eq(home,nroot(x1.domain(),n)));

    if (!x0.assigned()) (void) new (home) Pow<A,B>(home,x0,x1,n);
    return ES_OK;
  }

  template<class A, class B>
  forceinline
  Pow<A,B>::Pow(Space& home, bool share, Pow<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p), m_n(p.m_n) {}

  template<class A, class B>
  Actor*
  Pow<A,B>::copy(Space& home, bool share) {
    return new (home) Pow<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  Pow<A,B>::propagate(Space& home, const ModEventDelta&) {
    if ((x0.min() == 0.0) && (x0.max() == 0.0)) return ES_FAILED;
    GECODE_ME_CHECK(x1.eq(home,pow(x0.domain(),m_n)));

    if ((x1.min() == 0.0) && (x1.max() == 0.0)) {
      GECODE_ME_CHECK(x1.eq(home,0.0));
      return home.ES_SUBSUMED(*this);
    }
    
    if ((m_n % 2) == 0)
    {
      if (x0.min() >= 0)
        GECODE_ME_CHECK(x0.eq(home,nroot(x1.domain(),m_n)));
      else if (x0.max() <= 0)
        GECODE_ME_CHECK(x0.eq(home,-nroot(x1.domain(),m_n)));
      else
        GECODE_ME_CHECK(x0.eq(home,
                              hull(
                                  nroot(x1.domain(),m_n),
                                  -nroot(x1.domain(),m_n)
                              )
                        ));
    } else
      GECODE_ME_CHECK(x0.eq(home,nroot(x1.domain(),m_n)));
    return x0.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

  /*
   * Bounds consistent square root operator
   *
   */

  template<class A, class B>
  forceinline
  NthRoot<A,B>::NthRoot(Home home, A x0, B x1, int n)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1), m_n(n) {}

  template<class A, class B>
  ExecStatus
  NthRoot<A,B>::post(Home home, A x0, B x1, int n) {
    if (n == 0) return ES_FAILED;
    GECODE_ME_CHECK(x0.gq(home,0.0));
    (void) new (home) NthRoot<A,B>(home,x0,x1,n);
    return ES_OK;
  }

  template<class A, class B>
  forceinline
  NthRoot<A,B>::NthRoot(Space& home, bool share, NthRoot<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p), m_n(p.m_n) {}

  template<class A, class B>
  Actor*
  NthRoot<A,B>::copy(Space& home, bool share) {
    return new (home) NthRoot<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  NthRoot<A,B>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x1.eq(home,nroot(x0.domain(),m_n)));
    GECODE_ME_CHECK(x0.eq(home,pow(x1.domain(),m_n)));
    return x0.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }


}}}

// STATISTICS: float-prop

