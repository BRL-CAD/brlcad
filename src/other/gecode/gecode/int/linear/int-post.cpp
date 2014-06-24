/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

#include <algorithm>

#include <gecode/int/rel.hh>
#include <gecode/int/linear.hh>
#include <gecode/int/div.hh>

namespace Gecode { namespace Int { namespace Linear {

  /// Eliminate assigned views
  forceinline void
  eliminate(Term<IntView>* t, int &n, long long int& d) {
    for (int i=n; i--; )
      if (t[i].x.assigned()) {
        long long int ax = t[i].a * static_cast<long long int>(t[i].x.val());
        if (Limits::overflow_sub(d,ax))
          throw OutOfLimits("Int::linear");
        d=d-ax; t[i]=t[--n];
      }
  }

  /// Rewrite all inequations in terms of IRT_LQ
  forceinline void
  rewrite(IntRelType &irt, long long int &d,
          Term<IntView>* &t_p, int &n_p,
          Term<IntView>* &t_n, int &n_n) {
    switch (irt) {
    case IRT_EQ: case IRT_NQ: case IRT_LQ:
      break;
    case IRT_LE:
      d--; irt = IRT_LQ;
      break;
    case IRT_GR:
      d++;
      /* fall through */
    case IRT_GQ:
      irt = IRT_LQ;
      std::swap(n_p,n_n); std::swap(t_p,t_n); d = -d;
      break;
    default:
      throw UnknownRelation("Int::linear");
    }
  }

  /// Decide the required precision and check for overflow
  forceinline bool
  precision(Term<IntView>* t_p, int n_p,
            Term<IntView>* t_n, int n_n,
            long long int d) {
    long long int sl = 0;
    long long int su = 0;

    for (int i = n_p; i--; ) {
      long long int axmin = 
        t_p[i].a * static_cast<long long int>(t_p[i].x.min());
      if (Limits::overflow_add(sl,axmin))
        throw OutOfLimits("Int::linear");
      sl = sl + axmin;
      long long int axmax = 
        t_p[i].a * static_cast<long long int>(t_p[i].x.max());
      if (Limits::overflow_add(sl,axmax))
        throw OutOfLimits("Int::linear");
      su = su + axmax;
    }
    for (int i = n_n; i--; ) {
      long long int axmax = 
        t_n[i].a * static_cast<long long int>(t_n[i].x.max());
      if (Limits::overflow_sub(sl,axmax))
        throw OutOfLimits("Int::linear");
      sl = sl - axmax;
      long long int axmin = 
        t_n[i].a * static_cast<long long int>(t_n[i].x.min());
      if (Limits::overflow_sub(su,axmin))
        throw OutOfLimits("Int::linear");
      su = su - axmin;
    }

    bool is_ip = (sl >= Limits::min) && (su <= Limits::max);

    if (Limits::overflow_sub(sl,d))
      throw OutOfLimits("Int::linear");
    sl = sl - d;
    if (Limits::overflow_sub(su,d))
      throw OutOfLimits("Int::linear");
    su = su - d;

    is_ip = is_ip && (sl >= Limits::min) && (su <= Limits::max);

    for (int i = n_p; i--; ) {
      long long int axmin = 
        t_p[i].a * static_cast<long long int>(t_p[i].x.min());
      if (Limits::overflow_sub(sl,axmin))
        throw OutOfLimits("Int::linear");
      if (sl - axmin < Limits::min)
        is_ip = false;
      long long int axmax = 
        t_p[i].a * static_cast<long long int>(t_p[i].x.max());
      if (Limits::overflow_sub(su,axmax))
        throw OutOfLimits("Int::linear");
      if (su - axmax > Limits::max)
        is_ip = false;
    }
    for (int i = n_n; i--; ) {
      long long int axmin = 
        t_n[i].a * static_cast<long long int>(t_n[i].x.min());
      if (Limits::overflow_add(sl,axmin))
        throw OutOfLimits("Int::linear");
      if (sl + axmin < Limits::min)
        is_ip = false;
      long long int axmax = 
        t_n[i].a * static_cast<long long int>(t_n[i].x.max());
      if (Limits::overflow_add(su,axmax))
        throw OutOfLimits("Int::linear");
      if (su + axmax > Limits::max)
        is_ip = false;
    }
    return is_ip;
  }

