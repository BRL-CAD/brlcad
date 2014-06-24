/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2012
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

#ifndef __GECODE_FLOAT_BRANCH_HH__
#define __GECODE_FLOAT_BRANCH_HH__

#include <gecode/float.hh>

/**
 * \namespace Gecode::Float::Branch
 * \brief Float branchers
 */

namespace Gecode { namespace Float { namespace Branch {

  /**
   * \defgroup FuncFloatViewSel Merit-based float view selection for branchers
   *
   * Contains merit-based view selection strategies on float
   * views that can be used together with the generic view/value
   * brancher classes.
   *
   * All merit-based float view selection classes require 
   * \code #include <gecode/float/branch.hh> \endcode
   * \ingroup Other
   */

  /**
   * \brief Merit class for mimimum
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatViewSel
   */
  class MeritMin : public MeritBase<FloatView,double> {
  public:
    /// Constructor for initialization
    MeritMin(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritMin(Space& home, bool shared, MeritMin& m);
    /// Return minimum as merit for view \a x at position \a i
    double operator ()(const Space& home, FloatView x, int i);
  };

  /**
   * \brief Merit class for maximum of float view
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatViewSel
   */
  class MeritMax : public MeritBase<FloatView,double> {
  public:
    /// Constructor for initialization
    MeritMax(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritMax(Space& home, bool shared, MeritMax& m);
    /// Return maximum as merit for view \a x at position \a i
    double operator ()(const Space& home, FloatView x, int i);
  };

  /**
   * \brief Merit class for size of float view
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatViewSel
   */
  class MeritSize : public MeritBase<FloatView,double> {
  public:
    /// Constructor for initialization
    MeritSize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritSize(Space& home, bool shared, MeritSize& m);
    /// Return size as merit for view \a x at position \a i
    double operator ()(const Space& home, FloatView x, int i);
  };

  /**
   * \brief Merit class for size over degree
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatViewSel
   */
  class MeritDegreeSize : public MeritBase<FloatView,double> {
  public:
    /// Constructor for initialization
    MeritDegreeSize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritDegreeSize(Space& home, bool shared, MeritDegreeSize& m);
    /// Return size over degree as merit for view \a x at position \a i
    double operator ()(const Space& home, FloatView x, int i);
  };

  /**
   * \brief Merit class for size over afc
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatViewSel
   */
  class MeritAFCSize : public MeritBase<FloatView,double> {
  protected:
    /// AFC information
    AFC afc;
  public:
    /// Constructor for initialization
    MeritAFCSize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritAFCSize(Space& home, bool shared, MeritAFCSize& m);
    /// Return size over AFC as merit for view \a x at position \a i
    double operator ()(const Space& home, FloatView x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Dispose view selection
    void dispose(Space& home);
  };

  /**
   * \brief Merit class for size over activity
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatViewSel
   */
  class MeritActivitySize : public MeritBase<FloatView,double> {
  protected:
    /// Activity information
    Activity activity;
  public:
    /// Constructor for initialization
    MeritActivitySize(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritActivitySize(Space& home, bool shared, MeritActivitySize& m);
    /// Return size over activity as merit for view \a x at position \a i
    double operator ()(const Space& home, FloatView x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Dispose view selection
    void dispose(Space& home);
  };

}}}

#include <gecode/float/branch/merit.hpp>

namespace Gecode { namespace Float { namespace Branch {

  /// Return view selectors for float views
  GECODE_FLOAT_EXPORT
  ViewSel<FloatView>* viewsel(Space& home, const FloatVarBranch& fvb);

}}}

namespace Gecode { namespace Float { namespace Branch {

  /**
   * \defgroup FuncFloatValSel Float value selection for brancher
   *
   * Contains a description of value selection strategies on float
   * views that can be used together with the generic view/value
   * branchers.
   *
   * All value selection classes require 
   * \code #include <gecode/float/branch.hh> \endcode
   * \ingroup Other
   */

  /**
   * \brief Value selection class for values smaller than median of view
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatValSel
   */
  class ValSelLq : public ValSel<FloatView,FloatNumBranch> {
  public:
    /// Constructor for initialization
    ValSelLq(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelLq(Space& home, bool shared, ValSelLq& vs);
    /// Return value of view \a x at position \a i
    FloatNumBranch val(const Space& home, FloatView x, int i);
  };

  /**
   * \brief Value selection class for values smaller than median of view
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatValSel
   */
  class ValSelGq : public ValSel<FloatView,FloatNumBranch> {
  public:
    /// Constructor for initialization
    ValSelGq(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelGq(Space& home, bool shared, ValSelGq& vs);
    /// Return value of view \a x at position \a i
    FloatNumBranch val(const Space& home, FloatView x, int i);
  };

  /**
   * \brief Value selection class for random value of view
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatValSel
   */
  class ValSelRnd : public ValSel<FloatView,FloatNumBranch> {
  protected:
    /// The used random number generator
    Rnd r;
  public:
    /// Constructor for initialization
    ValSelRnd(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelRnd(Space& home, bool shared, ValSelRnd& vs);
    /// Return value of view \a x at position \a i
    FloatNumBranch val(const Space& home, FloatView x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Delete value selection
    void dispose(Space& home);
  };

}}}

#include <gecode/float/branch/val-sel.hpp>

namespace Gecode { namespace Float { namespace Branch {

  /**
   * \defgroup FuncFloatValCommit Float value commit classes
   *
   * Contains the value commit classes for float
   * views that can be used together with the generic view/value
   * branchers.
   *
   * All value commit classes require 
   * \code #include <gecode/float/branch.hh> \endcode
   * \ingroup Other
   */

  /**
   * \brief Value commit class for less or equal or greater or equal
   *
   * Requires \code #include <gecode/float/branch.hh> \endcode
   * \ingroup FuncFloatValCommit
   */
  class ValCommitLqGq  : public ValCommit<FloatView,FloatVal> {
  public:
    /// Constructor for initialization
    ValCommitLqGq(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValCommitLqGq(Space& home, bool shared, ValCommitLqGq& vc);
    /// Commit view \a x at position \a i to value \a n for alternative \a a
    ModEvent commit(Space& home, unsigned int a, FloatView x, int i, 
                    FloatNumBranch n);
    /// Create no-good literal for alternative \a a
    NGL* ngl(Space& home, unsigned int a, FloatView x, FloatNumBranch n) const;
    /// Print on \a o the alternative \a with view \a x at position \a i and value \a n
    void print(const Space& home, unsigned int a, FloatView x, int i, 
               FloatNumBranch n,
               std::ostream& o) const;
  };

}}}

#include <gecode/float/branch/val-commit.hpp>

namespace Gecode { namespace Float { namespace Branch {

  /// Return value and commit for float views
  GECODE_FLOAT_EXPORT
  ValSelCommitBase<FloatView,FloatNumBranch>* 
  valselcommit(Space& home, const FloatValBranch& svb);

  /// Return value and commit for float views
  GECODE_FLOAT_EXPORT
  ValSelCommitBase<FloatView,FloatNumBranch>* 
  valselcommit(Space& home, const FloatAssign& ia);

}}}

#endif

// STATISTICS: float-branch
