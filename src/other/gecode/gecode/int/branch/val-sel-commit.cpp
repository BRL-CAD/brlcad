/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2012
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

#include <gecode/int/branch.hh>

namespace Gecode { namespace Int { namespace Branch {

  ValSelCommitBase<IntView,int>* 
  valselcommitint(Space& home, int n, const IntValBranch& ivb) {
    assert((ivb.select() != IntValBranch::SEL_VALUES_MIN) &&
           (ivb.select() != IntValBranch::SEL_VALUES_MAX));
    switch (ivb.select()) {
    case IntValBranch::SEL_MIN:
      return new (home) 
        ValSelCommit<ValSelMin<IntView>,ValCommitEq<IntView> >(home,ivb);
    case IntValBranch::SEL_MED:
      return new (home) 
        ValSelCommit<ValSelMed<IntView>,ValCommitEq<IntView> >(home,ivb);
    case IntValBranch::SEL_MAX:
      return new (home) 
        ValSelCommit<ValSelMax<IntView>,ValCommitEq<IntView> >(home,ivb);
    case IntValBranch::SEL_RND:
      return new (home) 
        ValSelCommit<ValSelRnd<IntView>,ValCommitEq<IntView> >(home,ivb);
    case IntValBranch::SEL_SPLIT_MIN:
      return new (home) 
        ValSelCommit<ValSelAvg<IntView>,ValCommitLq<IntView> >(home,ivb);
    case IntValBranch::SEL_SPLIT_MAX:
      return new (home) 
        ValSelCommit<ValSelAvg<IntView>,ValCommitGr<IntView> >(home,ivb);
    case IntValBranch::SEL_RANGE_MIN:
      return new (home) 
        ValSelCommit<ValSelRangeMin,ValCommitLq<IntView> >(home,ivb);
    case IntValBranch::SEL_RANGE_MAX:
      return new (home) 
        ValSelCommit<ValSelRangeMax,ValCommitGq<IntView> >(home,ivb);
    case IntValBranch::SEL_VAL_COMMIT:
      if (ivb.commit() == NULL) {
        return new (home) 
          ValSelCommit<ValSelFunction<IntView>,ValCommitEq<IntView> >(home,ivb);
      } else {
        return new (home) 
          ValSelCommit<ValSelFunction<IntView>,ValCommitFunction<IntView> >(home,ivb);
      }
    case IntValBranch::SEL_NEAR_MIN:
    case IntValBranch::SEL_NEAR_MAX:
    case IntValBranch::SEL_NEAR_INC:
    case IntValBranch::SEL_NEAR_DEC:
      {
        IntSharedArray v(ivb.values());
        if (n != v.size())
          throw ArgumentSizeMismatch("Int::branch");
        for (int i=n; i--; )
          Limits::check(v[i],"Int::branch");
        switch (ivb.select()) {
        case IntValBranch::SEL_NEAR_MIN:
          return new (home) 
            ValSelCommit<ValSelNearMinMax<IntView,true>,
             ValCommitEq<IntView> >(home,ivb);
        case IntValBranch::SEL_NEAR_MAX:
          return new (home) 
            ValSelCommit<ValSelNearMinMax<IntView,false>,
             ValCommitEq<IntView> >(home,ivb);
        case IntValBranch::SEL_NEAR_INC:
          return new (home) 
            ValSelCommit<ValSelNearIncDec<IntView,true>,
             ValCommitEq<IntView> >(home,ivb);
        case IntValBranch::SEL_NEAR_DEC:
          return new (home) 
            ValSelCommit<ValSelNearIncDec<IntView,false>,
             ValCommitEq<IntView> >(home,ivb);
        default: GECODE_NEVER;
        }
      }
    default:
      throw UnknownBranching("Int::branch");
    }
  }

  ValSelCommitBase<IntView,int>* 
  valselcommitint(Space& home, const IntAssign& ia) {
    switch (ia.select()) {
    case IntAssign::SEL_MIN:
      return new (home) 
        ValSelCommit<ValSelMin<IntView>,ValCommitEq<IntView> >(home,ia);
    case IntAssign::SEL_MED:
      return new (home) 
        ValSelCommit<ValSelMed<IntView>,ValCommitEq<IntView> >(home,ia);
    case IntAssign::SEL_MAX:
      return new (home) 
        ValSelCommit<ValSelMax<IntView>,ValCommitEq<IntView> >(home,ia);
    case IntAssign::SEL_RND:
      return new (home) 
        ValSelCommit<ValSelRnd<IntView>,ValCommitEq<IntView> >(home,ia);
    case IntAssign::SEL_VAL_COMMIT:
      if (ia.commit() == NULL) {
        return new (home) 
          ValSelCommit<ValSelFunction<IntView>,ValCommitEq<IntView> >(home,ia);
      } else {
        return new (home) 
          ValSelCommit<ValSelFunction<IntView>,ValCommitFunction<IntView> >(home,ia);
      }
    default:
      throw UnknownBranching("Int::assign");
    }
  }

