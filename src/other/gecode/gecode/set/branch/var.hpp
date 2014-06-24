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
  SetVarBranch::SetVarBranch(void)
    : VarBranch(NULL), s(SEL_NONE) {}

  forceinline 
  SetVarBranch::SetVarBranch(Select s0, BranchTbl t)
    : VarBranch(t), s(s0) {}

  forceinline 
  SetVarBranch::SetVarBranch(Rnd r)
    : VarBranch(r), s(SEL_RND) {}

  forceinline 
  SetVarBranch::SetVarBranch(Select s0, double d, BranchTbl t)
    : VarBranch(d,t), s(s0) {}

  forceinline 
  SetVarBranch::SetVarBranch(Select s0, AFC a, BranchTbl t)
    : VarBranch(a,t), s(s0) {}

  forceinline 
  SetVarBranch::SetVarBranch(Select s0, Activity a, BranchTbl t)
    : VarBranch(a,t), s(s0) {}

  forceinline 
  SetVarBranch::SetVarBranch(Select s0, VoidFunction mf, BranchTbl t)
    : VarBranch(mf,t), s(s0) {}

  forceinline SetVarBranch::Select
  SetVarBranch::select(void) const {
    return s;
  }

  forceinline void
  SetVarBranch::expand(Home home, const SetVarArgs& x) {
    switch (select()) {
    case SEL_AFC_MIN: case SEL_AFC_MAX:
    case SEL_AFC_SIZE_MIN: case SEL_AFC_SIZE_MAX:
      if (!_afc.initialized())
        _afc = SetAFC(home,x,decay());
      break;
    case SEL_ACTIVITY_MIN: case SEL_ACTIVITY_MAX:
    case SEL_ACTIVITY_SIZE_MIN: case SEL_ACTIVITY_SIZE_MAX:
      if (!_act.initialized())
        _act = SetActivity(home,x,decay());
      break;
    default: ;
    }
  }

  inline SetVarBranch
  SET_VAR_NONE(void) {
    return SetVarBranch(SetVarBranch::SEL_NONE,NULL);
  }

  inline SetVarBranch
  SET_VAR_RND(Rnd r) {
    return SetVarBranch(r);
  }

  inline SetVarBranch
  SET_VAR_MERIT_MIN(SetBranchMerit bm, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_MERIT_MIN,
                        function_cast<VoidFunction>(bm),tbl);
  }

  inline SetVarBranch
  SET_VAR_MERIT_MAX(SetBranchMerit bm, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_MERIT_MAX,
                        function_cast<VoidFunction>(bm),tbl);
  }

  inline SetVarBranch
  SET_VAR_DEGREE_MIN(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_DEGREE_MIN,tbl);
  }

  inline SetVarBranch
  SET_VAR_DEGREE_MAX(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_DEGREE_MAX,tbl);
  }

  inline SetVarBranch
  SET_VAR_AFC_MIN(double d, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_AFC_MIN,d,tbl);
  }

  inline SetVarBranch
  SET_VAR_AFC_MIN(SetAFC a, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_AFC_MIN,a,tbl);
  }

  inline SetVarBranch
  SET_VAR_AFC_MAX(double d, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_AFC_MAX,d,tbl);
  }

  inline SetVarBranch
  SET_VAR_AFC_MAX(SetAFC a, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_AFC_MAX,a,tbl);
  }

  inline SetVarBranch
  SET_VAR_ACTIVITY_MIN(double d, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_ACTIVITY_MIN,d,tbl);
  }

  inline SetVarBranch
  SET_VAR_ACTIVITY_MIN(SetActivity a, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_ACTIVITY_MIN,a,tbl);
  }

  inline SetVarBranch
  SET_VAR_ACTIVITY_MAX(double d, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_ACTIVITY_MAX,d,tbl);
  }

  inline SetVarBranch
  SET_VAR_ACTIVITY_MAX(SetActivity a, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_ACTIVITY_MAX,a,tbl);
  }

  inline SetVarBranch
  SET_VAR_MIN_MIN(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_MIN_MIN,tbl);
  }

  inline SetVarBranch
  SET_VAR_MIN_MAX(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_MIN_MAX,tbl);
  }

  inline SetVarBranch
  SET_VAR_MAX_MIN(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_MAX_MIN,tbl);
  }

  inline SetVarBranch
  SET_VAR_MAX_MAX(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_MAX_MAX,tbl);
  }

  inline SetVarBranch
  SET_VAR_SIZE_MIN(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_SIZE_MIN,tbl);
  }

  inline SetVarBranch
  SET_VAR_SIZE_MAX(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_SIZE_MAX,tbl);
  }

  inline SetVarBranch
  SET_VAR_DEGREE_SIZE_MIN(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_DEGREE_SIZE_MIN,tbl);
  }

  inline SetVarBranch
  SET_VAR_DEGREE_SIZE_MAX(BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_DEGREE_SIZE_MAX,tbl);
  }

  inline SetVarBranch
  SET_VAR_AFC_SIZE_MIN(double d, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_AFC_SIZE_MIN,d,tbl);
  }

  inline SetVarBranch
  SET_VAR_AFC_SIZE_MIN(SetAFC a, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_AFC_SIZE_MIN,a,tbl);
  }

  inline SetVarBranch
  SET_VAR_AFC_SIZE_MAX(double d, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_AFC_SIZE_MAX,d,tbl);
  }

  inline SetVarBranch
  SET_VAR_AFC_SIZE_MAX(SetAFC a, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_AFC_SIZE_MAX,a,tbl);
  }

  inline SetVarBranch
  SET_VAR_ACTIVITY_SIZE_MIN(double d, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_ACTIVITY_SIZE_MIN,d,tbl);
  }

  inline SetVarBranch
  SET_VAR_ACTIVITY_SIZE_MIN(SetActivity a, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_ACTIVITY_SIZE_MIN,a,tbl);
  }

  inline SetVarBranch
  SET_VAR_ACTIVITY_SIZE_MAX(double d, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_ACTIVITY_SIZE_MAX,d,tbl);
  }

  inline SetVarBranch
  SET_VAR_ACTIVITY_SIZE_MAX(SetActivity a, BranchTbl tbl) {
    return SetVarBranch(SetVarBranch::SEL_ACTIVITY_SIZE_MAX,a,tbl);
  }

}

// STATISTICS: set-branch
