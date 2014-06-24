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

#include <gecode/set/branch.hh>

namespace Gecode { namespace Set { namespace Branch {

  ValSelCommitBase<SetView,int>* 
  valselcommit(Space& home, const SetValBranch& svb) {
    switch (svb.select()) {
    case SetValBranch::SEL_MIN_INC:
      return new (home) ValSelCommit<ValSelMin,ValCommitInc>(home,svb);
    case SetValBranch::SEL_MIN_EXC:
      return new (home) ValSelCommit<ValSelMin,ValCommitExc>(home,svb);
    case SetValBranch::SEL_MED_INC:
      return new (home) ValSelCommit<ValSelMed,ValCommitInc>(home,svb);
    case SetValBranch::SEL_MED_EXC:
      return new (home) ValSelCommit<ValSelMed,ValCommitExc>(home,svb);
    case SetValBranch::SEL_MAX_INC:
      return new (home) ValSelCommit<ValSelMax,ValCommitInc>(home,svb);
    case SetValBranch::SEL_MAX_EXC:
      return new (home) ValSelCommit<ValSelMax,ValCommitExc>(home,svb);
    case SetValBranch::SEL_RND_INC:
      return new (home) ValSelCommit<ValSelRnd,ValCommitInc>(home,svb);
    case SetValBranch::SEL_RND_EXC:
      return new (home) ValSelCommit<ValSelRnd,ValCommitExc>(home,svb);
    case SetValBranch::SEL_VAL_COMMIT:
      if (svb.commit() == NULL) {
        return new (home) 
          ValSelCommit<ValSelFunction<SetView>,ValCommitInc>(home,svb);
      } else {
        return new (home) 
          ValSelCommit<ValSelFunction<SetView>,ValCommitFunction<SetView> >(home,svb);
      }
    default:
      throw UnknownBranching("Set::branch");
    }
  }

  ValSelCommitBase<SetView,int>* 
  valselcommit(Space& home, const SetAssign& sa) {
    switch (sa.select()) {
    case SetAssign::SEL_MIN_INC:
      return new (home) ValSelCommit<ValSelMin,ValCommitInc>(home,sa);
    case SetAssign::SEL_MIN_EXC:
      return new (home) ValSelCommit<ValSelMin,ValCommitExc>(home,sa);
    case SetAssign::SEL_MED_INC:
      return new (home) ValSelCommit<ValSelMed,ValCommitInc>(home,sa);
    case SetAssign::SEL_MED_EXC:
      return new (home) ValSelCommit<ValSelMed,ValCommitExc>(home,sa);
    case SetAssign::SEL_MAX_INC:
      return new (home) ValSelCommit<ValSelMax,ValCommitInc>(home,sa);
    case SetAssign::SEL_MAX_EXC:
      return new (home) ValSelCommit<ValSelMax,ValCommitExc>(home,sa);
    case SetAssign::SEL_RND_INC:
      return new (home) ValSelCommit<ValSelRnd,ValCommitInc>(home,sa);
    case SetAssign::SEL_RND_EXC:
      return new (home) ValSelCommit<ValSelRnd,ValCommitExc>(home,sa);
    case SetAssign::SEL_VAL_COMMIT:
      if (sa.commit() == NULL) {
        return new (home) 
          ValSelCommit<ValSelFunction<SetView>,ValCommitInc>(home,sa);
      } else {
        return new (home) 
          ValSelCommit<ValSelFunction<SetView>,ValCommitFunction<SetView> >(home,sa);
      }
    default:
      throw UnknownBranching("Set::assign");
    }
  }

}}}

// STATISTICS: set-branch

