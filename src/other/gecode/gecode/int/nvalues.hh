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

#ifndef __GECODE_INT_NVALUES_HH__
#define __GECODE_INT_NVALUES_HH__

#include <gecode/int.hh>
#include <gecode/int/val-set.hh>

/**
 * \namespace Gecode::Int::NValues
 * \brief Number of values propagators
 */

namespace Gecode { namespace Int { namespace NValues {

  /// Event type for range-based overlap analysis
  enum RangeEventType {
    /// A range starts
    RET_FST = 0,
    /// A range ends
    RET_LST = 1,
    /// No further events
    RET_END = 2
  };

  /// Event for range-based overlap analysis
  class RangeEvent {
  public:
    /// The event type
    RangeEventType ret;
    /// The value for the range (first or last value, depending on type)
    int val;
    /// Which view does this range belong to
    int view;
    /// Order events: first by val, then by event type
    bool operator <(RangeEvent re) const;
  };

  /// Symmetric diagonal bit matrix
  class SymBitMatrix : public Support::BitSet<Region> {
  protected:
    /// Size of matrix
    int n;
    /// Return position in matrix
    int pos(int x, int y) const;
  public:
    /// Initialize matrix for dimension \a n by \a n
    SymBitMatrix(Region& r, int n);
    /// Is bit at position \a x, \a y set?
    bool get(int x, int y) const;
    /// Set bit at position \a x, \a y
    void set(int x, int y);
  };

}}}

#include <gecode/int/nvalues/range-event.hpp>
#include <gecode/int/nvalues/sym-bit-matrix.hpp>

#include <gecode/int/view-val-graph.hh>

namespace Gecode { namespace Int { namespace NValues {

  /// View-value graph for propagation of upper bound
  class Graph : public ViewValGraph::Graph<IntView> {
  protected:
    /// Number of matched edges
    int n_matched;
  public:
    /// Construct graph as not yet initialized
    Graph(void);
    /// Return size of maximal matching (excluding assigned views)
    int size(void) const;
    /// Initialize graph including values in \a vs
    void init(Space& home, const ValSet& vs, const ViewArray<IntView>& x);
    /// Synchronize graph with new view domains
    void sync(Space& home);
    /*
     * \brief Mark all edges used that can appear in some maximal matching
     *
     * Return true, if any edge can be in fact pruned.
     */
    bool mark(Space& home);
    /// Prune all values corresponding to unused edges
    ExecStatus prune(Space& home);
  };

}}}

#include <gecode/int/nvalues/graph.hpp>

namespace Gecode { namespace Int { namespace NValues {

