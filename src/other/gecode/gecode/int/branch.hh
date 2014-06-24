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

#ifndef __GECODE_INT_BRANCH_HH__
#define __GECODE_INT_BRANCH_HH__

#include <gecode/int.hh>

/**
 * \namespace Gecode::Int::Branch
 * \brief Integer branchers
 */

namespace Gecode { namespace Int { namespace Branch {

  /**
   * \defgroup FuncIntViewSel Merit-based integer view selection for branchers
   *
   * Contains merit-based view selection strategies on integer
   * views that can be used together with the generic view/value
   * brancher classes.
   *
   * All merit-based view selection classes require 
   * \code #include <gecode/int/branch.hh> \endcode
   * \ingroup Other
   */

  /**
   * \brief Merit class for mimimum of integer views
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntViewSel
   */
  template<class View>
  class MeritMin : public MeritBase<View,int> {
  public:
    /// Constructor for initialization
    MeritMin(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritMin(Space& home, bool shared, MeritMin& m);
    /// Return minimum as merit for view \a x at position \a i
    int operator ()(const Space& home, View x, int i);
  };

  /**
   * \brief Merit class for maximum
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntViewSel
   */
  template<class View>
  class MeritMax : public MeritBase<View,int> {
  public:
    /// Constructor for initialization
    MeritMax(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritMax(Space& home, bool shared, MeritMax& m);
    /// Return maximum as merit for view \a x at position \a i
    int operator ()(const Space& home, View x, int i);
  };

  /**
   * \brief Merit class for size
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntViewSel
   */
  template<class View>
  class MeritSize : public MeritBase<View,unsigned int> {
  public:
    /// Constructor for initialization
    MeritSize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritSize(Space& home, bool shared, MeritSize& m);
    /// Return size as merit for view \a x at position \a i
    unsigned int operator ()(const Space& home, View x, int i);
  };

  /**
   * \brief Merit class for size over degree
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntViewSel
   */
  template<class View>
  class MeritDegreeSize : public MeritBase<View,double> {
  public:
    /// Constructor for initialization
    MeritDegreeSize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritDegreeSize(Space& home, bool shared, MeritDegreeSize& m);
    /// Return size over degree as merit for view \a x at position \a i
    double operator ()(const Space& home, View x, int i);
  };

  /**
   * \brief Merit class for size over afc
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntViewSel
   */
  template<class View>
  class MeritAFCSize : public MeritBase<View,double> {
  protected:
    /// AFC information
    AFC afc;
  public:
    /// Constructor for initialization
    MeritAFCSize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritAFCSize(Space& home, bool shared, MeritAFCSize& m);
    /// Return size over AFC as merit for view \a x at position \a i
    double operator ()(const Space& home, View x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Dispose view selection
    void dispose(Space& home);
  };

  /**
   * \brief Merit class for size over activity
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntViewSel
   */
  template<class View>
  class MeritActivitySize : public MeritBase<View,double> {
  protected:
    /// Activity information
    Activity activity;
  public:
    /// Constructor for initialization
    MeritActivitySize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritActivitySize(Space& home, bool shared, MeritActivitySize& m);
    /// Return size over activity as merit for view \a x at position \a i
    double operator ()(const Space& home, View x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Dispose view selection
    void dispose(Space& home);
  };

  /**
   * \brief Merit class for minimum regret
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntViewSel
   */
  template<class View>
  class MeritRegretMin : public MeritBase<View,unsigned int> {
  public:
    /// Constructor for initialization
    MeritRegretMin(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritRegretMin(Space& home, bool shared, MeritRegretMin& m);
    /// Return minimum regret as merit for view \a x at position \a i
    unsigned int operator ()(const Space& home, View x, int i);
  };

  /**
   * \brief Merit class for maximum regret
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntViewSel
   */
  template<class View>
  class MeritRegretMax : public MeritBase<View,unsigned int> {
  public:
    /// Constructor for initialization
    MeritRegretMax(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritRegretMax(Space& home, bool shared, MeritRegretMax& m);
    /// Return maximum regret as merit for view \a x at position \a i
    unsigned int operator ()(const Space& home, View x, int i);
  };

}}}

#include <gecode/int/branch/merit.hpp>

namespace Gecode { namespace Int { namespace Branch {

