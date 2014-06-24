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

namespace Gecode {

  BrancherHandle
  branch(Home home, const IntVarArgs& x,
         IntVarBranch vars, IntValBranch vals, 
         IntBranchFilter bf, IntVarValPrint vvp) {
    using namespace Int;
    if (home.failed()) return BrancherHandle();
    vars.expand(home,x);
    ViewArray<IntView> xv(home,x);
    ViewSel<IntView>* vs[1] = { 
      Branch::viewselint(home,vars) 
    };
    switch (vals.select()) {
    case IntValBranch::SEL_VALUES_MIN:
      return Branch::ViewValuesBrancher<1,true>::post(home,xv,vs,bf,vvp);
      break;
    case IntValBranch::SEL_VALUES_MAX:
      return Branch::ViewValuesBrancher<1,false>::post(home,xv,vs,bf,vvp);
      break;
    default:
      return ViewValBrancher<IntView,1,int,2>::post
        (home,xv,vs,Branch::valselcommitint(home,x.size(),vals),bf,vvp);
    }
  }

  BrancherHandle
  branch(Home home, const IntVarArgs& x,
         TieBreak<IntVarBranch> vars, IntValBranch vals, 
         IntBranchFilter bf, IntVarValPrint vvp) {
    using namespace Int;
    if (home.failed()) return BrancherHandle();
    vars.a.expand(home,x);
    if ((vars.a.select() == IntVarBranch::SEL_NONE) ||
        (vars.a.select() == IntVarBranch::SEL_RND))
      vars.b = INT_VAR_NONE();
    vars.b.expand(home,x);
    if ((vars.b.select() == IntVarBranch::SEL_NONE) ||
        (vars.b.select() == IntVarBranch::SEL_RND))
      vars.c = INT_VAR_NONE();
    vars.c.expand(home,x);
    if ((vars.c.select() == IntVarBranch::SEL_NONE) ||
        (vars.c.select() == IntVarBranch::SEL_RND))
      vars.d = INT_VAR_NONE();
    vars.d.expand(home,x);
    if (vars.b.select() == IntVarBranch::SEL_NONE) {
      return branch(home,x,vars.a,vals,bf,vvp);
    } else {
      ViewArray<IntView> xv(home,x);
      if (vars.c.select() == IntVarBranch::SEL_NONE) {
        ViewSel<IntView>* vs[2] = { 
          Branch::viewselint(home,vars.a),Branch::viewselint(home,vars.b)
        };
        switch (vals.select()) {
        case IntValBranch::SEL_VALUES_MIN:
          return Branch::ViewValuesBrancher<2,true>::post(home,xv,vs,bf,vvp);
          break;
        case IntValBranch::SEL_VALUES_MAX:
          return Branch::ViewValuesBrancher<2,false>::post(home,xv,vs,bf,vvp);
          break;
        default:
          return ViewValBrancher<IntView,2,int,2>
            ::post(home,xv,vs,Branch::valselcommitint(home,x.size(),vals),
                   bf,vvp);
        }
      } else if (vars.d.select() == IntVarBranch::SEL_NONE) {
        ViewSel<IntView>* vs[3] = { 
          Branch::viewselint(home,vars.a),Branch::viewselint(home,vars.b),
          Branch::viewselint(home,vars.c)
        };
        switch (vals.select()) {
        case IntValBranch::SEL_VALUES_MIN:
          return Branch::ViewValuesBrancher<3,true>::post(home,xv,vs,bf,vvp);
          break;
        case IntValBranch::SEL_VALUES_MAX:
          return Branch::ViewValuesBrancher<3,false>::post(home,xv,vs,bf,vvp);
          break;
        default:
          return ViewValBrancher<IntView,3,int,2>
            ::post(home,xv,vs,Branch::valselcommitint(home,x.size(),vals),
                   bf,vvp);
        }
      } else {
        ViewSel<IntView>* vs[4] = { 
          Branch::viewselint(home,vars.a),Branch::viewselint(home,vars.b),
          Branch::viewselint(home,vars.c),Branch::viewselint(home,vars.d)
        };
        switch (vals.select()) {
        case IntValBranch::SEL_VALUES_MIN:
          return Branch::ViewValuesBrancher<4,true>::post(home,xv,vs,bf,vvp);
          break;
        case IntValBranch::SEL_VALUES_MAX:
          return Branch::ViewValuesBrancher<4,false>::post(home,xv,vs,bf,vvp);
          break;
        default:
          return ViewValBrancher<IntView,4,int,2>
            ::post(home,xv,vs,Branch::valselcommitint(home,x.size(),vals),
                   bf,vvp);
        }
      }
    }
  }

  BrancherHandle
  branch(Home home, IntVar x, IntValBranch vals, IntVarValPrint vvp) {
    IntVarArgs xv(1); xv[0]=x;
    return branch(home, xv, INT_VAR_NONE(), vals, NULL, vvp);
  }
  
