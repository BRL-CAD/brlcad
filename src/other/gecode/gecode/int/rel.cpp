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

#include <gecode/int/rel.hh>
#include <gecode/int/bool.hh>

#include <algorithm>

namespace Gecode {

  using namespace Int;

  void
  rel(Home home, IntVar x0, IntRelType irt, int n, IntConLevel) {
    Limits::check(n,"Int::rel");
    if (home.failed()) return;
    IntView x(x0);
    switch (irt) {
    case IRT_EQ: GECODE_ME_FAIL(x.eq(home,n)); break;
    case IRT_NQ: GECODE_ME_FAIL(x.nq(home,n)); break;
    case IRT_LQ: GECODE_ME_FAIL(x.lq(home,n)); break;
    case IRT_LE: GECODE_ME_FAIL(x.le(home,n)); break;
    case IRT_GQ: GECODE_ME_FAIL(x.gq(home,n)); break;
    case IRT_GR: GECODE_ME_FAIL(x.gr(home,n)); break;
    default: throw UnknownRelation("Int::rel");
    }
  }

  void
  rel(Home home, const IntVarArgs& x, IntRelType irt, int n, IntConLevel) {
    Limits::check(n,"Int::rel");
    if (home.failed()) return;
    switch (irt) {
    case IRT_EQ:
      for (int i=x.size(); i--; ) {
        IntView xi(x[i]); GECODE_ME_FAIL(xi.eq(home,n));
      }
      break;
    case IRT_NQ:
      for (int i=x.size(); i--; ) {
        IntView xi(x[i]); GECODE_ME_FAIL(xi.nq(home,n));
      }
      break;
    case IRT_LQ:
      for (int i=x.size(); i--; ) {
        IntView xi(x[i]); GECODE_ME_FAIL(xi.lq(home,n));
      }
      break;
    case IRT_LE:
      for (int i=x.size(); i--; ) {
        IntView xi(x[i]); GECODE_ME_FAIL(xi.le(home,n));
      }
      break;
    case IRT_GQ:
      for (int i=x.size(); i--; ) {
        IntView xi(x[i]); GECODE_ME_FAIL(xi.gq(home,n));
      }
      break;
    case IRT_GR:
      for (int i=x.size(); i--; ) {
        IntView xi(x[i]); GECODE_ME_FAIL(xi.gr(home,n));
      }
      break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }

  void
  rel(Home home, IntVar x0, IntRelType irt, IntVar x1, IntConLevel icl) {
    if (home.failed()) return;
    switch (irt) {
    case IRT_EQ:
      if ((icl == ICL_DOM) || (icl == ICL_DEF)) {
        GECODE_ES_FAIL((Rel::EqDom<IntView,IntView>::post(home,x0,x1)));
      } else {
        GECODE_ES_FAIL((Rel::EqBnd<IntView,IntView>::post(home,x0,x1)));
      }
      break;
    case IRT_NQ:
      GECODE_ES_FAIL(Rel::Nq<IntView>::post(home,x0,x1)); break;
    case IRT_GQ:
      std::swap(x0,x1); // Fall through
    case IRT_LQ:
      GECODE_ES_FAIL(Rel::Lq<IntView>::post(home,x0,x1)); break;
    case IRT_GR:
      std::swap(x0,x1); // Fall through
    case IRT_LE:
      GECODE_ES_FAIL(Rel::Le<IntView>::post(home,x0,x1)); break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }

  void
  rel(Home home, const IntVarArgs& x, IntRelType irt, IntVar y,
      IntConLevel icl) {
    if (home.failed()) return;
    switch (irt) {
    case IRT_EQ:
      {
        ViewArray<IntView> xv(home,x.size()+1);
        xv[x.size()]=y;
        for (int i=x.size(); i--; )
          xv[i]=x[i];
        if ((icl == ICL_DOM) || (icl == ICL_DEF)) {
          GECODE_ES_FAIL(Rel::NaryEqDom<IntView>::post(home,xv));
        } else {
          GECODE_ES_FAIL(Rel::NaryEqBnd<IntView>::post(home,xv));
        }
      }
      break;
    case IRT_NQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL(Rel::Nq<IntView>::post(home,x[i],y));
      }
      break;
    case IRT_GQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL(Rel::Lq<IntView>::post(home,y,x[i]));
      }
      break;
    case IRT_LQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL(Rel::Lq<IntView>::post(home,x[i],y));
      }
      break;
    case IRT_GR:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL(Rel::Le<IntView>::post(home,y,x[i]));
      }
      break;
    case IRT_LE:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL(Rel::Le<IntView>::post(home,x[i],y));
      }
      break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }


  void
  rel(Home home, IntVar x0, IntRelType irt, IntVar x1, Reify r,
      IntConLevel icl) {
    if (home.failed()) return;
    switch (irt) {
    case IRT_EQ:
      if ((icl == ICL_DOM) || (icl == ICL_DEF)) {
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Rel::ReEqDom<IntView,BoolView,RM_EQV>
                          ::post(home,x0,x1,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Rel::ReEqDom<IntView,BoolView,RM_IMP>
                          ::post(home,x0,x1,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Rel::ReEqDom<IntView,BoolView,RM_PMI>
                          ::post(home,x0,x1,r.var())));
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
      } else {
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Rel::ReEqBnd<IntView,BoolView,RM_EQV>
                          ::post(home,x0,x1,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Rel::ReEqBnd<IntView,BoolView,RM_IMP>
                          ::post(home,x0,x1,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Rel::ReEqBnd<IntView,BoolView,RM_PMI>
                          ::post(home,x0,x1,r.var())));
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
      }
      break;
    case IRT_NQ:
      {
        NegBoolView n(r.var());
        if (icl == ICL_BND) {
          switch (r.mode()) {
          case RM_EQV:
            GECODE_ES_FAIL((Rel::ReEqBnd<IntView,NegBoolView,RM_EQV>
                            ::post(home,x0,x1,n)));
            break;
          case RM_IMP:
            GECODE_ES_FAIL((Rel::ReEqBnd<IntView,NegBoolView,RM_PMI>
                            ::post(home,x0,x1,n)));
            break;
          case RM_PMI:
            GECODE_ES_FAIL((Rel::ReEqBnd<IntView,NegBoolView,RM_IMP>
                            ::post(home,x0,x1,n)));
            break;
          default: throw UnknownReifyMode("Int::rel");
          }
        } else {
          switch (r.mode()) {
          case RM_EQV:
            GECODE_ES_FAIL((Rel::ReEqDom<IntView,NegBoolView,RM_EQV>
                            ::post(home,x0,x1,n)));
            break;
          case RM_IMP:
            GECODE_ES_FAIL((Rel::ReEqDom<IntView,NegBoolView,RM_PMI>
                            ::post(home,x0,x1,n)));
            break;
          case RM_PMI:
            GECODE_ES_FAIL((Rel::ReEqDom<IntView,NegBoolView,RM_IMP>
                            ::post(home,x0,x1,n)));
            break;
          default: throw UnknownReifyMode("Int::rel");
          }
        }
      }
      break;
    case IRT_GQ:
      std::swap(x0,x1); // Fall through
    case IRT_LQ:
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReLq<IntView,BoolView,RM_EQV>
                        ::post(home,x0,x1,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReLq<IntView,BoolView,RM_IMP>
                        ::post(home,x0,x1,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReLq<IntView,BoolView,RM_PMI>
                        ::post(home,x0,x1,r.var())));
        break;
      default: throw UnknownReifyMode("Int::rel");
      }
      break;
    case IRT_LE:
      std::swap(x0,x1); // Fall through
    case IRT_GR:
      {
        NegBoolView n(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Rel::ReLq<IntView,NegBoolView,RM_EQV>
                          ::post(home,x0,x1,n)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Rel::ReLq<IntView,NegBoolView,RM_PMI>
                          ::post(home,x0,x1,n)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Rel::ReLq<IntView,NegBoolView,RM_IMP>
                          ::post(home,x0,x1,n)));
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
  rel(Home home, IntVar x, IntRelType irt, int n, Reify r,
      IntConLevel icl) {
    Limits::check(n,"Int::rel");
    if (home.failed()) return;
    switch (irt) {
    case IRT_EQ:
      if ((icl == ICL_DOM) || (icl == ICL_DEF)) {
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Rel::ReEqDomInt<IntView,BoolView,RM_EQV>
                          ::post(home,x,n,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Rel::ReEqDomInt<IntView,BoolView,RM_IMP>
                          ::post(home,x,n,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Rel::ReEqDomInt<IntView,BoolView,RM_PMI>
                          ::post(home,x,n,r.var())));
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
      } else {
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Rel::ReEqBndInt<IntView,BoolView,RM_EQV>
                          ::post(home,x,n,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Rel::ReEqBndInt<IntView,BoolView,RM_IMP>
                          ::post(home,x,n,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Rel::ReEqBndInt<IntView,BoolView,RM_PMI>
                          ::post(home,x,n,r.var())));
          break;
        default: throw UnknownReifyMode("Int::rel");
        }
      }
      break;
    case IRT_NQ:
      {
        NegBoolView nb(r.var());
        if (icl == ICL_BND) {
          switch (r.mode()) {
          case RM_EQV:
            GECODE_ES_FAIL((Rel::ReEqBndInt<IntView,NegBoolView,RM_EQV>
                            ::post(home,x,n,nb)));
            break;
          case RM_IMP:
            GECODE_ES_FAIL((Rel::ReEqBndInt<IntView,NegBoolView,RM_PMI>
                            ::post(home,x,n,nb)));
            break;
          case RM_PMI:
            GECODE_ES_FAIL((Rel::ReEqBndInt<IntView,NegBoolView,RM_IMP>
                            ::post(home,x,n,nb)));
            break;
          default: throw UnknownReifyMode("Int::rel");
          }
        } else {
          switch (r.mode()) {
          case RM_EQV:
            GECODE_ES_FAIL((Rel::ReEqDomInt<IntView,NegBoolView,RM_EQV>
                            ::post(home,x,n,nb)));
            break;
          case RM_IMP:
            GECODE_ES_FAIL((Rel::ReEqDomInt<IntView,NegBoolView,RM_PMI>
                            ::post(home,x,n,nb)));
            break;
          case RM_PMI:
            GECODE_ES_FAIL((Rel::ReEqDomInt<IntView,NegBoolView,RM_IMP>
                            ::post(home,x,n,nb)));
            break;
          default: throw UnknownReifyMode("Int::rel");
          }
        }
      }
      break;
    case IRT_LE:
      n--; // Fall through
    case IRT_LQ:
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReLqInt<IntView,BoolView,RM_EQV>
                        ::post(home,x,n,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReLqInt<IntView,BoolView,RM_IMP>
                        ::post(home,x,n,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReLqInt<IntView,BoolView,RM_PMI>
                        ::post(home,x,n,r.var())));
        break;
      default: throw UnknownReifyMode("Int::rel");
      }
      break;
    case IRT_GQ:
      n--; // Fall through
    case IRT_GR:
      {
        NegBoolView nb(r.var());
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL((Rel::ReLqInt<IntView,NegBoolView,RM_EQV>
                          ::post(home,x,n,nb)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((Rel::ReLqInt<IntView,NegBoolView,RM_PMI>
                          ::post(home,x,n,nb)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((Rel::ReLqInt<IntView,NegBoolView,RM_IMP>
                          ::post(home,x,n,nb)));
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
  rel(Home home, const IntVarArgs& x, IntRelType irt,
      IntConLevel icl) {
    if (home.failed() || ((irt != IRT_NQ) && (x.size() < 2))) 
      return;
    switch (irt) {
    case IRT_EQ:
      {
        ViewArray<IntView> xv(home,x);
        if ((icl == ICL_DOM) || (icl == ICL_DEF)) {
          GECODE_ES_FAIL(Rel::NaryEqDom<IntView>::post(home,xv));
        } else {
          GECODE_ES_FAIL(Rel::NaryEqBnd<IntView>::post(home,xv));
        }
      }
      break;
    case IRT_NQ:
      {
        ViewArray<IntView> y(home,x);
        GECODE_ES_FAIL((Rel::NaryNq<IntView>::post(home,y)));
      }
      break;
    case IRT_LE:
      {
        ViewArray<IntView> y(home,x);
        GECODE_ES_FAIL((Rel::NaryLqLe<IntView,1>::post(home,y)));
      }
      break;
    case IRT_LQ:
      {
        ViewArray<IntView> y(home,x);
        GECODE_ES_FAIL((Rel::NaryLqLe<IntView,0>::post(home,y)));
      }
      break;
    case IRT_GR:
      {
        ViewArray<IntView> y(home,x.size());
        for (int i=x.size(); i--; )
          y[i] = x[x.size()-1-i];
        GECODE_ES_FAIL((Rel::NaryLqLe<IntView,1>::post(home,y)));
      }
      for (int i=x.size()-1; i--; )
        GECODE_ES_FAIL(Rel::Le<IntView>::post(home,x[i+1],x[i]));
      break;
    case IRT_GQ:
      {
        ViewArray<IntView> y(home,x.size());
        for (int i=x.size(); i--; )
          y[i] = x[x.size()-1-i];
        GECODE_ES_FAIL((Rel::NaryLqLe<IntView,0>::post(home,y)));
      }
      break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }

  void
  rel(Home home, const IntVarArgs& x, IntRelType irt, const IntVarArgs& y,
      IntConLevel icl) {
    if (home.failed()) return;

    switch (irt) {
    case IRT_GR:
      {
        ViewArray<IntView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexLqLe<IntView>::post(home,yv,xv,true));
      }
      break;
    case IRT_LE:
      {
        ViewArray<IntView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexLqLe<IntView>::post(home,xv,yv,true));
      }
      break;
    case IRT_GQ:
      {
        ViewArray<IntView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexLqLe<IntView>::post(home,yv,xv,false));
      }
      break;
    case IRT_LQ:
      {
        ViewArray<IntView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexLqLe<IntView>::post(home,xv,yv,false));
      }
      break;
    case IRT_EQ:
      if (x.size() != y.size()) {
        home.fail();
      } else if ((icl == ICL_DOM) || (icl == ICL_DEF))
        for (int i=x.size(); i--; ) {
          GECODE_ES_FAIL((Rel::EqDom<IntView,IntView>
                          ::post(home,x[i],y[i])));
        }
      else
        for (int i=x.size(); i--; ) {
          GECODE_ES_FAIL((Rel::EqBnd<IntView,IntView>
                          ::post(home,x[i],y[i])));
        }
      break;
    case IRT_NQ:
      {
        ViewArray<IntView> xv(home,x), yv(home,y);
        GECODE_ES_FAIL(Rel::LexNq<IntView>::post(home,xv,yv));
      }
      break;
    default:
      throw UnknownRelation("Int::rel");
    }
  }

}

// STATISTICS: int-post
