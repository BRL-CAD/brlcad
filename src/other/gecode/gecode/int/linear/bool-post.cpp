/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Tias Guns <tias.guns@cs.kuleuven.be>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Tias Guns, 2009
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

#include <gecode/int/linear.hh>
#include <gecode/int/div.hh>

namespace Gecode { namespace Int { namespace Linear {

  /// Inverse the relation
  forceinline IntRelType 
  inverse(const IntRelType irt) {
    switch (irt) {
      case IRT_EQ: return IRT_NQ; break;
      case IRT_NQ: return IRT_EQ; break;
      case IRT_GQ: return IRT_LE; break;
      case IRT_LQ: return IRT_GR; break;
      case IRT_LE: return IRT_GQ; break;
      case IRT_GR: return IRT_LQ; break;
      default: GECODE_NEVER;
    }
    return IRT_EQ; // Avoid compiler warnings
  }

  /// Eliminate assigned views
  forceinline void
  eliminate(Term<BoolView>* t, int &n, long long int& d) {
    for (int i=n; i--; )
      if (t[i].x.one()) {
        d -= t[i].a; t[i]=t[--n];
      } else if (t[i].x.zero()) {
        t[i]=t[--n];
      }
    Limits::check(d,"Int::linear");
  }

  /// Rewrite non-strict relations
  forceinline void
  rewrite(IntRelType &r, long long int &d) {
      switch (r) {
      case IRT_EQ: case IRT_NQ: case IRT_LQ: case IRT_GQ:
        break;
      case IRT_LE:
        d--; r = IRT_LQ; break;
      case IRT_GR:
        d++; r = IRT_GQ; break;
      default:
        throw UnknownRelation("Int::linear");
      }
  }

  forceinline void
  post_pos_unit(Home home,
                Term<BoolView>* t_p, int n_p,
                IntRelType irt, IntView y, int c) {
    switch (irt) {
    case IRT_EQ:
      {
        ViewArray<BoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=t_p[i].x;
        GECODE_ES_FAIL((EqBoolView<BoolView,IntView>
                             ::post(home,x,y,c)));
      }
      break;
    case IRT_NQ:
      {
        ViewArray<BoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=t_p[i].x;
        GECODE_ES_FAIL((NqBoolView<BoolView,IntView>
                             ::post(home,x,y,c)));
      }
      break;
    case IRT_GQ:
      {
        ViewArray<BoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=t_p[i].x;
        GECODE_ES_FAIL((GqBoolView<BoolView,IntView>
                             ::post(home,x,y,c)));
      }
      break;
    case IRT_LQ:
      {
        ViewArray<NegBoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=NegBoolView(t_p[i].x);
        MinusView z(y);
        GECODE_ES_FAIL((GqBoolView<NegBoolView,MinusView>
                             ::post(home,x,z,n_p-c)));
      }
      break;
    default: GECODE_NEVER;
    }
  }

  forceinline void
  post_pos_unit(Home home,
                Term<BoolView>* t_p, int n_p,
                IntRelType irt, ZeroIntView, int c) {
    switch (irt) {
    case IRT_EQ:
      {
        ViewArray<BoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=t_p[i].x;
        GECODE_ES_FAIL((EqBoolInt<BoolView>::post(home,x,c)));
      }
      break;
    case IRT_NQ:
      {
        ViewArray<BoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=t_p[i].x;
        GECODE_ES_FAIL((NqBoolInt<BoolView>::post(home,x,c)));
      }
      break;
    case IRT_GQ:
      {
        ViewArray<BoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=t_p[i].x;
        GECODE_ES_FAIL((GqBoolInt<BoolView>::post(home,x,c)));
      }
      break;
    case IRT_LQ:
      {
        ViewArray<NegBoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=NegBoolView(t_p[i].x);
        GECODE_ES_FAIL((GqBoolInt<NegBoolView>::post(home,x,n_p-c)));
      }
      break;
    default: GECODE_NEVER;
    }
  }

