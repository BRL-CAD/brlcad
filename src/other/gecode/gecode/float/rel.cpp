/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

#include <gecode/float/rel.hh>

#include <algorithm>

namespace Gecode {

  void
  rel(Home home, FloatVar x0, FloatRelType frt, FloatVal n) {
    using namespace Float;
    Limits::check(n,"Float::rel");
    if (home.failed()) return;
    FloatView x(x0);
    switch (frt) {
    case FRT_EQ: GECODE_ME_FAIL(x.eq(home,n)); break;
    case FRT_NQ:
      GECODE_ES_FAIL((Rel::NqFloat<FloatView>::post(home,x,n)));
      break;
    case FRT_LQ: GECODE_ME_FAIL(x.lq(home,n)); break;
    case FRT_LE:
      GECODE_ME_FAIL(x.lq(home,n));
      GECODE_ES_FAIL((Rel::NqFloat<FloatView>::post(home,x,n)));
      break;
    case FRT_GQ: GECODE_ME_FAIL(x.gq(home,n)); break;
    case FRT_GR:
      GECODE_ME_FAIL(x.gq(home,n));
      GECODE_ES_FAIL((Rel::NqFloat<FloatView>::post(home,x,n)));
      break;
    default: throw UnknownRelation("Float::rel");
    }
  }

  void
  rel(Home home, FloatVar x0, FloatRelType frt, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    switch (frt) {
    case FRT_EQ:
      GECODE_ES_FAIL((Rel::Eq<FloatView,FloatView>::post(home,x0,x1)));
      break;
    case FRT_NQ:
      GECODE_ES_FAIL((Rel::Nq<FloatView,FloatView>::post(home,x0,x1)));
      break;
    case FRT_GQ:
      std::swap(x0,x1); // Fall through
    case FRT_LQ:
      GECODE_ES_FAIL((Rel::Lq<FloatView>::post(home,x0,x1))); break;
    case FRT_GR:
      std::swap(x0,x1); // Fall through
    case FRT_LE:
      GECODE_ES_FAIL((Rel::Le<FloatView>::post(home,x0,x1))); break;
    default:
      throw UnknownRelation("Float::rel");
    }
  }