  /**
   * \brief Number of values propagator for integer views base class
   *
   * Requires \code #include <gecode/int/nvalues.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VY>
  class IntBase 
    : public MixNaryOnePropagator<IntView,PC_INT_DOM,VY,PC_INT_BND> {
  protected:
    using MixNaryOnePropagator<IntView,PC_INT_DOM,VY,PC_INT_BND>::x;
    using MixNaryOnePropagator<IntView,PC_INT_DOM,VY,PC_INT_BND>::y;
    /// Value set storing the values of already assigned views
    ValSet vs;
    /// Constructor for posting
    IntBase(Home home, ValSet& vs, ViewArray<IntView>& x, VY y);
    /// Constructor for cloning \a p
    IntBase(Space& home, bool share, IntBase<VY>& p);
    /// Add values of assigned views to value set
    void add(Space& home);
    /**
     * Compute position of disjoint views in \a dis (with length \a n_dis) 
     * and eliminate subsumed views (all values included in the value set
     * \a vs).
     */
    void disjoint(Space& home, Region& r, int*& dis, int& n_dis);
    /// Eliminate subsumed views (all values included in the value set \a vs)
    void eliminate(Space& home);
    /// Return a size estimate based on the union of all values
    int size(Space& home) const;
    /// Propagate that all views must take values from value set
    ExecStatus all_in_valset(Space& home);
    /**
     * Perform pruning of the lower bound based on finding
     * an independent set, where \a dis and \a n_dis define the
     * set of disjoint views (not overlapping with the values in
     * the value set).
     *
     * Changes \a dis.
     */
    ExecStatus prune_lower(Space& home, int* dis, int n_dis);
    /**
     * Perform pruning of the upper bound based on finding
     * a maximal matching in the view value graph \a g.
     *
     * Requires that subsumed views have been eliminated.
     */
    ExecStatus prune_upper(Space& home, Graph& g);
  public:
    /// Cost function
    virtual PropCost cost(const Space&, const ModEventDelta&) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Equal to number of values propagator for integer views
   *
   * Requires \code #include <gecode/int/nvalues.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VY>
  class EqInt : public IntBase<VY> {
  protected:
    using IntBase<VY>::x;
    using IntBase<VY>::y;
    using IntBase<VY>::vs;
    using IntBase<VY>::add;
    using IntBase<VY>::all_in_valset;
    using IntBase<VY>::disjoint;
    using IntBase<VY>::prune_lower;
    using IntBase<VY>::prune_upper;
    /// View-value graph
    Graph g;
    /// Constructor for posting
    EqInt(Home home, ValSet& vs, ViewArray<IntView>& x, VY y);
    /// Constructor for cloning \a p
    EqInt(Space& home, bool share, EqInt<VY>& p);
  public:
    /// Copy propagator during cloning
    virtual Propagator* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\#\{x_0,\ldots,x_{|x|-1}\}=y\f$
    static ExecStatus post(Home home, ViewArray<IntView>& x, VY y);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Less or equal to number of values propagator for integer views
   *
   * Requires \code #include <gecode/int/nvalues.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VY>
  class LqInt : public IntBase<VY> {
  protected:
    using IntBase<VY>::x;
    using IntBase<VY>::y;
    using IntBase<VY>::vs;
    using IntBase<VY>::add;
    using IntBase<VY>::all_in_valset;
    using IntBase<VY>::disjoint;
    using IntBase<VY>::prune_lower;
    using IntBase<VY>::prune_upper;
    /// Constructor for posting
    LqInt(Home home, ValSet& vs, ViewArray<IntView>& x, VY y);
    /// Constructor for cloning \a p
    LqInt(Space& home, bool share, LqInt<VY>& p);
  public:
    /// Copy propagator during cloning
    virtual Propagator* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\#\{x_0,\ldots,x_{|x|-1}\}\leq y\f$
    static ExecStatus post(Home home, ViewArray<IntView>& x, VY y);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Greater or equal to number of values propagator for integer views
   *
   * Requires \code #include <gecode/int/nvalues.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VY>
  class GqInt : public IntBase<VY> {
  protected:
    using IntBase<VY>::x;
    using IntBase<VY>::y;
    using IntBase<VY>::vs;
    using IntBase<VY>::add;
    using IntBase<VY>::all_in_valset;
    using IntBase<VY>::disjoint;
    using IntBase<VY>::prune_lower;
    using IntBase<VY>::prune_upper;
    using IntBase<VY>::eliminate;
    /// View-value graph
    Graph g;
    /// Constructor for posting
    GqInt(Home home, ValSet& vs, ViewArray<IntView>& x, VY y);
    /// Constructor for cloning \a p
    GqInt(Space& home, bool share, GqInt<VY>& p);
  public:
    /// Copy propagator during cloning
    virtual Propagator* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\#\{x_0,\ldots,x_{|x|-1}\}\geq y\f$
    static ExecStatus post(Home home, ViewArray<IntView>& x, VY y);
  };

}}}

#include <gecode/int/nvalues/int-base.hpp>
#include <gecode/int/nvalues/int-eq.hpp>
#include <gecode/int/nvalues/int-lq.hpp>
#include <gecode/int/nvalues/int-gq.hpp>

namespace Gecode { namespace Int { namespace NValues {