  /**
   * \brief Posting n-ary propagators
   *
   */
  template<class Val, class View>
  forceinline void
  post_nary(Home home,
            ViewArray<View>& x, ViewArray<View>& y, IntRelType irt, Val c) {
    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL((Eq<Val,View,View >::post(home,x,y,c)));
      break;
    case IRT_NQ:
      GECODE_ES_FAIL((Nq<Val,View,View >::post(home,x,y,c)));
      break;
    case IRT_LQ:
      GECODE_ES_FAIL((Lq<Val,View,View >::post(home,x,y,c)));
      break;
    default: GECODE_NEVER;
    }
  }


/// Macro for posting binary special cases for linear constraints
#define GECODE_INT_PL_BIN(CLASS)                                             \
  switch (n_p) {                                                             \
  case 2:                                                                    \
    GECODE_ES_FAIL((CLASS<int,IntView,IntView>::post                         \
                         (home,t_p[0].x,t_p[1].x,c)));                       \
    break;                                                                   \
  case 1:                                                                    \
    GECODE_ES_FAIL((CLASS<int,IntView,MinusView>::post                       \
                         (home,t_p[0].x,MinusView(t_n[0].x),c)));            \
    break;                                                                   \
  case 0:                                                                    \
    GECODE_ES_FAIL((CLASS<int,MinusView,MinusView>::post                     \
                         (home,MinusView(t_n[0].x),MinusView(t_n[1].x),c))); \
    break;                                                                   \
  default: GECODE_NEVER;                                                     \
  }

/// Macro for posting ternary special cases for linear constraints
#define GECODE_INT_PL_TER(CLASS)                                        \
  switch (n_p) {                                                        \
  case 3:                                                               \
    GECODE_ES_FAIL((CLASS<int,IntView,IntView,IntView>::post            \
                         (home,t_p[0].x,t_p[1].x,t_p[2].x,c)));         \
    break;                                                              \
  case 2:                                                               \
    GECODE_ES_FAIL((CLASS<int,IntView,IntView,MinusView>::post          \
                         (home,t_p[0].x,t_p[1].x,                       \
                          MinusView(t_n[0].x),c)));                     \
    break;                                                              \
  case 1:                                                               \
    GECODE_ES_FAIL((CLASS<int,IntView,MinusView,MinusView>::post        \
                         (home,t_p[0].x,                                \
                          MinusView(t_n[0].x),MinusView(t_n[1].x),c))); \
    break;                                                              \
  case 0:                                                               \
    GECODE_ES_FAIL((CLASS<int,MinusView,MinusView,MinusView>::post      \
                         (home,MinusView(t_n[0].x),                     \
                          MinusView(t_n[1].x),MinusView(t_n[2].x),c))); \
    break;                                                              \
  default: GECODE_NEVER;                                                \
  }

