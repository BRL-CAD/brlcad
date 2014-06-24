/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2003
 *     Gabor Szokoli, 2003
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

namespace Gecode { namespace Float { namespace Rel {

  /*
   * Less or equal propagator
   *
   */

  template<class View>
  forceinline
  Lq<View>::Lq(Home home, View x0, View x1)
    : BinaryPropagator<View,PC_FLOAT_BND>(home,x0,x1) {}

  template<class View>
  ExecStatus
  Lq<View>::post(Home home, View x0, View x1) {
    GECODE_ME_CHECK(x0.lq(home,x1.max()));
    GECODE_ME_CHECK(x1.gq(home,x0.min()));
    if (!same(x0,x1) && (x0.max() > x1.min()))
      (void) new (home) Lq<View>(home,x0,x1);
    return ES_OK;
  }

  template<class View>
  forceinline
  Lq<View>::Lq(Space& home, bool share, Lq<View>& p)
    : BinaryPropagator<View,PC_FLOAT_BND>(home,share,p) {}

  template<class View>
  Actor*
  Lq<View>::copy(Space& home, bool share) {
    return new (home) Lq<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  Lq<View>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x0.lq(home,x1.max()));
    GECODE_ME_CHECK(x1.gq(home,x0.min()));
    return (x0.assigned() || x1.assigned() || (x0.max() <= x1.min())) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }


  /*
   * Less propagator
   *
   */
  template<class View>
  forceinline
  Le<View>::Le(Home home, View x0, View x1)
    : BinaryPropagator<View,PC_FLOAT_BND>(home,x0,x1) {}

  template<class View>
  ExecStatus
  Le<View>::post(Home home, View x0, View x1) {
    GECODE_ME_CHECK(x0.lq(home,x1.max()));
    GECODE_ME_CHECK(x1.gq(home,x0.min()));
    if (x0.assigned() && x1.assigned()) {
      if (x0.max() >= x1.min())
        return ES_FAILED;
    } else if (same(x0,x1))
      return ES_FAILED;
    if (x0.max() >= x1.min())
      (void) new (home) Le<View>(home,x0,x1);
    return ES_OK;
  }

  template<class View>
  forceinline
  Le<View>::Le(Space& home, bool share, Le<View>& p)
    : BinaryPropagator<View,PC_FLOAT_BND>(home,share,p) {}

  template<class View>
  Actor*
  Le<View>::copy(Space& home, bool share) {
    return new (home) Le<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  Le<View>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x0.lq(home,x1.max()));
    GECODE_ME_CHECK(x1.gq(home,x0.min()));
    if (x0.assigned() && x1.assigned()) {
      return (x0.max() >= x1.min()) ? ES_FAILED : home.ES_SUBSUMED(*this);
    } 
    return (x0.max() < x1.min()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }




