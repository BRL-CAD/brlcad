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
   * ASin projection function
   *
   */
template<class V>
void aSinProject(Rounding& r, const V& aSinIv, FloatNum& iv_min, FloatNum& iv_max, int& n_min, int& n_max) {
  #define I0__PI_2I    FloatVal(0,pi_half_upper())
  #define IPI_2__PII   FloatVal(pi_half_lower(),pi_upper())
  #define IPI__3PI_2I  FloatVal(pi_lower(),3*pi_half_upper())
  #define I3PI_2__2PII FloatVal(3*pi_half_lower(),pi_twice_upper())
  #define POS(X) ((I0__PI_2I.in(X))?0: (IPI_2__PII.in(X))?1: (IPI__3PI_2I.in(X))?2: 3 )
  #define ASININF_DOWN r.asin_down(aSinIv.min())
  #define ASINSUP_UP r.asin_up(aSinIv.max())
  
  // 0 <=> in [0;PI/2]
  // 1 <=> in [PI/2;PI]
  // 2 <=> in [PI;3*PI/2]
  // 3 <=> in [3*PI/2;2*PI]
  switch ( POS(iv_min) )
  {
    case 0:
      if (r.sin_down(iv_min) > aSinIv.max())    { n_min++; iv_min = -ASINSUP_UP;   }
      else if (r.sin_up(iv_min) < aSinIv.min()) {          iv_min = ASININF_DOWN;  }
    break;
    case 1:
      if (r.sin_down(iv_min) > aSinIv.max())    { n_min++;  iv_min = -ASINSUP_UP;   }
      else if (r.sin_up(iv_min) < aSinIv.min()) { n_min+=2; iv_min = ASININF_DOWN;  }
    break;
    case 2:
      if (r.sin_down(iv_min) > aSinIv.max())    { n_min++;  iv_min = -ASINSUP_UP;   }
      else if (r.sin_up(iv_min) < aSinIv.min()) { n_min+=2; iv_min = ASININF_DOWN; }
    break;
    case 3:
      if (r.sin_down(iv_min) > aSinIv.max())    { n_min+=3; iv_min = -ASINSUP_UP;    }
      else if (r.sin_up(iv_min) < aSinIv.min()) { n_min+=2; iv_min = ASININF_DOWN; }
    break;
    default:
      GECODE_NEVER;
    break;
  }

  // 0 <=> in [0;PI/2]
  // 1 <=> in [PI/2;PI]
  // 2 <=> in [PI;3*PI/2]
  // 3 <=> in [3*PI/2;2*PI]
  switch ( POS(iv_max) )
  {
    case 0:
      if (r.sin_down(iv_max) > aSinIv.max())    {           iv_max = ASINSUP_UP;    }
      else if (r.sin_up(iv_max) < aSinIv.min()) { n_max--;  iv_max = -ASININF_DOWN; }
    break;
    case 1:
      if (r.sin_down(iv_max) > aSinIv.max())    {          iv_max = ASINSUP_UP;    }
      else if (r.sin_up(iv_max) < aSinIv.min()) { n_max++; iv_max = -ASININF_DOWN; }
    break;
    case 2:
      if (r.sin_down(iv_max) > aSinIv.max())    {          iv_max = ASINSUP_UP;    }
      else if (r.sin_up(iv_max) < aSinIv.min()) { n_max++; iv_max = -ASININF_DOWN; }
    break;
    case 3:
      if (r.sin_down(iv_max) > aSinIv.max())    { n_max+=2; iv_max = ASINSUP_UP;    }
      else if (r.sin_up(iv_max) < aSinIv.min()) { n_max++;  iv_max = -ASININF_DOWN; }
    break;
    default:
      GECODE_NEVER;
    break;
  }
  #undef ASININF_DOWN
  #undef ASINSUP_UP
  #undef POS
  #undef I0__PI_2I
  #undef IPI_2__PII
  #undef IPI__3PI_2I
  #undef I3PI_2__2PII
}

