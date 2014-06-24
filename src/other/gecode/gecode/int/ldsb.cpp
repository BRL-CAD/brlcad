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

#include <gecode/int/ldsb.hh>
#include <gecode/int/branch.hh>

#include <map>

namespace Gecode { namespace Int { namespace LDSB {

  std::pair<int,int>
  findVar(int *indices, unsigned int n_values, unsigned int seq_size, int index) {
    unsigned int seq = 0;
    unsigned int pos = 0;
    for (unsigned int i = 0 ; i < n_values ; i++) {
      if (indices[i] == index)
        return std::pair<int,int>(seq,pos);
      pos++;
      if (pos == seq_size) {
        pos = 0;
        seq++;
      }
    }
    return std::pair<int,int>(-1,-1);
  }

}}}

namespace Gecode {
  using namespace Int::LDSB;
  
  SymmetryHandle VariableSymmetry(const IntVarArgs& vars) {
    ArgArray<VarImpBase*> a(vars.size());
    for (int i = 0 ; i < vars.size() ; i++)
      a[i] = vars[i].varimp();
    return SymmetryHandle(new VariableSymmetryObject(a));
  }
  SymmetryHandle VariableSymmetry(const BoolVarArgs& vars) {
    ArgArray<VarImpBase*> a(vars.size());
    for (int i = 0 ; i < vars.size() ; i++)
      a[i] = vars[i].varimp();
    return SymmetryHandle(new VariableSymmetryObject(a));
  }
  SymmetryHandle VariableSymmetry(const IntVarArgs& x,
                                  const IntArgs& indices) {
    IntVarArgs xs(indices.size());
    for (int i = 0 ; i < indices.size() ; i++)
      xs[i] = x[indices[i]];
    return VariableSymmetry(xs);
  }
  SymmetryHandle ValueSymmetry(const IntArgs& vs) {
    return SymmetryHandle(new ValueSymmetryObject(IntSet(vs)));
  }
  SymmetryHandle ValueSymmetry(const IntSet& vs) {
    return SymmetryHandle(new ValueSymmetryObject(vs));
  }
  SymmetryHandle ValueSymmetry(IntVar x) {
    return ValueSymmetry(IntSet(x.min(), x.max()));
  }
  SymmetryHandle VariableSequenceSymmetry(const IntVarArgs& vars, int ss) {
    ArgArray<VarImpBase*> a(vars.size());
    for (int i = 0 ; i < vars.size() ; i++)
      a[i] = vars[i].varimp();
    return SymmetryHandle(new VariableSequenceSymmetryObject(a, ss));
  }
  SymmetryHandle VariableSequenceSymmetry(const BoolVarArgs& vars, int ss) {
    ArgArray<VarImpBase*> a(vars.size());
    for (int i = 0 ; i < vars.size() ; i++)
      a[i] = vars[i].varimp();
    return SymmetryHandle(new VariableSequenceSymmetryObject(a, ss));
  }
  SymmetryHandle ValueSequenceSymmetry(const IntArgs& vs, int ss) {
    return SymmetryHandle(new ValueSequenceSymmetryObject(vs, ss));
  }

  SymmetryHandle values_reflect(int lower, int upper) {
    int n = (upper-lower+1)/2;
    IntArgs a(n*2);
    int i = lower;
    int j = upper;
    int k = 0;
    while (i < j) {
      a[k]   = j;
      a[n+k] = i;
      i++;
      j--;
      k++;
    }
    return ValueSequenceSymmetry(a,n);
  }
  SymmetryHandle values_reflect(const IntVar& x) {
    return values_reflect(x.min(), x.max());
  }
}


namespace Gecode { namespace Int { namespace LDSB {

  /// Map from variable implementation to index
  class VariableMap : public std::map<VarImpBase*,int> {};

  /*
   * The duplication in createIntSym/createBoolSym is undesirable,
   * and so is the use of dynamic_cast to tell the symmetries
   * apart.
   */

