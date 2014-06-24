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

namespace Gecode { namespace Set { namespace LDSB {

  template<class View, int n, class Val, unsigned int a>
  LDSBSetBrancher<View,n,Val,a>
  ::LDSBSetBrancher(Home home, ViewArray<View>& x,
                    ViewSel<View>* vs[n],
                    ValSelCommitBase<View,Val>* vsc,
                    SymmetryImp<View>** syms, int nsyms,
                    SetBranchFilter bf,
                    VarValPrint vvp)
    : LDSBBrancher<View,n,Val,a>(home, x, vs, vsc, syms, nsyms, bf, vvp),
      _prevPos(-1),
      _copiedSyms(NULL),
      _nCopiedSyms(0),
      _stable(false) {
    // Put all the value symmetries at the end of the list.
    int seen = 0;
    int dest = this->_nsyms - 1;
    for (int i = 0 ; i < this->_nsyms - seen ; i++) {
      if (dynamic_cast<ValueSymmetryImp<View>*>(this->_syms[i])) {
        SymmetryImp<View>* t = this->_syms[i];
        this->_syms[i] = this->_syms[dest];
        this->_syms[dest] = t;
        dest--;
        seen++;
      }
    }
    _nValueSymmetries = seen;
    _nNonValueSymmetries = this->_nsyms - seen;
  }

  template<class View, int n, class Val, unsigned int a>
  LDSBSetBrancher<View,n,Val,a>::
  LDSBSetBrancher(Space& home, bool shared, LDSBSetBrancher<View,n,Val,a>& b)
    : LDSBBrancher<View,n,Val,a>(home,shared,b),
      _prevPos(b._prevPos),
      _nNonValueSymmetries(b._nNonValueSymmetries),
      _nValueSymmetries(b._nValueSymmetries),
      _nCopiedSyms(b._nCopiedSyms),
      _leftBranchValues(b._leftBranchValues),
      _stable(b._stable) {
    if (_nCopiedSyms > 0) {
      _copiedSyms = home.alloc<ValueSymmetryImp<View>*>(_nCopiedSyms);
      for (int i = 0 ; i < _nCopiedSyms ; i++)
        _copiedSyms[i] = static_cast<ValueSymmetryImp<View>*>(
          b._copiedSyms[i]->copy(home, shared));
    } else {
      _copiedSyms = NULL;
    }
  }

  /**
   * \brief Bulk update of a value symmetry \a s, using \a usedValues
   *
   * Calculates the intersection and difference of the values in the
   * symmetry and \a usedValues, updates the symmetry to eliminate the
   * used values, and makes a new symmetry containing the intersection
   * values, if there are at least two.  Returns the new symmetry, or
   * NULL if the intersection has fewer than two elements.
   */ 
  template <class View>
  ValueSymmetryImp<View>*
  specialUpdate(Space& home, ValueSymmetryImp<View>* s, IntSet usedValues) {
    // Calculate intersection and difference.
    IntArgs intersection;
    IntArgs difference;
    int n = 0;
    for (int i = s->values.next(s->values.offset()) ;
         i <= s->values.max_bit() ; i = s->values.next(i+1)) {
      n++;
      if (usedValues.in(i))
        intersection << i;
      else
        difference << i;
    }

    for (IntSetValues v(usedValues) ; v() ; ++v) {
      s->update(Literal(0, v.val()));
    }
    
    if (intersection.size() < 2)
      return NULL;
    int *a = new int[intersection.size()];
    for (int i = 0 ; i < intersection.size() ; i++) {
      a[i] = intersection[i];
    }
    ValueSymmetryImp<View>* ns =
      new (home) ValueSymmetryImp<View>(home, a, intersection.size());
    delete [] a;
    return ns;
  }

