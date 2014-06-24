/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

#ifndef __GECODE_INT_NO_OVERLAP_HH__
#define __GECODE_INT_NO_OVERLAP_HH__

#include <gecode/int.hh>

/**
 * \namespace Gecode::Int::NoOverlap
 * \brief %No-overlap propagators
 */

namespace Gecode { namespace Int { namespace NoOverlap {

  /**
   * \brief Dimension combining coordinate and integer size information
   */
  class FixDim {
  protected:
    /// Coordinate
    IntView c;
    /// Size
    int s;
    /// Modify smallest start coordinate
    ExecStatus ssc(Space& home, int n);
    /// Modify largest end coordinate
    ExecStatus lec(Space& home, int n);
    /// Dimension must not overlap with coordinates \a n to \a m
    ExecStatus nooverlap(Space& home, int n, int m);
  public:
    /// Default constructor
    FixDim(void);
    /// Constructor
    FixDim(IntView c, int s);

    /// Return smallest start coordinate
    int ssc(void) const;
    /// Return largest start coordinate
    int lsc(void) const;
    /// Return smallest end coordinate
    int sec(void) const;
    /// Return largest end coordinate
    int lec(void) const;

    /// Dimension must not overlap with \a d
    ExecStatus nooverlap(Space& home, FixDim& d);

    /// Update dimension during cloning
    void update(Space& home, bool share, FixDim& d);

    /// Subscribe propagator \a p to dimension
    void subscribe(Space& home, Propagator& p);
    /// Cancel propagator \a p from dimension
    void cancel(Space& home, Propagator& p);
  };

  /**
   * \brief Dimension combining coordinate and integer view size information
   */
  class FlexDim {
  protected:
    /// Start coordinate
    IntView c0;
    /// Size
    IntView s;
    /// End coordinate
    IntView c1;
    /// Modify smallest start coordinate
    ExecStatus ssc(Space& home, int n);
    /// Modify largest end coordinate
    ExecStatus lec(Space& home, int n);
    /// Dimension must not overlap with coordinates \a n to \a m
    ExecStatus nooverlap(Space& home, int n, int m);
  public:
    /// Default constructor
    FlexDim(void);
    /// Constructor
    FlexDim(IntView c0, IntView s, IntView c1);

    /// Return smallest start coordinate
    int ssc(void) const;
    /// Return largest start coordinate
    int lsc(void) const;
    /// Return smallest end coordinate
    int sec(void) const;
    /// Return largest end coordinate
    int lec(void) const;

    /// Dimension must not overlap with \a d
    ExecStatus nooverlap(Space& home, FlexDim& d);

    /// Update dimension during cloning
    void update(Space& home, bool share, FlexDim& d);

    /// Subscribe propagator \a p to dimension
    void subscribe(Space& home, Propagator& p);
    /// Cancel propagator \a p from dimension
    void cancel(Space& home, Propagator& p);
  };

}}}

#include <gecode/int/no-overlap/dim.hpp>

namespace Gecode { namespace Int { namespace NoOverlap {

  /**
   * \brief Mandatory box class
   */
  template<class Dim, int n>
  class ManBox {
  protected:
    /// Dimensions
    Dim d[n];
  public:
    /// Access to dimension \a i
    const Dim& operator [](int i) const;
    /// Access to dimension \a i
    Dim& operator [](int i);
    /// Return number of dimensions
    static int dim(void);

    /// Whether box is mandatory
    bool mandatory(void) const;
    /// Whether box is optional
    bool optional(void) const;
    /// Whether box is excluded
    bool excluded(void) const;

    /// Exclude box
    ExecStatus exclude(Space& home);

    /// Check whether this box does not any longer overlap with \a b
    bool nooverlap(const ManBox<Dim,n>& b) const;
    /// Check whether this box overlaps with \a b
    bool overlap(const ManBox<Dim,n>& b) const;

    /// Propagate that this box does not overlap with \a b
    ExecStatus nooverlap(Space& home, ManBox<Dim,n>& b);

    /// Update box during cloning
    void update(Space& home, bool share, ManBox<Dim,n>& r);

    /// Subscribe propagator \a p to box
    void subscribe(Space& home, Propagator& p);
    /// Cancel propagator \a p from box
    void cancel(Space& home, Propagator& p);
  };

  /**
   * \brief Optional box class
   */
  template<class Dim, int n>
  class OptBox : public ManBox<Dim,n> {
  protected:
    using ManBox<Dim,n>::d;
    /// Whether box is optional or not
    BoolView o;
  public:
    /// Set Boolean view to \a o
    void optional(BoolView o);
    /// Whether box is mandatory
    bool mandatory(void) const;
    /// Whether box is optional
    bool optional(void) const;
    /// Whether box is excluded
    bool excluded(void) const;

    /// Exclude box
    ExecStatus exclude(Space& home);

    /// Update box during cloning
    void update(Space& home, bool share, OptBox<Dim,n>& r);

    /// Subscribe propagator \a p to box
    void subscribe(Space& home, Propagator& p);
    /// Cancel propagator \a p from box
    void cancel(Space& home, Propagator& p);
  };

}}}

#include <gecode/int/no-overlap/box.hpp>

namespace Gecode { namespace Int { namespace NoOverlap {

  /**
   * \brief Base class for no-overlap propagator
   *
   * Requires \code #include <gecode/int/no-overlap.hh> \endcode
   *
   * \ingroup FuncIntProp
   */
  template<class Box>
  class Base : public Propagator {
  protected:
    /// Boxes
    Box* b;
    /// Number of mandatory boxes: b[0] ... b[n-1]
    int n;
    /// Constructor for posting with \a n mandatory boxes
    Base(Home home, Box* b, int n);
    /// Constructor for cloning \a p with \a m boxes
    Base(Space& home, bool share, Base<Box>& p, int m);
    /**
     * \brief Partition \a n boxes \a b starting at position \a i
     *
     * Returns the number of mandatory boxes at the front of \a b.
     */
    static int partition(Box* b, int i, int n);
  public:
    /// Cost function
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Destructor
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief No-overlap propagator for mandatory boxes
   *
   * Requires \code #include <gecode/int/no-overlap.hh> \endcode
   *
   * \ingroup FuncIntProp
   */
  template<class Box>
  class ManProp : public Base<Box> {
  protected:
    using Base<Box>::b;
    using Base<Box>::n;
    /// Constructor for posting
    ManProp(Home home, Box* b, int n);
    /// Constructor for cloning \a p
    ManProp(Space& home, bool share, ManProp<Box>& p);
  public:
    /// Post propagator for boxes \a b
    static ExecStatus post(Home home, Box* b, int n);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Destructor
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief No-overlap propagator for optional boxes
   *
   * Requires \code #include <gecode/int/no-overlap.hh> \endcode
   *
   * \ingroup FuncIntProp
   */
  template<class Box>
  class OptProp : public Base<Box> {
  protected:
    using Base<Box>::b;
    using Base<Box>::n;
    /// Number of optional boxes: b[n] ... b[n+m-1]
    int m;
    /// Constructor for posting
    OptProp(Home home, Box* b, int n, int m);
    /// Constructor for cloning \a p
    OptProp(Space& home, bool share, OptProp<Box>& p);
  public:
    /// Post propagator for boxes \a b
    static ExecStatus post(Home home, Box* b, int n);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Destructor
    virtual size_t dispose(Space& home);
  };

}}}

#include <gecode/int/no-overlap/base.hpp>
#include <gecode/int/no-overlap/man.hpp>
#include <gecode/int/no-overlap/opt.hpp>

#endif

// STATISTICS: int-prop

