/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christopher Mears <chris.mears@monash.edu>
 *
 *  Copyright:
 *     Christopher Mears, 2012
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

#include <gecode/set/ldsb.hh>
#include <gecode/set/branch.hh>
#include <map>

namespace Gecode {
  using namespace Int::LDSB;
  /*
   * Implementation of some SymmetryHandle methods.
   */
  
  SymmetryHandle VariableSymmetry(const SetVarArgs& x) {
    ArgArray<VarImpBase*> a(x.size());
    for (int i = 0 ; i < x.size() ; i++)
      a[i] = x[i].varimp();
    return SymmetryHandle(new VariableSymmetryObject(a));
  }
  SymmetryHandle VariableSequenceSymmetry(const SetVarArgs& x, int ss) {
    ArgArray<VarImpBase*> a(x.size());
    for (int i = 0 ; i < x.size() ; i++)
      a[i] = x[i].varimp();
    return SymmetryHandle(new VariableSequenceSymmetryObject(a, ss));
  }
}

namespace Gecode { namespace Int { namespace LDSB {
  template <>
  ModEvent prune<Set::SetView>(Space& home, Set::SetView x, int v) {
    return x.exclude(home, v);
  }
}}}

namespace Gecode { namespace Set { namespace LDSB {

  /// Map from variable implementation to index
  class VariableMap : public std::map<VarImpBase*,int> {};

  /*
   * The createSetSym function is an almost exact copy of
   * createIntSym/createBoolSym.
   */
  SymmetryImp<SetView>* createSetSym(Space& home, const SymmetryHandle& s,
                                     VariableMap variableMap) {
    VariableSymmetryObject* varref    = 
      dynamic_cast<VariableSymmetryObject*>(s.ref);
    ValueSymmetryObject* valref    = 
      dynamic_cast<ValueSymmetryObject*>(s.ref);
    VariableSequenceSymmetryObject* varseqref = 
      dynamic_cast<VariableSequenceSymmetryObject*>(s.ref);
    ValueSequenceSymmetryObject* valseqref = 
      dynamic_cast<ValueSequenceSymmetryObject*>(s.ref);
    if (varref) {
      int n = varref->nxs;
      int* indices = home.alloc<int>(n);
      for (int i = 0 ; i < n ; i++) {
        VariableMap::const_iterator index = variableMap.find(varref->xs[i]);
        if (index == variableMap.end())
          throw 
            Int::LDSBUnbranchedVariable("VariableSymmetryObject::createSet");
        indices[i] = index->second;
      }
      return new (home) VariableSymmetryImp<SetView>(home, indices, n);
    }
    if (valref) {
      int n = valref->values.size();
      int *vs = home.alloc<int>(n);
      int i = 0;
      for (IntSetValues v(valref->values) ; v() ; ++v) {
        vs[i] = v.val();
        i++;
      }
      return new (home) ValueSymmetryImp<SetView>(home, vs, n);
    }
    if (varseqref) {
      int n = varseqref->nxs;
      int* indices = home.alloc<int>(n);
      for (int i = 0 ; i < n ; i++) {
        VariableMap::const_iterator index = 
          variableMap.find(varseqref->xs[i]);
        if (index == variableMap.end())
          throw 
       Int::LDSBUnbranchedVariable("VariableSequenceSymmetryObject::createSet");
        indices[i] = index->second;
      }
      return new (home) VariableSequenceSymmetryImp<SetView>(home, indices, n, 
        varseqref->seq_size);
    }
    if (valseqref) {
      unsigned int n = valseqref->values.size();
      int *vs = home.alloc<int>(n);
      for (unsigned int i = 0 ; i < n ; i++)
        vs[i] = valseqref->values[i];
      return new (home) ValueSequenceSymmetryImp<SetView>(home, vs, n, 
        valseqref->seq_size);
    }
    GECODE_NEVER;
    return NULL;
  }
}}}

namespace Gecode {

  using namespace Set::LDSB;

  BrancherHandle
  branch(Home home, const SetVarArgs& x,
         SetVarBranch vars, SetValBranch vals,
         const Symmetries& syms,
         SetBranchFilter bf, SetVarValPrint vvp) {
    using namespace Set;
    if (home.failed()) return BrancherHandle();
    vars.expand(home,x);
    ViewArray<SetView> xv(home,x);
    ViewSel<SetView>* vs[1] = { 
      Branch::viewsel(home,vars) 
    };

    // Construct mapping from each variable in the array to its index
    // in the array.
    VariableMap variableMap;
    for (int i = 0 ; i < x.size() ; i++)
      variableMap[x[i].varimp()] = i;
    
    // Convert the modelling-level Symmetries object into an array of
    // SymmetryImp objects.
    int n = syms.size();
    SymmetryImp<SetView>** array = 
      static_cast<Space&>(home).alloc<SymmetryImp<SetView>* >(n);
    for (int i = 0 ; i < n ; i++) {
      array[i] = createSetSym(home, syms[i], variableMap);
    }

    return LDSBSetBrancher<SetView,1,int,2>::post
      (home,xv,vs,Branch::valselcommit(home,vals),array,n,bf,vvp);
  }

  BrancherHandle
  branch(Home home, const SetVarArgs& x,
         TieBreak<SetVarBranch> vars, SetValBranch vals,
         const Symmetries& syms, 
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
      return branch(home,x,vars.a,vals,syms,bf,vvp);
    } else {
      // Construct mapping from each variable in the array to its index
      // in the array.
      VariableMap variableMap;
      for (int i = 0 ; i < x.size() ; i++)
        variableMap[x[i].varimp()] = i;
      
      // Convert the modelling-level Symmetries object into an array of
      // SymmetryImp objects.
      int n = syms.size();
      SymmetryImp<SetView>** array =
        static_cast<Space&>(home).alloc<SymmetryImp<SetView>* >(n);
      for (int i = 0 ; i < n ; i++) {
        array[i] = Set::LDSB::createSetSym(home, syms[i], variableMap);
      }

      ViewArray<SetView> xv(home,x);
      ValSelCommitBase<SetView,int>* vsc = Branch::valselcommit(home,vals); 
      if (vars.c.select() == SetVarBranch::SEL_NONE) {
        ViewSel<SetView>* vs[2] = { 
          Branch::viewsel(home,vars.a),Branch::viewsel(home,vars.b)
        };
        return 
          LDSBSetBrancher<SetView,2,int,2>::post(home,xv,vs,vsc,array,n,bf,vvp);
      } else if (vars.d.select() == SetVarBranch::SEL_NONE) {
        ViewSel<SetView>* vs[3] = { 
          Branch::viewsel(home,vars.a),Branch::viewsel(home,vars.b),
          Branch::viewsel(home,vars.c)
        };
        return 
          LDSBSetBrancher<SetView,3,int,2>::post(home,xv,vs,vsc,array,n,bf,vvp);
      } else {
        ViewSel<SetView>* vs[4] = { 
          Branch::viewsel(home,vars.a),Branch::viewsel(home,vars.b),
          Branch::viewsel(home,vars.c),Branch::viewsel(home,vars.d)
        };
        return 
          LDSBSetBrancher<SetView,4,int,2>::post(home,xv,vs,vsc,array,n,bf,vvp);
      }
    }
  }

}

// STATISTICS: set-branch
