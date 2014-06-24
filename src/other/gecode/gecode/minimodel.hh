/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2004
 *     Guido Tack, 2004
 *     Mikael Lagerkvist, 2005
 *     Vincent Barichard, 2012
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

#ifndef __GECODE_MINIMODEL_HH__
#define __GECODE_MINIMODEL_HH__

#include <gecode/kernel.hh>
#include <gecode/int.hh>
#ifdef GECODE_HAS_SET_VARS
#include <gecode/set.hh>
#endif
#ifdef GECODE_HAS_FLOAT_VARS
#include <gecode/float.hh>
#endif

#include <gecode/minimodel/exception.hpp>

#include <iostream>

/*
 * Support for DLLs under Windows
 *
 */

#if !defined(GECODE_STATIC_LIBS) && \
    (defined(__CYGWIN__) || defined(__MINGW32__) || defined(_MSC_VER))

#ifdef GECODE_BUILD_MINIMODEL
#define GECODE_MINIMODEL_EXPORT __declspec( dllexport )
#else
#define GECODE_MINIMODEL_EXPORT __declspec( dllimport )
#endif

#else

#ifdef GECODE_GCC_HAS_CLASS_VISIBILITY

#define GECODE_MINIMODEL_EXPORT __attribute__ ((visibility("default")))

#else

#define GECODE_MINIMODEL_EXPORT

#endif
#endif

// Configure auto-linking
#ifndef GECODE_BUILD_MINIMODEL
#define GECODE_LIBRARY_NAME "MiniModel"
#include <gecode/support/auto-link.hpp>
#endif

namespace Gecode {

  /// Minimalistic modeling support
  namespace MiniModel {}

  class LinIntRel;
#ifdef GECODE_HAS_SET_VARS
  class SetExpr;
#endif
#ifdef GECODE_HAS_FLOAT_VARS
  class LinFloatExpr;
#endif

  /// Base class for non-linear expressions over integer variables
  class NonLinIntExpr {
  public:
    /// Return variable constrained to be equal to the expression
    virtual IntVar post(Home home, IntVar* ret, IntConLevel icl) const = 0;
    /// Post expression to be in relation \a irt with \a c
    virtual void post(Home home, IntRelType irt, int c,
                      IntConLevel icl) const = 0;
    /// Post reified expression to be in relation \a irt with \a c
    virtual void post(Home home, IntRelType irt, int c,
                      BoolVar b, IntConLevel icl) const = 0;
    /// Destructor
    virtual ~NonLinIntExpr(void) {}
    /// Return fresh variable if \a x is NULL, \a x otherwise
    static IntVar result(Home home, IntVar* x) {
      if (x==NULL)
        return IntVar(home,Int::Limits::min,Int::Limits::max);
      return *x;
    }
    /// Constrain \a x to be equal to \a y if \a x is not NULL
    static IntVar result(Home home, IntVar* x, IntVar y) {
      if (x!=NULL)
        rel(home,*x,IRT_EQ,y);
      return y;
    }
    /// Memory management
    void* operator new(size_t size) { return heap.ralloc(size); }
    /// Memory management
    void operator delete(void* p, size_t) { heap.rfree(p); }
  };

  /// Linear expressions over integer variables
  class LinIntExpr {
    friend class LinIntRel;
#ifdef GECODE_HAS_SET_VARS
    friend class SetExpr;
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    friend class LinFloatExpr;
#endif
  public:
    /// Type of linear expression
    enum NodeType {
      NT_CONST,    ///< Integer constant
      NT_VAR_INT,  ///< Linear term with integer variable
      NT_VAR_BOOL, ///< Linear term with Boolean variable
      NT_NONLIN,   ///< Non-linear expression
      NT_SUM_INT,  ///< Sum of integer variables
      NT_SUM_BOOL, ///< Sum of Boolean variables
      NT_ADD,      ///< Addition of linear terms
      NT_SUB,      ///< Subtraction of linear terms
      NT_MUL       ///< Multiplication by coefficient
    };
  private:
    /// Nodes for linear expressions
    class Node;
    Node* n;
  public:
    /// Default constructor
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(void);
    /// Create expression for constant \a c
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(int c);
    /// Create expression
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(const IntVar& x, int a=1);
    /// Create expression
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(const BoolVar& x, int a=1);
    /// Create sum expression
    GECODE_MINIMODEL_EXPORT
    explicit LinIntExpr(const IntVarArgs& x);
    /// Create sum expression
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(const IntArgs& a, const IntVarArgs& x);
    /// Create sum expression
    GECODE_MINIMODEL_EXPORT
    explicit LinIntExpr(const BoolVarArgs& x);
    /// Create sum expression
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(const IntArgs& a, const BoolVarArgs& x);
    /// Copy constructor
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(const LinIntExpr& e);
    /// Create expression for type and subexpressions
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(const LinIntExpr& e0, NodeType t, const LinIntExpr& e1);
    /// Create expression for type and subexpression
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(const LinIntExpr& e0, NodeType t, int c);
    /// Create expression for multiplication
    GECODE_MINIMODEL_EXPORT
    LinIntExpr(int a, const LinIntExpr& e);
    /// Create non-linear expression
    GECODE_MINIMODEL_EXPORT
    explicit LinIntExpr(NonLinIntExpr* e);
    /// Assignment operator
    GECODE_MINIMODEL_EXPORT
    const LinIntExpr& operator =(const LinIntExpr& e);
    /// Post propagator
    GECODE_MINIMODEL_EXPORT
    void post(Home home, IntRelType irt, IntConLevel icl) const;
    /// Post reified propagator
    GECODE_MINIMODEL_EXPORT
    void post(Home home, IntRelType irt, const BoolVar& b,
              IntConLevel icl) const;
    /// Post propagator and return variable for value
    GECODE_MINIMODEL_EXPORT
    IntVar post(Home home, IntConLevel icl) const;
    /// Return non-linear expression inside, or NULL if not non-linear
    GECODE_MINIMODEL_EXPORT
    NonLinIntExpr* nle(void) const;
    /// Destructor
    GECODE_MINIMODEL_EXPORT
    ~LinIntExpr(void);
  };

  class BoolExpr;

  /// Linear relations over integer variables
  class LinIntRel {
    friend class BoolExpr;
  private:
    /// Linear expression describing the entire relation
    LinIntExpr e;
    /// Which relation
    IntRelType irt;
    /// Negate relation type
    static IntRelType neg(IntRelType irt);
    /// Default constructor
    LinIntRel(void);
  public:
    /// Create linear relation for expressions \a l and \a r
    LinIntRel(const LinIntExpr& l, IntRelType irt, const LinIntExpr& r);
    /// Create linear relation for expression \a l and integer \a r
    LinIntRel(const LinIntExpr& l, IntRelType irt, int r);
    /// Create linear relation for integer \a l and expression \a r
    LinIntRel(int l, IntRelType irt, const LinIntExpr& r);
    /// Post propagator for relation (if \a t is false for negated relation)
    void post(Home home, bool t,  IntConLevel icl) const;
    /// Post reified propagator for relation (if \a t is false for negated relation)
    void post(Home home, const BoolVar& b, bool t, IntConLevel icl) const;
  };

  /**
   * \defgroup TaskModelMiniModelLin Linear expressions and relations
   *
   * Linear expressions can be freely composed of sums and differences of
   * integer variables (Gecode::IntVar) or Boolean variables
   * (Gecode::BoolVar) possibly with integer coefficients and integer
   * constants.
   *
   * Note that both integer and Boolean variables are automatically
   * available as linear expressions.
   *
   * Linear relations are obtained from linear expressions with the normal
   * relation operators.
   *
   * \ingroup TaskModelMiniModel
   */

