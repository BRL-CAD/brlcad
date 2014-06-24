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

namespace Gecode {
  BrancherHandle
  branch(Home home, const SetVarArgs& x,
         SetVarBranch vars, SetValBranch vals, 
         SetBranchFilter bf, SetVarValPrint vvp) {
    using namespace Set;
    if (home.failed()) return BrancherHandle();
    vars.expand(home,x);
    ViewArray<SetView> xv(home,x);
    ViewSel<SetView>* vs[1] = { 
      Branch::viewsel(home,vars) 
    };
    return ViewValBrancher<SetView,1,int,2>::post
      (home,xv,vs,Branch::valselcommit(home,vals),bf,vvp);
  }

  BrancherHandle
  branch(Home home, const SetVarArgs& x,
         TieBreak<SetVarBranch> vars, SetValBranch vals, 
         SetBranchFilter bf, SetVarValPrint vvp) {
    using namespace Set;
    if (home.failed()) return BrancherHandle();
    vars.a.expand(home,x);
    if ((vars.a.select() == SetVarBranch::SEL_NONE) ||
        (vars.a.select() == SetVarBranch::SEL_RND))
      vars.b = SET_VAR_NONE();
    vars.b.expand(home,x);
    if ((vars.b.select() == SetVarBranch::SEL_NONE) ||
        (vars.b.select() == SetVarBranch::SEL_RND))
      vars.c = SET_VAR_NONE();
    vars.c.expand(home,x);
    if ((vars.c.select() == SetVarBranch::SEL_NONE) ||
        (vars.c.select() == SetVarBranch::SEL_RND))
      vars.d = SET_VAR_NONE();
    vars.d.expand(home,x);
    if (vars.b.select() == SetVarBranch::SEL_NONE) {
      return branch(home,x,vars.a,vals,bf,vvp);
    } else {
      ViewArray<SetView> xv(home,x);
      ValSelCommitBase<SetView,int>* vsc = Branch::valselcommit(home,vals); 
      if (vars.c.select() == SetVarBranch::SEL_NONE) {
        ViewSel<SetView>* vs[2] = { 
          Branch::viewsel(home,vars.a),Branch::viewsel(home,vars.b)
        };
        return ViewValBrancher<SetView,2,int,2>::post(home,xv,vs,vsc,bf,vvp);
      } else if (vars.d.select() == SetVarBranch::SEL_NONE) {
        ViewSel<SetView>* vs[3] = { 
          Branch::viewsel(home,vars.a),Branch::viewsel(home,vars.b),
          Branch::viewsel(home,vars.c)
        };
        return ViewValBrancher<SetView,3,int,2>::post(home,xv,vs,vsc,bf,vvp);
      } else {
        ViewSel<SetView>* vs[4] = { 
          Branch::viewsel(home,vars.a),Branch::viewsel(home,vars.b),
          Branch::viewsel(home,vars.c),Branch::viewsel(home,vars.d)
        };
        return ViewValBrancher<SetView,4,int,2>::post(home,xv,vs,vsc,bf,vvp);
      }
    }
  }

  BrancherHandle
  branch(Home home, SetVar x, SetValBranch vals, SetVarValPrint vvp) {
    SetVarArgs xv(1); xv[0]=x;
    return branch(home, xv, SET_VAR_NONE(), vals, NULL, vvp);
  }
  
  BrancherHandle
  assign(Home home, const SetVarArgs& x, SetAssign sa,
         SetBranchFilter bf, SetVarValPrint vvp) {
    using namespace Set;
    if (home.failed()) return BrancherHandle();
    ViewArray<SetView> xv(home,x);
    ViewSel<SetView>* vs[1] = { 
      new (home) ViewSelNone<SetView>(home,SET_VAR_NONE())
    };
    return ViewValBrancher<SetView,1,int,1>::post
      (home,xv,vs,Branch::valselcommit(home,sa),bf,vvp);
  }

  BrancherHandle
  assign(Home home, SetVar x, SetAssign sa, SetVarValPrint vvp) {
    SetVarArgs xv(1); xv[0]=x;
    return assign(home, xv, sa, NULL, vvp);
  }
  
}

// STATISTICS: set-post
