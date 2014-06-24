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

#ifndef __GECODE_INT_MEMBER_HH__
#define __GECODE_INT_MEMBER_HH__

#include <gecode/int.hh>
#include <gecode/int/val-set.hh>

/**
 * \namespace Gecode::Int::Member
 * \brief Membership propagators
 */

namespace Gecode { namespace Int { namespace Member {

  /**
   * \brief Membership propagator
   *
   * Requires \code #include <gecode/int/member.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class Prop : public NaryOnePropagator<View,PC_INT_DOM> {
  protected:
    using NaryOnePropagator<View,PC_INT_DOM>::x;
    using NaryOnePropagator<View,PC_INT_DOM>::y;
    /// Value set storing the values of already assigned views
    ValSet vs;
    /// Add values of assigned views in \a x to value set \a va
    static void add(Space& home, ValSet& vs, ViewArray<View>& x);
    /// Eliminate views from \a x that are not equal to \a y or ar subsumed by \a vs
    void eliminate(Space& home);
    /// Constructor for posting
    Prop(Home home, ValSet& vs, ViewArray<View>& x, View y);
    /// Constructor for cloning \a p
    Prop(Space& home, bool share, Prop<View>& p);
  public:
    /// Cost function
    virtual PropCost cost(const Space&, const ModEventDelta& med) const;
    /// Copy propagator during cloning
    virtual Propagator* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$y\in \{x_0,\ldots,x_{|x|-1}\}\f$
    static ExecStatus post(Home home, ViewArray<View>& x, View y);
    /// Post propagator for \f$y\in vs\cup \{x_0,\ldots,x_{|x|-1}\}\f$
    static ExecStatus post(Home home, ValSet& vs, ViewArray<View>& x, View y);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Reified membership propagator
   *
   * Requires \code #include <gecode/int/member.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, ReifyMode rm>
  class ReProp : public Prop<View> {
  protected:
    using Prop<View>::x;
    using Prop<View>::y;
    using Prop<View>::vs;
    using Prop<View>::add;
    using Prop<View>::eliminate;
    /// Boolean control variable
    BoolView b;
    /// Constructor for posting
    ReProp(Home home, ValSet& vs, ViewArray<View>& x, View y, BoolView b);
    /// Constructor for cloning \a p
    ReProp(Space& home, bool share, ReProp<View,rm>& p);
  public:
    /// Copy propagator during cloning
    virtual Propagator* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\left(y\in \{x_0,\ldots,x_{|x|-1}\}\right)\Leftrightarrow b\f$
    static ExecStatus post(Home home, ViewArray<View>& x, View y, BoolView b);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

}}}

#include <gecode/int/member/prop.hpp>
#include <gecode/int/member/re-prop.hpp>

#endif

// STATISTICS: int-prop
