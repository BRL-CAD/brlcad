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

#ifndef __GECODE_INT_LDSB_HH__
#define __GECODE_INT_LDSB_HH__

#include <gecode/int.hh>

/**
 * \namespace Gecode::Int::LDSB
 * \brief Symmetry breaking for integer variables
 */
namespace Gecode { namespace Int { namespace LDSB {

  /// A Literal is a pair of variable index and value.
  class Literal {
  public:
    /// Constructor for an empty literal
    Literal(void);
    /// Constructor
    Literal(int _var, int _val);

    /// Variable index.  The ViewArray that the index is meant
    /// for is assumed to be known by context.
    int _variable;
    /// The value of the literal.  For int and bool variables,
    /// this is the value itself; for set variables, this is
    /// one of the possible elements of the set.
    int _value;

    /// Less than.  The ordering is the lexicographical order
    /// on the (variable,value) pair.
    bool operator <(const Literal &rhs) const;
  };

  /**
   * \brief Find the location of an integer in a collection of
   * sequences.
   *
   * 
   * Given an array \a indices of integers (of length \a n_values),
   * which represents a collection of sequences each of size \a
   * seq_size, find the location of the first occurrence of the value
   * \a index.  Returns a pair of sequence number and position within
   * that sequence (both zero-based).
   */
  GECODE_INT_EXPORT
  std::pair<int,int>
  findVar(int *indices, unsigned int n_values, unsigned int seq_size, int index);
}}}

namespace Gecode {
  /// Traits of %ArgArray<VarImpBase*>
  template<>
  class ArrayTraits<ArgArray<VarImpBase*> > {
  public:
    typedef ArgArray<VarImpBase*>     StorageType;
    typedef VarImpBase*               ValueType;
    typedef ArgArray<VarImpBase*>     ArgsType;
  };

  /// An array of literals
  typedef ArgArray<Int::LDSB::Literal> LiteralArgs;
  /// Traits of %LiteralArgs
  template<>
  class ArrayTraits<LiteralArgs> {
  public:
    typedef LiteralArgs        StorageType;
    typedef Int::LDSB::Literal ValueType;
    typedef LiteralArgs        ArgsType;
  };
}

namespace Gecode { namespace Int { namespace LDSB {
  /// Implementation of a symmetry at the modelling level
  class GECODE_INT_EXPORT SymmetryObject {
  public:
    /// Number of references that point to this symmetry object.
    int nrefs;
    /// Default constructor
    SymmetryObject(void);
    /// Destructor
    virtual ~SymmetryObject(void);
  };
  /// Implementation of a variable symmetry at the modelling level
  class GECODE_INT_EXPORT VariableSymmetryObject : public SymmetryObject {
  public:
    /// Array of variables in symmetry
    VarImpBase** xs;
    /// Number of variables in symmetry
    int nxs;
    /// Constructor for creation
    VariableSymmetryObject(ArgArray<VarImpBase*> vars);
    /// Destructor
    ~VariableSymmetryObject(void);
  };
  /// Implementation of a value symmetry at the modelling level
  class GECODE_INT_EXPORT ValueSymmetryObject : public SymmetryObject {
  public:
    /// Set of symmetric values
    IntSet values;
    /// Constructor for creation
    ValueSymmetryObject(IntSet vs);
  };
  /// Implementation of a variable sequence symmetry at the modelling level
  class GECODE_INT_EXPORT VariableSequenceSymmetryObject : public SymmetryObject {
  public:
    /// Array of variables in symmetry
    VarImpBase** xs;
    /// Number of variables in symmetry
    int nxs;
    /// Size of each sequence in symmetry
    int seq_size;
    /// Constructor for creation
    VariableSequenceSymmetryObject(ArgArray<VarImpBase*> vars, int ss);
    /// Destructor
    ~VariableSequenceSymmetryObject(); 
  };
  /// Implementation of a value sequence symmetry at the modelling level
  class GECODE_INT_EXPORT ValueSequenceSymmetryObject : public SymmetryObject {
  public:
    /// Array of values in symmetry
    IntArgs values;
    /// Size of each sequence in symmetry
    int seq_size;
    /// Constructor for creation
    ValueSequenceSymmetryObject(IntArgs vs, int ss);
  };