  /**
   * \brief Number of values propagator for Boolean views base class
   *
   * Requires \code #include <gecode/int/nvalues.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VY>
  class BoolBase : public Propagator { 
  protected:
    /// View status: a zero has already been encountered
    static const int VS_ZERO = 1 << 0;
    /// View status: a one has already been encountered
    static const int VS_ONE  = 1 << 1;
    /// Status information about the views
    int status;
    /// The advisor council
    Council<ViewAdvisor<BoolView> > c;
    /// The view for counting the number of values
    VY y;
    /// Constructor for posting
    BoolBase(Home home, int status, ViewArray<BoolView>& x, VY y);
    /// Constructor for cloning \a p
    BoolBase(Space& home, bool share, BoolBase<VY>& p);
  public:
    /// Give advice to propagator
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    /// Cost function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Equal to number of values propagator for Boolean views
   *
   * Requires \code #include <gecode/int/nvalues.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VY>
  class EqBool : public BoolBase<VY> {
  protected:
    using BoolBase<VY>::VS_ZERO;
    using BoolBase<VY>::VS_ONE;
    using BoolBase<VY>::status;
    using BoolBase<VY>::c;
    using BoolBase<VY>::y;
    /// Constructor for posting
    EqBool(Home home, int status, ViewArray<BoolView>& x, VY y);
    /// Constructor for cloning \a p
    EqBool(Space& home, bool share, EqBool<VY>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Post propagator for \f$\#\{x_0,\ldots,x_{|x|-1}\}=y\f$
     *
     * The view array \a x is used only temporarily and hence can
     * be region allocated.
     */
    static ExecStatus post(Home home, ViewArray<BoolView>& x, VY y);
  };

  /**
   * \brief Less or equal to number of values propagator for Boolean views
   *
   * Requires \code #include <gecode/int/nvalues.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VY>
  class LqBool : public BoolBase<VY> {
  protected:
    using BoolBase<VY>::VS_ZERO;
    using BoolBase<VY>::VS_ONE;
    using BoolBase<VY>::status;
    using BoolBase<VY>::c;
    using BoolBase<VY>::y;
    /// Constructor for posting
    LqBool(Home home, int status, ViewArray<BoolView>& x, VY y);
    /// Constructor for cloning \a p
    LqBool(Space& home, bool share, LqBool<VY>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Post propagator for \f$\#\{x_0,\ldots,x_{|x|-1}\}\leq y\f$
     *
     * The view array \a x is used only temporarily and hence can
     * be region allocated.
     */
    static  ExecStatus post(Home home, ViewArray<BoolView>& x, VY y);
  };

  /**
   * \brief Greater or equal to number of values propagator for Boolean views
   *
   * Requires \code #include <gecode/int/nvalues.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VY>
  class GqBool : public BoolBase<VY> {
  protected:
    using BoolBase<VY>::VS_ZERO;
    using BoolBase<VY>::VS_ONE;
    using BoolBase<VY>::status;
    using BoolBase<VY>::c;
    using BoolBase<VY>::y;
    /// Constructor for posting
    GqBool(Home home, int status, ViewArray<BoolView>& x, VY y);
    /// Constructor for cloning \a p
    GqBool(Space& home, bool share, GqBool<VY>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Post propagator for \f$\#\{x_0,\ldots,x_{|x|-1}\}\geq y\f$
     *
     * The view array \a x is used only temporarily and hence can
     * be region allocated.
     */
    static  ExecStatus post(Home home, ViewArray<BoolView>& x, VY y);
  };

}}}

#include <gecode/int/nvalues/bool-base.hpp>
#include <gecode/int/nvalues/bool-eq.hpp>
#include <gecode/int/nvalues/bool-lq.hpp>
#include <gecode/int/nvalues/bool-gq.hpp>

#endif

// STATISTICS: int-prop