  template<class View, int n, class Val, unsigned int a>
  void
  LDSBSetBrancher<View,n,Val,a>::
  updatePart1(Space& home, int choicePos) {
    if (_nValueSymmetries > 0) {
      // If this is a different variable from the last commit, restore
      // the old value symmetries and update the copy.
      if (choicePos != _prevPos) {
        if (_prevPos != -1) {
          int i = 0;
          for (int j = _nNonValueSymmetries ; j < this->_nsyms ; j++) {
            this->_syms[j] = _copiedSyms[i];
            i++;
          }

          for (int i = 0 ; i < _nCopiedSyms ; i++) {
            ValueSymmetryImp<View>* ns =
              specialUpdate(home, _copiedSyms[i], _leftBranchValues);
            if (ns) {
              this->_syms = home.realloc<SymmetryImp<View>*>(this->_syms, 
                                                             this->_nsyms, 
                                                             this->_nsyms+1);
              this->_syms[this->_nsyms] = ns;
              this->_nsyms++;
              this->_nValueSymmetries++;
            }              
          }
        }
        
        // Reset for current variable, make copy of value symmetries
        _leftBranchValues = IntSet::empty;
        _prevPos = choicePos;
        if (_nCopiedSyms > 0) home.free(_copiedSyms, _nCopiedSyms);
        _nCopiedSyms = _nValueSymmetries;
        _copiedSyms = home.alloc<ValueSymmetryImp<View>*>(_nCopiedSyms);
        int i = 0;
        for (int j = _nNonValueSymmetries ; j < this->_nsyms ; j++) {
          ValueSymmetryImp<View>* vsi = 
            static_cast<ValueSymmetryImp<View>*>(this->_syms[j]);
          _copiedSyms[i] = 
            static_cast<ValueSymmetryImp<View>*>(vsi->copy(home, false));
          i++;
        }
      }
    }
  }

  // Compute choice
  template<class View, int n, class Val, unsigned int a>
  const Choice*
  LDSBSetBrancher<View,n,Val,a>::choice(Space& home) {
    // Making the PVC here is not so nice, I think.
    const Choice* c = ViewValBrancher<View,n,Val,a>::choice(home);
    const PosValChoice<Val>* pvc = static_cast<const PosValChoice<Val>* >(c);
    
    // Compute symmetries.

    int choicePos = pvc->pos().pos;
    delete c;

    assert(!_stable);
    updatePart1(home, choicePos);

    return LDSBBrancher<View,n,Val,a>::choice(home);
  }

  template<class View, int n, class Val, unsigned int a>
  ExecStatus
  LDSBSetBrancher<View,n,Val,a>
  ::commit(Space& home, const Choice& c, unsigned int b) {
    const LDSBChoice<Val>& pvc
      = static_cast<const LDSBChoice<Val>&>(c);
    int choicePos = pvc.pos().pos;
    int choiceVal = pvc.val();

    if (!_stable)
      updatePart1(home, choicePos);
    
    if (b == 0) {
      IntArgs ia;
      for (IntSetValues v(_leftBranchValues) ; v() ; ++v) {
        ia << v.val();
      }
      ia << choiceVal;
      _leftBranchValues = IntSet(ia);
        
      // Post the branching constraint.
      ExecStatus fromBase = ViewValBrancher<View,n,Val,a>::commit(home, c, b);
      GECODE_ES_CHECK(fromBase);
      for (int i = 0 ; i < this->_nsyms ; i++)
        this->_syms[i]->update(Literal(choicePos, choiceVal));
    } else if (b == 1) {
      // Post the branching constraint.
      ExecStatus fromBase = ViewValBrancher<View,n,Val,a>::commit(home, c, b);
      GECODE_ES_CHECK(fromBase);

      // Post prunings.
      int nliterals = pvc.nliterals();
      const Literal* literals = pvc.literals();
      for (int i = 0 ; i < nliterals ; i++) {
        const Literal& l = literals[i];
        ModEvent me = prune<View>(home, this->x[l._variable], l._value);
        GECODE_ME_CHECK(me);
      }
    }
    
    return ES_OK;
  }
    
  template<class View, int n, class Val, unsigned int a>
  Actor*
  LDSBSetBrancher<View,n,Val,a>::copy(Space& home, bool shared) {
    return new (home) LDSBSetBrancher<View,n,Val,a>(home,shared,*this);
  }

  template<class View, int n, class Val, unsigned int a>
  forceinline BrancherHandle
  LDSBSetBrancher<View,n,Val,a>::
  post(Home home, ViewArray<View>& x,
       ViewSel<View>* vs[n], ValSelCommitBase<View,Val>* vsc,
       SymmetryImp<View>** syms, int nsyms,
       SetBranchFilter bf,
       VarValPrint vvp) {
    return *new (home) LDSBSetBrancher<View,n,Val,a>(home,x,vs,vsc,syms,nsyms,bf,vvp);
  }

}}}

// STATISTICS: set-branch
