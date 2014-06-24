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
  FloatVarBranch::FloatVarBranch(void)
    : VarBranch(NULL), s(SEL_NONE) {}

  forceinline 
  FloatVarBranch::FloatVarBranch(Select s0, BranchTbl t)
    : VarBranch(t), s(s0) {}

  forceinline 
  FloatVarBranch::FloatVarBranch(Rnd r)
    : VarBranch(r), s(SEL_RND) {}

  forceinline 
  FloatVarBranch::FloatVarBranch(Select s0, double d, BranchTbl t)
    : VarBranch(d,t), s(s0) {}

  forceinline 
  FloatVarBranch::FloatVarBranch(Select s0, AFC a, BranchTbl t)
    : VarBranch(a,t), s(s0) {}

  forceinline 
  FloatVarBranch::FloatVarBranch(Select s0, Activity a, BranchTbl t)
    : VarBranch(a,t), s(s0) {}

  forceinline 
  FloatVarBranch::FloatVarBranch(Select s0, VoidFunction mf, BranchTbl t)
    : VarBranch(mf,t), s(s0) {}

  forceinline FloatVarBranch::Select
  FloatVarBranch::select(void) const {
    return s;
  }

  forceinline void
  FloatVarBranch::expand(Home home, const FloatVarArgs& x) {
    switch (select()) {
    case SEL_AFC_MIN: case SEL_AFC_MAX:
    case SEL_AFC_SIZE_MIN: case SEL_AFC_SIZE_MAX:
      if (!_afc.initialized())
        _afc = FloatAFC(home,x,decay());
      break;
    case SEL_ACTIVITY_MIN: case SEL_ACTIVITY_MAX:
    case SEL_ACTIVITY_SIZE_MIN: case SEL_ACTIVITY_SIZE_MAX:
      if (!_act.initialized())
        _act = FloatActivity(home,x,decay());
      break;
    default: ;
    }
  }


  inline FloatVarBranch
  FLOAT_VAR_NONE(void) {
    return FloatVarBranch(FloatVarBranch::SEL_NONE,NULL);
  }

  inline FloatVarBranch
  FLOAT_VAR_MERIT_MIN(FloatBranchMerit bm, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_MERIT_MIN,
                          function_cast<VoidFunction>(bm),tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_MERIT_MAX(FloatBranchMerit bm, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_MERIT_MAX,
                          function_cast<VoidFunction>(bm),tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_RND(Rnd r) {
    return FloatVarBranch(r);
  }

  inline FloatVarBranch
  FLOAT_VAR_DEGREE_MIN(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_DEGREE_MIN,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_DEGREE_MAX(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_DEGREE_MAX,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_AFC_MIN(double d, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_AFC_MIN,d,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_AFC_MIN(FloatAFC a, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_AFC_MIN,a,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_AFC_MAX(double d, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_AFC_MAX,d,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_AFC_MAX(FloatAFC a, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_AFC_MAX,a,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_ACTIVITY_MIN(double d, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_ACTIVITY_MIN,d,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_ACTIVITY_MIN(FloatActivity a, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_ACTIVITY_MIN,a,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_ACTIVITY_MAX(double d, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_ACTIVITY_MAX,d,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_ACTIVITY_MAX(FloatActivity a, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_ACTIVITY_MAX,a,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_MIN_MIN(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_MIN_MIN,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_MIN_MAX(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_MIN_MAX,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_MAX_MIN(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_MAX_MIN,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_MAX_MAX(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_MAX_MAX,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_SIZE_MIN(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_SIZE_MIN,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_SIZE_MAX(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_SIZE_MAX,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_DEGREE_SIZE_MIN(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_DEGREE_SIZE_MIN,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_DEGREE_SIZE_MAX(BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_DEGREE_SIZE_MAX,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_AFC_SIZE_MIN(double d, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_AFC_SIZE_MIN,d,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_AFC_SIZE_MIN(FloatAFC a, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_AFC_SIZE_MIN,a,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_AFC_SIZE_MAX(double d, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_AFC_SIZE_MAX,d,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_AFC_SIZE_MAX(FloatAFC a, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_AFC_SIZE_MAX,a,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_ACTIVITY_SIZE_MIN(double d, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_ACTIVITY_SIZE_MIN,d,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_ACTIVITY_SIZE_MIN(FloatActivity a, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_ACTIVITY_SIZE_MIN,a,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_ACTIVITY_SIZE_MAX(double d, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_ACTIVITY_SIZE_MAX,d,tbl);
  }

  inline FloatVarBranch
  FLOAT_VAR_ACTIVITY_SIZE_MAX(FloatActivity a, BranchTbl tbl) {
    return FloatVarBranch(FloatVarBranch::SEL_ACTIVITY_SIZE_MAX,a,tbl);
  }

}

// STATISTICS: float-branch
