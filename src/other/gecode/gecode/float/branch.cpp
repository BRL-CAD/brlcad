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

namespace Gecode {

  BrancherHandle
  branch(Home home, const FloatVarArgs& x,
         FloatVarBranch vars, FloatValBranch vals, 
         FloatBranchFilter bf, FloatVarValPrint vvp) {
    using namespace Float;
    if (home.failed()) return BrancherHandle();
    vars.expand(home,x);
    ViewArray<FloatView> xv(home,x);
    ViewSel<FloatView>* vs[1] = { 
      Branch::viewsel(home,vars) 
    };
    return ViewValBrancher<FloatView,1,FloatNumBranch,2>::post
      (home,xv,vs,Branch::valselcommit(home,vals),bf,vvp);
  }

  BrancherHandle
  branch(Home home, const FloatVarArgs& x,
         TieBreak<FloatVarBranch> vars, FloatValBranch vals, 
         FloatBranchFilter bf, FloatVarValPrint vvp) {
    using namespace Float;
    if (home.failed()) return BrancherHandle();
    vars.a.expand(home,x);
    if ((vars.a.select() == FloatVarBranch::SEL_NONE) ||
        (vars.a.select() == FloatVarBranch::SEL_RND))
      vars.b = FLOAT_VAR_NONE();
    vars.b.expand(home,x);
    if ((vars.b.select() == FloatVarBranch::SEL_NONE) ||
        (vars.b.select() == FloatVarBranch::SEL_RND))
      vars.c = FLOAT_VAR_NONE();
    vars.c.expand(home,x);
    if ((vars.c.select() == FloatVarBranch::SEL_NONE) ||
        (vars.c.select() == FloatVarBranch::SEL_RND))
      vars.d = FLOAT_VAR_NONE();
    vars.d.expand(home,x);
    if (vars.b.select() == FloatVarBranch::SEL_NONE) {
      return branch(home,x,vars.a,vals,bf,vvp);
    } else {
      ViewArray<FloatView> xv(home,x);
      ValSelCommitBase<FloatView,FloatNumBranch>* 
        vsc = Branch::valselcommit(home,vals); 
      if (vars.c.select() == FloatVarBranch::SEL_NONE) {
        ViewSel<FloatView>* vs[2] = { 
          Branch::viewsel(home,vars.a),Branch::viewsel(home,vars.b)
        };
        return ViewValBrancher<FloatView,2,FloatNumBranch,2>
          ::post(home,xv,vs,vsc,bf,vvp);
      } else if (vars.d.select() == FloatVarBranch::SEL_NONE) {
        ViewSel<FloatView>* vs[3] = { 
          Branch::viewsel(home,vars.a),Branch::viewsel(home,vars.b),
          Branch::viewsel(home,vars.c)
        };
        return ViewValBrancher<FloatView,3,FloatNumBranch,2>
          ::post(home,xv,vs,vsc,bf,vvp);
      } else {
        ViewSel<FloatView>* vs[4] = { 
          Branch::viewsel(home,vars.a),Branch::viewsel(home,vars.b),
          Branch::viewsel(home,vars.c),Branch::viewsel(home,vars.d)
        };
        return ViewValBrancher<FloatView,4,FloatNumBranch,2>
          ::post(home,xv,vs,vsc,bf,vvp);
      }
    }
  }

  BrancherHandle
  branch(Home home, FloatVar x, FloatValBranch vals, FloatVarValPrint vvp) {
    FloatVarArgs xv(1); xv[0]=x;
    return branch(home, xv, FLOAT_VAR_NONE(), vals, NULL, vvp);
  }
  
  BrancherHandle
  assign(Home home, const FloatVarArgs& x, FloatAssign fa,
         FloatBranchFilter bf, FloatVarValPrint vvp) {
    using namespace Float;
    if (home.failed()) return BrancherHandle();
    ViewArray<FloatView> xv(home,x);
    ViewSel<FloatView>* vs[1] = { 
      new (home) ViewSelNone<FloatView>(home,FLOAT_VAR_NONE())
    };
    return ViewValBrancher<FloatView,1,FloatNumBranch,1>::post
      (home,xv,vs,Branch::valselcommit(home,fa),bf,vvp);
  }

  BrancherHandle
  assign(Home home, FloatVar x, FloatAssign fa, FloatVarValPrint vvp) {
    FloatVarArgs xv(1); xv[0]=x;
    return assign(home, xv, fa, NULL, vvp);
  }
  
}

// STATISTICS: float-post
