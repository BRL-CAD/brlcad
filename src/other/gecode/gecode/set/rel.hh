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

#ifndef __GECODE_SET_REL_HH__
#define __GECODE_SET_REL_HH__

#include <gecode/set.hh>

namespace Gecode { namespace Set { namespace Rel {

  /**
   * \namespace Gecode::Set::Rel
   * \brief Standard set relation propagators
   */

  /**
   * \brief %Propagator for the subset constraint
   *
   * Requires \code #include <gecode/set/rel.hh> \endcode
   * \ingroup FuncSetProp
   */

  template<class View0, class View1>
  class Subset :
    public MixBinaryPropagator<View0,PC_SET_CGLB,View1,PC_SET_CLUB> {
  protected:
    using MixBinaryPropagator<View0,PC_SET_CGLB,View1,PC_SET_CLUB>::x0;
    using MixBinaryPropagator<View0,PC_SET_CGLB,View1,PC_SET_CLUB>::x1;
    /// Constructor for cloning \a p
    Subset(Space& home, bool share,Subset& p);
    /// Constructor for posting
    Subset(Home home,View0, View1);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x\subseteq y\f$
    static  ExecStatus post(Home home,View0 x,View1 y);
  };

  /**
   * \brief %Propagator for the negated subset constraint
   *
   * Requires \code #include <gecode/set/rel.hh> \endcode
   * \ingroup FuncSetProp
   */

  template<class View0, class View1>
  class NoSubset :
    public MixBinaryPropagator<View0,PC_SET_CLUB,View1,PC_SET_CGLB> {
  protected:
    using MixBinaryPropagator<View0,PC_SET_CLUB,View1,PC_SET_CGLB>::x0;
    using MixBinaryPropagator<View0,PC_SET_CLUB,View1,PC_SET_CGLB>::x1;
    /// Constructor for cloning \a p
    NoSubset(Space& home, bool share,NoSubset& p);
    /// Constructor for posting
    NoSubset(Home home,View0,View1);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x\subseteq y\f$
    static  ExecStatus post(Home home,View0 x,View1 y);
  };

  /**
   * \brief %Reified subset propagator
   *
   * Requires \code #include <gecode/set/rel.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0, class View1, ReifyMode rm>
  class ReSubset : public Propagator {
  protected:
    View0 x0;
    View1 x1;
    Gecode::Int::BoolView b;

    /// Constructor for cloning \a p
    ReSubset(Space& home, bool share,ReSubset&);
    /// Constructor for posting
    ReSubset(Home home,View0, View1, Gecode::Int::BoolView);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Cost function (defined as PC_TERNARY_LO)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ (x\subseteq y) \Leftrightarrow b \f$
    static ExecStatus post(Home home,View0 x, View1 y,
                           Gecode::Int::BoolView b);
  };

  /**
   * \brief %Propagator for set equality
   *
   * Requires \code #include <gecode/set/rel.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0, class View1>
  class Eq : public MixBinaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY> {
  protected:
    using MixBinaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::x0;
    using MixBinaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::x1;
    /// Constructor for cloning \a p
    Eq(Space& home, bool share,Eq& p);
    /// Constructor for posting
    Eq(Home home,View0, View1);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x=y \f$
    static  ExecStatus  post(Home home,View0,View1);
  };

  /**
   * \brief %Reified equality propagator
   *
   * Requires \code #include <gecode/set/rel.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0, class View1, class CtrlView, ReifyMode rm>
  class ReEq : public Propagator {
  protected:
    View0 x0;
    View1 x1;
    CtrlView b;

    /// Constructor for cloning \a p
    ReEq(Space& home, bool share,ReEq&);
    /// Constructor for posting
    ReEq(Home home,View0, View1, CtrlView);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Cost function (defined as PC_TERNARY_LO)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ (x=y) \Leftrightarrow b\f$
    static ExecStatus post(Home home,View0 x, View1 y,
                           CtrlView b);
  };

  /**
   * \brief %Propagator for set less than or equal
   *
   * Propagates strict inequality if \a strict is true.
   *
   * Requires \code #include <gecode/set/rel.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0, class View1, bool strict=false>
  class Lq : public MixBinaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY> {
  protected:
    using MixBinaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::x0;
    using MixBinaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::x1;
    /// Constructor for cloning \a p
    Lq(Space& home, bool share,Lq& p);
    /// Constructor for posting
    Lq(Home home,View0, View1);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x\leq y \f$
    static  ExecStatus  post(Home home,View0,View1);
  };

  /**
   * \brief %Reified propagator for set less than or equal
   *
   * Propagates strict inequality if \a strict is true.
   *
   * Requires \code #include <gecode/set/rel.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0, class View1, ReifyMode rm, bool strict=false>
  class ReLq : public Propagator {
  protected:
    View0 x0;
    View1 x1;
    Gecode::Int::BoolView b;

    /// Constructor for cloning \a p
    ReLq(Space& home, bool share,ReLq&);
    /// Constructor for posting
    ReLq(Home home,View0, View1, Gecode::Int::BoolView);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Cost function (defined as PC_TERNARY_LO)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ (x\leq y) \Leftrightarrow b\f$
    static ExecStatus post(Home home,View0 x, View1 y,
                           Gecode::Int::BoolView b);
  };

  /**
   * \brief %Propagator for negated equality
   *
   * Requires \code #include <gecode/set/rel.hh> \endcode
   * \ingroup FuncSetProp
   */

  template<class View0, class View1>
  class Distinct :
    public MixBinaryPropagator<View0,PC_SET_VAL,View1,PC_SET_VAL> {
  protected:
    using MixBinaryPropagator<View0,PC_SET_VAL,View1,PC_SET_VAL>::x0;
    using MixBinaryPropagator<View0,PC_SET_VAL,View1,PC_SET_VAL>::x1;
    /// Constructor for cloning \a p
    Distinct(Space& home, bool share,Distinct& p);
    /// Constructor for posting
    Distinct(Home home,View0,View1);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x\neq y \f$
    static  ExecStatus  post(Home home,View0,View1);
  };

  /**
   * \brief %Propagator for negated equality
   *
   * This propagator actually propagates the distinctness, after the
   * Distinct propagator waited for one variable to become
   * assigned.
   *
   * Requires \code #include <gecode/set/rel.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0>
  class DistinctDoit : public UnaryPropagator<View0,PC_SET_ANY> {
  protected:
    using UnaryPropagator<View0,PC_SET_ANY>::x0;
    /// The view that is already assigned
    ConstSetView y;
    /// Constructor for cloning \a p
    DistinctDoit(Space& home, bool share,DistinctDoit&);
    /// Constructor for posting
    DistinctDoit(Home home, View0, ConstSetView);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home, bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x\neq y \f$
    static ExecStatus post(Home home, View0, ConstSetView);
  };

}}}

#include <gecode/set/rel/common.hpp>
#include <gecode/set/rel/subset.hpp>
#include <gecode/set/rel/nosubset.hpp>
#include <gecode/set/rel/re-subset.hpp>
#include <gecode/set/rel/eq.hpp>
#include <gecode/set/rel/re-eq.hpp>
#include <gecode/set/rel/nq.hpp>
#include <gecode/set/rel/lq.hpp>
#include <gecode/set/rel/re-lq.hpp>

#endif

// STATISTICS: set-prop
