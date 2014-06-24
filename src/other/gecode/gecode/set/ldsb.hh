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

#ifndef __GECODE_SET_LDSB_HH__
#define __GECODE_SET_LDSB_HH__

#include <gecode/set.hh>
#include <gecode/int/ldsb.hh>

/**
 * \namespace Gecode::Set::LDSB
 * \brief Symmetry breaking for set variables
 */
namespace Gecode { namespace Set { namespace LDSB {

  using namespace Int::LDSB;
  
  /**
   * \brief Symmetry-breaking brancher with generic view and value
   * selection
   *
   * Implements view-based branching for an array of views (of type
   * \a View) on set variables and value (of type \a Val).
   *
   */
  template<class View, int n, class Val, unsigned int a>
  class LDSBSetBrancher : public LDSBBrancher<View,n,Val,a> {
  public:
    /// Function type for printing variable and value selection
    typedef void (*VarValPrint)(const Space& home, const BrancherHandle& bh,
                                unsigned int b,
                                typename View::VarType x, int i,
                                const Val& m,
                                std::ostream& o);
    /// Position of previous variable that was branched on
    int _prevPos;
    /// Number of non-value symmetries
    int _nNonValueSymmetries;
    /// Number of value symmetries
    int _nValueSymmetries;
    /// Copy of value symmetries from the first node where the current
    /// variable was branched on.
    ValueSymmetryImp<View>** _copiedSyms;
    /// Number of copied symmetries
    int _nCopiedSyms;
    /// Set of values used on left branches for the current variable
    IntSet _leftBranchValues;
    /**
     * \brief Is the state of the brancher "stable"?
     * 
     * The brancher is unstable if we are about to run either "choice"
     * or "commit", but "updatePart1" has not been run.  After
     * "updatePart1" has been run the brancher is stable, until the
     * second part of the update is done (in commit).
     */
    bool _stable;
    
    /// Constructor for cloning \a b
    LDSBSetBrancher(Space& home, bool share, LDSBSetBrancher& b);
    /// Constructor for creation
    LDSBSetBrancher(Home home, 
                    ViewArray<View>& x,
                    ViewSel<View>* vs[n], 
                    ValSelCommitBase<View,Val>* vsc,
                    SymmetryImp<View>** syms, int nsyms,
                    SetBranchFilter bf,
                    VarValPrint vvp);
    /// Return choice
    virtual const Choice* choice(Space& home);
    /// Perform commit for choice \a c and alternative \a b
    virtual ExecStatus commit(Space& home, const Choice& c, unsigned int b);
    /// Perform cloning
    virtual Actor* copy(Space& home, bool share);
    /// Brancher post function
    static BrancherHandle post(Home home, 
                               ViewArray<View>& x,
                               ViewSel<View>* vs[n],
                               ValSelCommitBase<View,Val>* vsc,
                               SymmetryImp<View>** _syms,
                               int _nsyms,
                               SetBranchFilter bf,
                               VarValPrint vvp);

    /**
     * \brief Part one of the update phase
     *
     * If the branching is at a new variable, restore previously
     * copied value symmetries, do bulk update of values used on left
     * branches for the previous variable, and make fresh copy of
     * resulting value symmetries.
     */
    void updatePart1(Space& home, int choicePos);
  };

}}}

namespace Gecode { namespace Int { namespace LDSB {
  template <>
  ArgArray<Literal>
  VariableSequenceSymmetryImp<Set::SetView>
  ::symmetric(Literal l, const ViewArray<Set::SetView>& x) const;
}}}

#include <gecode/set/ldsb/brancher.hpp>

#endif

// STATISTICS: set-branch