/*
   * Bounds consistent sinus operator
   *
   */

  template<class A, class B>
  forceinline
  Sin<A,B>::Sin(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1) {}

  template<class A, class B>
  ExecStatus
  Sin<A,B>::post(Home home, A x0, B x1) {
    if (same(x0,x1)) {
      GECODE_ME_CHECK(x0.eq(home,0.0));
    } else {
      GECODE_ME_CHECK(x1.gq(home,-1.0));
      GECODE_ME_CHECK(x1.lq(home,1.0));
      (void) new (home) Sin<A,B>(home,x0,x1);
    }
    
    return ES_OK;
  }


  template<class A, class B>
  forceinline
  Sin<A,B>::Sin(Space& home, bool share, Sin<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  Sin<A,B>::copy(Space& home, bool share) {
    return new (home) Sin<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  Sin<A,B>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x1.eq(home,sin(x0.val())));
    Rounding r;
    int n_min = 2*static_cast<int>(r.div_up(x0.min(), pi_twice_upper()));
    int n_max = 2*static_cast<int>(r.div_up(x0.max(), pi_twice_upper()));
    if (x0.min() < 0) n_min-=2;
    if (x0.max() < 0) n_max-=2;
    FloatNum iv_min = r.sub_down(x0.min(),r.mul_down(n_min, pi_upper()));
    FloatNum iv_max = r.sub_up  (x0.max(),r.mul_down(n_max, pi_upper()));
    aSinProject(r,x1,iv_min,iv_max,n_min,n_max);
    FloatNum n_iv_min = r.add_down(iv_min,r.mul_down(n_min, pi_upper()));
    FloatNum n_iv_max = r.add_up  (iv_max,r.mul_down(n_max, pi_upper()));
    if (n_iv_min > n_iv_max) return ES_FAILED;
    GECODE_ME_CHECK(x0.eq(home,FloatVal(n_iv_min,n_iv_max)));
    GECODE_ME_CHECK(x1.eq(home,sin(x0.val()))); // Redo sin because with x0 reduction, sin may be more accurate
    return (x0.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

  /*
   * Bounds consistent cosinus operator
   *
   */

  template<class A, class B>
  forceinline
  Cos<A,B>::Cos(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1) {}

  template<class A, class B>
  ExecStatus
  Cos<A,B>::post(Home home, A x0, B x1) {
    if (same(x0,x1)) {
      GECODE_ME_CHECK(x0.gq(home,0.7390851332151));
      GECODE_ME_CHECK(x0.lq(home,0.7390851332152));
      bool mod;
      do {
        mod = false;
        GECODE_ME_CHECK_MODIFIED(mod,x0.eq(home,cos(x0.val())));
      } while (mod);
    } else {
      GECODE_ME_CHECK(x1.gq(home,-1.0));
      GECODE_ME_CHECK(x1.lq(home,1.0));
      (void) new (home) Cos<A,B>(home,x0,x1);
    }
    return ES_OK;
  }


  template<class A, class B>
  forceinline
  Cos<A,B>::Cos(Space& home, bool share, Cos<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  Cos<A,B>::copy(Space& home, bool share) {
    return new (home) Cos<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  Cos<A,B>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x1.eq(home,cos(x0.val())));
    Rounding r;
    FloatVal x0Trans = x0.val() + FloatVal::pi_half();
    int n_min = 2*static_cast<int>(r.div_up(x0Trans.min(), pi_twice_upper()));
    int n_max = 2*static_cast<int>(r.div_up(x0Trans.max(), pi_twice_upper()));
    if (x0Trans.min() < 0) n_min-=2;
    if (x0Trans.max() < 0) n_max-=2;
    FloatNum iv_min = r.sub_down(x0Trans.min(),r.mul_down(n_min, pi_upper()));
    FloatNum iv_max = r.sub_up  (x0Trans.max(),r.mul_down(n_max, pi_upper()));
    aSinProject(r,x1,iv_min,iv_max,n_min,n_max);
    FloatNum n_iv_min = r.add_down(iv_min,r.mul_down(n_min, pi_upper()));
    FloatNum n_iv_max = r.add_up  (iv_max,r.mul_down(n_max, pi_upper()));
    if (n_iv_min > n_iv_max) return ES_FAILED;
    GECODE_ME_CHECK(x0.eq(home,FloatVal(n_iv_min,n_iv_max) - FloatVal::pi_half()));
    GECODE_ME_CHECK(x1.eq(home,cos(x0.val()))); // Redo sin because with x0 reduction, sin may be more accurate
    return (x0.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

}}}

// STATISTICS: float-prop

