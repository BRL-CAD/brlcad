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

#include <gecode/int/bool.hh>
#include <gecode/int/rel.hh>

namespace Gecode {

  void
  rel(Home home, BoolVar x0, IntRelType irt, BoolVar x1, IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL((Bool::Eq<BoolView,BoolView>
                           ::post(home,x0,x1)));
      break;
    case IRT_NQ:
      {
        NegBoolView n1(x1);
        GECODE_ES_FAIL((Bool::Eq<BoolView,NegBoolView>
                             ::post(home,x0,n1)));
      }
      break;
    case IRT_GQ:
      GECODE_ES_FAIL(Bool::Lq<BoolView>::post(home,x1,x0));
      break;
    case IRT_LQ:
      GECODE_ES_FAIL(Bool::Lq<BoolView>::post(home,x0,x1));
      break;
    case IRT_GR:
      GECODE_ES_FAIL(Bool::Le<BoolView>::post(home,x1,x0));
      break;
    case IRT_LE:
      GECODE_ES_FAIL(Bool::Le<BoolView>::post(home,x0,x1));
      break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }

  void
  rel(Home home, BoolVar x0, IntRelType irt, int n, IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    BoolView x(x0);
    if (n == 0) {
      switch (irt) {
      case IRT_LQ:
      case IRT_EQ:
        GECODE_ME_FAIL(x.zero(home)); break;
      case IRT_NQ:
      case IRT_GR:
        GECODE_ME_FAIL(x.one(home)); break;
      case IRT_LE:
        home.fail(); break;
      case IRT_GQ:
        break;
      default:
        throw UnknownRelation("Int::rel");
      }
    } else if (n == 1) {
      switch (irt) {
      case IRT_GQ:
      case IRT_EQ:
        GECODE_ME_FAIL(x.one(home)); break;
      case IRT_NQ:
      case IRT_LE:
        GECODE_ME_FAIL(x.zero(home)); break;
      case IRT_GR:
        home.fail(); break;
      case IRT_LQ:
        break;
      default:
        throw UnknownRelation("Int::rel");
      }
    } else {
      throw NotZeroOne("Int::rel");
    }
  }

