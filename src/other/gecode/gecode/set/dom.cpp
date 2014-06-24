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

#include <gecode/set.hh>
#include <gecode/set/rel.hh>

namespace Gecode {

  void
  dom(Home home, SetVar s, SetRelType r, int i) {
    Set::Limits::check(i, "Set::dom");
    IntSet d(i,i);
    dom(home, s, r, d);
  }

  void
  dom(Home home, const SetVarArgs& s, SetRelType r, int i) {
    Set::Limits::check(i, "Set::dom");
    IntSet d(i,i);
    dom(home, s, r, d);
  }

  void
  dom(Home home, SetVar s, SetRelType r, int i, int j) {
    Set::Limits::check(i, "Set::dom");
    Set::Limits::check(j, "Set::dom");
    IntSet d(i,j);
    dom(home, s, r, d);
  }

  void
  dom(Home home, const SetVarArgs& s, SetRelType r, int i, int j) {
    Set::Limits::check(i, "Set::dom");
    Set::Limits::check(j, "Set::dom");
    IntSet d(i,j);
    dom(home, s, r, d);
  }

  void
  dom(Home home, SetVar s, SetRelType r, const IntSet& is) {
    Set::Limits::check(is, "Set::dom");
    if (home.failed()) return;

    Set::SetView _s(s);

    switch (r) {
    case SRT_EQ:
      {
        if (is.ranges() == 1) {
          GECODE_ME_FAIL(_s.include(home, is.min(), is.max()));
          GECODE_ME_FAIL(_s.intersect(home, is.min(), is.max()));
        } else {
          IntSetRanges rd1(is);
          GECODE_ME_FAIL(_s.includeI(home, rd1));
          IntSetRanges rd2(is);
          GECODE_ME_FAIL(_s.intersectI(home, rd2));
        }
      }
      break;
    case SRT_LQ:
      {
        Set::ConstSetView cv(home, is);
        GECODE_ES_FAIL(
          (Set::Rel::Lq<Set::SetView,Set::ConstSetView,false>
            ::post(home,s,cv)));
      }
      break;
    case SRT_LE:
      {
        Set::ConstSetView cv(home, is);
        GECODE_ES_FAIL(
          (Set::Rel::Lq<Set::SetView,Set::ConstSetView,true>
            ::post(home,s,cv)));
      }
      break;
    case SRT_GQ:
      {
        Set::ConstSetView cv(home, is);
        GECODE_ES_FAIL(
          (Set::Rel::Lq<Set::ConstSetView,Set::SetView,false>
            ::post(home,cv,s)));
      }
      break;
    case SRT_GR:
      {
        Set::ConstSetView cv(home, is);
        GECODE_ES_FAIL(
          (Set::Rel::Lq<Set::ConstSetView,Set::SetView,true>
            ::post(home,cv,s)));
      }
      break;
    case SRT_DISJ:
      {
        if (is.ranges() == 1) {
          GECODE_ME_FAIL(_s.exclude(home, is.min(), is.max()));
        } else {
          IntSetRanges rd(is);
          GECODE_ME_FAIL(_s.excludeI(home, rd));
        }
      }
      break;
    case SRT_NQ:
      {
        Set::ConstSetView cv(home, is);
        GECODE_ES_FAIL(
                       (Set::Rel::DistinctDoit<Set::SetView>::post(home, s,
                                                                   cv)));
      }
      break;
    case SRT_SUB:
      {
         if (is.ranges() == 1) {
           GECODE_ME_FAIL(_s.intersect(home, is.min(), is.max()));
         } else {
          IntSetRanges rd(is);
          GECODE_ME_FAIL(_s.intersectI(home, rd));
         }
      }
      break;
    case SRT_SUP:
      {
        if (is.ranges() == 1) {
          GECODE_ME_FAIL(_s.include(home, is.min(), is.max()));
        } else {
          IntSetRanges rd(is);
          GECODE_ME_FAIL(_s.includeI(home, rd));
        }
      }
      break;
    case SRT_CMPL:
      {
        if (is.ranges() == 1) {
          GECODE_ME_FAIL(_s.exclude(home, is.min(), is.max()));
          GECODE_ME_FAIL(
                         _s.include(home,
                                    Set::Limits::min,
                                    is.min()-1) );
          GECODE_ME_FAIL(
                         _s.include(home, is.max()+1,
                                    Set::Limits::max) );
        } else {
          IntSetRanges rd1(is);
          Set::RangesCompl<IntSetRanges > rdC1(rd1);
          GECODE_ME_FAIL(_s.includeI(home, rdC1));
          IntSetRanges rd2(is);
          Set::RangesCompl<IntSetRanges > rdC2(rd2);
          GECODE_ME_FAIL(_s.intersectI(home, rdC2));
        }
      }
      break;
    default:
      throw Set::UnknownRelation("Set::dom");
    }
  }