  void
  rel(Home home, FloatVar x0, FloatRelType frt, FloatVar x1, Reify r) {
    using namespace Float;
    if (home.failed()) return;
    switch (frt) {
    case FRT_EQ:
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReEq<FloatView,Int::BoolView,RM_EQV>::
                             post(home,x0,x1,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReEq<FloatView,Int::BoolView,RM_IMP>::
                             post(home,x0,x1,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReEq<FloatView,Int::BoolView,RM_PMI>::
                             post(home,x0,x1,r.var())));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    case FRT_NQ:
    {
      Int::NegBoolView n(r.var());
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReEq<FloatView,Int::NegBoolView,RM_EQV>::
                             post(home,x0,x1,n)));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReEq<FloatView,Int::NegBoolView,RM_PMI>::
                             post(home,x0,x1,n)));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReEq<FloatView,Int::NegBoolView,RM_IMP>::
                             post(home,x0,x1,n)));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    }
    case FRT_GQ:
      std::swap(x0,x1); // Fall through
    case FRT_LQ:
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReLq<FloatView,Int::BoolView,RM_EQV>::
                             post(home,x0,x1,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReLq<FloatView,Int::BoolView,RM_IMP>::
                             post(home,x0,x1,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReLq<FloatView,Int::BoolView,RM_PMI>::
                             post(home,x0,x1,r.var())));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    case FRT_LE:
      std::swap(x0,x1); // Fall through
    case FRT_GR:
    {
      Int::NegBoolView n(r.var());
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReLq<FloatView,Int::NegBoolView,RM_EQV>::
                             post(home,x0,x1,n)));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReLq<FloatView,Int::NegBoolView,RM_PMI>::
                             post(home,x0,x1,n)));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReLq<FloatView,Int::NegBoolView,RM_IMP>::
                             post(home,x0,x1,n)));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    }
    default:
      throw Int::UnknownRelation("Float::rel");
    }
  }

  void
  rel(Home home, FloatVar x, FloatRelType frt, FloatVal n, Reify r) {
    using namespace Float;
    Limits::check(n,"Float::rel");
    if (home.failed()) return;
    switch (frt) {
    case FRT_EQ:
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReEqFloat<FloatView,Int::BoolView,RM_EQV>::
                             post(home,x,n,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReEqFloat<FloatView,Int::BoolView,RM_IMP>::
                             post(home,x,n,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReEqFloat<FloatView,Int::BoolView,RM_PMI>::
                             post(home,x,n,r.var())));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    case FRT_NQ:
    {
      Int::NegBoolView nb(r.var());
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReEqFloat<FloatView,Int::NegBoolView,RM_EQV>::
                             post(home,x,n,nb)));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReEqFloat<FloatView,Int::NegBoolView,RM_PMI>::
                             post(home,x,n,nb)));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReEqFloat<FloatView,Int::NegBoolView,RM_IMP>::
                             post(home,x,n,nb)));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    }
    case FRT_LQ:
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReLqFloat<FloatView,Int::BoolView,RM_EQV>::
                             post(home,x,n,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReLqFloat<FloatView,Int::BoolView,RM_IMP>::
                             post(home,x,n,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReLqFloat<FloatView,Int::BoolView,RM_PMI>::
                             post(home,x,n,r.var())));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    case FRT_LE:
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReLeFloat<FloatView,Int::BoolView,RM_EQV>::
                             post(home,x,n,r.var())));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReLeFloat<FloatView,Int::BoolView,RM_IMP>::
                             post(home,x,n,r.var())));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReLeFloat<FloatView,Int::BoolView,RM_PMI>::
                             post(home,x,n,r.var())));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    case FRT_GQ:
    {
      Int::NegBoolView nb(r.var());
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReLeFloat<FloatView,Int::NegBoolView,RM_EQV>::
                             post(home,x,n,nb)));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReLeFloat<FloatView,Int::NegBoolView,RM_PMI>::
                             post(home,x,n,nb)));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReLeFloat<FloatView,Int::NegBoolView,RM_IMP>::
                             post(home,x,n,nb)));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    }
    case FRT_GR:
    {
      Int::NegBoolView nb(r.var());
      switch (r.mode()) {
      case RM_EQV:
        GECODE_ES_FAIL((Rel::ReLqFloat<FloatView,Int::NegBoolView,RM_EQV>::
                             post(home,x,n,nb)));
        break;
      case RM_IMP:
        GECODE_ES_FAIL((Rel::ReLqFloat<FloatView,Int::NegBoolView,RM_PMI>::
                             post(home,x,n,nb)));
        break;
      case RM_PMI:
        GECODE_ES_FAIL((Rel::ReLqFloat<FloatView,Int::NegBoolView,RM_IMP>::
                             post(home,x,n,nb)));
        break;
      default: throw Int::UnknownReifyMode("Float::rel");
      }
      break;
    }
    default:
      throw Int::UnknownRelation("Float::rel");
    }
  }

  void
  rel(Home home, const FloatVarArgs& x, FloatRelType frt, FloatVal c) {
    using namespace Float;
    Limits::check(c,"Float::rel");
    if (home.failed()) return;
    switch (frt) {
    case FRT_EQ:
      for (int i=x.size(); i--; ) {
        FloatView xi(x[i]); GECODE_ME_FAIL(xi.eq(home,c));
      }
      break;
    case FRT_NQ:
      for (int i=x.size(); i--; ) {
        FloatView xi(x[i]); GECODE_ES_FAIL((Rel::NqFloat<FloatView>::post(home,xi,c)));
      }
      break;
    case FRT_LQ:
      for (int i=x.size(); i--; ) {
        FloatView xi(x[i]); GECODE_ME_FAIL(xi.lq(home,c));
      }
      break;
    case FRT_LE:
      for (int i=x.size(); i--; ) {
        FloatView xi(x[i]); GECODE_ME_FAIL(xi.lq(home,c));
        GECODE_ES_FAIL((Rel::NqFloat<FloatView>::post(home,xi,c)));
      }
      break;
    case FRT_GQ:
      for (int i=x.size(); i--; ) {
        FloatView xi(x[i]); GECODE_ME_FAIL(xi.gq(home,c));
      }
      break;
    case FRT_GR:
      for (int i=x.size(); i--; ) {
        FloatView xi(x[i]); GECODE_ME_FAIL(xi.gq(home,c));
        GECODE_ES_FAIL((Rel::NqFloat<FloatView>::post(home,xi,c)));
      }
      break;
    default:
      throw UnknownRelation("Float::rel");
    }
  }

  void
  rel(Home home, const FloatVarArgs& x, FloatRelType frt, FloatVar y) {
    using namespace Float;
    if (home.failed()) return;
    switch (frt) {
    case FRT_EQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL((Rel::Eq<FloatView,FloatView>::post(home,y,x[i])));
      }
      break;
    case FRT_NQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL((Rel::Nq<FloatView,FloatView>::post(home,y,x[i])));
      }
      break;
    case FRT_GQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL((Rel::Lq<FloatView>::post(home,y,x[i])));
      }
      break;
    case FRT_GR:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL((Rel::Le<FloatView>::post(home,y,x[i])));
      }
      break;
    case FRT_LQ:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL((Rel::Lq<FloatView>::post(home,x[i],y)));
      }
      break;
    case FRT_LE:
      for (int i=x.size(); i--; ) {
        GECODE_ES_FAIL((Rel::Le<FloatView>::post(home,x[i],y)));
      }
      break;
    default:
      throw UnknownRelation("Float::rel");
    }
  }
}

// STATISTICS: float-post