  /// Implementation of a single symmetry.
  template<class View>
  class SymmetryImp {
  public:
    /// Compute symmetric literals
    virtual ArgArray<Literal> symmetric(Literal, const ViewArray<View>&) const = 0;
    /// Left-branch update
    virtual void update(Literal) = 0;
    /// Copy function
    virtual SymmetryImp<View>* copy(Space& home, bool share) const = 0;
    /// Disposal
    virtual size_t dispose(Space& home) = 0;
    /// Placement new operator
    static void* operator new(size_t s, Space& home);
    /// Return memory to space
    static void  operator delete(void*,Space&);
    /// Needed for exceptions
    static void  operator delete(void*);
  };
  /// Implementation of a variable symmetry.
  template <class View>
  class VariableSymmetryImp : public SymmetryImp<View> {
  protected:
    /// Symmetric variable indices
    Support::BitSetOffset<Space> indices;
  public:
    /// Constructor for creation
    VariableSymmetryImp<View>(Space& home, int* vs, unsigned int n);
    /// Copy constructor
    VariableSymmetryImp<View>(Space& home, const VariableSymmetryImp<View>& other);
    /// Disposal
    virtual size_t dispose(Space& home);
    /// Left-branch update
    void update(Literal);
    /// Compute symmetric literals
    virtual ArgArray<Literal> symmetric(Literal, const ViewArray<View>&) const;
    /// Copy function
    SymmetryImp<View>* copy(Space& home, bool share) const;
  };
  /// Implementation of a value symmetry.
  template <class View>
  class ValueSymmetryImp : public SymmetryImp<View>
  {
  public:
    /// Symmetric values
    Support::BitSetOffset<Space> values;
    /// Constructor for creation
    ValueSymmetryImp<View>(Space& home, int* vs, unsigned int n);
    /// Copy constructor
    ValueSymmetryImp<View>(Space& home, const ValueSymmetryImp<View>& other);
    /// Disposal
    virtual size_t dispose(Space& home);
    /// Left-branch update
    void update(Literal);
    /// Compute symmetric literals
    virtual ArgArray<Literal> symmetric(Literal, const ViewArray<View>&) const;
    /// Copy function
    SymmetryImp<View>* copy(Space& home, bool share) const;
  };
  /// Implementation of a variable sequence symmetry.
  template <class View>
  class VariableSequenceSymmetryImp : public SymmetryImp<View>
  {
  protected:
    /// Array of variable indices
    unsigned int *indices;
    /// Total number of indices (n_seqs * seq_size)
    unsigned int n_indices;
    /// Size of each sequence in symmetry
    unsigned int seq_size;
    /// Number of sequences in symmetry
    unsigned int n_seqs;

    /// Map from variable's index to its sequence and position.
    // e.g. lookup[2] == 10 indicates that the variable with index 2
    // occurs at position 10 in the "indices" array.
    // If a variable occurs more than once, only the first occurrence
    // is recorded.
    // A value of -1 indicates that the variable does not occur in
    // "indices".
    int *lookup;
    /// Size of lookup
    unsigned int lookup_size;

    /// Get the value in the specified sequence at the specified
    /// position.  (Both are zero-based.)
    int getVal(unsigned int sequence, unsigned int position) const;
  public:
    /// Constructor for creation
    VariableSequenceSymmetryImp<View>(Space& home, int *_indices, unsigned int n, unsigned int seqsize);
    /// Copy constructor
    VariableSequenceSymmetryImp<View>(Space& home, bool share, const VariableSequenceSymmetryImp<View>& s);
    /// Disposal
    virtual size_t dispose(Space& home);
    /// Search left-branch update
    void update(Literal);
    /// Compute symmetric literals
    virtual ArgArray<Literal> symmetric(Literal, const ViewArray<View>&) const;
    /// Copy function
    SymmetryImp<View>* copy(Space& home, bool share) const;
  };
  /// Implementation of a value sequence symmetry.
  template <class View>
  class ValueSequenceSymmetryImp : public SymmetryImp<View>
  {
  protected:
    /// Set of sequences
    int *values;
    /// Total number of values (n_seqs * seq_size)
    unsigned int n_values;
    /// Size of each sequence in symmetry
    unsigned int seq_size;
    /// Number of sequences in symmetry
    unsigned int n_seqs;
    /// Which sequences are dead
    Support::BitSet<Space> dead_sequences;
    /// Get the value in the specified sequence at the specified
    /// position.  (Both are zero-based.)
    int getVal(unsigned int sequence, unsigned int position) const;
  private:
    ValueSequenceSymmetryImp<View>(const ValueSequenceSymmetryImp<View>&);
  public:
    /// Constructor for creation
    ValueSequenceSymmetryImp<View>(Space& home, int* _values, unsigned int n, unsigned int seqsize);
    /// Copy constructor
    ValueSequenceSymmetryImp<View>(Space& home, const ValueSequenceSymmetryImp<View>& vss);
    /// Disposal
    virtual size_t dispose(Space& home);
    /// Left-branch update
    void update(Literal);
    /// Compute symmetric literals
    virtual ArgArray<Literal> symmetric(Literal, const ViewArray<View>&) const;
    /// Copy function
    SymmetryImp<View>* copy(Space& home, bool share) const;
  };