  void
  post(Home home, Term<IntView>* t, int n, IntRelType irt, int c,
       IntConLevel icl) {

    Limits::check(c,"Int::linear");

    long long int d = c;

    eliminate(t,n,d);

    Term<IntView> *t_p, *t_n;
    int n_p, n_n, gcd=1;
    bool is_unit = normalize<IntView>(t,n,t_p,n_p,t_n,n_n,gcd);

    rewrite(irt,d,t_p,n_p,t_n,n_n);

    // Divide by gcd
    if (gcd > 1) {
      switch (irt) {
      case IRT_EQ:
        if ((d % gcd) != 0) {
          home.fail();
          return;
        }
        d /= gcd;
        break;
      case IRT_NQ: 
        if ((d % gcd) != 0)
          return;
        d /= gcd;
        break;
      case IRT_LQ:
        d = floor_div_xp(d,static_cast<long long int>(gcd));
        break;
      default: GECODE_NEVER;
      }
    }

    if (n == 0) {
      switch (irt) {
      case IRT_EQ: if (d != 0) home.fail(); break;
      case IRT_NQ: if (d == 0) home.fail(); break;
      case IRT_LQ: if (d < 0)  home.fail(); break;
      default: GECODE_NEVER;
      }
      return;
    }

    if (n == 1) {
      if (n_p == 1) {
        LLongScaleView y(t_p[0].a,t_p[0].x);
        switch (irt) {
        case IRT_EQ: GECODE_ME_FAIL(y.eq(home,d)); break;
        case IRT_NQ: GECODE_ME_FAIL(y.nq(home,d)); break;
        case IRT_LQ: GECODE_ME_FAIL(y.lq(home,d)); break;
        default: GECODE_NEVER;
        }
      } else {
        LLongScaleView y(t_n[0].a,t_n[0].x);
        switch (irt) {
        case IRT_EQ: GECODE_ME_FAIL(y.eq(home,-d)); break;
        case IRT_NQ: GECODE_ME_FAIL(y.nq(home,-d)); break;
        case IRT_LQ: GECODE_ME_FAIL(y.gq(home,-d)); break;
        default: GECODE_NEVER;
        }
      }
      return;
    }

    bool is_ip = precision(t_p,n_p,t_n,n_n,d);

    if (is_unit && is_ip && (icl != ICL_DOM)) {
      // Unit coefficients with integer precision
      c = static_cast<int>(d);
      if (n == 2) {
        switch (irt) {
        case IRT_EQ: GECODE_INT_PL_BIN(EqBin); break;
        case IRT_NQ: GECODE_INT_PL_BIN(NqBin); break;
        case IRT_LQ: GECODE_INT_PL_BIN(LqBin); break;
        default: GECODE_NEVER;
        }
      } else if (n == 3) {
        switch (irt) {
        case IRT_EQ: GECODE_INT_PL_TER(EqTer); break;
        case IRT_NQ: GECODE_INT_PL_TER(NqTer); break;
        case IRT_LQ: GECODE_INT_PL_TER(LqTer); break;
        default: GECODE_NEVER;
        }
      } else {
        ViewArray<IntView> x(home,n_p);
        for (int i = n_p; i--; )
          x[i] = t_p[i].x;
        ViewArray<IntView> y(home,n_n);
        for (int i = n_n; i--; )
          y[i] = t_n[i].x;
        post_nary<int,IntView>(home,x,y,irt,c);
      }
    } else if (is_ip) {
      if ((n==2) && is_unit && (icl == ICL_DOM) && (irt == IRT_EQ)) {
        // Binary domain-consistent equality
        c = static_cast<int>(d);
        if (c == 0) {
          switch (n_p) {
          case 2: {
            IntView x(t_p[0].x);
            MinusView y(t_p[1].x);
            GECODE_ES_FAIL(
              (Rel::EqDom<IntView,MinusView>::post(home,x,y)));
            break;
          }
          case 1: {
            IntView x(t_p[0].x);
            IntView y(t_n[0].x);
            GECODE_ES_FAIL(
              (Rel::EqDom<IntView,IntView>::post(home,x,y)));
            break;
          }
          case 0: {
            IntView x(t_n[0].x);
            MinusView y(t_n[1].x);
            GECODE_ES_FAIL(
              (Rel::EqDom<IntView,MinusView>::post(home,x,y)));
            break;
          }
          default:
            GECODE_NEVER;
          }
        } else {
          switch (n_p) {
          case 2: {
            MinusView x(t_p[0].x);
            OffsetView y(t_p[1].x, -c);
            GECODE_ES_FAIL(
              (Rel::EqDom<MinusView,OffsetView>::post(home,x,y)));
            break;
          }
          case 1: {
            IntView x(t_p[0].x);
            OffsetView y(t_n[0].x, c);
            GECODE_ES_FAIL(
              (Rel::EqDom<IntView,OffsetView>::post(home,x,y)));
            break;
          }
          case 0: {
            MinusView x(t_n[0].x);
            OffsetView y(t_n[1].x, c);
            GECODE_ES_FAIL(
              (Rel::EqDom<MinusView,OffsetView>::post(home,x,y)));
            break;
          }
          default:
            GECODE_NEVER;
          }          
        }
      } else {
        // Arbitrary coefficients with integer precision
        c = static_cast<int>(d);
        ViewArray<IntScaleView> x(home,n_p);
        for (int i = n_p; i--; )
          x[i] = IntScaleView(t_p[i].a,t_p[i].x);
        ViewArray<IntScaleView> y(home,n_n);
        for (int i = n_n; i--; )
          y[i] = IntScaleView(t_n[i].a,t_n[i].x);
        if ((icl == ICL_DOM) && (irt == IRT_EQ)) {
          GECODE_ES_FAIL((DomEq<int,IntScaleView>::post(home,x,y,c)));
        } else {
          post_nary<int,IntScaleView>(home,x,y,irt,c);
        }
      }
    } else {
      // Arbitrary coefficients with long long precision
      ViewArray<LLongScaleView> x(home,n_p);
      for (int i = n_p; i--; )
        x[i] = LLongScaleView(t_p[i].a,t_p[i].x);
      ViewArray<LLongScaleView> y(home,n_n);
      for (int i = n_n; i--; )
        y[i] = LLongScaleView(t_n[i].a,t_n[i].x);
      if ((icl == ICL_DOM) && (irt == IRT_EQ)) {
        GECODE_ES_FAIL((DomEq<long long int,LLongScaleView>
                        ::post(home,x,y,d)));
      } else {
        post_nary<long long int,LLongScaleView>(home,x,y,irt,d);
      }
    }
  }

#undef GECODE_INT_PL_BIN
#undef GECODE_INT_PL_TER