  /// Return view selectors for integer views
  GECODE_INT_EXPORT
  ViewSel<IntView>* viewselint(Space& home, const IntVarBranch& ivb);
  /// Return view selectors for Boolean views
  GECODE_INT_EXPORT
  ViewSel<BoolView>* viewselbool(Space& home, const IntVarBranch& ivb);

}}}

namespace Gecode { namespace Int { namespace Branch {

  /**
   * \defgroup FuncIntValSel Integer value selection for brancher
   *
   * Contains a description of value selection strategies on integer
   * views that can be used together with the generic view/value
   * branchers.
   *
   * All value selection classes require 
   * \code #include <gecode/int/branch.hh> \endcode
   * \ingroup Other
   */

  /**
   * \brief Value selection class for mimimum of view
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValSel
   */
  template<class View>
  class ValSelMin : public ValSel<View,int> {
  public:
    /// Constructor for initialization
    ValSelMin(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelMin(Space& home, bool shared, ValSelMin& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, View x, int i);
  };

  /**
   * \brief Value selection class for maximum of view
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValSel
   */
  template<class View>
  class ValSelMax : public ValSel<View,int> {
  public:
    /// Constructor for initialization
    ValSelMax(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelMax(Space& home, bool shared, ValSelMax& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, View x, int i);
  };

  /**
   * \brief Value selection class for median of view
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValSel
   */
  template<class View>
  class ValSelMed : public ValSel<View,int> {
  public:
    /// Constructor for initialization
    ValSelMed(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelMed(Space& home, bool shared, ValSelMed& vs);
    /// Return value of view \a x at position  i
    int val(const Space& home, View x, int i);
  };

  /**
   * \brief Value selection class for average of view
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValSel
   */
  template<class View>
  class ValSelAvg : public ValSel<View,int> {
  public:
    /// Constructor for initialization
    ValSelAvg(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelAvg(Space& home, bool shared, ValSelAvg& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, View x, int i);
  };

