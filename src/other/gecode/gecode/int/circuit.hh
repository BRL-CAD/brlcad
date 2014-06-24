/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
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

#ifndef __GECODE_INT_CIRCUIT_HH__
#define __GECODE_INT_CIRCUIT_HH__

#include <gecode/int.hh>
#include <gecode/int/distinct.hh>

/**
 * \namespace Gecode::Int::Circuit
 * \brief %Circuit propagators
 */

namespace Gecode { namespace Int { namespace Circuit {

  /**
   * \brief Base-class for circuit propagator
   *
   * Provides routines for checking that the induced variable value graph
   * is strongly connected and for pruning short cycles.
   *
   */
  template<class View, class Offset>
  class Base : public NaryPropagator<View,Int::PC_INT_DOM> {
  protected:
    using NaryPropagator<View,Int::PC_INT_DOM>::x;
    /// Array for performing value propagation for distinct
    ViewArray<View> y;
    /// Offset transformation
    Offset o;
    /// Constructor for cloning \a p
    Base(Space& home, bool share, Base& p);
    /// Constructor for posting
    Base(Home home, ViewArray<View>& x, Offset& o);
    /// Check whether the view value graph is strongly connected
    ExecStatus connected(Space& home);
    /// Ensure path property: prune edges that could give to small cycles
    ExecStatus path(Space& home);
  public:
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief "Value-consistent" circuit propagator
   *
   * Propagates value-consistent distinct, checks that
   * the induced variable value graph is stronlgy connected, and
   * prunes too short cycles.
   *
   * Requires \code #include <gecode/int/circuit.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, class Offset>
  class Val : public Base<View,Offset> {
  protected:
    using Base<View,Offset>::x;
    using Base<View,Offset>::y;
    using Base<View,Offset>::connected;
    using Base<View,Offset>::path;
    using Base<View,Offset>::o;
    /// Constructor for cloning \a p
    Val(Space& home, bool share, Val& p);
    /// Constructor for posting
    Val(Home home, ViewArray<View>& x, Offset& o);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost function (returns high linear)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for circuit on \a x
    static  ExecStatus post(Home home, ViewArray<View>& x, Offset& o);
  };

  /**
   * \brief "Domain consistent" circuit propagator
   *
   * Propagates domain consistent distinct, checks that
   * the induced variable value graph is stronlgy connected, and
   * prunes too shot cycles.
   *
   * Requires \code #include <gecode/int/circuit.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, class Offset>
  class Dom : public Base<View,Offset> {
  protected:
    using Base<View,Offset>::x;
    using Base<View,Offset>::y;
    using Base<View,Offset>::connected;
    using Base<View,Offset>::path;
    using Base<View,Offset>::o;
    /// Propagation controller for propagating distinct
    Int::Distinct::DomCtrl<View> dc;
    /// Constructor for cloning \a p
    Dom(Space& home, bool share, Dom& p);
    /// Constructor for posting
    Dom(Home home, ViewArray<View>& x, Offset& o);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If in stage for naive value propagation, the cost is
     * low linear. Otherwise it is high quadratic.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for circuit on \a x
    static  ExecStatus post(Home home, ViewArray<View>& x, Offset& o);
  };

}}}

#include <gecode/int/circuit/base.hpp>
#include <gecode/int/circuit/val.hpp>
#include <gecode/int/circuit/dom.hpp>

#endif

// STATISTICS: int-prop
