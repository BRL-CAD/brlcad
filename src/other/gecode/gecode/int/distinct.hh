/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Guido Tack, 2004
 *     Gabor Szokoli, 2003
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

#ifndef __GECODE_INT_DISTINCT_HH__
#define __GECODE_INT_DISTINCT_HH__

#include <gecode/int.hh>

#include <gecode/int/view-val-graph.hh>
#include <gecode/int/rel.hh>

/**
 * \namespace Gecode::Int::Distinct
 * \brief %Distinct propagators
 */

namespace Gecode { namespace Int { namespace Distinct {

  /**
   * \brief Naive value distinct propagator
   *
   * Eliminates values of assigned views of type \a View.
   *
   * Requires \code #include <gecode/int/distinct.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class Val : public NaryPropagator<View,PC_INT_VAL> {
  protected:
    using NaryPropagator<View,PC_INT_VAL>::x;

    /// Constructor for posting
    Val(Home home, ViewArray<View>& x);
    /// Constructor for cloning \a p
    Val(Space& home, bool share, Val<View>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor*     copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for view array \a x
    static ExecStatus post(Home home, ViewArray<View>& x);
  };

  /**
   * \brief Eliminate singletons by naive value propagation
   *
   * This is actually the propagation algorithm for Distinct::Val.
   * It is available as separate function as it is reused for
   * both bounds consistent and domain consistent distinct
   * propagators.
   *
   * If \a complete is true, computes fixpoint, otherwise might not
   * compute fixpoint. This can be helpful when used together with
   * bounds or domain propagation to protect from pathological cases
   * which can be handled more efficiently with bounds propagation.
   */
  template<class View, bool complete>
  ExecStatus prop_val(Space& home, ViewArray<View>&);



  /**
   * \brief Bounds consistent distinct propagator
   *
   * The propagator uses staging: first it uses naive value-based
   * propagation and only then uses bounds consistent propagation.
   * Due to using naive value-based propagation, the propagator
   * might actually achieve stronger consistency than just
   * bounds consistency.
   *
   * The algorithm is taken from:
   *        A. Lopez-Ortiz, C.-G. Quimper, J. Tromp, and P. van Beek.
   *        A fast and simple algorithm for bounds consistency of the
   *        alldifferent constraint. IJCAI-2003.
   *
   * This implementation uses the code that is provided by
   * Peter Van Beek:
   *   http://ai.uwaterloo.ca/~vanbeek/software/software.html
   * The code (originally by John Tromp) here has only been slightly modified
   * to fit %Gecode (taking idempotent/non-idempotent propagation into account)
   * and uses a more efficient layout of datastructures (keeping the number
   * of different arrays small).
   *
   * Requires \code #include <gecode/int/distinct.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class Bnd : public Propagator {
  protected:
    /// Views on which to perform bounds-propagation
    ViewArray<View> x;
    /// Views on which to perform value-propagation (subset of \c x)
    ViewArray<View> y;
    /// Minimum (approximation) of view in \a x
    int min_x;
    /// Maximum (approximation) of view in \a x
    int max_x;
    /// Constructor for posting
    Bnd(Home home, ViewArray<View>& x);
    /// Constructor for cloning \a p
    Bnd(Space& home, bool share, Bnd<View>& p);
  public:
    /// Post propagator for view array \a x
    static ExecStatus post(Home home, ViewArray<View>& x);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Cost function
     *
     * If in stage for naive value propagation, the cost is
     * low linear. Otherwise it is low quadratic.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Destructor
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Perform bounds consistent distinct propagation
   *
   * This is actually the propagation algorithm for Distinct::Bnd.
   * It is available as separate function as it is reused for
   * both bounds consistent and domain consistent distinct
   * propagators.
   */
  template<class View>
  ExecStatus prop_bnd(Space& home, ViewArray<View>& x, int& min_x, int& max_x);

  /**
   * \brief Perform bounds consistent distinct propagation
   *
   * This is actually the propagation algorithm for Distinct::Bnd.
   * It is available as separate function as it is reused for
   * both bounds consistent and domain consistent distinct
   * propagators.
   */
  template<class View>
  ExecStatus prop_bnd(Space& home, ViewArray<View>& x);


