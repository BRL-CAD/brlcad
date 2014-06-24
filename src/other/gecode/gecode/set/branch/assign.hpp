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
  SetAssign::SetAssign(Select s0)
    : s(s0) {}

  forceinline 
  SetAssign::SetAssign(Select s0, Rnd r)
    : ValBranch(r), s(s0) {}

  forceinline 
  SetAssign::SetAssign(VoidFunction v, VoidFunction c)
    : ValBranch(v,c), s(SEL_VAL_COMMIT) {}

  forceinline SetAssign::Select
  SetAssign::select(void) const {
    return s;
  }


  inline SetAssign
  SET_ASSIGN_MIN_INC(void) {
    return SetAssign(SetAssign::SEL_MIN_INC);
  }

  inline SetAssign
  SET_ASSIGN_MIN_EXC(void) {
    return SetAssign(SetAssign::SEL_MIN_EXC);
  }

  inline SetAssign
  SET_ASSIGN_MED_INC(void) {
    return SetAssign(SetAssign::SEL_MED_INC);
  }

  inline SetAssign
  SET_ASSIGN_MED_EXC(void) {
    return SetAssign(SetAssign::SEL_MED_EXC);
  }

  inline SetAssign
  SET_ASSIGN_MAX_INC(void) {
    return SetAssign(SetAssign::SEL_MAX_INC);
  }

  inline SetAssign
  SET_ASSIGN_MAX_EXC(void) {
    return SetAssign(SetAssign::SEL_MAX_EXC);
  }

  inline SetAssign
  SET_ASSIGN_RND_INC(Rnd r) {
    return SetAssign(SetAssign::SEL_RND_INC,r);
  }

  inline SetAssign
  SET_ASSIGN_RND_EXC(Rnd r) {
    return SetAssign(SetAssign::SEL_RND_EXC,r);
  }

  inline SetAssign
  SET_ASSIGN(SetBranchVal v, SetBranchCommit c) {
    return SetAssign(function_cast<VoidFunction>(v),
                     function_cast<VoidFunction>(c));
  }

}

// STATISTICS: set-branch