  /// %Choice storing position and value, and symmetric literals to be
  ///  excluded on the right branch.
  template<class Val>
  class GECODE_VTABLE_EXPORT LDSBChoice : public PosValChoice<Val> {
  private:
    /// Set of literals to be excluded
    const Literal * const _literals;
    /// Number of literals
    const int _nliterals;
  public:
    /// Initialize choice for brancher \a b, position \a p, value \a
    /// n, and set of literals \a literals (of size \a nliterals)
    LDSBChoice(const Brancher& b, unsigned int a, const Pos& p, const Val& n, 
               const Literal* literals, int nliterals);
    /// Destructor
    ~LDSBChoice(void);
    /// Return literals
    const Literal* literals(void) const;
    /// Return number of literals
    int nliterals(void) const;
    /// Report size occupied
    virtual size_t size(void) const;
    /// Archive into \a e
    virtual void archive(Archive& e) const;
  };

  /**
   * \brief Symmetry-breaking brancher with generic view and value
   * selection
   *
   * Implements view-based branching for an array of views (of type
   * \a View) and value (of type \a Val).
   *
   */
  template<class View, int n, class Val, unsigned int a>
  class LDSBBrancher : public ViewValBrancher<View,n,Val,a> {
    typedef typename ViewBrancher<View,n>::BranchFilter BranchFilter;
  public:
    /// Array of symmetry implementations
    SymmetryImp<View>** _syms;
    /// Number of symmetry implementations
    int _nsyms;
    // Position of variable that last choice was created for
    int _prevPos;
  protected:
    /// Function type for printing variable and value selection
    typedef void (*VarValPrint)(const Space& home, const BrancherHandle& bh,
                                unsigned int b,
                                typename View::VarType x, int i,
                                const Val& m,
                                std::ostream& o);
    /// Constructor for cloning \a b
    LDSBBrancher(Space& home, bool share, LDSBBrancher& b);
    /// Constructor for creation
    LDSBBrancher(Home home, 
                 ViewArray<View>& x,
                 ViewSel<View>* vs[n], 
                 ValSelCommitBase<View,Val>* vsc,
                 SymmetryImp<View>** syms, int nsyms,
                 BranchFilter bf,
                 VarValPrint vvp);
  public:
    /// Return choice
    virtual const Choice* choice(Space& home);
    /// Return choice
    virtual const Choice* choice(const Space& home, Archive& e);
    /// Perform commit for choice \a c and alternative \a b
    virtual ExecStatus commit(Space& home, const Choice& c, unsigned int b);
    /// Perform cloning
    virtual Actor* copy(Space& home, bool share);
    /// Delete brancher and return its size
    virtual size_t dispose(Space& home);
    /// Brancher post function
    static BrancherHandle post(Home home,
                               ViewArray<View>& x,
                               ViewSel<View>* vs[n],
                               ValSelCommitBase<View,Val>* vsc,
                               SymmetryImp<View>** syms,
                               int nsyms,
                               BranchFilter bf,
                               VarValPrint vvp);
  };

  /// Exclude value \v from variable view \x
  template<class View>
  ModEvent prune(Space& home, View x, int v);

}}}

#include <gecode/int/ldsb/brancher.hpp>
#include <gecode/int/ldsb/sym-imp.hpp>

#endif

// STATISTICS: int-branch

