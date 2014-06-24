/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
 *     Gabor Szokoli, 2004
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

#ifndef __GECODE_SET_BRANCH_HH__
#define __GECODE_SET_BRANCH_HH__

#include <gecode/set.hh>

/**
 * \namespace Gecode::Set::Branch
 * \brief %Set branchings
 */

namespace Gecode { namespace Set { namespace Branch {

  /**
   * \defgroup FuncSetViewSel Merit-based set view selection for branchers
   *
   * Contains merit-based view selection strategies on set
   * views that can be used together with the generic view/value
   * brancher classes.
   *
   * All merit-based set view selection classes require 
   * \code #include <gecode/set/branch.hh> \endcode
   * \ingroup Other
   */

  /**
   * \brief Merit class for mimimum of set views
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetViewSel
   */
  class MeritMin : public MeritBase<SetView,int> {
  public:
    /// Constructor for initialization
    MeritMin(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritMin(Space& home, bool shared, MeritMin& m);
    /// Return minimum as merit for view \a x at position \a i
    int operator ()(const Space& home, SetView x, int i);
  };

  /**
   * \brief Merit class for maximum of set view
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetViewSel
   */
  class MeritMax : public MeritBase<SetView,int> {
  public:
    /// Constructor for initialization
    MeritMax(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritMax(Space& home, bool shared, MeritMax& m);
    /// Return maximum as merit for view \a x at position \a i
    int operator ()(const Space& home, SetView x, int i);
  };

  /**
   * \brief Merit class for size of set view
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetViewSel
   */
  class MeritSize : public MeritBase<SetView,unsigned int> {
  public:
    /// Constructor for initialization
    MeritSize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritSize(Space& home, bool shared, MeritSize& m);
    /// Return size as merit for view \a x at position \a i
    unsigned int operator ()(const Space& home, SetView x, int i);
  };

  /**
   * \brief Merit class for size over degree
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetViewSel
   */
  class MeritDegreeSize : public MeritBase<SetView,double> {
  public:
    /// Constructor for initialization
    MeritDegreeSize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritDegreeSize(Space& home, bool shared, MeritDegreeSize& m);
    /// Return size over degree as merit for view \a x at position \a i
    double operator ()(const Space& home, SetView x, int i);
  };

  /**
   * \brief Merit class for size over afc
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetViewSel
   */
  class MeritAFCSize : public MeritBase<SetView,double> {
  protected:
    /// AFC information
    AFC afc;
  public:
    /// Constructor for initialization
    MeritAFCSize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritAFCSize(Space& home, bool shared, MeritAFCSize& m);
    /// Return size over AFC as merit for view \a x at position \a i
    double operator ()(const Space& home, SetView x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Dispose view selection
    void dispose(Space& home);
  };

  /**
   * \brief Merit class for size over activity
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetViewSel
   */
  class MeritActivitySize : public MeritBase<SetView,double> {
  protected:
    /// Activity information
    Activity activity;
  public:
    /// Constructor for initialization
    MeritActivitySize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritActivitySize(Space& home, bool shared, MeritActivitySize& m);
    /// Return size over activity as merit for view \a x at position \a i
    double operator ()(const Space& home, SetView x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Dispose view selection
    void dispose(Space& home);
  };

}}}

#include <gecode/set/branch/merit.hpp>

namespace Gecode { namespace Set { namespace Branch {

  /// Return view selectors for set views
  GECODE_SET_EXPORT
  ViewSel<SetView>* viewsel(Space& home, const SetVarBranch& svb);

}}}

namespace Gecode { namespace Set { namespace Branch {

  /**
   * \defgroup FuncSetValSel Set value selection for brancher
   *
   * Contains a description of value selection strategies on set
   * views that can be used together with the generic view/value
   * branchers.
   *
   * All value selection classes require 
   * \code #include <gecode/set/branch.hh> \endcode
   * \ingroup Other
   */

  /**
   * \brief Value selection class for mimimum of view
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetValSel
   */
  class ValSelMin : public ValSel<SetView,int> {
  public:
    /// Constructor for initialization
    ValSelMin(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelMin(Space& home, bool shared, ValSelMin& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, SetView x, int i);
  };

  /**
   * \brief Value selection class for maximum of view
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetValSel
   */
  class ValSelMax : public ValSel<SetView,int> {
  public:
    /// Constructor for initialization
    ValSelMax(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelMax(Space& home, bool shared, ValSelMax& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, SetView x, int i);
  };

  /**
   * \brief Value selection class for median of view
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetValSel
   */
  class ValSelMed : public ValSel<SetView,int> {
  public:
    /// Constructor for initialization
    ValSelMed(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelMed(Space& home, bool shared, ValSelMed& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, SetView x, int i);
  };

