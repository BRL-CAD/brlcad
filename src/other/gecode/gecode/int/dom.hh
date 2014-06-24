/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
 *     Guido Tack, 2004
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

#ifndef __GECODE_INT_DOM_HH__
#define __GECODE_INT_DOM_HH__

#include <gecode/int.hh>
#include <gecode/int/rel.hh>

/**
 * \namespace Gecode::Int::Dom
 * \brief Domain propagators
 */

namespace Gecode { namespace Int { namespace Dom {

  /**
   * \brief Reified range dom-propagator
   *
   * Requires \code #include <gecode/int/dom.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, ReifyMode rm>
  class ReRange : public ReUnaryPropagator<View,PC_INT_BND,BoolView> {
  protected:
    using ReUnaryPropagator<View,PC_INT_BND,BoolView>::x0;
    using ReUnaryPropagator<View,PC_INT_BND,BoolView>::b;
    /// Minimum of range
    int min;
    /// Maximum of range
    int max;
    /// Constructor for cloning \a p
    ReRange(Space& home, bool share, ReRange& p);
    /// Constructor for creation
    ReRange(Home home, View x, int min, int max, BoolView b);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ (l\leq x \leq m) \Leftrightarrow b\f$
    static ExecStatus post(Home home, View x, int min, int max, BoolView b);
  };

  /**
   * \brief Reified domain dom-propagator
   *
   * Requires \code #include <gecode/int/dom.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, ReifyMode rm>
  class ReIntSet : public ReUnaryPropagator<View,PC_INT_DOM,BoolView> {
  protected:
    using ReUnaryPropagator<View,PC_INT_DOM,BoolView>::x0;
    using ReUnaryPropagator<View,PC_INT_DOM,BoolView>::b;

    /// %Domain
    IntSet is;
    /// Constructor for cloning \a p
    ReIntSet(Space& home, bool share, ReIntSet& p);
    /// Constructor for creation
    ReIntSet(Home home, View x, const IntSet& s, BoolView b);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ (x \in d) \Leftrightarrow b\f$
    static ExecStatus post(Home home, View x, const IntSet& s, BoolView b);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

}}}

#include <gecode/int/dom/range.hpp>
#include <gecode/int/dom/set.hpp>

#endif

// STATISTICS: int-prop