  BrancherHandle
  assign(Home home, const IntVarArgs& x, IntAssign ia,
         IntBranchFilter bf, IntVarValPrint vvp) {
    using namespace Int;
    if (home.failed()) return BrancherHandle();
    ViewArray<IntView> xv(home,x);
    ViewSel<IntView>* vs[1] = { 
      new (home) ViewSelNone<IntView>(home,INT_VAR_NONE())
    };
    return ViewValBrancher<IntView,1,int,1>::post
      (home,xv,vs,Branch::valselcommitint(home,ia),bf,vvp);
  }

  BrancherHandle
  assign(Home home, IntVar x, IntAssign ia, IntVarValPrint vvp) {
    IntVarArgs xv(1); xv[0]=x;
    return assign(home, xv, ia, NULL, vvp);
  }
  

  BrancherHandle
  branch(Home home, const BoolVarArgs& x,
         IntVarBranch vars, IntValBranch vals, 
         BoolBranchFilter bf, BoolVarValPrint vvp) {
    using namespace Int;
    if (home.failed()) return BrancherHandle();
    vars.expand(home,x);
    ViewArray<BoolView> xv(home,x);
    ViewSel<BoolView>* vs[1] = { 
      Branch::viewselbool(home,vars) 
    };
    return ViewValBrancher<BoolView,1,int,2>::post
      (home,xv,vs,Branch::valselcommitbool(home,x.size(),vals),bf,vvp);
  }

  BrancherHandle
  branch(Home home, const BoolVarArgs& x,
         TieBreak<IntVarBranch> vars, IntValBranch vals, 
         BoolBranchFilter bf, BoolVarValPrint vvp) {
    using namespace Int;
    if (home.failed()) return BrancherHandle();
    vars.a.expand(home,x);
    if ((vars.a.select() == IntVarBranch::SEL_NONE) ||
        (vars.a.select() == IntVarBranch::SEL_RND))
      vars.b = INT_VAR_NONE();
    vars.b.expand(home,x);
    if ((vars.b.select() == IntVarBranch::SEL_NONE) ||
        (vars.b.select() == IntVarBranch::SEL_RND))
      vars.c = INT_VAR_NONE();
    vars.c.expand(home,x);
    if ((vars.c.select() == IntVarBranch::SEL_NONE) ||
        (vars.c.select() == IntVarBranch::SEL_RND))
      vars.d = INT_VAR_NONE();
    vars.d.expand(home,x);
    if (vars.b.select() == IntVarBranch::SEL_NONE) {
      return branch(home,x,vars.a,vals,bf,vvp);
    } else {
      ViewArray<BoolView> xv(home,x);
      ValSelCommitBase<BoolView,int>* 
        vsc = Branch::valselcommitbool(home,x.size(),vals); 
      if (vars.c.select() == IntVarBranch::SEL_NONE) {
        ViewSel<BoolView>* vs[2] = { 
          Branch::viewselbool(home,vars.a),Branch::viewselbool(home,vars.b)
        };
        return ViewValBrancher<BoolView,2,int,2>::post(home,xv,vs,vsc,bf,vvp);
      } else if (vars.d.select() == IntVarBranch::SEL_NONE) {
        ViewSel<BoolView>* vs[3] = { 
          Branch::viewselbool(home,vars.a),Branch::viewselbool(home,vars.b),
          Branch::viewselbool(home,vars.c)
        };
        return ViewValBrancher<BoolView,3,int,2>::post(home,xv,vs,vsc,bf,vvp);
      } else {
        ViewSel<BoolView>* vs[4] = { 
          Branch::viewselbool(home,vars.a),Branch::viewselbool(home,vars.b),
          Branch::viewselbool(home,vars.c),Branch::viewselbool(home,vars.d)
        };
        return ViewValBrancher<BoolView,4,int,2>::post(home,xv,vs,vsc,bf,vvp);
      }
    }
  }

  BrancherHandle
  branch(Home home, BoolVar x, IntValBranch vals, BoolVarValPrint vvp) {
    BoolVarArgs xv(1); xv[0]=x;
    return branch(home, xv, INT_VAR_NONE(), vals, NULL, vvp);
  }
  
  BrancherHandle
  assign(Home home, const BoolVarArgs& x, IntAssign ia,
         BoolBranchFilter bf, BoolVarValPrint vvp) {
    using namespace Int;
    if (home.failed()) return BrancherHandle();
    ViewArray<BoolView> xv(home,x);
    ViewSel<BoolView>* vs[1] = { 
      new (home) ViewSelNone<BoolView>(home,INT_VAR_NONE())
    };
    return ViewValBrancher<BoolView,1,int,1>::post
      (home,xv,vs,Branch::valselcommitbool(home,ia),bf,vvp);
  }

  BrancherHandle
  assign(Home home, BoolVar x, IntAssign ia, BoolVarValPrint vvp) {
    BoolVarArgs xv(1); xv[0]=x;
    return assign(home, xv, ia, NULL, vvp);
  }
  
}

// STATISTICS: int-post