  /**
   * \brief Value selection class for random value of view
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetValSel
   */
  class ValSelRnd : public ValSel<SetView,int> {
  protected:
    /// The used random number generator
    Rnd r;
  public:
    /// Constructor for initialization
    ValSelRnd(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelRnd(Space& home, bool shared, ValSelRnd& vs);
    /// Return value of view \a x at position \a i
    int val(const Space& home, SetView x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Delete value selection
    void dispose(Space& home);
  };

}}}

#include <gecode/set/branch/val-sel.hpp>

namespace Gecode { namespace Set { namespace Branch {

  /// No-good literal for inclusion
  class IncNGL : public ViewValNGL<SetView,int,PC_SET_ANY> {
  public:
    /// Constructor for creation
    IncNGL(Space& home, SetView x, int n);
    /// Constructor for cloning \a ngl
    IncNGL(Space& home, bool share, IncNGL& ngl);
    /// Test the status of the no-good literal
    GECODE_SET_EXPORT
    virtual NGL::Status status(const Space& home) const;
    /// Propagate the negation of the no-good literal
    GECODE_SET_EXPORT
    virtual ExecStatus prune(Space& home);
    /// Create copy
    GECODE_SET_EXPORT
    virtual NGL* copy(Space& home, bool share);
  };

  /// No-good literal for exclusion
  class ExcNGL : public ViewValNGL<SetView,int,PC_SET_ANY> {
  public:
    /// Constructor for creation
    ExcNGL(Space& home, SetView x, int n);
    /// Constructor for cloning \a ngl
    ExcNGL(Space& home, bool share, ExcNGL& ngl);
    /// Test the status of the no-good literal
    GECODE_SET_EXPORT
    virtual NGL::Status status(const Space& home) const;
    /// Propagate the negation of the no-good literal
    GECODE_SET_EXPORT
    virtual ExecStatus prune(Space& home);
    /// Create copy
    GECODE_SET_EXPORT
    virtual NGL* copy(Space& home, bool share);
  };

}}}

#include <gecode/set/branch/ngl.hpp>

namespace Gecode { namespace Set { namespace Branch {

  /**
   * \defgroup FuncSetValCommit Set value commit classes
   *
   * Contains the value commit classes for set
   * views that can be used together with the generic view/value
   * branchers.
   *
   * All value commit classes require 
   * \code #include <gecode/set/branch.hh> \endcode
   * \ingroup Other
   */

  /**
   * \brief Value commit class for inclusion
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetValCommit
   */
  class ValCommitInc : public ValCommit<SetView,int> {
  public:
    /// Constructor for initialization
    ValCommitInc(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValCommitInc(Space& home, bool shared, ValCommitInc& vc);
    /// Commit view \a x at position \a i to value \a n for alternative \a a
    ModEvent commit(Space& home, unsigned int a, SetView x, int i, int n);
    /// Create no-good literal for alternative \a a
    NGL* ngl(Space& home, unsigned int a, View x, int n) const;
    /// Print on \a o the alternative \a with view \a x at position \a i and value \a n
    void print(const Space& home, unsigned int a, SetView x, int i, int n,
               std::ostream& o) const;
  };

  /**
   * \brief Value commit class for exclusion
   *
   * Requires \code #include <gecode/set/branch.hh> \endcode
   * \ingroup FuncSetValCommit
   */
  class ValCommitExc : public ValCommit<SetView,int> {
  public:
    /// Constructor for initialization
    ValCommitExc(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValCommitExc(Space& home, bool shared, ValCommitExc& vc);
    /// Commit view \a x at position \a i to value \a n for alternative \a a
    ModEvent commit(Space& home, unsigned int a, SetView x, int i, int n);
    /// Create no-good literal for alternative \a a
    NGL* ngl(Space& home, unsigned int a, View x, int n) const;
    /// Print on \a o the alternative \a with view \a x at position \a i and value \a n
    void print(const Space& home, unsigned int a, SetView x, int i, int n,
               std::ostream& o) const;
  };

}}}

#include <gecode/set/branch/val-commit.hpp>

namespace Gecode { namespace Set { namespace Branch {

  /// Return value and commit for set views
  GECODE_SET_EXPORT
  ValSelCommitBase<SetView,int>* 
  valselcommit(Space& home, const SetValBranch& svb);

  /// Return value and commit for set views
  GECODE_SET_EXPORT
  ValSelCommitBase<SetView,int>* 
  valselcommit(Space& home, const SetAssign& ia);

}}}

#endif

// STATISTICS: set-branch

