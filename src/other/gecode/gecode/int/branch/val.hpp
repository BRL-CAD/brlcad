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

namespace Gecode {

  forceinline 
  IntValBranch::IntValBranch(Select s0)
    : s(s0) {}

  forceinline 
  IntValBranch::IntValBranch(Rnd r)
    : ValBranch(r), s(SEL_RND) {}

  forceinline 
  IntValBranch::IntValBranch(VoidFunction v, VoidFunction c)
    : ValBranch(v,c), s(SEL_VAL_COMMIT) {}

  forceinline 
  IntValBranch::IntValBranch(Select s0, IntSharedArray n0)
    : n(n0), s(s0) {}

  forceinline IntValBranch::Select
  IntValBranch::select(void) const {
    return s;
  }

  forceinline IntSharedArray
  IntValBranch::values(void) const {
    return n;
  }


  inline IntValBranch
  INT_VAL_MIN(void) {
    return IntValBranch(IntValBranch::SEL_MIN);
  }

  inline IntValBranch
  INT_VAL_MED(void) {
    return IntValBranch(IntValBranch::SEL_MED);
  }

  inline IntValBranch
  INT_VAL_MAX(void) {
    return IntValBranch(IntValBranch::SEL_MAX);
  }

  inline IntValBranch
  INT_VAL_RND(Rnd r) {
    return IntValBranch(r);
  }

  inline IntValBranch
  INT_VAL_SPLIT_MIN(void) {
    return IntValBranch(IntValBranch::SEL_SPLIT_MIN);
  }

  inline IntValBranch
  INT_VAL_SPLIT_MAX(void) {
    return IntValBranch(IntValBranch::SEL_SPLIT_MAX);
  }

  inline IntValBranch
  INT_VAL_RANGE_MIN(void) {
    return IntValBranch(IntValBranch::SEL_RANGE_MIN);
  }

  inline IntValBranch
  INT_VAL_RANGE_MAX(void) {
    return IntValBranch(IntValBranch::SEL_RANGE_MAX);
  }

  inline IntValBranch
  INT_VAL(IntBranchVal v, IntBranchCommit c) {
    return IntValBranch(function_cast<VoidFunction>(v),
                        function_cast<VoidFunction>(c));
  }

  inline IntValBranch
  INT_VAL(BoolBranchVal v, BoolBranchCommit c) {
    return IntValBranch(function_cast<VoidFunction>(v),
                        function_cast<VoidFunction>(c));
  }

  inline IntValBranch
  INT_VALUES_MIN(void) {
    return IntValBranch(IntValBranch::SEL_VALUES_MIN);
  }

  inline IntValBranch
  INT_VALUES_MAX(void) {
    return IntValBranch(IntValBranch::SEL_VALUES_MAX);
  }

  inline IntValBranch
  INT_VAL_NEAR_MIN(IntSharedArray n) {
    return IntValBranch(IntValBranch::SEL_NEAR_MIN,n);
  }

  inline IntValBranch
  INT_VAL_NEAR_MAX(IntSharedArray n) {
    return IntValBranch(IntValBranch::SEL_NEAR_MAX,n);
  }

  inline IntValBranch
  INT_VAL_NEAR_INC(IntSharedArray n) {
    return IntValBranch(IntValBranch::SEL_NEAR_INC,n);
  }

  inline IntValBranch
  INT_VAL_NEAR_DEC(IntSharedArray n) {
    return IntValBranch(IntValBranch::SEL_NEAR_DEC,n);
  }

}

// STATISTICS: int-branch