  ValSelCommitBase<BoolView,int>* 
  valselcommitbool(Space& home, int n, const IntValBranch& ivb) {
    switch (ivb.select()) {
    case IntValBranch::SEL_MIN:
    case IntValBranch::SEL_MED:
    case IntValBranch::SEL_SPLIT_MIN:
    case IntValBranch::SEL_RANGE_MIN:
    case IntValBranch::SEL_VALUES_MIN:
      return new (home) 
        ValSelCommit<ValSelMin<BoolView>,ValCommitEq<BoolView> >(home,ivb);
    case IntValBranch::SEL_MAX:
    case IntValBranch::SEL_SPLIT_MAX:
    case IntValBranch::SEL_RANGE_MAX:
    case IntValBranch::SEL_VALUES_MAX:
      return new (home) 
        ValSelCommit<ValSelMax<BoolView>,ValCommitEq<BoolView> >(home,ivb);
    case IntValBranch::SEL_RND:
      return new (home) 
        ValSelCommit<ValSelRnd<BoolView>,ValCommitEq<BoolView> >(home,ivb);
    case IntValBranch::SEL_VAL_COMMIT:
      if (ivb.commit() == NULL) {
        return new (home) 
          ValSelCommit<ValSelFunction<BoolView>,ValCommitEq<BoolView> >(home,ivb);
      } else {
        return new (home) 
          ValSelCommit<ValSelFunction<BoolView>,ValCommitFunction<BoolView> >(home,ivb);
      }
    case IntValBranch::SEL_NEAR_MIN:
    case IntValBranch::SEL_NEAR_MAX:
    case IntValBranch::SEL_NEAR_INC:
    case IntValBranch::SEL_NEAR_DEC:
      {
        IntSharedArray v(ivb.values());
        if (n != v.size())
          throw ArgumentSizeMismatch("Int::branch");
        for (int i=n; i--; )
          if ((v[i] < 0) || (v[i] > 1))
            throw OutOfLimits("Int::branch");
        switch (ivb.select()) {
        case IntValBranch::SEL_NEAR_MIN:
          return new (home) 
            ValSelCommit<ValSelNearMinMax<BoolView,true>,
             ValCommitEq<BoolView> >(home,ivb);
        case IntValBranch::SEL_NEAR_MAX:
          return new (home) 
            ValSelCommit<ValSelNearMinMax<BoolView,false>,
             ValCommitEq<BoolView> >(home,ivb);
        case IntValBranch::SEL_NEAR_INC:
          return new (home) 
            ValSelCommit<ValSelNearIncDec<BoolView,true>,
             ValCommitEq<BoolView> >(home,ivb);
        case IntValBranch::SEL_NEAR_DEC:
          return new (home) 
            ValSelCommit<ValSelNearIncDec<BoolView,false>,
             ValCommitEq<BoolView> >(home,ivb);
        default: GECODE_NEVER;
        }
      }
    default:
      throw UnknownBranching("Int::branch");
    }
  }

  ValSelCommitBase<BoolView,int>* 
  valselcommitbool(Space& home, const IntAssign& ia) {
    switch (ia.select()) {
    case IntAssign::SEL_MIN:
      return new (home) 
        ValSelCommit<ValSelMin<BoolView>,ValCommitEq<BoolView> >(home,ia);
    case IntAssign::SEL_MED:
      return new (home) 
        ValSelCommit<ValSelMed<BoolView>,ValCommitEq<BoolView> >(home,ia);
    case IntAssign::SEL_MAX:
      return new (home) 
        ValSelCommit<ValSelMax<BoolView>,ValCommitEq<BoolView> >(home,ia);
    case IntAssign::SEL_RND:
      return new (home) 
        ValSelCommit<ValSelRnd<BoolView>,ValCommitEq<BoolView> >(home,ia);
    case IntAssign::SEL_VAL_COMMIT:
      if (ia.commit() == NULL) {
        return new (home) 
          ValSelCommit<ValSelFunction<BoolView>,ValCommitEq<BoolView> >(home,ia);
      } else {
        return new (home) 
          ValSelCommit<ValSelFunction<BoolView>,ValCommitFunction<BoolView> >(home,ia);
      }
    default:
      throw UnknownBranching("Bool::assign");
    }
  }

}}}


// STATISTICS: int-branch

