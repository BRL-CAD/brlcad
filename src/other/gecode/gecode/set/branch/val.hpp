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
  SetValBranch::SetValBranch(Select s0)
    : s(s0) {}

  forceinline 
  SetValBranch::SetValBranch(Select s0, Rnd r)
    : ValBranch(r), s(s0) {}

  forceinline 
  SetValBranch::SetValBranch(VoidFunction v, VoidFunction c)
    : ValBranch(v,c), s(SEL_VAL_COMMIT) {}

  forceinline SetValBranch::Select
  SetValBranch::select(void) const {
    return s;
  }


  inline SetValBranch
  SET_VAL_MIN_INC(void) {
    return SetValBranch(SetValBranch::SEL_MIN_INC);
  }

  inline SetValBranch
  SET_VAL_MIN_EXC(void) {
    return SetValBranch(SetValBranch::SEL_MIN_EXC);
  }

  inline SetValBranch
  SET_VAL_MED_INC(void) {
    return SetValBranch(SetValBranch::SEL_MED_INC);
  }

  inline SetValBranch
  SET_VAL_MED_EXC(void) {
    return SetValBranch(SetValBranch::SEL_MED_EXC);
  }

  inline SetValBranch
  SET_VAL_MAX_INC(void) {
    return SetValBranch(SetValBranch::SEL_MAX_INC);
  }

  inline SetValBranch
  SET_VAL_MAX_EXC(void) {
    return SetValBranch(SetValBranch::SEL_MAX_EXC);
  }

  inline SetValBranch
  SET_VAL_RND_INC(Rnd r) {
    return SetValBranch(SetValBranch::SEL_RND_INC,r);
  }

  inline SetValBranch
  SET_VAL_RND_EXC(Rnd r) {
    return SetValBranch(SetValBranch::SEL_RND_EXC,r);
  }

  inline SetValBranch
  SET_VAL(SetBranchVal v, SetBranchCommit c) {
    return SetValBranch(function_cast<VoidFunction>(v),
                        function_cast<VoidFunction>(c));
  }

}

// STATISTICS: set-branch