  /**
   * \brief Value selection class for random value of view
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValSel
   */
  template<class View>
  class ValSelRnd : public ValSel<View,int> {
  protected:
    /// The used random number generator
    Rnd r;
  public:
    /// Constructor for initialization
    ValSelRnd(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelRnd(Space& home, bool shared, ValSelRnd& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, View x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Delete value selection
    void dispose(Space& home);
  };

  /**
   * \brief Value selection class for minimum range of integer view
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValSel
   */
  class ValSelRangeMin : public ValSel<IntView,int> {
  public:
    /// Constructor for initialization
    ValSelRangeMin(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelRangeMin(Space& home, bool shared, ValSelRangeMin& vs);
    /// Return value of integer view \a x at position \a i
    int val(const Space& home, IntView x, int i);
  };

  /**
   * \brief Value selection class for maximum range of integer view
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValSel
   */
  class ValSelRangeMax : public ValSel<IntView,int> {
  public:
    /// Constructor for initialization
    ValSelRangeMax(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelRangeMax(Space& home, bool shared, ValSelRangeMax& vs);
    /// Return value of integer view \a x at position \a i
    int val(const Space& home, IntView x, int i);
  };

  /**
   * \brief Value selection class for nearest value
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValSel
   */
  template<class View, bool min>
  class ValSelNearMinMax : public ValSel<View,int> {
  protected:
    /// The used values
    IntSharedArray c;
  public:
    /// Constructor for initialization
    ValSelNearMinMax(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelNearMinMax(Space& home, bool shared, ValSelNearMinMax& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, View x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Delete value selection
    void dispose(Space& home);
  };

  /**
   * \brief Value selection class for nearest value
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValSel
   */
  template<class View, bool inc>
  class ValSelNearIncDec : public ValSel<View,int> {
  protected:
    /// The used values
    IntSharedArray c;
  public:
    /// Constructor for initialization
    ValSelNearIncDec(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelNearIncDec(Space& home, bool shared, ValSelNearIncDec& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, View x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Delete value selection
    void dispose(Space& home);
  };

}}}

#include <gecode/int/branch/val-sel.hpp>

namespace Gecode { namespace Int { namespace Branch {

  /// No-good literal for equality
  template<class View>
  class EqNGL : public ViewValNGL<View,int,PC_INT_VAL> {
    using ViewValNGL<View,int,PC_INT_VAL>::x;
    using ViewValNGL<View,int,PC_INT_VAL>::n;
  public:
    /// Constructor for creation
    EqNGL(Space& home, View x, int n);
    /// Constructor for cloning \a ngl
    EqNGL(Space& home, bool share, EqNGL& ngl);
    /// Test the status of the no-good literal
    virtual NGL::Status status(const Space& home) const;
    /// Propagate the negation of the no-good literal
    virtual ExecStatus prune(Space& home);
    /// Create copy
    virtual NGL* copy(Space& home, bool share);
  };

  /// No-good literal for disequality
  template<class View>
  class NqNGL : public ViewValNGL<View,int,PC_INT_DOM> {
    using ViewValNGL<View,int,PC_INT_DOM>::x;
    using ViewValNGL<View,int,PC_INT_DOM>::n;
  public:
    /// Constructor for creation
    NqNGL(Space& home, View x, int n);
    /// Constructor for cloning \a ngl
    NqNGL(Space& home, bool share, NqNGL& ngl);
    /// Test the status of the no-good literal
    virtual NGL::Status status(const Space& home) const;
    /// Propagate the negation of the no-good literal
    virtual ExecStatus prune(Space& home);
    /// Create copy
    virtual NGL* copy(Space& home, bool share);
  };

  /// No-good literal for less or equal
  template<class View>
  class LqNGL : public ViewValNGL<View,int,PC_INT_BND> {
    using ViewValNGL<View,int,PC_INT_BND>::x;
    using ViewValNGL<View,int,PC_INT_BND>::n;
  public:
    /// Constructor for creation
    LqNGL(Space& home, View x, int n);
    /// Constructor for cloning \a ngl
    LqNGL(Space& home, bool share, LqNGL& ngl);
    /// Test the status of the no-good literal
    virtual NGL::Status status(const Space& home) const;
    /// Propagate the negation of the no-good literal
    virtual ExecStatus prune(Space& home);
    /// Create copy
    virtual NGL* copy(Space& home, bool share);
  };

  /// No-good literal for greater or equal
  template<class View>
  class GqNGL : public ViewValNGL<View,int,PC_INT_BND> {
    using ViewValNGL<View,int,PC_INT_BND>::x;
    using ViewValNGL<View,int,PC_INT_BND>::n;
  public:
    /// Constructor for creation
    GqNGL(Space& home, View x, int n);
    /// Constructor for cloning \a ngl
    GqNGL(Space& home, bool share, GqNGL& ngl);
    /// Test the status of the no-good literal
    virtual NGL::Status status(const Space& home) const;
    /// Propagate the negation of the no-good literal
    virtual ExecStatus prune(Space& home);
    /// Create copy
    virtual NGL* copy(Space& home, bool share);
  };

}}}

#include <gecode/int/branch/ngl.hpp>

namespace Gecode { namespace Int { namespace Branch {

  /**
   * \defgroup FuncIntValCommit Integer value commit classes
   *
   * Contains the value commit classes for integer and Boolean
   * views that can be used together with the generic view/value
   * branchers.
   *
   * All value commit classes require 
   * \code #include <gecode/int/branch.hh> \endcode
   * \ingroup Other
   */

  /**
   * \brief Value commit class for equality
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValCommit
   */
  template<class View>
  class ValCommitEq : public ValCommit<View,int> {
  public:
    /// Constructor for initialization
    ValCommitEq(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValCommitEq(Space& home, bool shared, ValCommitEq& vc);
    /// Commit view \a x at position \a i to value \a n for alternative \a a
    ModEvent commit(Space& home, unsigned int a, View x, int i, int n);
    /// Create no-good literal for alternative \a a
    NGL* ngl(Space& home, unsigned int a, View x, int n) const;
    /// Print on \a o the alternative \a with view \a x at position \a i and value \a n
    void print(const Space& home, unsigned int a, View x, int i, int n,
               std::ostream& o) const;
  };

  /**
   * \brief Value commit class for less or equal
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValCommit
   */
  template<class View>
  class ValCommitLq : public ValCommit<View,int> {
  public:
    /// Constructor for initialization
    ValCommitLq(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValCommitLq(Space& home, bool shared, ValCommitLq& vc);
    /// Commit view \a x at position \a i to value \a n for alternative \a a
    ModEvent commit(Space& home, unsigned int a, View x, int i, int n);
    /// Create no-good literal for alternative \a a
    NGL* ngl(Space& home, unsigned int a, View x, int n) const;
    /// Print on \a o the alternative \a with view \a x at position \a i and value \a n
    void print(const Space& home, unsigned int a, View x, int i, int n,
               std::ostream& o) const;
  };

  /**
   * \brief Value commit class for greater or equal
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValCommit
   */
  template<class View>
  class ValCommitGq : public ValCommit<View,int> {
  public:
    /// Constructor for initialization
    ValCommitGq(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValCommitGq(Space& home, bool shared, ValCommitGq& vc);
    /// Commit view \a x at position \a i to value \a n for alternative \a a
    ModEvent commit(Space& home, unsigned int a, View x, int i, int n);
    /// Create no-good literal for alternative \a a
    NGL* ngl(Space& home, unsigned int a, View x, int n) const;
    /// Print on \a o the alternative \a with view \a x at position \a i and value \a n
    void print(const Space& home, unsigned int a, View x, int i, int n,
               std::ostream& o) const;
  };

  /**
   * \brief Value commit class for greater
   *
   * Requires \code #include <gecode/int/branch.hh> \endcode
   * \ingroup FuncIntValCommit
   */
  template<class View>
  class ValCommitGr : public ValCommit<View,int> {
  public:
    /// Constructor for initialization
    ValCommitGr(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValCommitGr(Space& home, bool shared, ValCommitGr& vc);
    /// Commit view \a x at position \a i to value \a n for alternative \a a
    ModEvent commit(Space& home, unsigned int a, View x, int i, int n);
    /// Create no-good literal for alternative \a a
    NGL* ngl(Space& home, unsigned int a, View x, int n) const;
    /// Print on \a o the alternative \a with view \a x at position \a i and value \a n
    void print(const Space& home, unsigned int a, View x, int i, int n,
               std::ostream& o) const;
  };

}}}

#include <gecode/int/branch/val-commit.hpp>

namespace Gecode { namespace Int { namespace Branch {

  /// Return value and commit for integer views
  GECODE_INT_EXPORT
  ValSelCommitBase<IntView,int>* 
  valselcommitint(Space& home, int n, const IntValBranch& ivb);

  /// Return value and commit for Boolean views
  GECODE_INT_EXPORT
  ValSelCommitBase<BoolView,int>* 
  valselcommitbool(Space& home, int n, const IntValBranch& ivb);

  /// Return value and commit for integer views
  GECODE_INT_EXPORT
  ValSelCommitBase<IntView,int>* 
  valselcommitint(Space& home, const IntAssign& ia);

  /// Return value and commit for Boolean views
  GECODE_INT_EXPORT
  ValSelCommitBase<BoolView,int>* 
  valselcommitbool(Space& home, const IntAssign& ia);

}}}

namespace Gecode { namespace Int { namespace Branch {

  /**
   * \brief %Brancher by view and values selection
   *
   */
  template<int n, bool min>
  class ViewValuesBrancher : public ViewBrancher<IntView,n> {
    typedef typename ViewBrancher<IntView,n>::BranchFilter BranchFilter;
  protected:
    using ViewBrancher<IntView,n>::x;
    /// Print function
    IntVarValPrint vvp;
    /// Constructor for cloning \a b
    ViewValuesBrancher(Space& home, bool shared, ViewValuesBrancher& b);
    /// Constructor for creation
    ViewValuesBrancher(Home home, ViewArray<IntView>& x,
                       ViewSel<IntView>* vs[n], 
                       BranchFilter bf, IntVarValPrint vvp);
  public:
    /// Return choice
    virtual const Choice* choice(Space& home);
    /// Return choice
    virtual const Choice* choice(const Space& home, Archive& e);
    /// Perform commit for choice \a c and alternative \a a
    virtual ExecStatus commit(Space& home, const Choice& c, unsigned int a);
    /// Create no-good literal for choice \a c and alternative \a a
    virtual NGL* ngl(Space& home, const Choice& c, unsigned int a) const;
    /**
     * \brief Print branch for choice \a c and alternative \a a
     *
     * Prints an explanation of the alternative \a a of choice \a c
     * on the stream \a o.
     *
     */
    virtual void print(const Space& home, const Choice& c, unsigned int a,
                       std::ostream& o) const;
    /// Perform cloning
    virtual Actor* copy(Space& home, bool share);
    /// Constructor for creation
    static BrancherHandle post(Home home, ViewArray<IntView>& x,
                               ViewSel<IntView>* vs[n], 
                               BranchFilter bf, IntVarValPrint vvp);
  };

}}}

#include <gecode/int/branch/view-values.hpp>

#endif

// STATISTICS: int-branch
