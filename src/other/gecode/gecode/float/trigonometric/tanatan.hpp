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
   * ATan projection function
   *
   */
  template<class V>
  void 
  aTanProject(Rounding& r, const V& aTanIv, FloatNum& iv_min, FloatNum& iv_max, int& n_min, int& n_max) {
    #define I0__PI_2I    FloatVal(0,pi_half_upper())
    #define POS(X) ((I0__PI_2I.in(X))?0:1)
    #define ATANINF_DOWN r.atan_down(aTanIv.min())
    #define ATANSUP_UP   r.atan_up(aTanIv.max())
    
    // 0 <=> in [0;PI/2]
    // 1 <=> in [PI/2;PI]
    switch ( POS(iv_min) )
    {
      case 0:
        if (r.tan_down(iv_min) > aTanIv.max())    { n_min++; iv_min = ATANINF_DOWN; }
        else if (r.tan_up(iv_min) < aTanIv.min()) {          iv_min = ATANINF_DOWN; }
        break;
      case 1:
        if (r.tan_down(iv_min) > aTanIv.max())    { n_min+=2; iv_min = ATANINF_DOWN; }
        else if (r.tan_up(iv_min) < aTanIv.min()) { n_min++;  iv_min = ATANINF_DOWN; }
        break;
      default:
        GECODE_NEVER;
        break;
    }
    
    // 0 <=> in [0;PI/2]
    // 1 <=> in [PI/2;PI]
    switch ( POS(iv_max) )
    {
      case 0:
        if (r.tan_down(iv_max) > aTanIv.max())    {          iv_max = ATANSUP_UP; }
        else if (r.tan_up(iv_max) < aTanIv.min()) { n_max--; iv_max = ATANSUP_UP; }
        break;
      case 1:
        if (r.tan_down(iv_max) > aTanIv.max())    { n_max++; iv_max = ATANSUP_UP; }
        else if (r.tan_up(iv_max) < aTanIv.min()) {          iv_max = ATANSUP_UP; }
        break;
      default:
        GECODE_NEVER;
        break;
    }
    #undef ATANINF_DOWN
    #undef ATANSUP_UP
    #undef POS
    #undef I0__PI_2I
  }

  /*
   * Bounds consistent tangent operator
   *
   */

  template<class A, class B>
  forceinline
  Tan<A,B>::Tan(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1) {}

  template<class A, class B>
  ExecStatus
  Tan<A,B>::post(Home home, A x0, B x1) {
    if (same(x0,x1)) {
      #define I0__PI_2I    FloatVal(0,pi_half_upper())
      if (I0__PI_2I.in(x0.max()))  GECODE_ME_CHECK(x0.lq(home,0));
      if (I0__PI_2I.in(-x0.min())) GECODE_ME_CHECK(x0.gq(home,0));
      #undef I0__PI_2I
    }

    (void) new (home) Tan<A,B>(home,x0,x1);
    return ES_OK;
  }

  template<class A, class B>
  forceinline
  Tan<A,B>::Tan(Space& home, bool share, Tan<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  Tan<A,B>::copy(Space& home, bool share) {
    return new (home) Tan<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  Tan<A,B>::propagate(Space& home, const ModEventDelta&) {
    Rounding r;
    int n_min = static_cast<int>(r.div_up(x0.min() + pi_half_upper(), pi_upper()));
    int n_max = static_cast<int>(r.div_up(x0.max() + pi_half_upper(), pi_upper()));

    if (same(x0,x1)) {
      #define I0__PI_2I    FloatVal(0,pi_half_upper())
      if (I0__PI_2I.in(x0.max()))  GECODE_ME_CHECK(x0.lq(home,0));
      if (I0__PI_2I.in(-x0.min())) GECODE_ME_CHECK(x0.gq(home,0));
      #undef I0__PI_2I
      
      n_min = static_cast<int>(r.div_up(x0.min(), pi_upper()));
      n_max = static_cast<int>(r.div_up(x0.max(), pi_upper()));

      FloatNum x0_min;
      FloatNum x0_max;
      FloatNum t = x0.min();
      do {
        x0_min = t;
        if (r.tan_down(x0_min) > x0_min) n_min++;
        t = r.add_down(r.mul_up(n_min,pi_upper()),r.tan_down(x0_min));
      } while (t > x0_min);
      t = r.sub_down(r.mul_up(2*n_max,pi_upper()),x0.max());
      do {
        x0_max = t;
        if (r.tan_down(x0_max) < x0_max) n_max--;
        t = r.add_up(r.mul_up(n_max,pi_upper()),r.tan_up(x0_max));
      } while (t > x0_max);
      x0_max = r.sub_up(r.mul_up(2*n_max,pi_upper()),x0_max);
      
      if (x0_min > x0_max) return ES_FAILED;
      GECODE_ME_CHECK(x0.eq(home,FloatVal(x0_min,x0_max)));      
    } else {
      GECODE_ME_CHECK(x1.eq(home,tan(x0.val())));
      n_min = static_cast<int>(r.div_up(x0.min(), pi_upper()));
      n_max = static_cast<int>(r.div_up(x0.max(), pi_upper()));
      if (x0.min() < 0) n_min--;
      if (x0.max() < 0) n_max--;
      FloatNum iv_min = r.sub_down(x0.min(),r.mul_down(n_min, pi_upper()));
      FloatNum iv_max = r.sub_up  (x0.max(),r.mul_down(n_max, pi_upper()));
      aTanProject(r,x1,iv_min,iv_max,n_min,n_max);
      FloatNum n_iv_min = r.add_down(iv_min,r.mul_down(n_min, pi_upper()));
      FloatNum n_iv_max = r.add_up  (iv_max,r.mul_down(n_max, pi_upper()));
      if (n_iv_min > n_iv_max) return ES_FAILED;
      GECODE_ME_CHECK(x0.eq(home,FloatVal(n_iv_min,n_iv_max)));
      GECODE_ME_CHECK(x1.eq(home,tan(x0.val()))); // Redo tan because with x0 reduction, sin may be more accurate
    }

    return (x0.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

  /*
   * Bounds consistent arc tangent operator
   *
   */

  template<class A, class B>
  forceinline
  ATan<A,B>::ATan(Home home, A x0, B x1)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,x0,x1) {}

  template<class A, class B>
  ExecStatus
  ATan<A,B>::post(Home home, A x0, B x1) {
    if (same(x0,x1)) {
      GECODE_ME_CHECK(x0.eq(home,0.0));
    } else {
      (void) new (home) ATan<A,B>(home,x0,x1);
    }
    return ES_OK;
  }


  template<class A, class B>
  forceinline
  ATan<A,B>::ATan(Space& home, bool share, ATan<A,B>& p)
    : MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>(home,share,p) {}

  template<class A, class B>
  Actor*
  ATan<A,B>::copy(Space& home, bool share) {
    return new (home) ATan<A,B>(home,share,*this);
  }

  template<class A, class B>
  ExecStatus
  ATan<A,B>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x1.eq(home,atan(x0.domain())));
    GECODE_ME_CHECK(x0.eq(home,tan(x1.domain())));
    return (x0.assigned() && x1.assigned()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

}}}

// STATISTICS: float-prop