  void
  dom(Home home, const SetVarArgs& s, SetRelType r, const IntSet& is) {
    Set::Limits::check(is, "Set::dom");
    if (home.failed()) return;

    switch (r) {
    case SRT_EQ:
      {
        if (is.ranges() == 1) {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            GECODE_ME_FAIL(_s.include(home, is.min(), is.max()));
            GECODE_ME_FAIL(_s.intersect(home, is.min(), is.max()));
          }
        } else {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            IntSetRanges rd1(is);
            GECODE_ME_FAIL(_s.includeI(home, rd1));
            IntSetRanges rd2(is);
            GECODE_ME_FAIL(_s.intersectI(home, rd2));
          }
        }
      }
      break;
    case SRT_LQ:
      {
        Set::ConstSetView cv(home, is);
        for (int i=s.size(); i--; ) {
          Set::SetView _s(s[i]);
          GECODE_ES_FAIL((Set::Rel::Lq<Set::SetView,Set::ConstSetView,false>
                          ::post(home,_s,cv)));
        }
      }
      break;
    case SRT_LE:
      {
        Set::ConstSetView cv(home, is);
        for (int i=s.size(); i--; ) {
          Set::SetView _s(s[i]);
          GECODE_ES_FAIL((Set::Rel::Lq<Set::SetView,Set::ConstSetView,true>
                          ::post(home,_s,cv)));
        }
      }
      break;
    case SRT_GQ:
      {
        Set::ConstSetView cv(home, is);
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            GECODE_ES_FAIL((Set::Rel::Lq<Set::ConstSetView,Set::SetView,false>
                            ::post(home,cv,_s)));
          }
      }
      break;
    case SRT_GR:
      {
        Set::ConstSetView cv(home, is);
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            GECODE_ES_FAIL((Set::Rel::Lq<Set::ConstSetView,Set::SetView,true>
                            ::post(home,cv,_s)));
          }
      }
      break;
    case SRT_DISJ:
      {
        if (is.ranges() == 1) {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            GECODE_ME_FAIL(_s.exclude(home, is.min(), is.max()));
          }
        } else {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            IntSetRanges rd(is);
            GECODE_ME_FAIL(_s.excludeI(home, rd));
          }
        }
      }
      break;
    case SRT_NQ:
      {
        Set::ConstSetView cv(home, is);
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            GECODE_ES_FAIL((Set::Rel::DistinctDoit<Set::SetView>
                            ::post(home,_s,cv)));
          }
      }
      break;
    case SRT_SUB:
      {
         if (is.ranges() == 1) {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            GECODE_ME_FAIL(_s.intersect(home, is.min(), is.max()));
          }
         } else {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            IntSetRanges rd(is);
            GECODE_ME_FAIL(_s.intersectI(home, rd));
          }
         }
      }
      break;
    case SRT_SUP:
      {
        if (is.ranges() == 1) {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            GECODE_ME_FAIL(_s.include(home, is.min(), is.max()));
          }
        } else {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            IntSetRanges rd(is);
            GECODE_ME_FAIL(_s.includeI(home, rd));
          }
        }
      }
      break;
    case SRT_CMPL:
      {
        if (is.ranges() == 1) {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            GECODE_ME_FAIL(_s.exclude(home, is.min(), is.max()));
            GECODE_ME_FAIL(_s.include(home,
                                      Set::Limits::min,
                                      is.min()-1) );
            GECODE_ME_FAIL(_s.include(home, is.max()+1,
                                      Set::Limits::max) );
          }
        } else {
          for (int i=s.size(); i--; ) {
            Set::SetView _s(s[i]);
            IntSetRanges rd1(is);
            Set::RangesCompl<IntSetRanges > rdC1(rd1);
            GECODE_ME_FAIL(_s.includeI(home, rdC1));
            IntSetRanges rd2(is);
            Set::RangesCompl<IntSetRanges > rdC2(rd2);
            GECODE_ME_FAIL(_s.intersectI(home, rdC2));
          }
        }
      }
      break;
    default:
      throw Set::UnknownRelation("Set::dom");
    }
  }

  void
  dom(Home home, SetVar s, SetRelType rt, int i, Reify r) {
    Set::Limits::check(i, "Set::dom");
    IntSet d(i,i);
    dom(home, s, rt, d, r);
  }

  void
  dom(Home home, SetVar s, SetRelType rt, int i, int j, Reify r) {
    Set::Limits::check(i, "Set::dom");
    Set::Limits::check(j, "Set::dom");
    IntSet d(i,j);
    dom(home, s, rt, d, r);
  }

  void
  dom(Home home, SetVar s, SetRelType rt, const IntSet& is, Reify r) {
    Set::Limits::check(is, "Set::dom");
    if (home.failed()) return;
    switch (rt) {
    case SRT_EQ:
      {
        Set::ConstSetView cv(home, is);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReEq<Set::SetView,
             Set::ConstSetView,Gecode::Int::BoolView,RM_EQV>
             ::post(home, s, cv, r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReEq<Set::SetView,
             Set::ConstSetView,Gecode::Int::BoolView,RM_IMP>
             ::post(home, s, cv, r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReEq<Set::SetView,
             Set::ConstSetView,Gecode::Int::BoolView,RM_PMI>
             ::post(home, s, cv, r.var())));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    case SRT_LQ:
      {
        Set::ConstSetView cv(home, is);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::SetView,
             Set::ConstSetView,RM_EQV,false>::post(home, s, cv, r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::SetView,
             Set::ConstSetView,RM_IMP,false>::post(home, s, cv, r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::SetView,
             Set::ConstSetView,RM_PMI,false>::post(home, s, cv, r.var())));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    case SRT_LE:
      {
        Set::ConstSetView cv(home, is);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::SetView,
             Set::ConstSetView,RM_EQV,true>::post(home, s, cv, r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::SetView,
             Set::ConstSetView,RM_IMP,true>::post(home, s, cv, r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::SetView,
             Set::ConstSetView,RM_PMI,true>::post(home, s, cv, r.var())));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    case SRT_GQ:
      {
        Set::ConstSetView cv(home, is);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::ConstSetView,Set::SetView,RM_EQV,false>
            ::post(home,cv,s,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::ConstSetView,Set::SetView,RM_IMP,false>
            ::post(home,cv,s,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::ConstSetView,Set::SetView,RM_PMI,false>
            ::post(home,cv,s,r.var())));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    case SRT_GR:
      {
        Set::ConstSetView cv(home, is);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::ConstSetView,Set::SetView,RM_EQV,true>
            ::post(home,cv,s,r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::ConstSetView,Set::SetView,RM_IMP,true>
            ::post(home,cv,s,r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReLq<Set::ConstSetView,Set::SetView,RM_PMI,true>
            ::post(home,cv,s,r.var())));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    case SRT_NQ:
      {
        Gecode::Int::NegBoolView notb(r.var());
        Set::ConstSetView cv(home, is);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReEq<Set::SetView,
             Set::ConstSetView,Gecode::Int::NegBoolView,RM_EQV>
             ::post(home, s, cv, notb)));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReEq<Set::SetView,
             Set::ConstSetView,Gecode::Int::NegBoolView,RM_PMI>
             ::post(home, s, cv, notb)));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReEq<Set::SetView,
             Set::ConstSetView,Gecode::Int::NegBoolView,RM_IMP>
             ::post(home, s, cv, notb)));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    case SRT_SUB:
      {
        Set::ConstSetView cv(home, is);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReSubset<Set::SetView,
             Set::ConstSetView,RM_EQV>::post(home, s, cv, r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReSubset<Set::SetView,
             Set::ConstSetView,RM_IMP>::post(home, s, cv, r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReSubset<Set::SetView,
             Set::ConstSetView,RM_PMI>::post(home, s, cv, r.var())));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    case SRT_SUP:
      {
        Set::ConstSetView cv(home, is);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReSubset<Set::ConstSetView,Set::SetView,RM_EQV>
            ::post(home, cv, s, r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReSubset<Set::ConstSetView,Set::SetView,RM_IMP>
            ::post(home, cv, s, r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReSubset<Set::ConstSetView,Set::SetView,RM_PMI>
            ::post(home, cv, s, r.var())));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    case SRT_DISJ:
      {
        // x||y <=> b is equivalent to
        // ( y <= complement(x) and x<=complement(y) ) <=> b

        // compute complement of is
        IntSetRanges dr1(is);
        Set::RangesCompl<IntSetRanges > dc1(dr1);
        IntSet dcompl(dc1);
        Set::ConstSetView cvcompl(home, dcompl);
        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReSubset<Set::SetView,
             Set::ConstSetView,RM_EQV>::post(home, s, cvcompl, r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReSubset<Set::SetView,
             Set::ConstSetView,RM_IMP>::post(home, s, cvcompl, r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReSubset<Set::SetView,
             Set::ConstSetView,RM_PMI>::post(home, s, cvcompl, r.var())));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    case SRT_CMPL:
      {
        Set::SetView sv(s);

        IntSetRanges dr1(is);
        Set::RangesCompl<IntSetRanges> dc1(dr1);
        IntSet dcompl(dc1);
        Set::ConstSetView cvcompl(home, dcompl);

        switch (r.mode()) {
        case RM_EQV:
          GECODE_ES_FAIL(
            (Set::Rel::ReEq<Set::SetView,
             Set::ConstSetView,Gecode::Int::BoolView,RM_EQV>
             ::post(home, s, cvcompl, r.var())));
          break;
        case RM_IMP:
          GECODE_ES_FAIL(
            (Set::Rel::ReEq<Set::SetView,
             Set::ConstSetView,Gecode::Int::BoolView,RM_IMP>
             ::post(home, s, cvcompl, r.var())));
          break;
        case RM_PMI:
          GECODE_ES_FAIL(
            (Set::Rel::ReEq<Set::SetView,
             Set::ConstSetView,Gecode::Int::BoolView,RM_PMI>
             ::post(home, s, cvcompl, r.var())));
          break;
        default: throw Gecode::Int::UnknownReifyMode("Set::dom");
        }
      }
      break;
    default:
      throw Set::UnknownRelation("Set::dom");
    }
  }

  void
  dom(Home home, SetVar x, SetVar d) {
    using namespace Set;    
    if (home.failed()) return;
    SetView xv(x), dv(d);
    if (!same(xv,dv)) {
      GECODE_ME_FAIL(xv.cardMax(home,dv.cardMax()));
      GECODE_ME_FAIL(xv.cardMin(home,dv.cardMin()));
      GlbRanges<SetView> lb(dv);
      GECODE_ME_FAIL(xv.includeI(home,lb));
      LubRanges<SetView> ub(dv);
      GECODE_ME_FAIL(xv.intersectI(home,ub));
    }
  }

  void
  dom(Home home, const SetVarArgs& x, const SetVarArgs& d) {
    using namespace Set;    
    if (x.size() != d.size())
      throw ArgumentSizeMismatch("Set::dom");
    for (int i=x.size(); i--; ) {
      if (home.failed()) return;
      SetView xv(x[i]), dv(d[i]);
      if (!same(xv,dv)) {
        GECODE_ME_FAIL(xv.cardMax(home,dv.cardMax()));
        GECODE_ME_FAIL(xv.cardMin(home,dv.cardMin()));
        GlbRanges<SetView> lb(dv);
        GECODE_ME_FAIL(xv.includeI(home,lb));
        LubRanges<SetView> ub(dv);
        GECODE_ME_FAIL(xv.intersectI(home,ub));
      }
    }
  }
  
}

// STATISTICS: set-post