  void
  rel(Home home, BoolVar x0, IntRelType irt, BoolVar x1, Reify r,
      IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    switch (irt) {
    case IRT_EQ:
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Bool::Eqv<BoolView,BoolView,BoolView>
                        ::post(home,x0,x1,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReEqBnd<BoolView,BoolView,RM_IMP>
                        ::post(home,x0,x1,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReEqBnd<BoolView,BoolView,RM_PMI>
                        ::post(home,x0,x1,r.var())));
        break;
      default: throw UnknownReifyMode("Int::rel");
      }
      break;
    case IRT_NQ:
      {
        NegBoolView nr(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Bool::Eqv<BoolView,BoolView,NegBoolView>
                          ::post(home,x0,x1,nr)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Rel::ReEqBnd<BoolView,NegBoolView,RM_PMI>
                          ::post(home,x0,x1,nr)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Rel::ReEqBnd<BoolView,NegBoolView,RM_IMP>
                          ::post(home,x0,x1,nr)));
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
      }
      break;
    case IRT_GQ:
      std::swap(x0,x1); // Fall through
    case IRT_LQ:
      switch (r.mode()) {
      case RM_EQV:
        {
          NegBoolView n0(x0);
          GECODE_ES_FAIL((Bool::Or<NegBoolView,BoolView,BoolView>
                          ::post(home,n0,x1,r.var())));
        }
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReLq<BoolView,BoolView,RM_IMP>
                        ::post(home,x0,x1,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReLq<BoolView,BoolView,RM_PMI>
                        ::post(home,x0,x1,r.var())));
        break;
      default: throw UnknownReifyMode("Int::rel");
      }
      break;
    case IRT_LE:
      std::swap(x0,x1); // Fall through
    case IRT_GR:
      {
        NegBoolView nr(r.var());
        switch (r.mode()) {
        case RM_EQV:
          {
            NegBoolView n0(x0);
            GECODE_ES_FAIL((Bool::Or<NegBoolView,BoolView,NegBoolView>
                            ::post(home,n0,x1,nr)));
          }
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Rel::ReLq<BoolView,NegBoolView,RM_PMI>
                          ::post(home,x0,x1,nr)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Rel::ReLq<BoolView,NegBoolView,RM_IMP>
                          ::post(home,x0,x1,nr)));
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
      }
      break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }

  void
  rel(Home home, BoolVar x0, IntRelType irt, int n, Reify r,
      IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    BoolView x(x0);
    BoolView y(r.var());
    if (n == 0) {
      switch (irt) {
      case IRT_LQ:
      case IRT_EQ:
        switch (r.mode()) {
        case RM_EQV:
          {
            NegBoolView ny(y);
            GECODE_ES_FAIL((Bool::Eq<BoolView,NegBoolView>
                            ::post(home,x,ny)));
          }
          break;
        case RM_IMP:
          {
            NegBoolView nx(x); NegBoolView ny(y);
            GECODE_ES_FAIL((Bool::BinOrTrue<NegBoolView,NegBoolView>
                            ::post(home,nx,ny)));
          }
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Bool::BinOrTrue<BoolView,BoolView>
                          ::post(home,x,y)));
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
        break;
      case IRT_NQ:
      case IRT_GR:
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Bool::Eq<BoolView,BoolView>
                          ::post(home,x,y)));
          break;
        case RM_IMP:
          {
            NegBoolView ny(y);
            GECODE_ES_FAIL((Bool::BinOrTrue<BoolView,NegBoolView>
                            ::post(home,x,ny)));
          }
          break;
        case RM_PMI:
          {
            NegBoolView nx(x);
            GECODE_ES_FAIL((Bool::BinOrTrue<NegBoolView,BoolView>
                            ::post(home,nx,y)));
          }
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
        break;
      case IRT_LE:
        switch (r.mode()) {
        case RM_EQV:
        case RM_IMP:
          GECODE_ME_FAIL(y.zero(home));
          break;
        case RM_PMI:
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
        break;
      case IRT_GQ:
        switch (r.mode()) {
        case RM_EQV:
        case RM_PMI:
          GECODE_ME_FAIL(y.one(home));
          break;
        case RM_IMP:
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
        break;
      default:
        throw UnknownRelation("Int::rel");
      }
    } else if (n == 1) {
      switch (irt) {
      case IRT_NQ:
      case IRT_LE:
        switch (r.mode()) {
        case RM_EQV:
          {
            NegBoolView ny(y);
            GECODE_ES_FAIL((Bool::Eq<BoolView,NegBoolView>
                            ::post(home,x,ny)));
          }
          break;
        case RM_IMP:
          {
            NegBoolView nx(x); NegBoolView ny(y);
            GECODE_ES_FAIL((Bool::BinOrTrue<NegBoolView,NegBoolView>
                            ::post(home,nx,ny)));
          }
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Bool::BinOrTrue<BoolView,BoolView>
                          ::post(home,x,y)));
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
        break;
      case IRT_EQ:
      case IRT_GQ:
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Bool::Eq<BoolView,BoolView>
                          ::post(home,x,y)));
          break;
        case RM_IMP:
          {
            NegBoolView ny(y);
            GECODE_ES_FAIL((Bool::BinOrTrue<BoolView,NegBoolView>
                            ::post(home,x,ny)));
          }
          break;
        case RM_PMI:
          {
            NegBoolView nx(x);
            GECODE_ES_FAIL((Bool::BinOrTrue<NegBoolView,BoolView>
                            ::post(home,nx,y)));
          }
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
        break;
      case IRT_GR:
        switch (r.mode()) {
        case RM_EQV:
        case RM_IMP:
          GECODE_ME_FAIL(y.zero(home));
          break;
        case RM_PMI:
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
        break;
      case IRT_LQ:
        switch (r.mode()) {
        case RM_EQV:
        case RM_PMI:
          GECODE_ME_FAIL(y.one(home));
          break;
        case RM_IMP:
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
        break;
      default:
        throw UnknownRelation("Int::rel");
      }
    } else {
      throw NotZeroOne("Int::rel");
    }
  }

  void
  rel(Home home, const BoolVarArgs& x, IntRelType irt, BoolVar y,
      IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    switch (irt) {
    case IRT_EQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL((Bool::Eq<BoolView,BoolView>
                             ::post(home,x[i],y)));
      }
      break;
    case IRT_NQ:
      {
        NegBoolView n(y);
        for (int i=x.size(); i--; ) {
          GECODE_ES_FAIL((Bool::Eq<BoolView,NegBoolView>
                               ::post(home,x[i],n)));
        }
      }
      break;
    case IRT_GQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL(Bool::Lq<BoolView>::post(home,y,x[i]));
      }
      break;
    case IRT_LQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL(Bool::Lq<BoolView>::post(home,x[i],y));
      }
      break;
    case IRT_GR:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL(Bool::Le<BoolView>::post(home,y,x[i]));
      }
      break;
    case IRT_LE:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL(Bool::Le<BoolView>::post(home,x[i],y));
      }
      break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }

  void
  rel(Home home, const BoolVarArgs& x, IntRelType irt, int n,
      IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    if (n == 0) {
      switch (irt) {
      case IRT_LQ:
      case IRT_EQ:
        for (int i=x.size(); i--; ) {
          BoolView xi(x[i]); GECODE_ME_FAIL(xi.zero(home));
        }
        break;
      case IRT_NQ:
      case IRT_GR:
        for (int i=x.size(); i--; ) {
          BoolView xi(x[i]); GECODE_ME_FAIL(xi.one(home));
        }
        break;
      case IRT_LE:
        home.fail(); break;
      case IRT_GQ:
        break;
      default:
        throw UnknownRelation("Int::rel");
      }
    } else if (n == 1) {
      switch (irt) {
      case IRT_GQ:
      case IRT_EQ:
        for (int i=x.size(); i--; ) {
          BoolView xi(x[i]); GECODE_ME_FAIL(xi.one(home));
        }
        break;
      case IRT_NQ:
      case IRT_LE:
        for (int i=x.size(); i--; ) {
          BoolView xi(x[i]); GECODE_ME_FAIL(xi.zero(home));
        }
        break;
      case IRT_GR:
        home.fail(); break;
      case IRT_LQ:
        break;
      default:
        throw UnknownRelation("Int::rel");
      }
    } else {
      throw NotZeroOne("Int::rel");
    }
  }

  void
  rel(Home home, const BoolVarArgs& x, IntRelType irt, IntConLevel) {
    using namespace Int;
    if (home.failed() || ((irt != IRT_NQ) && (x.size() < 2))) 
      return;

    switch (irt) {
    case IRT_EQ:
      {
        ViewArray<BoolView> y(home,x);
        GECODE_ES_FAIL(Bool::NaryEq<BoolView>::post(home,y));
      }
      break;
    case IRT_NQ:
      {
        ViewArray<BoolView> y(home,x);
        GECODE_ES_FAIL((Rel::NaryNq<BoolView>::post(home,y)));
      }
      break;
    case IRT_LE:
      if (x.size() == 2) {
        GECODE_ES_FAIL(Bool::Le<BoolView>::post(home,x[0],x[1]));
      } else {
        home.fail();
      }
      break;
    case IRT_LQ:
      {
        ViewArray<BoolView> y(home,x);
        GECODE_ES_FAIL(Bool::NaryLq<BoolView>::post(home,y));
      }
      break;
    case IRT_GR:
      if (x.size() == 2) {
        GECODE_ES_FAIL(Bool::Le<BoolView>::post(home,x[1],x[0]));
      } else {
        home.fail();
      }
      break;
    case IRT_GQ:
      {
        ViewArray<BoolView> y(home,x.size());
        for (int i=x.size(); i--; )
          y[i] = x[x.size()-1-i];
        GECODE_ES_FAIL(Bool::NaryLq<BoolView>::post(home,y));
      }
      for (int i=x.size()-1; i--; )
        GECODE_ES_FAIL(Bool::Lq<BoolView>::post(home,x[i+1],x[i]));
      break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }

  void
  rel(Home home, const BoolVarArgs& x, IntRelType irt, const BoolVarArgs& y,
      IntConLevel) {
    using namespace Int;
    if (x.size() != y.size())
      throw ArgumentSizeMismatch("Int::rel");
    if (home.failed()) return;

    switch (irt) {
    case IRT_GR:
      {
        ViewArray<BoolView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexLqLe<BoolView>::post(home,yv,xv,true));
      }
      break;
    case IRT_LE:
      {
        ViewArray<BoolView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexLqLe<BoolView>::post(home,xv,yv,true));
      }
      break;
    case IRT_GQ:
      {
        ViewArray<BoolView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexLqLe<BoolView>::post(home,yv,xv,false));
      }
      break;
    case IRT_LQ:
      {
        ViewArray<BoolView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexLqLe<BoolView>::post(home,xv,yv,false));
      }
      break;
    case IRT_EQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL((Bool::Eq<BoolView,BoolView>
                             ::post(home,x[i],y[i])));
      }
      break;
    case IRT_NQ:
      {
        ViewArray<BoolView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexNq<BoolView>::post(home,xv,yv));
      }
      break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }

  void
  rel(Home home, BoolVar x0, BoolOpType o, BoolVar x1, BoolVar x2,
      IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    switch (o) {
    case BOT_AND:
      {
        NegBoolView n0(x0); NegBoolView n1(x1); NegBoolView n2(x2);
        GECODE_ES_FAIL((Bool::Or<NegBoolView,NegBoolView,NegBoolView>
                        ::post(home,n0,n1,n2)));
      }
      break;
    case BOT_OR:
      GECODE_ES_FAIL((Bool::Or<BoolView,BoolView,BoolView>
                      ::post(home,x0,x1,x2)));
      break;
    case BOT_IMP: 
      {
        NegBoolView n0(x0);
        GECODE_ES_FAIL((Bool::Or<NegBoolView,BoolView,BoolView>
                        ::post(home,n0,x1,x2)));
      }
      break;
    case BOT_EQV:
      GECODE_ES_FAIL((Bool::Eqv<BoolView,BoolView,BoolView>
                      ::post(home,x0,x1,x2)));
      break;
    case BOT_XOR:
      {
        NegBoolView n2(x2);
        GECODE_ES_FAIL((Bool::Eqv<BoolView,BoolView,NegBoolView>
                        ::post(home,x0,x1,n2)));
      }
      break;
    default:
      throw UnknownOperation("Int::rel");
    }
  }

  void
  rel(Home home, BoolVar x0, BoolOpType o, BoolVar x1, int n,
      IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    if (n == 0) {
      switch (o) {
      case BOT_AND:
        {
          NegBoolView n0(x0); NegBoolView n1(x1);
          GECODE_ES_FAIL((Bool::BinOrTrue<NegBoolView,NegBoolView>
                               ::post(home,n0,n1)));
        }
        break;
      case BOT_OR:
        {
          BoolView b0(x0); BoolView b1(x1);
          GECODE_ME_FAIL(b0.zero(home));
          GECODE_ME_FAIL(b1.zero(home));
        }
        break;
      case BOT_IMP:
        {
          BoolView b0(x0); BoolView b1(x1);
          GECODE_ME_FAIL(b0.one(home));
          GECODE_ME_FAIL(b1.zero(home));
        }
        break;
      case BOT_EQV:
        {
          NegBoolView n0(x0);
          GECODE_ES_FAIL((Bool::Eq<NegBoolView,BoolView>::post(home,n0,x1)));
        }
        break;
      case BOT_XOR:
        GECODE_ES_FAIL((Bool::Eq<BoolView,BoolView>::post(home,x0,x1)));
        break;
      default:
        throw UnknownOperation("Int::rel");
      }
    } else if (n == 1) {
      switch (o) {
      case BOT_AND:
        {
          BoolView b0(x0); BoolView b1(x1);
          GECODE_ME_FAIL(b0.one(home));
          GECODE_ME_FAIL(b1.one(home));
        }
        break;
      case BOT_OR:
        GECODE_ES_FAIL((Bool::BinOrTrue<BoolView,BoolView>::post(home,x0,x1)));
        break;
      case BOT_IMP:
        {
          NegBoolView n0(x0);
          GECODE_ES_FAIL((Bool::BinOrTrue<NegBoolView,BoolView>
                          ::post(home,n0,x1)));
        }
        break;
      case BOT_EQV:
        GECODE_ES_FAIL((Bool::Eq<BoolView,BoolView>::post(home,x0,x1)));
        break;
      case BOT_XOR:
        {
          NegBoolView n0(x0);
          GECODE_ES_FAIL((Bool::Eq<NegBoolView,BoolView>::post(home,n0,x1)));
        }
        break;
      default:
        throw UnknownOperation("Int::rel");
      }
    } else {
      throw NotZeroOne("Int::rel");
    }
  }

  void
  rel(Home home, BoolOpType o, const BoolVarArgs& x, BoolVar y,
      IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    int m = x.size();
    Region r(home);
    switch (o) {
    case BOT_AND:
      {
        ViewArray<NegBoolView> b(home,m);
        for (int i=m; i--; ) {
          NegBoolView nb(x[i]); b[i]=nb;
        }
        NegBoolView ny(y);
        b.unique(home);
        GECODE_ES_FAIL((Bool::NaryOr<NegBoolView,NegBoolView>
                             ::post(home,b,ny)));
      }
      break;
    case BOT_OR:
      {
        ViewArray<BoolView> b(home,x);
        b.unique(home);
        GECODE_ES_FAIL((Bool::NaryOr<BoolView,BoolView>::post(home,b,y)));
      }
      break;
    case BOT_IMP:
      if (m < 2) {
        throw TooFewArguments("Int::rel");
      } else {
        ViewArray<NegBoolView> a(home,x.size()-1);
        for (int i=x.size()-1; i--; )
          a[i]=NegBoolView(x[i]);
        ViewArray<BoolView> b(home,1);
        b[0]=x[x.size()-1];
        GECODE_ES_FAIL((Bool::Clause<BoolView,NegBoolView>
                        ::post(home,b,a,y)));
      }
      break;
    case BOT_EQV:
      {
        ViewArray<BoolView> xy(home, x.size() + 1);
        for (int i=x.size(); i--; )
          xy[i] = x[i];
        xy[x.size()] = y;
        GECODE_ES_FAIL(Bool::NaryEqv::post(home,xy,0));
      }
      break;
    case BOT_XOR:
      {
        ViewArray<BoolView> xy(home, x.size() + 1);
        for (int i=x.size(); i--; )
          xy[i] = x[i];
        xy[x.size()] = y;
        GECODE_ES_FAIL(Bool::NaryEqv::post(home,xy,1));
      }
      break;
    default:
      throw UnknownOperation("Int::rel");
    }
  }

  void
  rel(Home home, BoolOpType o, const BoolVarArgs& x, int n,
      IntConLevel) {
    using namespace Int;
    if ((n < 0) || (n > 1))
      throw NotZeroOne("Int::rel");
    if (home.failed()) return;
    int m = x.size();
    Region r(home);
    switch (o) {
    case BOT_AND:
      if (n == 0) {
        ViewArray<NegBoolView> b(home,m);
        for (int i=m; i--; ) {
          NegBoolView nb(x[i]); b[i]=nb;
        }
        b.unique(home);
        GECODE_ES_FAIL(Bool::NaryOrTrue<NegBoolView>::post(home,b));
      } else {
        for (int i=m; i--; ) {
          BoolView b(x[i]); GECODE_ME_FAIL(b.one(home));
        }
      }
      break;
    case BOT_OR:
      if (n == 0) {
        for (int i=m; i--; ) {
          BoolView b(x[i]); GECODE_ME_FAIL(b.zero(home));
        }
      } else {
        ViewArray<BoolView> b(home,x);
        b.unique(home);
        GECODE_ES_FAIL(Bool::NaryOrTrue<BoolView>::post(home,b));
      }
      break;
    case BOT_IMP:
      if (m < 2) {
        throw TooFewArguments("Int::rel");
      } else if (n == 0) {
        for (int i=m-1; i--; )
          GECODE_ME_FAIL(BoolView(x[i]).one(home));
        GECODE_ME_FAIL(BoolView(x[m-1]).zero(home));
      } else {
        ViewArray<NegBoolView> a(home,x.size()-1);
        for (int i=x.size()-1; i--; )
          a[i]=NegBoolView(x[i]);
        ViewArray<BoolView> b(home,1);
        b[0]=x[x.size()-1];
        GECODE_ES_FAIL((Bool::ClauseTrue<BoolView,NegBoolView>
                        ::post(home,b,a)));
      }
      break;
    case BOT_EQV:
      {
        ViewArray<BoolView> b(home,x);
        GECODE_ES_FAIL(Bool::NaryEqv::post(home,b,n));
      }
      break;
    case BOT_XOR:
      {
        ViewArray<BoolView> b(home,x);
        GECODE_ES_FAIL(Bool::NaryEqv::post(home,b,1^n));
      }
      break;
    default:
      throw UnknownOperation("Int::rel");
    }
  }

  void
  clause(Home home, BoolOpType o, const BoolVarArgs& x, const BoolVarArgs& y,
         int n, IntConLevel) {
    using namespace Int;
    if ((n < 0) || (n > 1))
      throw NotZeroOne("Int::rel");
    if (home.failed()) return;
    switch (o) {
    case BOT_AND:
      if (n == 0) {
        ViewArray<NegBoolView> xv(home,x.size());
        for (int i=x.size(); i--; ) {
          NegBoolView n(x[i]); xv[i]=n;
        }
        ViewArray<BoolView> yv(home,y);
        xv.unique(home); yv.unique(home);
        GECODE_ES_FAIL((Bool::ClauseTrue<NegBoolView,BoolView>
                        ::post(home,xv,yv)));
      } else {
        for (int i=x.size(); i--; ) {
          BoolView b(x[i]); GECODE_ME_FAIL(b.one(home));
        }
        for (int i=y.size(); i--; ) {
          BoolView b(y[i]); GECODE_ME_FAIL(b.zero(home));
        }
      }
      break;
    case BOT_OR:
      if (n == 0) {
        for (int i=x.size(); i--; ) {
          BoolView b(x[i]); GECODE_ME_FAIL(b.zero(home));
        }
        for (int i=y.size(); i--; ) {
          BoolView b(y[i]); GECODE_ME_FAIL(b.one(home));
        }
      } else {
        ViewArray<BoolView> xv(home,x);
        ViewArray<NegBoolView> yv(home,y.size());
        for (int i=y.size(); i--; ) {
          NegBoolView n(y[i]); yv[i]=n;
        }
        xv.unique(home); yv.unique(home);
        GECODE_ES_FAIL((Bool::ClauseTrue<BoolView,NegBoolView>
                        ::post(home,xv,yv)));
      }
      break;
    default:
      throw IllegalOperation("Int::clause");
    }
  }

  void
  clause(Home home, BoolOpType o, const BoolVarArgs& x, const BoolVarArgs& y,
         BoolVar z, IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    switch (o) {
    case BOT_AND:
      {
        ViewArray<NegBoolView> xv(home,x.size());
        for (int i=x.size(); i--; ) {
          NegBoolView n(x[i]); xv[i]=n;
        }
        ViewArray<BoolView> yv(home,y);
        xv.unique(home); yv.unique(home);
        NegBoolView nz(z);
        GECODE_ES_FAIL((Bool::Clause<NegBoolView,BoolView>
                        ::post(home,xv,yv,nz)));
      }
      break;
    case BOT_OR:
      {
        ViewArray<BoolView> xv(home,x);
        ViewArray<NegBoolView> yv(home,y.size());
        for (int i=y.size(); i--; ) {
          NegBoolView n(y[i]); yv[i]=n;
        }
        xv.unique(home); yv.unique(home);
        GECODE_ES_FAIL((Bool::Clause<BoolView,NegBoolView>
                        ::post(home,xv,yv,z)));
      }
      break;
    default:
      throw IllegalOperation("Int::clause");
    }
  }

  void
  ite(Home home, BoolVar b, IntVar x, IntVar y, IntVar z,
      IntConLevel icl) {
    using namespace Int;
    if (home.failed()) return;
    if (icl == ICL_BND) {
      GECODE_ES_FAIL(Bool::IteBnd<IntView>::post(home,b,x,y,z));
    } else {
      GECODE_ES_FAIL(Bool::IteDom<IntView>::post(home,b,x,y,z));
    }
  }

}

// STATISTICS: int-post