  //@{
  /// Construct linear expression as sum of integer variable and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(int, const IntVar&);
  /// Construct linear expression as sum of Boolean variable and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(int, const BoolVar&);
  /// Construct linear expression as sum of linear expression and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(int, const LinIntExpr&);
  /// Construct linear expression as sum of integer variable and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const IntVar&, int);
  /// Construct linear expression as sum of Boolean variable and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const BoolVar&, int);
  /// Construct linear expression as sum of linear expression and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const LinIntExpr&, int);
  /// Construct linear expression as sum of integer variables
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const IntVar&, const IntVar&);
  /// Construct linear expression as sum of integer and Boolean variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const IntVar&, const BoolVar&);
  /// Construct linear expression as sum of Boolean and integer variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const BoolVar&, const IntVar&);
  /// Construct linear expression as sum of Boolean variables
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const BoolVar&, const BoolVar&);
  /// Construct linear expression as sum of integer variable and linear expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const IntVar&, const LinIntExpr&);
  /// Construct linear expression as sum of Boolean variable and linear expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const BoolVar&, const LinIntExpr&);
  /// Construct linear expression as sum of linear expression and integer variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const LinIntExpr&, const IntVar&);
  /// Construct linear expression as sum of linear expression and Boolean variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const LinIntExpr&, const BoolVar&);
  /// Construct linear expression as sum of linear expressions
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator +(const LinIntExpr&, const LinIntExpr&);

  /// Construct linear expression as sum of integer variable and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(int, const IntVar&);
  /// Construct linear expression as sum of Boolean variable and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(int, const BoolVar&);
  /// Construct linear expression as sum of integer and linear expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(int, const LinIntExpr&);
  /// Construct linear expression as sum of integer variable and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const IntVar&, int);
  /// Construct linear expression as sum of Boolean variable and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const BoolVar&, int);
  /// Construct linear expression as sum of linear expression and integer
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const LinIntExpr&, int);
  /// Construct linear expression as sum of integer variables
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const IntVar&, const IntVar&);
  /// Construct linear expression as sum of integer and Boolean variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const IntVar&, const BoolVar&);
  /// Construct linear expression as sum of Boolean and integer variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const BoolVar&, const IntVar&);
  /// Construct linear expression as sum of Boolean variables
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const BoolVar&, const BoolVar&);
  /// Construct linear expression as sum of integer variable and linear expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const IntVar&, const LinIntExpr&);
  /// Construct linear expression as sum of Boolean variable and linear expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const BoolVar&, const LinIntExpr&);
  /// Construct linear expression as sum of linear expression and integer variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const LinIntExpr&, const IntVar&);
  /// Construct linear expression as sum of linear expression and Boolean variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const LinIntExpr&, const BoolVar&);
  /// Construct linear expression as sum of linear expressions
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const LinIntExpr&, const LinIntExpr&);

  /// Construct linear expression as negative of integer variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const IntVar&);
  /// Construct linear expression as negative of Boolean variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const BoolVar&);
  /// Construct linear expression as negative of linear expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator -(const LinIntExpr&);

  /// Construct linear expression as product of integer coefficient and integer variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator *(int, const IntVar&);
  /// Construct linear expression as product of integer coefficient and Boolean variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator *(int, const BoolVar&);
  /// Construct linear expression as product of integer coefficient and integer variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator *(const IntVar&, int);
  /// Construct linear expression as product of integer coefficient and Boolean variable
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator *(const BoolVar&, int);
  /// Construct linear expression as product of integer coefficient and linear expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator *(const LinIntExpr&, int);
  /// Construct linear expression as product of integer coefficient and linear expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator *(int, const LinIntExpr&);

  /// Construct linear expression as sum of integer variables
  GECODE_MINIMODEL_EXPORT LinIntExpr
  sum(const IntVarArgs& x);
  /// Construct linear expression as sum of integer variables with coefficients
  GECODE_MINIMODEL_EXPORT LinIntExpr
  sum(const IntArgs& a, const IntVarArgs& x);
  /// Construct linear expression as sum of Boolean variables
  GECODE_MINIMODEL_EXPORT LinIntExpr
  sum(const BoolVarArgs& x);
  /// Construct linear expression as sum of Boolean variables with coefficients
  GECODE_MINIMODEL_EXPORT LinIntExpr
  sum(const IntArgs& a, const BoolVarArgs& x);
  /// Construct linear expression as sum of \ref IntArgs
  GECODE_MINIMODEL_EXPORT LinIntExpr
  sum(const IntArgs& args);

  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(int l, const IntVar& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(int l, const BoolVar& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(int l, const LinIntExpr& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const IntVar& l, int r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const BoolVar& l, int r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const LinIntExpr& l, int r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const IntVar& l, const IntVar& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const IntVar& l, const BoolVar& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const BoolVar& l, const IntVar& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const BoolVar& l, const BoolVar& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const IntVar& l, const LinIntExpr& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const BoolVar& l, const LinIntExpr& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const LinIntExpr& l, const IntVar& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const LinIntExpr& l, const BoolVar& r);
  /// Construct linear equality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator ==(const LinIntExpr& l, const LinIntExpr& r);

  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(int l, const IntVar& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(int l, const BoolVar& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(int l, const LinIntExpr& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const IntVar& l, int r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const BoolVar& l, int r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const LinIntExpr& l, int r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const IntVar& l, const IntVar& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const IntVar& l, const BoolVar& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const BoolVar& l, const IntVar& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const BoolVar& l, const BoolVar& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const IntVar& l, const LinIntExpr& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const BoolVar& l, const LinIntExpr& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const LinIntExpr& l, const IntVar& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const LinIntExpr& l, const BoolVar& r);
  /// Construct linear disequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator !=(const LinIntExpr& l, const LinIntExpr& r);

  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(int l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(int l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(int l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const IntVar& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const BoolVar& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const LinIntExpr& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const IntVar& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const IntVar& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const BoolVar& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const BoolVar& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const IntVar& l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const BoolVar& l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const LinIntExpr& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const LinIntExpr& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <(const LinIntExpr& l, const LinIntExpr& r);

  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(int l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(int l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(int l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const IntVar& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const BoolVar& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const LinIntExpr& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const IntVar& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const IntVar& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const BoolVar& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const BoolVar& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const IntVar& l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const BoolVar& l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const LinIntExpr& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const LinIntExpr& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator <=(const LinIntExpr& l, const LinIntExpr& r);

  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(int l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(int l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(int l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const IntVar& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const BoolVar& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const LinIntExpr& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const IntVar& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const IntVar& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const BoolVar& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const BoolVar& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const IntVar& l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const BoolVar& l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const LinIntExpr& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const LinIntExpr& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >(const LinIntExpr& l, const LinIntExpr& r);

  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(int l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(int l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(int l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const IntVar& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const BoolVar& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const LinIntExpr& l, int r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const IntVar& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const IntVar& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const BoolVar& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const BoolVar& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const IntVar& l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const BoolVar& l, const LinIntExpr& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const LinIntExpr& l, const IntVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const LinIntExpr& l, const BoolVar& r);
  /// Construct linear inequality relation
  GECODE_MINIMODEL_EXPORT LinIntRel
  operator >=(const LinIntExpr& l, const LinIntExpr& r);
  //@}

#ifdef GECODE_HAS_FLOAT_VARS
  /// Base class for non-linear float expressions
  class NonLinFloatExpr {
  public:
    /// Return variable constrained to be equal to the expression
    virtual FloatVar post(Home home, FloatVar* ret) const = 0;
    /// Post expression to be in relation \a frt with \a c
    virtual void post(Home home, FloatRelType frt, FloatVal c) const = 0;
    /// Post reified expression to be in relation \a frt with \a c
    virtual void post(Home home, FloatRelType frt, FloatVal c,
                      BoolVar b) const = 0;
    /// Destructor
    virtual ~NonLinFloatExpr(void) {}
    /// Return fresh variable if \a x is NULL, \a x otherwise
    static FloatVar result(Home home, FloatVar* x) {
      if (x == NULL)
        return FloatVar(home,Float::Limits::min,Float::Limits::max);
      return *x;
    }
    /// Constrain \a x to be equal to \a y if \a x is not NULL
    static FloatVar result(Home home, FloatVar* x, FloatVar y) {
      if (x!=NULL)
        rel(home,*x,FRT_EQ,y);
      return y;
    }
    /// Memory management
    void* operator new(size_t size) { return heap.ralloc(size); }
    /// Memory management
    void operator delete(void* p, size_t) { heap.rfree(p); }
  };

  /// %Float expressions
  class LinFloatExpr {
    friend class LinFloatRel;
  public:
    /// Type of linear expression
    enum NodeType {
      NT_CONST,    ///< Float value constant
      NT_VAR,      ///< Linear term with variable
      NT_NONLIN,   ///< Non-linear expression
      NT_SUM,      ///< Sum of float variables
      NT_ADD,      ///< Addition of linear terms
      NT_SUB,      ///< Subtraction of linear terms
      NT_MUL       ///< Multiplication by coefficient
    };
  private:
    /// Nodes for linear expressions
    class Node;
    Node* n;
  public:
    /// Default constructor
    GECODE_MINIMODEL_EXPORT
    LinFloatExpr(void);
    /// Create expression for constant \a c
    GECODE_MINIMODEL_EXPORT
    LinFloatExpr(const FloatVal& c);
    /// Create expression
    GECODE_MINIMODEL_EXPORT
    LinFloatExpr(const FloatVar& x);
    /// Create expression
    GECODE_MINIMODEL_EXPORT
    LinFloatExpr(const FloatVar& x, FloatVal a);
    /// Create sum expression
    GECODE_MINIMODEL_EXPORT
    explicit LinFloatExpr(const FloatVarArgs& x);
    /// Create sum expression
    GECODE_MINIMODEL_EXPORT
    LinFloatExpr(const FloatValArgs& a, const FloatVarArgs& x);
    /// Copy constructor
    GECODE_MINIMODEL_EXPORT
    LinFloatExpr(const LinFloatExpr& e);
    /// Create expression for type and subexpressions
    GECODE_MINIMODEL_EXPORT
    LinFloatExpr(const LinFloatExpr& e0, NodeType t, const LinFloatExpr& e1);
    /// Create expression for type and subexpression
    GECODE_MINIMODEL_EXPORT
    LinFloatExpr(const LinFloatExpr& e0, NodeType t, const FloatVal& c);
    /// Create expression for multiplication
    GECODE_MINIMODEL_EXPORT
    LinFloatExpr(FloatVal a, const LinFloatExpr& e);
    /// Create non-linear expression
    GECODE_MINIMODEL_EXPORT
    explicit LinFloatExpr(NonLinFloatExpr* e);
    /// Assignment operator
    GECODE_MINIMODEL_EXPORT
    const LinFloatExpr& operator =(const LinFloatExpr& e);
    /// Post propagator
    GECODE_MINIMODEL_EXPORT
    void post(Home home, FloatRelType frt) const;
    /// Post reified propagator
    GECODE_MINIMODEL_EXPORT
    void post(Home home, FloatRelType frt, const BoolVar& b) const;
    /// Post propagator and return variable for value
    GECODE_MINIMODEL_EXPORT
    FloatVar post(Home home) const;
    /// Return non-linear expression inside, or NULL if not non-linear
    GECODE_MINIMODEL_EXPORT
    NonLinFloatExpr* nlfe(void) const;
    /// Destructor
    GECODE_MINIMODEL_EXPORT
    ~LinFloatExpr(void);
  };

  class BoolExpr;

  /// Linear relations
  class LinFloatRel {
    friend class BoolExpr;
  private:
    /// Linear float expression describing the entire relation
    LinFloatExpr e;
    /// Which relation
    FloatRelType frt;
    /// Negate relation type
    static FloatRelType neg(FloatRelType frt);
    /// Default constructor
    LinFloatRel(void);
  public:
    /// Create linear float relation for expressions \a l and \a r
    LinFloatRel(const LinFloatExpr& l, FloatRelType frt, const LinFloatExpr& r);
    /// Create linear float relation for expression \a l and FloatVal \a r
    LinFloatRel(const LinFloatExpr& l, FloatRelType frt, FloatVal r);
    /// Create linear float relation for FloatVal \a l and expression \a r
    LinFloatRel(FloatVal l, FloatRelType frt, const LinFloatExpr& r);
    /// Post propagator for relation (if \a t is false for negated relation)
    void post(Home home, bool t) const;
    /// Post reified propagator for relation (if \a t is false for negated relation)
    void post(Home home, const BoolVar& b, bool t) const;
  };

  /**
   * \defgroup TaskModelMiniModelFloat Linear float expressions and relations
   *
   * Linear float expressions can be freely composed of sums and differences of
   * float variables (Gecode::FloatVar) with float coefficients and float
   * constants.
   *
   * Linear float relations are obtained from linear float expressions with the normal
   * relation operators.
   *
   * \ingroup TaskModelMiniModel
   */
  //@{
  /// Construct linear float expression as sum of float variable and float
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator +(const FloatVal&, const FloatVar&);
  /// Construct linear float expression as sum of linear float expression and float
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator +(const FloatVal&, const LinFloatExpr&);
  /// Construct linear float expression as sum of float variable and float
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator +(const FloatVar&, const FloatVal&);
  /// Construct linear float expression as sum of linear float expression and float
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator +(const LinFloatExpr&, const FloatVal&);
  /// Construct linear float expression as sum of float variables
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator +(const FloatVar&, const FloatVar&);
  /// Construct linear float expression as sum of float variable and linear float expression
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator +(const FloatVar&, const LinFloatExpr&);
  /// Construct linear float expression as sum of linear float expression and float variable
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator +(const LinFloatExpr&, const FloatVar&);
  /// Construct linear float expression as sum of linear float expressions
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator +(const LinFloatExpr&, const LinFloatExpr&);

  /// Construct linear float expression as sum of float variable and float
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const FloatVal&, const FloatVar&);
  /// Construct linear float expression as sum of float and linear float expression
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const FloatVal&, const LinFloatExpr&);
  /// Construct linear float expression as sum of float variable and float
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const FloatVar&, const FloatVal&);
  /// Construct linear float expression as sum of linear float expression and float
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const LinFloatExpr&, const FloatVal&);
  /// Construct linear float expression as sum of float variables
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const FloatVar&, const FloatVar&);
  /// Construct linear float expression as sum of float variable and linear float expression
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const FloatVar&, const LinFloatExpr&);
  /// Construct linear float expression as sum of linear float expression and float variable
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const LinFloatExpr&, const FloatVar&);
  /// Construct linear float expression as sum of linear float expressions
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const LinFloatExpr&, const LinFloatExpr&);

  /// Construct linear float expression as negative of float variable
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const FloatVar&);
  /// Construct linear float expression as negative of linear float expression
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator -(const LinFloatExpr&);

  /// Construct linear float expression as product of float coefficient and float variable
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator *(const FloatVal&, const FloatVar&);
  /// Construct linear float expression as product of float coefficient and float variable
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator *(const FloatVar&, const FloatVal&);
  /// Construct linear float expression as product of float coefficient and linear float expression
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator *(const LinFloatExpr&, const FloatVal&);
  /// Construct linear float expression as product of float coefficient and linear float expression
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator *(const FloatVal&, const LinFloatExpr&);

  /// Construct linear float expression as sum of float variables
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  sum(const FloatVarArgs& x);
  /// Construct linear float expression as sum of float variables with coefficients
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  sum(const FloatValArgs& a, const FloatVarArgs& x);

  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const FloatVal& l, const FloatVar& r);
  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const FloatVal& l, const LinFloatExpr& r);
  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const FloatVar& l, const FloatVal& r);
  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const LinFloatExpr& l, const FloatVal& r);
  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const FloatVar& l, const FloatVar& r);
  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const FloatVar& l, const BoolVar& r);
  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const FloatVar& l, const LinFloatExpr& r);
  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const LinFloatExpr& l, const FloatVar& r);
  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const LinFloatExpr& l, const BoolVar& r);
  /// Construct linear float equality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator ==(const LinFloatExpr& l, const LinFloatExpr& r);

  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const FloatVal& l, const FloatVar& r);
  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const FloatVal& l, const LinFloatExpr& r);
  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const FloatVar& l, const FloatVal& r);
  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const LinFloatExpr& l, const FloatVal& r);
  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const FloatVar& l, const FloatVar& r);
  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const FloatVar& l, const BoolVar& r);
  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const FloatVar& l, const LinFloatExpr& r);
  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const LinFloatExpr& l, const FloatVar& r);
  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const LinFloatExpr& l, const BoolVar& r);
  /// Construct linear float disequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator !=(const LinFloatExpr& l, const LinFloatExpr& r);

  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <(const FloatVal& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <(const FloatVal& l, const LinFloatExpr& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <(const FloatVar& l, const FloatVal& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <(const LinFloatExpr& l, const FloatVal& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <(const FloatVar& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <(const FloatVar& l, const LinFloatExpr& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <(const LinFloatExpr& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <(const LinFloatExpr& l, const LinFloatExpr& r);

  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <=(const FloatVal& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <=(const FloatVal& l, const LinFloatExpr& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <=(const FloatVar& l, const FloatVal& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <=(const LinFloatExpr& l, const FloatVal& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <=(const FloatVar& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <=(const FloatVar& l, const LinFloatExpr& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <=(const LinFloatExpr& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator <=(const LinFloatExpr& l, const LinFloatExpr& r);

  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >(const FloatVal& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >(const FloatVal& l, const LinFloatExpr& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >(const FloatVar& l, const FloatVal& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >(const LinFloatExpr& l, const FloatVal& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >(const FloatVar& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >(const FloatVar& l, const LinFloatExpr& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >(const LinFloatExpr& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >(const LinFloatExpr& l, const LinFloatExpr& r);

  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >=(const FloatVal& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >=(const FloatVal& l, const LinFloatExpr& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >=(const FloatVar& l, const FloatVal& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >=(const LinFloatExpr& l, const FloatVal& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >=(const FloatVar& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >=(const FloatVar& l, const LinFloatExpr& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >=(const LinFloatExpr& l, const FloatVar& r);
  /// Construct linear float inequality relation
  GECODE_MINIMODEL_EXPORT LinFloatRel
  operator >=(const LinFloatExpr& l, const LinFloatExpr& r);
  //@}
#endif

#ifdef GECODE_HAS_SET_VARS
  /// %Set expressions
  class SetExpr {
  public:
    /// Type of set expression
    enum NodeType {
      NT_VAR,    ///< Variable
      NT_CONST,  ///< Constant
      NT_LEXP,   ///< Linear expression
      NT_CMPL,   ///< Complement
      NT_INTER,  ///< Intersection
      NT_UNION,  ///< Union
      NT_DUNION  ///< Disjoint union
    };
    /// %Node for set expression
    class Node;
  private:
    /// Pointer to node for expression
    Node* n;
  public:
    /// Default constructor
    SetExpr(void);
    /// Copy constructor
    GECODE_MINIMODEL_EXPORT
    SetExpr(const SetExpr& e);
    /// Construct expression for type and subexpresssions
    GECODE_MINIMODEL_EXPORT
    SetExpr(const SetExpr& l, NodeType t, const SetExpr& r);
    /// Construct expression for variable
    GECODE_MINIMODEL_EXPORT
    SetExpr(const SetVar& x);
    /// Construct expression for integer variable
    GECODE_MINIMODEL_EXPORT
    explicit SetExpr(const LinIntExpr& x);
    /// Construct expression for constant
    GECODE_MINIMODEL_EXPORT
    SetExpr(const IntSet& s);
    /// Construct expression for negation
    GECODE_MINIMODEL_EXPORT
    SetExpr(const SetExpr& e, NodeType t);
    /// Post propagators for expression
    GECODE_MINIMODEL_EXPORT
    SetVar post(Home home) const;
    /// Post propagators for relation
    GECODE_MINIMODEL_EXPORT
    void post(Home home, SetRelType srt, const SetExpr& e) const;
    /// Post propagators for reified relation
    GECODE_MINIMODEL_EXPORT
    void post(Home home, BoolVar b, bool t,
              SetRelType srt, const SetExpr& e) const;
    /// Assignment operator
    GECODE_MINIMODEL_EXPORT
    const SetExpr& operator =(const SetExpr& e);
    /// Destructor
    GECODE_MINIMODEL_EXPORT
    ~SetExpr(void);
  };

  /// Comparison relation (for two-sided comparisons)
  class SetCmpRel {
  public:
    /// Left side of relation
    SetExpr l;
    /// Right side of relation
    SetExpr r;
    /// Which relation
    SetRelType srt;
    /// Constructor
    SetCmpRel(const SetExpr& l, SetRelType srt, const SetExpr& r);
  };

  /// %Set relations
  class SetRel {
  private:
    /// Expression
    SetExpr _e0;
    /// Relation
    SetRelType _srt;
    /// Expression
    SetExpr _e1;
  public:
    /// Default constructor
    SetRel(void);
    /// Constructor
    SetRel(const SetExpr& e0, SetRelType srt, const SetExpr& e1);
    /// Constructor
    SetRel(const SetCmpRel& r);
    /// Post propagators for relation (or negated relation if \a t is false)
    void post(Home home, bool t) const;
    /// Post propagators for reified relation (or negated relation if \a t is false)
    void post(Home home, BoolVar b, bool t) const;
  };

  /**
   * \defgroup TaskModelMiniModelSet Set expressions and relations
   *
   * Set expressions and relations can be freely composed of variables
   * with the usual connectives.
   *
   * \ingroup TaskModelMiniModel
   */

  //@{
  /// Singleton expression
  GECODE_MINIMODEL_EXPORT SetExpr
  singleton(const LinIntExpr&);
  /// Complement expression
  GECODE_MINIMODEL_EXPORT SetExpr
  operator -(const SetExpr&);
  /// Intersection of set expressions
  GECODE_MINIMODEL_EXPORT SetExpr
  operator &(const SetExpr&, const SetExpr&);
  /// Union of set expressions
  GECODE_MINIMODEL_EXPORT SetExpr
  operator |(const SetExpr&, const SetExpr&);
  /// Disjoint union of set expressions
  GECODE_MINIMODEL_EXPORT SetExpr
  operator +(const SetExpr&, const SetExpr&);
  /// Difference of set expressions
  GECODE_MINIMODEL_EXPORT SetExpr
  operator -(const SetExpr&, const SetExpr&);

  /// Intersection of set variables
  GECODE_MINIMODEL_EXPORT SetExpr
  inter(const SetVarArgs&);
  /// Union of set variables
  GECODE_MINIMODEL_EXPORT SetExpr
  setunion(const SetVarArgs&);
  /// Disjoint union of set variables
  GECODE_MINIMODEL_EXPORT SetExpr
  setdunion(const SetVarArgs&);

  /// Cardinality of set expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  cardinality(const SetExpr&);
  /// Minimum element of set expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  min(const SetExpr&);
  /// Minimum element of set expression
  GECODE_MINIMODEL_EXPORT LinIntExpr
  max(const SetExpr&);

  /// Equality of set expressions
  GECODE_MINIMODEL_EXPORT SetRel
  operator ==(const SetExpr&, const SetExpr&);
  /// Disequality of set expressions
  GECODE_MINIMODEL_EXPORT SetRel
  operator !=(const SetExpr&, const SetExpr&);
  /// Subset of set expressions
  GECODE_MINIMODEL_EXPORT SetCmpRel
  operator <=(const SetExpr&, const SetExpr&);
  /// Subset of set expressions
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator <=(const SetCmpRel&, const SetExpr&);
  /// Superset of set expressions
  GECODE_MINIMODEL_EXPORT SetCmpRel
  operator >=(const SetExpr&, const SetExpr&);
  /// Superset of set expressions
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator >=(const SetCmpRel&, const SetExpr&);
  /// Disjointness of set expressions
  GECODE_MINIMODEL_EXPORT SetRel
  operator ||(const SetExpr&, const SetExpr&);
  //@}
#endif

  /// Boolean expressions
  class BoolExpr {
  public:
    /// Type of Boolean expression
    enum NodeType {
      NT_VAR,       ///< Variable
      NT_NOT,       ///< Negation
      NT_AND,       ///< Conjunction
      NT_OR,        ///< Disjunction
      NT_EQV,       ///< Equivalence
      NT_RLIN,      ///< Reified linear relation
      NT_RLINFLOAT, ///< Reified linear relation
      NT_RSET,      ///< Reified set relation
      NT_MISC       ///< Other Boolean expression
    };
    /// Miscealloneous Boolean expressions
    class MiscExpr {
    public:
      /** Constrain \a b to be equivalent to the expression
       *  (negated if \a neg)
       */
      virtual void post(Space& home, BoolVar b, bool neg,
                        IntConLevel icl) = 0;
      /// Destructor
      virtual GECODE_MINIMODEL_EXPORT ~MiscExpr(void);
      /// Memory management
      static void* operator new(size_t size);
      /// Memory management
      static void  operator delete(void* p, size_t size);
    };
    /// %Node for Boolean expression
    class Node;
  private:
    /// Pointer to node for expression
    Node* n;
  public:
    /// Default constructor
    GECODE_MINIMODEL_EXPORT
    BoolExpr(void);
    /// Copy constructor
    GECODE_MINIMODEL_EXPORT
    BoolExpr(const BoolExpr& e);
    /// Construct expression for type and subexpresssions
    GECODE_MINIMODEL_EXPORT
    BoolExpr(const BoolExpr& l, NodeType t, const BoolExpr& r);
    /// Construct expression for variable
    GECODE_MINIMODEL_EXPORT
    BoolExpr(const BoolVar& x);
    /// Construct expression for negation
    GECODE_MINIMODEL_EXPORT
    BoolExpr(const BoolExpr& e, NodeType t);
    /// Construct expression for reified linear relation
    GECODE_MINIMODEL_EXPORT
    BoolExpr(const LinIntRel& rl);
#ifdef GECODE_HAS_FLOAT_VARS
    /// Construct expression for reified float relation
    GECODE_MINIMODEL_EXPORT
    BoolExpr(const LinFloatRel& rfl);
#endif
#ifdef GECODE_HAS_SET_VARS
    /// Construct expression for reified set relation
    GECODE_MINIMODEL_EXPORT
    BoolExpr(const SetRel& rs);
    /// Construct expression for reified set relation
    GECODE_MINIMODEL_EXPORT
    BoolExpr(const SetCmpRel& rs);
#endif
    /// Construct expression for miscellaneous Boolean expression
    GECODE_MINIMODEL_EXPORT
    explicit BoolExpr(MiscExpr* m);
    /// Post propagators for expression
    GECODE_MINIMODEL_EXPORT
    BoolVar expr(Home home, IntConLevel icl) const;
    /// Post propagators for relation
    GECODE_MINIMODEL_EXPORT
    void rel(Home home, IntConLevel icl) const;
    /// Assignment operator
    GECODE_MINIMODEL_EXPORT
    const BoolExpr& operator =(const BoolExpr& e);
    /// Destructor
    GECODE_MINIMODEL_EXPORT
    ~BoolExpr(void);
  };

  /**
   * \defgroup TaskModelMiniModelBool Boolean expressions
   *
   * Boolean expressions can be freely composed of variables with
   * the usual connectives and reified linear expressions.
   *
   * \ingroup TaskModelMiniModel
   */

  //@{
  /// Negated Boolean expression
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator !(const BoolExpr&);
  /// Conjunction of Boolean expressions
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator &&(const BoolExpr&, const BoolExpr&);
  /// Disjunction of Boolean expressions
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator ||(const BoolExpr&, const BoolExpr&);
  /// Exclusive-or of Boolean expressions
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator ^(const BoolExpr&, const BoolExpr&);

  /// Non-equivalence of Boolean expressions
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator !=(const BoolExpr&, const BoolExpr&);
  /// Equivalence of Boolean expressions
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator ==(const BoolExpr&, const BoolExpr&);
  /// Implication of Boolean expressions
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator >>(const BoolExpr&, const BoolExpr&);
  /// Reverse implication of Boolean expressions
  GECODE_MINIMODEL_EXPORT BoolExpr
  operator <<(const BoolExpr&, const BoolExpr&);

  //@}

  /**
   * \defgroup TaskModelMiniModelPost Posting of expressions and relations
   *
   * \ingroup TaskModelMiniModel
   */
  //@{
  /// Post linear expression and return its value
  GECODE_MINIMODEL_EXPORT IntVar 
  expr(Home home, const LinIntExpr& e, IntConLevel icl=ICL_DEF);
#ifdef GECODE_HAS_FLOAT_VARS
  /// Post float expression and return its value
  GECODE_MINIMODEL_EXPORT FloatVar
  expr(Home home, const LinFloatExpr& e);
#endif
#ifdef GECODE_HAS_SET_VARS
  /// Post set expression and return its value
  GECODE_MINIMODEL_EXPORT SetVar
  expr(Home home, const SetExpr& e);
#endif
  /// Post Boolean expression and return its value
  GECODE_MINIMODEL_EXPORT BoolVar
  expr(Home home, const BoolExpr& e, IntConLevel icl=ICL_DEF);
  /// Post Boolean relation
  GECODE_MINIMODEL_EXPORT void 
  rel(Home home, const BoolExpr& e, IntConLevel icl=ICL_DEF);
  //@}

}

#include <gecode/minimodel/int-rel.hpp>
#include <gecode/minimodel/float-rel.hpp>
#include <gecode/minimodel/bool-expr.hpp>
#include <gecode/minimodel/set-expr.hpp>
#include <gecode/minimodel/set-rel.hpp>

namespace Gecode {

  namespace MiniModel {
    class ExpInfo;
  }

  /**
   * \brief Regular expressions over integer values
   *
   * \ingroup TaskModelMiniModel
   */
  class GECODE_MINIMODEL_EXPORT REG {
    friend class MiniModel::ExpInfo;
  private:
    /// Implementation of the actual expression tree
    class Exp;
    /// The expression tree
    Exp* e;
    /// Initialize with given expression tree \a
    REG(Exp* e);
  public:
    /// Initialize as empty sequence (epsilon)
    REG(void);
    /// Initialize as single integer \a s
    REG(int s);
    /**
     * \brief Initialize as alternative of integers
     *
     * Throws an exception of type MiniModel::TooFewArguments if \a x
     * is empty.
     */
    REG(const IntArgs& x);

    /// Initialize from regular expression \a r
    REG(const REG& r);
    /// Assign to regular expression \a r
    const REG& operator =(const REG& r);

    /// Return expression for: this expression followed by \a r
    REG operator +(const REG& r);
    /// This expression is followed by \a r
    REG& operator +=(const REG& r);
    /// Return expression for: this expression or \a r
    REG operator |(const REG& r);
    /// This expression or \a r
    REG& operator |=(const REG& r);
    /// Return expression for: this expression arbitrarily often (Kleene star)
    REG operator *(void);
    /// Return expression for: this expression at least once
    REG operator +(void);
    /// Return expression for: this expression at least \a n and at most \a m times
    REG operator ()(unsigned int n, unsigned int m);
    /// Return expression for: this expression at least \a n times
    REG operator ()(unsigned int n);
    /// Print expression
    template<class Char, class Traits>
    std::basic_ostream<Char,Traits>&
    print(std::basic_ostream<Char,Traits>& os) const;
    /// Return DFA for regular expression
    operator DFA(void);
    /// Destructor
    ~REG(void);
  };

  /** \relates Gecode::REG
   * Print regular expression \a r
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const REG& r);


  /**
   * \defgroup TaskModelMiniModelArith Arithmetic functions
   *
   * \ingroup TaskModelMiniModel
   */
  //@{
  /// \brief Return expression for \f$|e|\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  abs(const LinIntExpr& e);
  /// \brief Return expression for \f$\min(x,y)\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  min(const LinIntExpr& x, const LinIntExpr& y);
  /// \brief Return expression for \f$\min(x)\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  min(const IntVarArgs& x);
  /// \brief Return expression for \f$\max(x,y)\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  max(const LinIntExpr& x, const LinIntExpr& y);
  /// \brief Return expression for \f$\max(x)\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  max(const IntVarArgs& x);
#ifdef GECODE_HAS_FLOAT_VARS
  /// \brief Return expression as product of float variables
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator *(const FloatVar&, const FloatVar&);
  /// \brief Return expression as product of float variable and linear float expression
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator *(const FloatVar&, const LinFloatExpr&);
  /// \brief Return expression as product of linear float expression and float variable
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator *(const LinFloatExpr&, const FloatVar&);
#endif
  /// \brief Return expression for \f$x\cdot y\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator *(const LinIntExpr& x, const LinIntExpr& y);
  /// \brief Return expression for \f$x\ \mathrm{div}\ y\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator /(const LinIntExpr& x, const LinIntExpr& y);
  /// \brief Return expression for \f$x\ \mathrm{mod}\ y\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  operator %(const LinIntExpr& x, const LinIntExpr& y);
  /// \brief Return expression for \f$x^2\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  sqr(const LinIntExpr& x);
  /// \brief Return expression for \f$\lfloor\sqrt{x}\rfloor\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  sqrt(const LinIntExpr& x);
  /// \brief Return expression for \f$x^n\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  pow(const LinIntExpr& x, int n);
  /// \brief Return expression for \f$\lfloor\sqrt[n]{x}\rfloor\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  nroot(const LinIntExpr& x, int n);
  /// \brief Return expression for \f$x[y]\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  element(const IntVarArgs& x, const LinIntExpr& y);
  /// \brief Return expression for \f$x[y]\f$
  GECODE_MINIMODEL_EXPORT BoolExpr
  element(const BoolVarArgs& x, const LinIntExpr& y);
  /// \brief Return expression for \f$x[y]\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  element(const IntArgs& x, const LinIntExpr& y);
  /// \brief Return expression for if-then-else \f$b?x:y\f$
  GECODE_MINIMODEL_EXPORT LinIntExpr
  ite(const BoolExpr& b, const LinIntExpr& x, const LinIntExpr& y);
  //@}

#ifdef GECODE_HAS_FLOAT_VARS
  /// \brief Return expression for \f$|e|\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  abs(const LinFloatExpr& e);
  /// \brief Return expression for \f$\min(x,y)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  min(const LinFloatExpr& x, const LinFloatExpr& y);
  /// \brief Return expression for \f$\min(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  min(const FloatVarArgs& x);
  /// \brief Return expression for \f$\max(x,y)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  max(const LinFloatExpr& x, const LinFloatExpr& y);
  /// \brief Return expression for \f$\max(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  max(const FloatVarArgs& x);
  /// \brief Return expression for \f$x\cdot y\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator *(const LinFloatExpr& x, const LinFloatExpr& y);
  /// \brief Return expression for \f$x/y\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  operator /(const LinFloatExpr& x, const LinFloatExpr& y);
  /// \brief Return expression for \f$x^2\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  sqr(const LinFloatExpr& x);
  /// \brief Return expression for \f$\sqrt{x}\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  sqrt(const LinFloatExpr& x);
  /// \brief Return expression for \f$x^n\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  pow(const LinFloatExpr& x, int n);
  /// \brief Return expression for \f$x^{1/n}\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  nroot(const LinFloatExpr& x, int n);
  //@}

#ifdef GECODE_HAS_MPFR
  /**
   * \defgroup TaskModelMiniModelTrans Transcendental functions
   *
   * \ingroup TaskModelMiniModel
   */
  //@{
  /// \brief Return expression for \f$ \mathrm{exp}(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  exp(const LinFloatExpr& x);
  /// \brief Return expression for \f$ \mathrm{log}(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  log(const LinFloatExpr& x);
  //@}

  /**
   * \defgroup TaskModelMiniModelTrigo Trigonometric functions
   *
   * \ingroup TaskModelMiniModel
   */
  //@{
  /// \brief Return expression for \f$ \mathrm{asin}(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  asin(const LinFloatExpr& x);
  /// \brief Return expression for \f$ \mathrm{sin}(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  sin(const LinFloatExpr& x);
  /// \brief Return expression for \f$ \mathrm{acos}(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  acos(const LinFloatExpr& x);
  /// \brief Return expression for \f$ \mathrm{cos}(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  cos(const LinFloatExpr& x);
  /// \brief Return expression for \f$ \mathrm{atan}(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  atan(const LinFloatExpr& x);
  /// \brief Return expression for \f$ \mathrm{tan}(x)\f$
  GECODE_MINIMODEL_EXPORT LinFloatExpr
  tan(const LinFloatExpr& x);
  //@}
#endif
#endif

  /**
   * \defgroup TaskModelMiniModelChannel Channel functions
   *
   * \ingroup TaskModelMiniModel
   */
  //@{
  /// Return Boolean variable equal to \f$x\f$
  inline BoolVar
  channel(Home home, IntVar x,
          IntConLevel icl=ICL_DEF) {
    (void) icl;
    BoolVar b(home,0,1); channel(home,b,x);
    return b;
  }
  /// Return integer variable equal to \f$b\f$
  inline IntVar
  channel(Home home, BoolVar b,
          IntConLevel icl=ICL_DEF) {
    (void) icl;
    IntVar x(home,0,1); channel(home,b,x);
    return x;
  }
#ifdef GECODE_HAS_FLOAT_VARS 
  /// Return integer variable equal to \f$f\f$
  inline IntVar
  channel(Home home, FloatVar f) {
    int min = static_cast<int>(std::max(static_cast<double>(Int::Limits::min),
                                        std::ceil(f.min())));
    int max = static_cast<int>(std::min(static_cast<double>(Int::Limits::max),
                                        std::floor(f.max())));
    IntVar x(home,min,max);
    channel(home,f,x);
    return x;
  }
#endif
#ifdef GECODE_HAS_SET_VARS 
  /// Return set variable equal to \f$\{x_0,\dots,x_{n-1}\}\f$
  inline SetVar
  channel(Home home, const IntVarArgs& x, IntConLevel icl=ICL_DEF) {
    (void) icl;
    SetVar s(home,IntSet::empty,Set::Limits::min,Set::Limits::max);
    rel(home,SOT_UNION,x,s);
    nvalues(home,x,IRT_EQ,expr(home,cardinality(s)));
    return s;
  }
#endif
  //@}

}

namespace Gecode {

  /**
   * \defgroup TaskModelMiniModelIntAlias Aliases for integer constraints
   *
   * Contains definitions of common constraints which have different
   * names in Gecode.
   *
   * \ingroup TaskModelMiniModel
   */

  //@{
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=n\}\leq m\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  atmost(Home home, const IntVarArgs& x, int n, int m,
         IntConLevel icl=ICL_DEF) {
    count(home,x,n,IRT_LQ,m,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}\leq m\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  atmost(Home home, const IntVarArgs& x, IntVar y, int m,
         IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_LQ,m,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y_i\}\leq m\f$
   *
   * Supports domain consistent propagation only.
   *
   * Throws an exception of type Int::ArgumentSizeMismatch, if
   *  \a x and \a y are of different size.
   */
  inline void
  atmost(Home home, const IntVarArgs& x, const IntArgs& y, int m,
         IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_LQ,m,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=n\}\leq z\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  atmost(Home home, const IntVarArgs& x, int n, IntVar z,
         IntConLevel icl=ICL_DEF) {
    count(home,x,n,IRT_LQ,z,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}\leq z\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  atmost(Home home, const IntVarArgs& x, IntVar y, IntVar z,
         IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_LQ,z,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y_i\}\leq z\f$
   *
   * Supports domain consistent propagation only.
   *
   * Throws an exception of type Int::ArgumentSizeMismatch, if
   *  \a x and \a y are of different size.
   */
  inline void
  atmost(Home home, const IntVarArgs& x, const IntArgs& y, IntVar z,
         IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_LQ,z,icl);
  }

  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=n\}\geq m\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  atleast(Home home, const IntVarArgs& x, int n, int m,
          IntConLevel icl=ICL_DEF) {
    count(home,x,n,IRT_GQ,m,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}\geq m\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  atleast(Home home, const IntVarArgs& x, IntVar y, int m,
          IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_GQ,m,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y_i\}\geq m\f$
   *
   * Supports domain consistent propagation only.
   *
   * Throws an exception of type Int::ArgumentSizeMismatch, if
   *  \a x and \a y are of different size.
   */
  inline void
  atleast(Home home, const IntVarArgs& x, const IntArgs& y, int m,
          IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_GQ,m,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=n\}\geq z\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  atleast(Home home, const IntVarArgs& x, int n, IntVar z,
          IntConLevel icl=ICL_DEF) {
    count(home,x,n,IRT_GQ,z,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}\geq z\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  atleast(Home home, const IntVarArgs& x, IntVar y, IntVar z,
          IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_GQ,z,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y_i\}\geq z\f$
   *
   * Supports domain consistent propagation only.
   *
   * Throws an exception of type Int::ArgumentSizeMismatch, if
   *  \a x and \a y are of different size.
   */
  inline void
  atleast(Home home, const IntVarArgs& x, const IntArgs& y, IntVar z,
          IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_GQ,z,icl);
  }

  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=n\}=m\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  exactly(Home home, const IntVarArgs& x, int n, int m,
          IntConLevel icl=ICL_DEF) {
    count(home,x,n,IRT_EQ,m,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}=m\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  exactly(Home home, const IntVarArgs& x, IntVar y, int m,
          IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_EQ,m,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y_i\}=m\f$
   *
   * Supports domain consistent propagation only.
   *
   * Throws an exception of type Int::ArgumentSizeMismatch, if
   *  \a x and \a y are of different size.
   */
  inline void
  exactly(Home home, const IntVarArgs& x, const IntArgs& y, int m,
          IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_EQ,m,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=n\}=z\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  exactly(Home home, const IntVarArgs& x, int n, IntVar z,
          IntConLevel icl=ICL_DEF) {
    count(home,x,n,IRT_EQ,z,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}=z\f$
   *
   * Supports domain consistent propagation only.
   */
  inline void
  exactly(Home home, const IntVarArgs& x, IntVar y, IntVar z,
          IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_EQ,z,icl);
  }
  /** \brief Post constraint \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y_i\}=z\f$
   *
   * Supports domain consistent propagation only.
   *
   * Throws an exception of type Int::ArgumentSizeMismatch, if
   *  \a x and \a y are of different size.
   */
  inline void
  exactly(Home home, const IntVarArgs& x, const IntArgs& y, IntVar z,
          IntConLevel icl=ICL_DEF) {
    count(home,x,y,IRT_EQ,z,icl);
  }
  /** \brief Post lexical order between \a x and \a y.
   */
  inline void
  lex(Home home, const IntVarArgs& x, IntRelType r, const IntVarArgs& y,
      IntConLevel icl=ICL_DEF) {
    rel(home,x,r,y,icl);
  }
  /** \brief Post lexical order between \a x and \a y.
   */
  inline void
  lex(Home home, const BoolVarArgs& x, IntRelType r, const BoolVarArgs& y,
      IntConLevel icl=ICL_DEF) {
    rel(home,x,r,y,icl);
  }
  /** \brief Post constraint \f$\{x_0,\dots,x_{n-1}\}=y\f$
   */
  inline void
  values(Home home, const IntVarArgs& x, IntSet y,
         IntConLevel icl=ICL_DEF) {
    dom(home,x,y,icl);
    nvalues(home,x,IRT_EQ,y.size(),icl);
  }

  //@}

#ifdef GECODE_HAS_SET_VARS
  /**
   * \defgroup TaskModelMiniModelSetAlias Aliases for set constraints
   *
   * Contains definitions of common constraints which have different
   * names in Gecode.
   *
   * \ingroup TaskModelMiniModel
   */

  //@{
  /** \brief Post constraint \f$\{x_0,\dots,x_{n-1}\}=y\f$
   * 
   * In addition to constraining \a y to the union of the \a x, this
   * also posts an nvalue constraint for additional cardinality propagation.
   */
  inline void
  channel(Home home, const IntVarArgs& x, SetVar y) {
    rel(home,SOT_UNION,x,y);
    nvalues(home,x,IRT_EQ,expr(home,cardinality(y)));
  }
  
  /** \brief Post constraint \f$\bigcup_{i\in y}\{x_i\}=z\f$
   */
  inline void
  range(Home home, const IntVarArgs& x, SetVar y, SetVar z) {
    element(home,SOT_UNION,x,y,z);
  }

  /** \brief Post constraint \f$\bigcup_{i\in z}\{j\ |\ x_j=i\}=z\f$
   *
   * Note that this creates one temporary set variable for each element
   * in the upper bound of \a z, so make sure that the bound is tight.
   */
  inline void
  roots(Home home, const IntVarArgs& x, SetVar y, SetVar z) {
    SetVarArgs xiv(home,z.lubMax()+1,IntSet::empty,0,x.size()-1);
    channel(home,x,xiv);
    element(home,SOT_UNION,xiv,z,y);
  }
  
  //@}
#endif
}

namespace Gecode {

  template<class> class Matrix;

  /** \brief A slice of a matrix.
   *
   * This class represents a slice of the matrix. It is used to get
   * context-dependent behaviour. The slice will be automatically
   * converted to an ArgsType Args-array or to a Matrix<ArgsType>
   * depending on the context where it is used.
   */
  template<class A>
  class Slice {
  public:
    /// The type of the Args-array type for ValueType values
    typedef typename ArrayTraits<A>::ArgsType ArgsType;
  private:
    ArgsType _r;     ///< The elements of the slice
    unsigned int _fc, ///< From column
      _tc,            ///< To column
      _fr,            ///< From row
      _tr;            ///< To row
  public:
    /// Construct slice
    Slice(const Matrix<A>& a, int fc, int tc, int fr, int tr);
    /** \brief Reverses the contents of the slice, and returns a
     *  reference to it.
     */
    Slice& reverse(void);
    /// Cast to array type
    operator ArgsType(void);
    /// Cast to matrix type
    operator Matrix<ArgsType>(void);

    /// Cast to array type
    operator const ArgsType(void) const;
    /// Cast to matrix type
    operator const Matrix<ArgsType>(void) const;
  };
  
  /// Concatenate \a x and \a y
  template<class A>
  typename Slice<A>::ArgsType
  operator+(const Slice<A>& x, const Slice<A>& y);

  /// Concatenate \a x and \a y
  template<class A>
  typename Slice<A>::ArgsType
  operator+(const Slice<A>& x, const typename ArrayTraits<A>::ArgsType& y);

  /// Concatenate \a x and \a y
  template<class A>
  typename Slice<A>::ArgsType
  operator+(const typename ArrayTraits<A>::ArgsType& x, const Slice<A>& y);

  /// Concatenate \a x and \a y
  template<class A>
  typename Slice<A>::ArgsType
  operator+(const Slice<A>& x, const typename ArrayTraits<A>::ValueType& y);

  /// Concatenate \a x and \a y
  template<class A>
  typename Slice<A>::ArgsType
  operator+(const typename ArrayTraits<A>::ValueType& x, const Slice<A>& y);

  /** \brief Matrix-interface for arrays
   *
   * This class allows for wrapping some array and accessing it as a
   * matrix.
   *
   * \note This is a light-weight wrapper, and is not intended for
   * storing variables directly instead of in an array.
   *
   * \ingroup TaskModelMiniModel
   */
  template<class A>
  class Matrix {
  public:
    /// The type of elements of this array
    typedef typename ArrayTraits<A>::ValueType ValueType;
    /// The type of the Args-array type for ValueType values
    typedef typename ArrayTraits<A>::ArgsType ArgsType;

  private:
    /// The type of storage for this array
    typedef typename ArrayTraits<A>::StorageType StorageType;
    StorageType _a; ///< The array wrapped
    int _w; ///< The width of the matrix
    int _h; ///< The height of the matrix

  public:
    /** \brief Basic constructor
     *
     * Constructs a Matrix from the array \a a, using \a w and \a h as
     * the width and height of the matrix.
     *
     * The elements in the wrapped array \a a are accessed in
     * row-major order.
     *
     * \exception MiniModel::ArgumentSizeMismatch Raised if the
     *            parameters \a w and \a h doesn't match the size
     *            of the array \a a.
     */
    Matrix(A a, int w, int h);

    /** \brief Basic constructor
     *
     * Constructs a square Matrix from the array \a a, using \a n as
     * the length of the sides.
     *
     * The elements in the wrapped array \a a are accessed in
     * row-major order.
     *
     * \exception MiniModel::ArgumentSizeMismatch Raised if the
     *            parameter \a n doesn't match the size
     *            of the array \a a.
     */
    Matrix(A a, int n);

    /// Return the width of the matrix
    int width(void) const;
    /// Return the height of the matrix
    int height(void) const;
    /// Return an Args-array of the contents of the matrix
    ArgsType const get_array(void) const;

    /** \brief Access element (\a c, \a r) of the matrix
     *
     * \exception MiniModel::ArgumentOutOfRange Raised if \a c or \a r
     *            are out of range.
     */
    ValueType& operator ()(int c, int r);

    /** \brief Access element (\a c, \a r) of the matrix
     *
     * \exception MiniModel::ArgumentOutOfRange Raised if \a c or \a r
     *            are out of range.
     */
    const ValueType& operator ()(int c, int r) const;

    /** \brief Access slice of the matrix
     *
     * This function allows accessing a slice of the matrix, located at
     * columns \f$[fc,tc)\f$ and rows \f$[fr,tr)\f$. The result of this
     * function is an object that can be converted into either a
     * Matrix<ArgsType> or into ArgsType.
     *
     * For further information, see Slice.
     */
    Slice<A> slice(int fc, int tc, int fr, int tr) const;

    /// Access row \a r.
    Slice<A> row(int r) const;

    /// Access column \a c.
    Slice<A> col(int c) const;
  };

  /** \relates Gecode::Matrix
   * Print matrix \a m
   */
  template<class Char, class Traits, class A>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const Matrix<A>& m);

  /** \relates Gecode::Matrix
   * Print slice \a s
   */
  template<class Char, class Traits, class A>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const Slice<A>& s);

  /** \brief Element constraint for matrix
   *
   * Here, \a x and \a y are the coordinates and \a z is the value
   * at position \a m(x,y).
   * \relates Gecode::Matrix
   */
  void element(Home home, const Matrix<IntArgs>& m, IntVar x, IntVar y,  
               IntVar z, IntConLevel icl=ICL_DEF);
  /** \brief Element constraint for matrix
   *
   * Here, \a x and \a y are the coordinates and \a z is the value
   * at position \a m(x,y).
   * \relates Gecode::Matrix
   */
  void element(Home home, const Matrix<IntArgs>& m, IntVar x, IntVar y,  
               BoolVar z, IntConLevel icl=ICL_DEF);
  /** \brief Element constraint for matrix
   *
   * Here, \a x and \a y are the coordinates and \a z is the value
   * at position \a m(x,y).
   * \relates Gecode::Matrix
   */
  void element(Home home, const Matrix<IntVarArgs>& m, IntVar x, IntVar y,  
               IntVar z, IntConLevel icl=ICL_DEF);
  /** \brief Element constraint for matrix
   *
   * Here, \a x and \a y are the coordinates and \a z is the value
   * at position \a m(x,y).
   * \relates Gecode::Matrix
   */
  void element(Home home, const Matrix<BoolVarArgs>& m, IntVar x, IntVar y,  
               BoolVar z, IntConLevel icl=ICL_DEF);
#ifdef GECODE_HAS_SET_VARS
  /** \brief Element constraint for matrix
   *
   * Here, \a x and \a y are the coordinates and \a z is the value
   * at position \a m(x,y).
   * \relates Gecode::Matrix
   */
  void element(Home home, const Matrix<IntSetArgs>& m, IntVar x, IntVar y,  
               SetVar z);
  /** \brief Element constraint for matrix
   *
   * Here, \a x and \a y are the coordinates and \a z is the value
   * at position \a m(x,y).
   * \relates Gecode::Matrix
   */
  void element(Home home, const Matrix<SetVarArgs>& m, IntVar x, IntVar y,  
               SetVar z);
#endif

  /** \brief Interchangeable rows symmetry specification.
   * \relates Gecode::Matrix
   */
  template<class A>
  SymmetryHandle rows_interchange(const Matrix<A>& m);
  /** \brief Interchangeable columns symmetry specification.
   * \relates Gecode::Matrix
   */
  template<class A>
  SymmetryHandle columns_interchange(const Matrix<A>& m);
  /** \brief Reflect rows symmetry specification.
   * \relates Gecode::Matrix
   */
  template<class A>
  SymmetryHandle rows_reflect(const Matrix<A>& m);
  /** \brief Reflect columns symmetry specification.
   * \relates Gecode::Matrix
   */
  template<class A>
  SymmetryHandle columns_reflect(const Matrix<A>& m);
  /** \brief Reflect around main diagonal symmetry specification.
   *
   * The matrix \m must be square.  
   * \relates Gecode::Matrix
   */
  template<class A>
  SymmetryHandle diagonal_reflect(const Matrix<A>& m);
}

#include <gecode/minimodel/matrix.hpp>
#include <gecode/minimodel/ldsb.hpp>

/**
 * \addtogroup TaskModelMiniModelLin
 * @{
 */ 
namespace Gecode {
  /// Construct linear expression as sum of \ref IntArgs \ref Slice elements
  GECODE_MINIMODEL_EXPORT LinIntExpr 
  sum(const Slice<IntArgs>& slice);
  /// Construct linear expression as sum of \ref IntArgs \ref Matrix elements
  GECODE_MINIMODEL_EXPORT LinIntExpr 
  sum(const Matrix<IntArgs>& matrix);
}
/** @}*/

namespace Gecode {

  /**
   * \defgroup TaskModelMiniModelOptimize Support for cost-based optimization
   *
   * Provides for minimizing or maximizing the cost value as defined by
   * a cost-member function of a space.
   *
   * \ingroup TaskModelMiniModel
   */
  //@{
  namespace MiniModel {

    /// Baseclass for integer-based cost-based optimization
    template<IntRelType irt>
    class IntOptimizeSpace : public Space {
    public:
      /// Default constructor
      IntOptimizeSpace(void);
      /// Constructor for cloning
      IntOptimizeSpace(bool share, IntOptimizeSpace& s);
      /// Member function constraining according to cost
      virtual void constrain(const Space& best);
      /// Return variable with current cost
      virtual IntVar cost(void) const = 0;
    };

#ifdef GECODE_HAS_FLOAT_VARS 

    /// Baseclass for float-based cost-based optimization
    template<FloatRelType frt>
    class FloatOptimizeSpace : public Space {
    public:
      /// Default constructor
      FloatOptimizeSpace(void);
      /// Constructor for cloning
      FloatOptimizeSpace(bool share, FloatOptimizeSpace& s);
      /// Member function constraining according to cost
      virtual void constrain(const Space& best);
      /// Return variable with current cost
      virtual FloatVar cost(void) const = 0;
    };

#endif

  }

  /// Class for minimizing integer cost
  typedef MiniModel::IntOptimizeSpace<IRT_LE> MinimizeSpace;
  /// Class for maximizing integer cost
  typedef MiniModel::IntOptimizeSpace<IRT_GR> MaximizeSpace;
  /// Class for minimizing integer cost
  typedef MiniModel::IntOptimizeSpace<IRT_LE> IntMinimizeSpace;
  /// Class for maximizing integer cost
  typedef MiniModel::IntOptimizeSpace<IRT_GR> IntMaximizeSpace;

#ifdef GECODE_HAS_FLOAT_VARS 

  /// Class for minimizing float cost
  typedef MiniModel::FloatOptimizeSpace<FRT_LE> FloatMinimizeSpace;
  /// Class for maximizing float cost
  typedef MiniModel::FloatOptimizeSpace<FRT_GR> FloatMaximizeSpace;

#endif

  //@}

}

#include <gecode/minimodel/optimize.hpp>

#endif

// IFDEF: GECODE_HAS_INT_VARS
// STATISTICS: minimodel-any