  /// Create an integer symmetry implementation from a symmetry handle
  SymmetryImp<IntView>*
  createIntSym(Space& home, const SymmetryHandle& s,
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
          throw LDSBUnbranchedVariable("VariableSymmetryObject::createInt");
        indices[i] = index->second;
      }
      return new (home) VariableSymmetryImp<IntView>(home, indices, n);
    }
    if (valref) {
      int n = valref->values.size();
      int *vs = home.alloc<int>(n);
      int i = 0;
      for (IntSetValues v(valref->values) ; v() ; ++v) {
        vs[i] = v.val();
        i++;
      }
      return new (home) ValueSymmetryImp<IntView>(home, vs, n);
    }
    if (varseqref) {
      int n = varseqref->nxs;
      int* indices = home.alloc<int>(n);
      for (int i = 0 ; i < n ; i++) {
        VariableMap::const_iterator index = 
          variableMap.find(varseqref->xs[i]);
        if (index == variableMap.end())
          throw LDSBUnbranchedVariable("VariableSequenceSymmetryObject::createInt");
        indices[i] = index->second;
      }
      return new (home) VariableSequenceSymmetryImp<IntView>(home, indices, n, 
        varseqref->seq_size);
    }
    if (valseqref) {
      unsigned int n = valseqref->values.size();
      int *vs = home.alloc<int>(n);
      for (unsigned int i = 0 ; i < n ; i++)
        vs[i] = valseqref->values[i];
      return new (home) ValueSequenceSymmetryImp<IntView>(home, vs, n, 
        valseqref->seq_size);
    }
    GECODE_NEVER;
    return NULL;
  }

  /// Create a boolean symmetry implementation from a symmetry handle
  SymmetryImp<BoolView>* createBoolSym(Space& home, const SymmetryHandle& s,
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
          throw LDSBUnbranchedVariable("VariableSymmetryObject::createBool");
        indices[i] = index->second;
      }
      return new (home) VariableSymmetryImp<BoolView>(home, indices, n);
    }
    if (valref) {
      int n = valref->values.size();
      int *vs = home.alloc<int>(n);
      int i = 0;
      for (IntSetValues v(valref->values) ; v() ; ++v) {
        vs[i] = v.val();
        i++;
      }
      return new (home) ValueSymmetryImp<BoolView>(home, vs, n);
    }
    if (varseqref) {
      int n = varseqref->nxs;
      int* indices = home.alloc<int>(n);
      for (int i = 0 ; i < n ; i++) {
        VariableMap::const_iterator index = 
          variableMap.find(varseqref->xs[i]);
        if (index == variableMap.end())
          throw LDSBUnbranchedVariable("VariableSequenceSymmetryObject::createBool");
        indices[i] = index->second;
      }
      return new (home) VariableSequenceSymmetryImp<BoolView>(home, indices, 
        n, varseqref->seq_size);
    }
    if (valseqref) {
      unsigned int n = valseqref->values.size();
      int *vs = home.alloc<int>(n);
      for (unsigned int i = 0 ; i < n ; i++)
        vs[i] = valseqref->values[i];
      return new (home) ValueSequenceSymmetryImp<BoolView>(home, vs, n, 
        valseqref->seq_size);
    }
    GECODE_NEVER;
    return NULL;
  }
}}}

namespace Gecode {

  using namespace Int::LDSB;

  BrancherHandle
  branch(Home home, const IntVarArgs& x,
         IntVarBranch vars, IntValBranch vals,
         const Symmetries& syms, 
         IntBranchFilter bf, IntVarValPrint vvp) {
    using namespace Int;
    if (home.failed()) return BrancherHandle();
    vars.expand(home,x);
    ViewArray<IntView> xv(home,x);
    ViewSel<IntView>* vs[1] = { 
      Branch::viewselint(home,vars) 
    };
    switch (vals.select()) {
    case IntValBranch::SEL_SPLIT_MIN:
    case IntValBranch::SEL_SPLIT_MAX:
    case IntValBranch::SEL_RANGE_MIN:
    case IntValBranch::SEL_RANGE_MAX:
    case IntValBranch::SEL_VALUES_MIN:
    case IntValBranch::SEL_VALUES_MAX:
      throw LDSBBadValueSelection("Int::LDSB::branch");
      break;
    case IntValBranch::SEL_VAL_COMMIT:
      if (vals.commit() != NULL)
        throw LDSBBadValueSelection("Int::LDSB::branch");
      // If vals.commit() returns NULL, it means it will commit with
      // binary branching, which is OK for LDSB, so we fall through.
    default:
      // Construct mapping from each variable in the array to its index
      // in the array.
      VariableMap variableMap;
      for (int i = 0 ; i < x.size() ; i++)
        variableMap[x[i].varimp()] = i;

      // Convert the modelling-level Symmetries object into an array of
      // SymmetryImp objects.
      int n = syms.size();
      SymmetryImp<IntView>** array =
        static_cast<Space&>(home).alloc<SymmetryImp<IntView>* >(n);
      for (int i = 0 ; i < n ; i++) {
        array[i] = createIntSym(home, syms[i], variableMap);
      }

      return LDSBBrancher<IntView,1,int,2>::post
        (home,xv,vs,Branch::valselcommitint(home,x.size(),vals),
         array,n,bf,vvp);
    }
  }