  forceinline void
  post_pos_unit(Home home,
                Term<BoolView>* t_p, int n_p,
                IntRelType irt, int c, Reify r,
                IntConLevel) {
    switch (irt) {
    case IRT_EQ:
      {
        ViewArray<BoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=t_p[i].x;
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,BoolView,RM_EQV>::
                          post(home,x,c,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,BoolView,RM_IMP>::
                          post(home,x,c,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,BoolView,RM_PMI>::
                          post(home,x,c,r.var())));
          break;
        default: GECODE_NEVER;
        }
      }
      break;
    case IRT_NQ:
      {
        ViewArray<BoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=t_p[i].x;
        NegBoolView nb(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,NegBoolView,RM_EQV>::
                          post(home,x,c,nb)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,NegBoolView,RM_PMI>::
                          post(home,x,c,nb)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,NegBoolView,RM_IMP>::
                          post(home,x,c,nb)));
          break;
        default: GECODE_NEVER;
        }
      }
      break;
    case IRT_GQ:
      {
        ViewArray<BoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=t_p[i].x;
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReGqBoolInt<BoolView,BoolView,RM_EQV>::
                          post(home,x,c,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReGqBoolInt<BoolView,BoolView,RM_IMP>::
                          post(home,x,c,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReGqBoolInt<BoolView,BoolView,RM_PMI>::
                          post(home,x,c,r.var())));
          break;
        default: GECODE_NEVER;
        }
      }
      break;
    case IRT_LQ:
      {
        ViewArray<NegBoolView> x(home,n_p);
        for (int i=n_p; i--; )
          x[i]=NegBoolView(t_p[i].x);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReGqBoolInt<NegBoolView,BoolView,RM_EQV>::
                          post(home,x,n_p-c,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReGqBoolInt<NegBoolView,BoolView,RM_IMP>::
                          post(home,x,n_p-c,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReGqBoolInt<NegBoolView,BoolView,RM_PMI>::
                          post(home,x,n_p-c,r.var())));
          break;
        default: GECODE_NEVER;
        }
      }
      break;
    default: GECODE_NEVER;
    }
  }

  forceinline void
  post_neg_unit(Home home,
                Term<BoolView>* t_n, int n_n,
                IntRelType irt, IntView y, int c) {
    switch (irt) {
    case IRT_EQ:
      {
        ViewArray<BoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=t_n[i].x;
        MinusView z(y);
        GECODE_ES_FAIL((EqBoolView<BoolView,MinusView>
                        ::post(home,x,z,-c)));
      }
      break;
    case IRT_NQ:
      {
        ViewArray<BoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=t_n[i].x;
        MinusView z(y);
        GECODE_ES_FAIL((NqBoolView<BoolView,MinusView>
                        ::post(home,x,z,-c)));
      }
      break;
    case IRT_GQ:
      {
        ViewArray<NegBoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=NegBoolView(t_n[i].x);
        GECODE_ES_FAIL((GqBoolView<NegBoolView,IntView>
                        ::post(home,x,y,n_n+c)));
      }
      break;
    case IRT_LQ:
      {
        ViewArray<BoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=t_n[i].x;
        MinusView z(y);
        GECODE_ES_FAIL((GqBoolView<BoolView,MinusView>
                        ::post(home,x,z,-c)));
      }
      break;
    default: GECODE_NEVER;
    }
  }

  forceinline void
  post_neg_unit(Home home,
                Term<BoolView>* t_n, int n_n,
                IntRelType irt, ZeroIntView, int c) {
    switch (irt) {
    case IRT_EQ:
      {
        ViewArray<BoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=t_n[i].x;
        GECODE_ES_FAIL((EqBoolInt<BoolView>::post(home,x,-c)));
      }
      break;
    case IRT_NQ:
      {
        ViewArray<BoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=t_n[i].x;
        GECODE_ES_FAIL((NqBoolInt<BoolView>::post(home,x,-c)));
      }
      break;
    case IRT_GQ:
      {
        ViewArray<NegBoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=NegBoolView(t_n[i].x);
        GECODE_ES_FAIL((GqBoolInt<NegBoolView>::post(home,x,n_n+c)));
      }
      break;
    case IRT_LQ:
      {
        ViewArray<BoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=t_n[i].x;
        GECODE_ES_FAIL((GqBoolInt<BoolView>::post(home,x,-c)));
      }
      break;
    default: GECODE_NEVER;
    }
  }

  forceinline void
  post_neg_unit(Home home,
                Term<BoolView>* t_n, int n_n,
                IntRelType irt, int c, Reify r,
                IntConLevel) {
    switch (irt) {
    case IRT_EQ:
      {
        ViewArray<BoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=t_n[i].x;
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,BoolView,RM_EQV>::
                          post(home,x,-c,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,BoolView,RM_IMP>::
                          post(home,x,-c,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,BoolView,RM_PMI>::
                          post(home,x,-c,r.var())));
          break;
        default: GECODE_NEVER;
        }
      }
      break;
    case IRT_NQ:
      {
        ViewArray<BoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=t_n[i].x;
        NegBoolView nb(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,NegBoolView,RM_EQV>::
                          post(home,x,-c,nb)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,NegBoolView,RM_PMI>::
                          post(home,x,-c,nb)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReEqBoolInt<BoolView,NegBoolView,RM_IMP>::
                          post(home,x,-c,nb)));
          break;
        default: GECODE_NEVER;
        }
      }
      break;
    case IRT_GQ:
      {
        ViewArray<NegBoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=NegBoolView(t_n[i].x);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReGqBoolInt<NegBoolView,BoolView,RM_EQV>::
                          post(home,x,n_n+c,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReGqBoolInt<NegBoolView,BoolView,RM_IMP>::
                          post(home,x,n_n+c,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReGqBoolInt<NegBoolView,BoolView,RM_PMI>::
                          post(home,x,n_n+c,r.var())));
          break;
        default: GECODE_NEVER;
        }
      }
      break;
    case IRT_LQ:
      {
        ViewArray<BoolView> x(home,n_n);
        for (int i=n_n; i--; )
          x[i]=t_n[i].x;
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((ReGqBoolInt<BoolView,BoolView,RM_EQV>::
                          post(home,x,-c,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReGqBoolInt<BoolView,BoolView,RM_IMP>::
                          post(home,x,-c,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReGqBoolInt<BoolView,BoolView,RM_PMI>::
                          post(home,x,-c,r.var())));
          break;
        default: GECODE_NEVER;
        }
      }
      break;
    default: GECODE_NEVER;
    }
  }

  forceinline void
  post_mixed(Home home,
             Term<BoolView>* t_p, int n_p,
             Term<BoolView>* t_n, int n_n,
             IntRelType irt, IntView y, int c) {
    ScaleBoolArray b_p(home,n_p);
    {
      ScaleBool* f=b_p.fst();
      for (int i=n_p; i--; ) {
        f[i].x=t_p[i].x; f[i].a=t_p[i].a;
      }
    }
    ScaleBoolArray b_n(home,n_n);
    {
      ScaleBool* f=b_n.fst();
      for (int i=n_n; i--; ) {
        f[i].x=t_n[i].x; f[i].a=t_n[i].a;
      }
    }
    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL((EqBoolScale<ScaleBoolArray,ScaleBoolArray,IntView>
                      ::post(home,b_p,b_n,y,c)));
      break;
    case IRT_NQ:
      GECODE_ES_FAIL((NqBoolScale<ScaleBoolArray,ScaleBoolArray,IntView>
                      ::post(home,b_p,b_n,y,c)));
      break;
    case IRT_LQ:
      GECODE_ES_FAIL((LqBoolScale<ScaleBoolArray,ScaleBoolArray,IntView>
                      ::post(home,b_p,b_n,y,c)));
      break;
    case IRT_GQ:
      {
        MinusView m(y);
        GECODE_ES_FAIL((LqBoolScale<ScaleBoolArray,ScaleBoolArray,MinusView>
                        ::post(home,b_n,b_p,m,-c)));
      }
      break;
    default:
      GECODE_NEVER;
    }
  }


  forceinline void
  post_mixed(Home home,
             Term<BoolView>* t_p, int n_p,
             Term<BoolView>* t_n, int n_n,
             IntRelType irt, ZeroIntView y, int c) {
    ScaleBoolArray b_p(home,n_p);
    {
      ScaleBool* f=b_p.fst();
      for (int i=n_p; i--; ) {
        f[i].x=t_p[i].x; f[i].a=t_p[i].a;
      }
    }
    ScaleBoolArray b_n(home,n_n);
    {
      ScaleBool* f=b_n.fst();
      for (int i=n_n; i--; ) {
        f[i].x=t_n[i].x; f[i].a=t_n[i].a;
      }
    }
    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL(
                     (EqBoolScale<ScaleBoolArray,ScaleBoolArray,ZeroIntView>
                      ::post(home,b_p,b_n,y,c)));
      break;
    case IRT_NQ:
      GECODE_ES_FAIL(
                     (NqBoolScale<ScaleBoolArray,ScaleBoolArray,ZeroIntView>
                      ::post(home,b_p,b_n,y,c)));
      break;
    case IRT_LQ:
      GECODE_ES_FAIL(
                     (LqBoolScale<ScaleBoolArray,ScaleBoolArray,ZeroIntView>
                      ::post(home,b_p,b_n,y,c)));
      break;
    case IRT_GQ:
      GECODE_ES_FAIL(
                     (LqBoolScale<ScaleBoolArray,ScaleBoolArray,ZeroIntView>
                      ::post(home,b_n,b_p,y,-c)));
      break;
    default:
      GECODE_NEVER;
    }
  }

  template<class View>
  forceinline void
  post_all(Home home,
           Term<BoolView>* t, int n,
           IntRelType irt, View x, int c) {

    Limits::check(c,"Int::linear");

    long long int d = c;

    eliminate(t,n,d);

    Term<BoolView> *t_p, *t_n;
    int n_p, n_n, gcd=0;
    bool unit = normalize<BoolView>(t,n,t_p,n_p,t_n,n_n,gcd);

    rewrite(irt,d);

    c = static_cast<int>(d);

    if (n == 0) {
      switch (irt) {
      case IRT_EQ: GECODE_ME_FAIL(x.eq(home,-c)); break;
      case IRT_NQ: GECODE_ME_FAIL(x.nq(home,-c)); break;
      case IRT_GQ: GECODE_ME_FAIL(x.lq(home,-c)); break;
      case IRT_LQ: GECODE_ME_FAIL(x.gq(home,-c)); break;
      default: GECODE_NEVER;
      }
      return;
    }

    // Check for overflow
    {
      long long int sl = static_cast<long long int>(x.max())+c;
      long long int su = static_cast<long long int>(x.min())+c;
      for (int i=n_p; i--; )
        su -= t_p[i].a;
      for (int i=n_n; i--; )
        sl += t_n[i].a;
      Limits::check(sl,"Int::linear");
      Limits::check(su,"Int::linear");
    }

    if (unit && (n_n == 0)) {
      /// All coefficients are 1
      post_pos_unit(home,t_p,n_p,irt,x,c);
    } else if (unit && (n_p == 0)) {
      // All coefficients are -1
      post_neg_unit(home,t_n,n_n,irt,x,c);
    } else {
      // Mixed coefficients
      post_mixed(home,t_p,n_p,t_n,n_n,irt,x,c);
    }
  }


  void
  post(Home home,
       Term<BoolView>* t, int n, IntRelType irt, IntView x, int c,
       IntConLevel) {
    post_all(home,t,n,irt,x,c);
  }

  void
  post(Home home,
       Term<BoolView>* t, int n, IntRelType irt, int c,
       IntConLevel) {
    ZeroIntView x;
    post_all(home,t,n,irt,x,c);
  }

  void
  post(Home home,
       Term<BoolView>* t, int n, IntRelType irt, IntView x, Reify r,
       IntConLevel icl) {
    int l, u;
    estimate(t,n,0,l,u);
    IntVar z(home,l,u); IntView zv(z);
    post_all(home,t,n,IRT_EQ,zv,0);
    rel(home,z,irt,x,r,icl);
  }

  void
  post(Home home,
       Term<BoolView>* t, int n, IntRelType irt, int c, Reify r,
       IntConLevel icl) {

    if (r.var().one()) {
      if (r.mode() != RM_PMI)
        post(home,t,n,irt,c,icl);
      return;
    }
    if (r.var().zero()) {
      if (r.mode() != RM_IMP)
        post(home,t,n,inverse(irt),c,icl);
      return;
    }

    Limits::check(c,"Int::linear");

    long long int d = c;

    eliminate(t,n,d);

    Term<BoolView> *t_p, *t_n;
    int n_p, n_n, gcd=1;
    bool unit = normalize<BoolView>(t,n,t_p,n_p,t_n,n_n,gcd);

    rewrite(irt,d);

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
        if ((d % gcd) == 0) {
          if (r.mode() != RM_IMP)
            GECODE_ME_FAIL(BoolView(r.var()).one(home));
          return;
        }
        d /= gcd;
        break;
      case IRT_LQ:
        d = floor_div_xp(d,static_cast<long long int>(gcd));
        break;
      case IRT_GQ:
        d = ceil_div_xp(d,static_cast<long long int>(gcd));
        break;
      default: GECODE_NEVER;
      }
    }

    c = static_cast<int>(d);

    if (n == 0) {
      bool fail = false;
      switch (irt) {
      case IRT_EQ: fail = (0 != c); break;
      case IRT_NQ: fail = (0 == c); break;
      case IRT_GQ: fail = (0 < c); break;
      case IRT_LQ: fail = (0 > c); break;
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

    // Check for overflow
    {
      long long int sl = c;
      long long int su = c;
      for (int i=n_p; i--; )
        su -= t_p[i].a;
      for (int i=n_n; i--; )
        sl += t_n[i].a;
      Limits::check(sl,"Int::linear");
      Limits::check(su,"Int::linear");
    }

    if (unit && (n_n == 0)) {
      /// All coefficients are 1
      post_pos_unit(home,t_p,n_p,irt,c,r,icl);
    } else if (unit && (n_p == 0)) {
      // All coefficients are -1
      post_neg_unit(home,t_n,n_n,irt,c,r,icl);
    } else {
      // Mixed coefficients
      /*
       * Denormalize: Make all t_n coefficients negative again
       * (t_p and t_n are shared in t)
       */
      for (int i=n_n; i--; )
        t_n[i].a = -t_n[i].a;

      // Note: still slow implementation
      int l, u;
      estimate(t,n,0,l,u);
      IntVar z(home,l,u); IntView zv(z);
      post_all(home,t,n,IRT_EQ,zv,0);
      rel(home,z,irt,c,r,icl);
    }
  }

}}}

// STATISTICS: int-post

