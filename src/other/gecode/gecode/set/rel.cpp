/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004, 2005
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

#include <gecode/set/rel.hh>
#include <gecode/set/rel-op.hh>
#include <gecode/set/int.hh>

namespace Gecode {
  using namespace Set;
  using namespace Set::Rel;
  using namespace Set::RelOp;

  template<class View0, class View1>
  void
  rel_post(Home home, View0 x0, SetRelType r, View1 x1) {
    if (home.failed()) return;
    switch (r) {
    case SRT_EQ:
      GECODE_ES_FAIL((Eq<View0,View1>::post(home,x0,x1)));
      break;
    case SRT_NQ:
      GECODE_ES_FAIL((Distinct<View0,View1>::post(home,x0,x1)));
      break;
    case SRT_SUB:
      GECODE_ES_FAIL((Subset<View0,View1>::post(home, x0,x1)));
      break;
    case SRT_SUP:
      GECODE_ES_FAIL((Subset<View1,View0>::post(home, x1,x0)));
      break;
    case SRT_DISJ:
      {
        EmptyView emptyset;
        GECODE_ES_FAIL((SuperOfInter<View0,View1,EmptyView>
                        ::post(home, x0, x1, emptyset)));
      }
      break;
    case SRT_CMPL:
      {
        ComplementView<View0> cx0(x0);
        GECODE_ES_FAIL((Eq<ComplementView<View0>, View1>
                        ::post(home, cx0, x1)));
      }
      break;
    case SRT_LQ:
      GECODE_ES_FAIL((Lq<View0,View1,false>::post(home,x0,x1)));
      break;
    case SRT_LE:
      GECODE_ES_FAIL((Lq<View0,View1,true>::post(home,x0,x1)));
      break;
    case SRT_GQ:
      GECODE_ES_FAIL((Lq<View1,View0,false>::post(home,x1,x0)));
      break;
    case SRT_GR:
      GECODE_ES_FAIL((Lq<View1,View0,true>::post(home,x1,x0)));
      break;
    default:
      throw UnknownRelation("Set::rel");
    }
  }

  template<class View0, class View1, ReifyMode rm>
  void
  rel_re(Home home, View0 x, SetRelType r, View1 y, BoolVar b) {
    if (home.failed()) return;
    switch (r) {
    case SRT_EQ:
      GECODE_ES_FAIL((ReEq<View0,View1,Gecode::Int::BoolView,rm>
                      ::post(home, x,y,b)));
      break;
    case SRT_NQ:
      {
        Gecode::Int::NegBoolView notb(b);
        switch (rm) {
        case RM_EQV:
          GECODE_ES_FAIL((ReEq<View0,View1,Gecode::Int::NegBoolView,RM_EQV>
                         ::post(home,x,y,notb)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL((ReEq<View0,View1,Gecode::Int::NegBoolView,RM_PMI>
                         ::post(home,x,y,notb)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL((ReEq<View0,View1,Gecode::Int::NegBoolView,RM_IMP>
                         ::post(home,x,y,notb)));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::rel");
        }
      }
      break;
    case SRT_SUB:
      GECODE_ES_FAIL((ReSubset<View0,View1,rm>::post(home, x,y,b)));
      break;
    case SRT_SUP:
      GECODE_ES_FAIL((ReSubset<View1,View0,rm>::post(home, y,x,b)));
      break;
    case SRT_DISJ:
      {
        // x||y <=> b is equivalent to
        // ( y <= complement(x) ) <=> b

        ComplementView<View0> xc(x);
        GECODE_ES_FAIL((ReSubset<View1,ComplementView<View0>,rm>
                        ::post(home, y, xc, b)));
      }
      break;
    case SRT_CMPL:
      {
        ComplementView<View0> xc(x);
        GECODE_ES_FAIL((ReEq<ComplementView<View0>,View1,
                        Gecode::Int::BoolView,rm>
                        ::post(home, xc, y, b)));
      }
      break;
    case SRT_LQ:
      GECODE_ES_FAIL((ReLq<View0,View1,rm,false>::post(home,x,y,b)));
      break;
    case SRT_LE:
      GECODE_ES_FAIL((ReLq<View0,View1,rm,true>::post(home,x,y,b)));
      break;
    case SRT_GQ:
      GECODE_ES_FAIL((ReLq<View1,View0,rm,false>::post(home,y,x,b)));
      break;
    case SRT_GR:
      GECODE_ES_FAIL((ReLq<View1,View0,rm,true>::post(home,y,x,b)));
      break;
    default:
      throw UnknownRelation("Set::rel");
    }
  }

  void
  rel(Home home, SetVar x, SetRelType r, SetVar y) {
    rel_post<SetView,SetView>(home,x,r,y);
  }

  void
  rel(Home home, SetVar s, SetRelType r, IntVar x) {
    Gecode::Int::IntView xv(x);
    SingletonView xsingle(xv);
    rel_post<SetView,SingletonView>(home,s,r,xv);
  }

  void
  rel(Home home, IntVar x, SetRelType r, SetVar s) {
    switch (r) {
    case SRT_SUB:
      rel(home, s, SRT_SUP, x);
      break;
    case SRT_SUP:
      rel(home, s, SRT_SUB, x);
      break;
    default:
      rel(home, s, r, x);
    }
  }

  void
  rel(Home home, SetVar x, SetRelType rt, SetVar y, Reify r) {
    switch (r.mode()) {
    case RM_EQV:
      rel_re<SetView,SetView,RM_EQV>(home,x,rt,y,r.var());
      break;
    case RM_IMP:
      rel_re<SetView,SetView,RM_IMP>(home,x,rt,y,r.var());
      break;
    case RM_PMI:
      rel_re<SetView,SetView,RM_PMI>(home,x,rt,y,r.var());
      break;
    default: throw Gecode::Int::UnknownReifyMode("Set::rel");
    }
  }

  void
  rel(Home home, SetVar s, SetRelType rt, IntVar x, Reify r) {
    Gecode::Int::IntView xv(x);
    SingletonView xsingle(xv);
    switch (r.mode()) {
    case RM_EQV:
      rel_re<SetView,SingletonView,RM_EQV>(home,s,rt,xsingle,r.var());
      break;
    case RM_IMP:
      rel_re<SetView,SingletonView,RM_IMP>(home,s,rt,xsingle,r.var());
      break;
    case RM_PMI:
      rel_re<SetView,SingletonView,RM_PMI>(home,s,rt,xsingle,r.var());
      break;
    default: throw Gecode::Int::UnknownReifyMode("Set::rel");
    }
  }

  void
  rel(Home home, IntVar x, SetRelType rt, SetVar s, Reify r) {
    switch (rt) {
    case SRT_SUB:
      rel(home, s, SRT_SUP, x, r);
      break;
    case SRT_SUP:
      rel(home, s, SRT_SUB, x, r);
      break;
    default:
      rel(home, s, rt, x, r);
    }
  }

}

// STATISTICS: set-post