  /*
   * Reified less or equal propagator
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLq<View,CtrlView,rm>::ReLq(Home home, View x0, View x1, CtrlView b)
    : Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,x0,x1,b) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLq<View,CtrlView,rm>::post(Home home, View x0, View x1, CtrlView b) {
    if (b.one()) {
      if (rm == RM_PMI)
        return ES_OK;
      return Lq<View>::post(home,x0,x1);
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return ES_OK;
      return Le<View>::post(home,x1,x0);
    }
    if (!same(x0,x1)) {
      switch (rtest_lq(x0,x1)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home)); 
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home)); 
        break;
      case RT_MAYBE:
        if (!x0.assigned() || !x1.assigned())
          (void) new (home) ReLq<View,CtrlView,rm>(home,x0,x1,b);
        else {
          if (rm != RM_IMP)
            GECODE_ME_CHECK(b.one_none(home)); 
        }
        break;
      default: GECODE_NEVER;
      }
    } else if (rm != RM_IMP) {
      GECODE_ME_CHECK(b.one_none(home));
    }
    return ES_OK;
  }

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLq<View,CtrlView,rm>::ReLq(Space& home, bool share, ReLq& p)
    : Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,share,p) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReLq<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReLq<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLq<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_REWRITE(*this,Lq<View>::post(home(*this),x0,x1));
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_REWRITE(*this,Le<View>::post(home(*this),x1,x0));
    } else {
      switch (rtest_lq(x0,x1)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home)); 
        break;
      case RT_MAYBE:
        if (!x0.assigned() || !x1.assigned())
          return ES_FIX;
        else {
          if (rm != RM_IMP)
            GECODE_ME_CHECK(b.one_none(home));
          break;
        }
      default: GECODE_NEVER;
      }
    }
    return home.ES_SUBSUMED(*this);
  }

  /*
   * Reified less or equal propagator involving one variable
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLqFloat<View,CtrlView,rm>::ReLqFloat(Home home, View x, FloatVal c0, CtrlView b)
    : Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,x,b), c(c0) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLqFloat<View,CtrlView,rm>::post(Home home, View x, FloatVal c, CtrlView b) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x.lq(home,c.max()));
    } if (b.zero()) {
      if (rm != RM_IMP) {
        GECODE_ME_CHECK(x.gq(home,c.min()));
        if (x.assigned() && (x.min() <= c.max()))
          return ES_FAILED;
        (void) new (home) ReLqFloat<View,CtrlView,rm>(home,x,c,b);
      }
    } else {
      switch (rtest_lq(x,c)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one(home));
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        break;
      case RT_MAYBE:
        (void) new (home) ReLqFloat<View,CtrlView,rm>(home,x,c,b);
        break;
      default: GECODE_NEVER;
      }
    }
    return ES_OK;
  }


  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLqFloat<View,CtrlView,rm>::ReLqFloat(Space& home, bool share, ReLqFloat& p)
    : Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,share,p), c(p.c) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReLqFloat<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReLqFloat<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLqFloat<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x0.lq(home,c.max()));
    } else if (b.zero()) {
      if (rm != RM_IMP)
      {
        GECODE_ME_CHECK(x0.gq(home,c.min()));
        if (x0.assigned()) {
          return (x0.min() <= c.max()) ? ES_FAILED : home.ES_SUBSUMED(*this);
        } 
      }
    } else {
      switch (rtest_lq(x0,c)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one(home));
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        break;
      case RT_MAYBE:
        return ES_FIX;
      default: GECODE_NEVER;
      }
    }
    return home.ES_SUBSUMED(*this);
  }


  /*
   * Reified less
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLeFloat<View,CtrlView,rm>::ReLeFloat(Home home, View x, FloatVal c0, CtrlView b)
    : Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,x,b), c(c0) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLeFloat<View,CtrlView,rm>::post(Home home, View x, FloatVal c, CtrlView b) {
    if (b.one()) {
      if (rm != RM_PMI)
      {
        GECODE_ME_CHECK(x.lq(home,c.max()));
        if (x.assigned() && (x.max() >= c.min()))
          return ES_FAILED;
        (void) new (home) ReLeFloat<View,CtrlView,rm>(home,x,c,b);
      }
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(x.gq(home,c.min()));
    } else {
      switch (rtest_le(x,c)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one(home));
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        break;
      case RT_MAYBE:
        (void) new (home) ReLeFloat<View,CtrlView,rm>(home,x,c,b);
        break;
      default: GECODE_NEVER;
      }
    }
    return ES_OK;
  }


  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLeFloat<View,CtrlView,rm>::ReLeFloat(Space& home, bool share, ReLeFloat& p)
    : Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>(home,share,p), c(p.c) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReLeFloat<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReLeFloat<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLeFloat<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm != RM_PMI)
      {
        GECODE_ME_CHECK(x0.lq(home,c.max()));
        if (x0.assigned()) {
          return (x0.max() >= c.min()) ? ES_FAILED : home.ES_SUBSUMED(*this);
        } 
      }
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(x0.gq(home,c.min()));
    } else {
      switch (rtest_le(x0,c)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one(home));
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        break;
      case RT_MAYBE:
        return ES_FIX;
      default: GECODE_NEVER;
      }
    }
    return home.ES_SUBSUMED(*this);
  }

}}}

// STATISTICS: float-prop

