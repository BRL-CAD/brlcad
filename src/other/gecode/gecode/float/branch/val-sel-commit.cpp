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

#include <gecode/float/branch.hh>

namespace Gecode { namespace Float { namespace Branch {

  ValSelCommitBase<FloatView,FloatNumBranch>* 
  valselcommit(Space& home, const FloatValBranch& fvb) {
    switch (fvb.select()) {
    case FloatValBranch::SEL_SPLIT_MIN:
      return new (home) ValSelCommit<ValSelLq,ValCommitLqGq>(home,fvb);
    case FloatValBranch::SEL_SPLIT_MAX:
      return new (home) ValSelCommit<ValSelGq,ValCommitLqGq>(home,fvb);
    case FloatValBranch::SEL_SPLIT_RND:
      return new (home) ValSelCommit<ValSelRnd,ValCommitLqGq>(home,fvb);
    case FloatValBranch::SEL_VAL_COMMIT:
      if (fvb.commit() == NULL) {
        return new (home)
          ValSelCommit<ValSelFunction<FloatView>,ValCommitLqGq>(home,fvb);
      } else {
        return new (home)
          ValSelCommit<ValSelFunction<FloatView>,ValCommitFunction<FloatView> >(home,fvb);
      }
    default:
      throw UnknownBranching("Float::branch");
    }
  }

  ValSelCommitBase<FloatView,FloatNumBranch>* 
  valselcommit(Space& home, const FloatAssign& fa) {
    switch (fa.select()) {
    case FloatAssign::SEL_MIN:
      return new (home) ValSelCommit<ValSelLq,ValCommitLqGq>(home,fa);
    case FloatAssign::SEL_MAX:
      return new (home) ValSelCommit<ValSelGq,ValCommitLqGq>(home,fa);
    case FloatAssign::SEL_RND:
      return new (home) ValSelCommit<ValSelRnd,ValCommitLqGq>(home,fa);
    case FloatAssign::SEL_VAL_COMMIT:
      if (fa.commit() == NULL) {
        return new (home)
          ValSelCommit<ValSelFunction<FloatView>,ValCommitLqGq>(home,fa);
      } else {
        return new (home)
          ValSelCommit<ValSelFunction<FloatView>,ValCommitFunction<FloatView> >(home,fa);
      }
    default:
      throw UnknownBranching("Float::assign");
    }
  }

}}}

// STATISTICS: float-branch