  BrancherHandle
  branch(Home home, const IntVarArgs& x,
         TieBreak<IntVarBranch> vars, IntValBranch vals,
         const Symmetries& syms, 
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
      SymmetryImp<IntView>** array =
        static_cast<Space&>(home).alloc<SymmetryImp<IntView>* >(n);
      for (int i = 0 ; i < n ; i++) {
        array[i] = createIntSym(home, syms[i], variableMap);
      }

      ViewArray<IntView> xv(home,x);
      if (vars.c.select() == IntVarBranch::SEL_NONE) {
        ViewSel<IntView>* vs[2] = {
          Branch::viewselint(home,vars.a),Branch::viewselint(home,vars.b)
        };
        switch (vals.select()) {
        case IntValBranch::SEL_SPLIT_MIN:
        case IntValBranch::SEL_SPLIT_MAX:
        case IntValBranch::SEL_RANGE_MIN:
        case IntValBranch::SEL_RANGE_MAX:
        case IntValBranch::SEL_VALUES_MIN:
        case IntValBranch::SEL_VALUES_MAX:
          throw LDSBBadValueSelection("Int::LDSB::branch");
          break;
        case IntValBranch::SEL_VAL_COMMIT:
          if (vals.commit() != NULL)
            throw LDSBBadValueSelection("Int::LDSB::branch");
          // If vals.commit() returns NULL, it means it will commit with
          // binary branching, which is OK for LDSB, so we fall through.
        default:
          return LDSBBrancher<IntView,2,int,2>
            ::post(home,xv,vs,Branch::valselcommitint(home,x.size(),vals),
                   array,n,bf,vvp);
        }
      } else if (vars.d.select() == IntVarBranch::SEL_NONE) {
        ViewSel<IntView>* vs[3] = {
          Branch::viewselint(home,vars.a),Branch::viewselint(home,vars.b),
          Branch::viewselint(home,vars.c)
        };
        switch (vals.select()) {
        case IntValBranch::SEL_SPLIT_MIN:
        case IntValBranch::SEL_SPLIT_MAX:
        case IntValBranch::SEL_RANGE_MIN:
        case IntValBranch::SEL_RANGE_MAX:
        case IntValBranch::SEL_VALUES_MIN:
        case IntValBranch::SEL_VALUES_MAX:
          throw LDSBBadValueSelection("Int::LDSB::branch");
          break;
        case IntValBranch::SEL_VAL_COMMIT:
          if (vals.commit() != NULL)
            throw LDSBBadValueSelection("Int::LDSB::branch");
          // If vals.commit() returns NULL, it means it will commit with
          // binary branching, which is OK for LDSB, so we fall through.
        default:
          return LDSBBrancher<IntView,3,int,2>
            ::post(home,xv,vs,Branch::valselcommitint(home,x.size(),vals),
                   array,n,bf,vvp);
        }
      } else {
        ViewSel<IntView>* vs[4] = {
          Branch::viewselint(home,vars.a),Branch::viewselint(home,vars.b),
          Branch::viewselint(home,vars.c),Branch::viewselint(home,vars.d)
        };
        switch (vals.select()) {
        case IntValBranch::SEL_SPLIT_MIN:
        case IntValBranch::SEL_SPLIT_MAX:
        case IntValBranch::SEL_RANGE_MIN:
        case IntValBranch::SEL_RANGE_MAX:
        case IntValBranch::SEL_VALUES_MIN:
        case IntValBranch::SEL_VALUES_MAX:
          throw LDSBBadValueSelection("Int::LDSB::branch");
          break;
        case IntValBranch::SEL_VAL_COMMIT:
          if (vals.commit() != NULL)
            throw LDSBBadValueSelection("Int::LDSB::branch");
          // If vals.commit() returns NULL, it means it will commit with
          // binary branching, which is OK for LDSB, so we fall through.
        default:
          return LDSBBrancher<IntView,4,int,2>
            ::post(home,xv,vs,Branch::valselcommitint(home,x.size(),vals),
                   array,n,bf,vvp);
        }
      }
    }
    GECODE_NEVER;
    return BrancherHandle();
  }

  BrancherHandle
  branch(Home home, const BoolVarArgs& x,
         IntVarBranch vars, IntValBranch vals,
         const Symmetries& syms, 
         BoolBranchFilter bf, BoolVarValPrint vvp) {
    using namespace Int;
    if (home.failed()) return BrancherHandle();
    vars.expand(home,x);
    ViewArray<BoolView> xv(home,x);
    ViewSel<BoolView>* vs[1] = { 
      Branch::viewselbool(home,vars) 
    };

    // Construct mapping from each variable in the array to its index
    // in the array.
    VariableMap variableMap;
    for (int i = 0 ; i < x.size() ; i++)
      variableMap[x[i].varimp()] = i;

    // Convert the modelling-level Symmetries object into an array of
    // SymmetryImp objects.
    int n = syms.size();
    SymmetryImp<BoolView>** array =
      static_cast<Space&>(home).alloc<SymmetryImp<BoolView>* >(n);
    for (int i = 0 ; i < n ; i++) {
      array[i] = createBoolSym(home, syms[i], variableMap);
    }

    // Technically these "bad" value selection could in fact work with
    // LDSB, because they degenerate to binary splitting for
    // Booleans.  Nonetheless, we explicitly forbid them for
    // consistency with the integer version.
    switch (vals.select()) {
    case IntValBranch::SEL_SPLIT_MIN:
    case IntValBranch::SEL_SPLIT_MAX:
    case IntValBranch::SEL_RANGE_MIN:
    case IntValBranch::SEL_RANGE_MAX:
    case IntValBranch::SEL_VALUES_MIN:
    case IntValBranch::SEL_VALUES_MAX:
      throw LDSBBadValueSelection("Int::LDSB::branch");
      break;
    case IntValBranch::SEL_VAL_COMMIT:
      if (vals.commit() != NULL)
        throw LDSBBadValueSelection("Int::LDSB::branch");
      // If vals.commit() returns NULL, it means it will commit with
      // binary branching, which is OK for LDSB, so we fall through.
    default:
      return LDSBBrancher<BoolView,1,int,2>::post
        (home,xv,vs,Branch::valselcommitbool(home,x.size(),vals),array,n,bf,vvp);
    }
    GECODE_NEVER;
    return BrancherHandle();
  }


  BrancherHandle
  branch(Home home, const BoolVarArgs& x,
         TieBreak<IntVarBranch> vars, IntValBranch vals,
         const Symmetries& syms, 
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
      SymmetryImp<BoolView>** array =
        static_cast<Space&>(home).alloc<SymmetryImp<BoolView>* >(n);
      for (int i = 0 ; i < n ; i++) {
        array[i] = createBoolSym(home, syms[i], variableMap);
      }

      // Technically these "bad" value selection could in fact work with
      // LDSB, because they degenerate to binary splitting for
      // Booleans.  Nonetheless, we explicitly forbid them for
      // consistency with the integer version.
      switch (vals.select()) {
      case IntValBranch::SEL_SPLIT_MIN:
      case IntValBranch::SEL_SPLIT_MAX:
      case IntValBranch::SEL_RANGE_MIN:
      case IntValBranch::SEL_RANGE_MAX:
      case IntValBranch::SEL_VALUES_MIN:
      case IntValBranch::SEL_VALUES_MAX:
        throw LDSBBadValueSelection("Int::LDSB::branch");
        break;
      case IntValBranch::SEL_VAL_COMMIT:
        if (vals.commit() != NULL)
          throw LDSBBadValueSelection("Int::LDSB::branch");
        // If vals.commit() returns NULL, it means it will commit with
        // binary branching, which is OK for LDSB, so we fall through.
      default:
        ;
        // Do nothing and continue.
      }

      ViewArray<BoolView> xv(home,x);
      ValSelCommitBase<BoolView,int>* 
        vsc = Branch::valselcommitbool(home,x.size(),vals); 
      if (vars.c.select() == IntVarBranch::SEL_NONE) {
        ViewSel<BoolView>* vs[2] = { 
          Branch::viewselbool(home,vars.a),Branch::viewselbool(home,vars.b)
        };
        return 
          LDSBBrancher<BoolView,2,int,2>::post(home,xv,vs,vsc,array,n,bf,vvp);
      } else if (vars.d.select() == IntVarBranch::SEL_NONE) {
        ViewSel<BoolView>* vs[3] = { 
          Branch::viewselbool(home,vars.a),Branch::viewselbool(home,vars.b),
          Branch::viewselbool(home,vars.c)
        };
        return 
          LDSBBrancher<BoolView,3,int,2>::post(home,xv,vs,vsc,array,n,bf,vvp);
      } else {
        ViewSel<BoolView>* vs[4] = { 
          Branch::viewselbool(home,vars.a),Branch::viewselbool(home,vars.b),
          Branch::viewselbool(home,vars.c),Branch::viewselbool(home,vars.d)
        };
        return 
          LDSBBrancher<BoolView,4,int,2>::post(home,xv,vs,vsc,array,n,bf,vvp);
      }
    }
    GECODE_NEVER;
    return BrancherHandle();
  }

}



// STATISTICS: int-branch