  /// View-value graph for propagation
  template<class View>
  class Graph : public ViewValGraph::Graph<View> {
  public:
    using ViewValGraph::Graph<View>::view;
    using ViewValGraph::Graph<View>::n_view;
    using ViewValGraph::Graph<View>::val;
    using ViewValGraph::Graph<View>::n_val;
    using ViewValGraph::Graph<View>::count;
    using ViewValGraph::Graph<View>::scc;
    using ViewValGraph::Graph<View>::match;
    /// Construct graph as not yet initialized
    Graph(void);
    /// Initialize graph
    ExecStatus init(Space& home, ViewArray<View>& x);
    /// Mark edges in graph, return true if pruning is at all possible
    bool mark(Space& home);
    /// Prune unmarked edges, \a assigned is true if a view got assigned
    ExecStatus prune(Space& home, bool& assigned);
    /// Synchronize graph with new view domains
    bool sync(Space& home);
  };

  /**
   * \brief Propagation controller for domain consistent distinct
   *
   * The propagation controller provides convenient access to
   * performing incremental domain consistent distinct propagation
   * so that the routines can be reused easily.
   *
   * Requires \code #include <gecode/int/distinct.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class DomCtrl {
  protected:
    /// Propagation is performed on a view-value graph
    Graph<View> g;
  public:
    /// Initialize with non-initialized view-value graph
    DomCtrl(void);
    /// Check whether a view-value graph is available
    bool available(void);
    /// Initialize view-value graph for views \a x
    ExecStatus init(Space& home, ViewArray<View>& x);
    /// Synchronize available view-value graph
    ExecStatus sync(Space& home);
    /// Perform propagation, \a assigned is true if a view gets assigned
    ExecStatus propagate(Space& home, bool& assigned);
  };

  /**
   * \brief Domain consistent distinct propagator
   *
   * The propagator uses staging: first it uses naive value-based
   * propagation and only then uses domain consistent propagation.
   *
   * The algorithm is taken from:
   *    Jean-Charles Régin, A filtering algorithm for constraints
   *    of difference in CSPs, Proceedings of the Twelfth National
   *    Conference on Artificial Intelligence, pages 362--367.
   *    Seattle, WA, USA, 1994.
   *
   * Requires \code #include <gecode/int/distinct.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class Dom : public NaryPropagator<View,PC_INT_DOM> {
  protected:
    using NaryPropagator<View,PC_INT_DOM>::x;
    /// Propagation controller
    DomCtrl<View> dc;
    /// Constructor for cloning \a p
    Dom(Space& home, bool share, Dom<View>& p);
    /// Constructor for posting
    Dom(Home home, ViewArray<View>& x);
  public:
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Cost function
     *
     * If in stage for naive value propagation, the cost is
     * low linear. Otherwise it is high quadratic.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Post propagator for views \a x
    static ExecStatus post(Home home, ViewArray<View>& x);
  };

  /**
   * \brief Ternary domain consistent distinct propagator
   *
   * Requires \code #include <gecode/int/distinct.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class TerDom : public TernaryPropagator<View,PC_INT_DOM> {
  protected:
    using TernaryPropagator<View,PC_INT_DOM>::x0;
    using TernaryPropagator<View,PC_INT_DOM>::x1;
    using TernaryPropagator<View,PC_INT_DOM>::x2;

    /// Constructor for cloning \a p
    TerDom(Space& home, bool share, TerDom<View>& p);
    /// Constructor for posting
    TerDom(Home home, View x0, View x1, View x2);
  public:
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Post propagator for views \a x
    static  ExecStatus post(Home home, View x0, View x1, View x2);
  };

}}}

#include <gecode/int/distinct/val.hpp>
#include <gecode/int/distinct/bnd.hpp>
#include <gecode/int/distinct/ter-dom.hpp>
#include <gecode/int/distinct/graph.hpp>
#include <gecode/int/distinct/dom-ctrl.hpp>
#include <gecode/int/distinct/dom.hpp>

#endif

// STATISTICS: int-prop