  /**
   * \brief Posting reified n-ary propagators
   *
   */
  template<class Val, class View>
  forceinline void
  post_nary(Home home,
            ViewArray<View>& x, ViewArray<View>& y,
            IntRelType irt, Val c, Reify r) {
    switch (irt) {
    case IRT_EQ:
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((ReEq<Val,View,View,BoolView,RM_EQV>::
                        post(home,x,y,c,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((ReEq<Val,View,View,BoolView,RM_IMP>::
                        post(home,x,y,c,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((ReEq<Val,View,View,BoolView,RM_PMI>::
                        post(home,x,y,c,r.var())));
        break;
      default: GECODE_NEVER;
      }
      break;
    case IRT_NQ:
      {
        NegBoolView n(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReEq<Val,View,View,NegBoolView,RM_EQV>::
                          post(home,x,y,c,n)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReEq<Val,View,View,NegBoolView,RM_PMI>::
                          post(home,x,y,c,n)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReEq<Val,View,View,NegBoolView,RM_IMP>::
                          post(home,x,y,c,n)));
          break;
        default: GECODE_NEVER;
        }
      }
      break;
    case IRT_LQ:
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReLq<Val,View,View,RM_EQV>::
                          post(home,x,y,c,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReLq<Val,View,View,RM_IMP>::
                          post(home,x,y,c,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReLq<Val,View,View,RM_PMI>::
                          post(home,x,y,c,r.var())));
          break;
        default: GECODE_NEVER;
        }
      break;
    default: GECODE_NEVER;
    }
  }

  template<class CtrlView>
  forceinline void
  posteqint(Home home, IntView& x, int c, CtrlView b, ReifyMode rm,
            IntConLevel icl) {
    if (icl == ICL_DOM) {
      switch (rm) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReEqDomInt<IntView,CtrlView,RM_EQV>::
                        post(home,x,c,b)));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReEqDomInt<IntView,CtrlView,RM_IMP>::
                        post(home,x,c,b)));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReEqDomInt<IntView,CtrlView,RM_PMI>::
                        post(home,x,c,b)));
        break;
      default: GECODE_NEVER;
      }
    } else {
      switch (rm) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReEqBndInt<IntView,CtrlView,RM_EQV>::
                        post(home,x,c,b)));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReEqBndInt<IntView,CtrlView,RM_IMP>::
                        post(home,x,c,b)));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReEqBndInt<IntView,CtrlView,RM_PMI>::
                        post(home,x,c,b)));
        break;
      default: GECODE_NEVER;
      }
    }
  }

  void
  post(Home home,
       Term<IntView>* t, int n, IntRelType irt, int c, Reify r,
       IntConLevel icl) {
    Limits::check(c,"Int::linear");
    long long int d = c;

    eliminate(t,n,d);

    Term<IntView> *t_p, *t_n;
    int n_p, n_n, gcd=1;
    bool is_unit = normalize<IntView>(t,n,t_p,n_p,t_n,n_n,gcd);

    rewrite(irt,d,t_p,n_p,t_n,n_n);

    // Divide by gcd
    if (gcd > 1) {
      switch (irt) {
      case IRT_EQ:
        if ((d % gcd) != 0) {
          if (r.mode() != RM_PMI)
            GECODE_ME_FAIL(BoolView(r.var()).zero(home));
          return;
        }
        d /= gcd;
        break;
      case IRT_NQ: 
        if ((d % gcd) != 0) {
          if (r.mode() != RM_IMP)
            GECODE_ME_FAIL(BoolView(r.var()).one(home));
          return;
        }
        d /= gcd;
        break;
      case IRT_LQ:
        d = floor_div_xp(d,static_cast<long long int>(gcd));
        break;
      default: GECODE_NEVER;
      }
    }

    if (n == 0) {
      bool fail = false;
      switch (irt) {
      case IRT_EQ: fail = (d != 0); break;
      case IRT_NQ: fail = (d == 0); break;
      case IRT_LQ: fail = (0 > d); break;
      default: GECODE_NEVER;
      }
      if (fail) {
        if (r.mode() != RM_PMI)
          GECODE_ME_FAIL(BoolView(r.var()).zero(home));
      } else {
        if (r.mode() != RM_IMP)
          GECODE_ME_FAIL(BoolView(r.var()).one(home));
      }
      return;
    }

    bool is_ip = precision(t_p,n_p,t_n,n_n,d);

    if (is_unit && is_ip) {
      c = static_cast<int>(d);
      if (n == 1) {
        switch (irt) {
        case IRT_EQ:
          if (n_p == 1) {
            posteqint<BoolView>(home,t_p[0].x,c,r.var(),r.mode(),icl);
          } else {
            posteqint<BoolView>(home,t_p[0].x,-c,r.var(),r.mode(),icl);
          }
          break;
        case IRT_NQ:
          {
            NegBoolView nb(r.var());
            ReifyMode rm = r.mode();
            switch (rm) {
            case RM_IMP: rm = RM_PMI; break;
            case RM_PMI: rm = RM_IMP; break;
            default: ;
            }
            if (n_p == 1) {
              posteqint<NegBoolView>(home,t_p[0].x,c,nb,rm,icl);
            } else {
              posteqint<NegBoolView>(home,t_p[0].x,-c,nb,rm,icl);
            }
          }
          break;
        case IRT_LQ:
          if (n_p == 1) {
            switch (r.mode()) {
            case RM_EQV:
              GECODE_ES_FAIL((Rel::ReLqInt<IntView,BoolView,RM_EQV>::
                              post(home,t_p[0].x,c,r.var())));
              break;
            case RM_IMP:
              GECODE_ES_FAIL((Rel::ReLqInt<IntView,BoolView,RM_IMP>::
                              post(home,t_p[0].x,c,r.var())));
              break;
            case RM_PMI:
              GECODE_ES_FAIL((Rel::ReLqInt<IntView,BoolView,RM_PMI>::
                              post(home,t_p[0].x,c,r.var())));
              break;
            default: GECODE_NEVER;
            }
          } else {
            NegBoolView nb(r.var());
            switch (r.mode()) {
            case RM_EQV:
              GECODE_ES_FAIL((Rel::ReLqInt<IntView,NegBoolView,RM_EQV>::
                              post(home,t_n[0].x,-c-1,nb)));
              break;
            case RM_IMP:
              GECODE_ES_FAIL((Rel::ReLqInt<IntView,NegBoolView,RM_PMI>::
                              post(home,t_n[0].x,-c-1,nb)));
              break;
            case RM_PMI:
              GECODE_ES_FAIL((Rel::ReLqInt<IntView,NegBoolView,RM_IMP>::
                              post(home,t_n[0].x,-c-1,nb)));
              break;
            default: GECODE_NEVER;
            }
          }
          break;
        default: GECODE_NEVER;
        }
      } else if (n == 2) {
        switch (irt) {
        case IRT_EQ:
          switch (n_p) {
          case 2:
            switch (r.mode()) {
            case RM_EQV:
              GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,BoolView,RM_EQV>::
                              post(home,t_p[0].x,t_p[1].x,c,r.var())));
              break;
            case RM_IMP:
              GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,BoolView,RM_IMP>::
                              post(home,t_p[0].x,t_p[1].x,c,r.var())));
              break;
            case RM_PMI:
              GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,BoolView,RM_PMI>::
                              post(home,t_p[0].x,t_p[1].x,c,r.var())));
              break;
            default: GECODE_NEVER;
            }
            break;
          case 1:
            switch (r.mode()) {
            case RM_EQV:
              GECODE_ES_FAIL((ReEqBin<int,IntView,MinusView,BoolView,RM_EQV>::
                              post(home,t_p[0].x,MinusView(t_n[0].x),c,
                                   r.var())));
              break;
            case RM_IMP:
              GECODE_ES_FAIL((ReEqBin<int,IntView,MinusView,BoolView,RM_IMP>::
                              post(home,t_p[0].x,MinusView(t_n[0].x),c,
                                   r.var())));
              break;
            case RM_PMI:
              GECODE_ES_FAIL((ReEqBin<int,IntView,MinusView,BoolView,RM_PMI>::
                              post(home,t_p[0].x,MinusView(t_n[0].x),c,
                                   r.var())));
              break;
            default: GECODE_NEVER;
            }
            break;
          case 0:
            switch (r.mode()) {
            case RM_EQV:
              GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,BoolView,RM_EQV>::
                              post(home,t_n[0].x,t_n[1].x,-c,r.var())));
              break;
            case RM_IMP:
              GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,BoolView,RM_IMP>::
                              post(home,t_n[0].x,t_n[1].x,-c,r.var())));
              break;
            case RM_PMI:
              GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,BoolView,RM_PMI>::
                              post(home,t_n[0].x,t_n[1].x,-c,r.var())));
              break;
            default: GECODE_NEVER;
            }
            break;
          default: GECODE_NEVER;
          }
          break;
        case IRT_NQ:
          {
            NegBoolView nb(r.var());
            switch (n_p) {
            case 2:
              switch (r.mode()) {
              case RM_EQV:
                GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,NegBoolView,RM_EQV>::
                                post(home,t_p[0].x,t_p[1].x,c,nb)));
                break;
              case RM_IMP:
                GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,NegBoolView,RM_PMI>::
                                post(home,t_p[0].x,t_p[1].x,c,nb)));
                break;
              case RM_PMI:
                GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,NegBoolView,RM_IMP>::
                                post(home,t_p[0].x,t_p[1].x,c,nb)));
                break;
              default: GECODE_NEVER;
              }
              break;
            case 1:
              switch (r.mode()) {
              case RM_EQV:
                GECODE_ES_FAIL((ReEqBin<int,IntView,MinusView,NegBoolView,RM_EQV>::
                                post(home,t_p[0].x,MinusView(t_n[0].x),c,nb)));
                break;
              case RM_IMP:
                GECODE_ES_FAIL((ReEqBin<int,IntView,MinusView,NegBoolView,RM_PMI>::
                                post(home,t_p[0].x,MinusView(t_n[0].x),c,nb)));
                break;
              case RM_PMI:
                GECODE_ES_FAIL((ReEqBin<int,IntView,MinusView,NegBoolView,RM_IMP>::
                                post(home,t_p[0].x,MinusView(t_n[0].x),c,nb)));
                break;
              default: GECODE_NEVER;
              }
              break;
            case 0:
              switch (r.mode()) {
              case RM_EQV:
                GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,NegBoolView,RM_EQV>::
                                post(home,t_p[0].x,t_p[1].x,-c,nb)));
                break;
              case RM_IMP:
                GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,NegBoolView,RM_PMI>::
                                post(home,t_p[0].x,t_p[1].x,-c,nb)));
                break;
              case RM_PMI:
                GECODE_ES_FAIL((ReEqBin<int,IntView,IntView,NegBoolView,RM_IMP>::
                                post(home,t_p[0].x,t_p[1].x,-c,nb)));
                break;
              default: GECODE_NEVER;
              }
              break;
            default: GECODE_NEVER;
            }
          }
          break;
        case IRT_LQ:
          switch (n_p) {
          case 2:
            switch (r.mode()) {
            case RM_EQV:
              GECODE_ES_FAIL((ReLqBin<int,IntView,IntView,RM_EQV>::
                              post(home,t_p[0].x,t_p[1].x,c,r.var())));
              break;
            case RM_IMP:
              GECODE_ES_FAIL((ReLqBin<int,IntView,IntView,RM_IMP>::
                              post(home,t_p[0].x,t_p[1].x,c,r.var())));
              break;
            case RM_PMI:
              GECODE_ES_FAIL((ReLqBin<int,IntView,IntView,RM_PMI>::
                              post(home,t_p[0].x,t_p[1].x,c,r.var())));
              break;
            default: GECODE_NEVER;
            }
            break;
          case 1:
            switch (r.mode()) {
            case RM_EQV:
              GECODE_ES_FAIL((ReLqBin<int,IntView,MinusView,RM_EQV>::
                              post(home,t_p[0].x,MinusView(t_n[0].x),c,
                                   r.var())));
              break;
            case RM_IMP:
              GECODE_ES_FAIL((ReLqBin<int,IntView,MinusView,RM_IMP>::
                              post(home,t_p[0].x,MinusView(t_n[0].x),c,
                                   r.var())));
              break;
            case RM_PMI:
              GECODE_ES_FAIL((ReLqBin<int,IntView,MinusView,RM_PMI>::
                              post(home,t_p[0].x,MinusView(t_n[0].x),c,
                                   r.var())));
              break;
            default: GECODE_NEVER;
            }
            break;
          case 0:
            switch (r.mode()) {
            case RM_EQV:
              GECODE_ES_FAIL((ReLqBin<int,MinusView,MinusView,RM_EQV>::
                              post(home,MinusView(t_n[0].x),
                                   MinusView(t_n[1].x),c,r.var())));
              break;
            case RM_IMP:
              GECODE_ES_FAIL((ReLqBin<int,MinusView,MinusView,RM_IMP>::
                              post(home,MinusView(t_n[0].x),
                                   MinusView(t_n[1].x),c,r.var())));
              break;
            case RM_PMI:
              GECODE_ES_FAIL((ReLqBin<int,MinusView,MinusView,RM_PMI>::
                              post(home,MinusView(t_n[0].x),
                                   MinusView(t_n[1].x),c,r.var())));
              break;
            default: GECODE_NEVER;
            }
            break;
          default: GECODE_NEVER;
          }
          break;
        default: GECODE_NEVER;
        }
      } else {
        ViewArray<IntView> x(home,n_p);
        for (int i = n_p; i--; )
          x[i] = t_p[i].x;
        ViewArray<IntView> y(home,n_n);
        for (int i = n_n; i--; )
          y[i] = t_n[i].x;
        post_nary<int,IntView>(home,x,y,irt,c,r);
      }
    } else if (is_ip) {
      // Arbitrary coefficients with integer precision
      c = static_cast<int>(d);
      ViewArray<IntScaleView> x(home,n_p);
      for (int i = n_p; i--; )
        x[i] = IntScaleView(t_p[i].a,t_p[i].x);
      ViewArray<IntScaleView> y(home,n_n);
      for (int i = n_n; i--; )
        y[i] = IntScaleView(t_n[i].a,t_n[i].x);
      post_nary<int,IntScaleView>(home,x,y,irt,c,r);
    } else {
      // Arbitrary coefficients with long long precision
      ViewArray<LLongScaleView> x(home,n_p);
      for (int i = n_p; i--; )
        x[i] = LLongScaleView(t_p[i].a,t_p[i].x);
      ViewArray<LLongScaleView> y(home,n_n);
      for (int i = n_n; i--; )
        y[i] = LLongScaleView(t_n[i].a,t_n[i].x);
      post_nary<long long int,LLongScaleView>(home,x,y,irt,d,r);
    }
  }

}}}

// STATISTICS: int-post
