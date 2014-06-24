/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

#ifndef __GECODE_INT_ARITHMETIC_HH__
#define __GECODE_INT_ARITHMETIC_HH__

#include <gecode/int.hh>

#include <gecode/int/rel.hh>
#include <gecode/int/linear.hh>

/**
 * \namespace Gecode::Int::Arithmetic
 * \brief Numerical (arithmetic) propagators
 */

namespace Gecode { namespace Int { namespace Arithmetic {

  /**
   * \brief Bounds consistent absolute value propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class AbsBnd : public BinaryPropagator<View,PC_INT_BND> {
  protected:
    using BinaryPropagator<View,PC_INT_BND>::x0;
    using BinaryPropagator<View,PC_INT_BND>::x1;

    /// Constructor for cloning \a p
    AbsBnd(Space& home, bool share, AbsBnd& p);
    /// Constructor for posting
    AbsBnd(Home home, View x0, View x1);
  public:

    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If a view has been assigned, the cost is low unary.
     * Otherwise it is low binary.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post bounds consistent propagator \f$ |x_0|=x_1\f$
    static  ExecStatus  post(Home home, View x0, View x1);
  };

  /**
   * \brief Domain consistent absolute value propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class AbsDom : public BinaryPropagator<View,PC_INT_DOM> {
  protected:
    using BinaryPropagator<View,PC_INT_DOM>::x0;
    using BinaryPropagator<View,PC_INT_DOM>::x1;

    /// Constructor for cloning \a p
    AbsDom(Space& home, bool share, AbsDom& p);
    /// Constructor for posting
    AbsDom(Home home, View x0, View x1);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If a view has been assigned, the cost is low binary.
     * If in stage for bounds propagation, the cost is
     * low binary. Otherwise it is high binary.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post domain consistent propagator \f$ |x_0|=x_1\f$
    static  ExecStatus  post(Home home, View x0, View x1);
  };

}}}

#include <gecode/int/arithmetic/abs.hpp>

namespace Gecode { namespace Int { namespace Arithmetic {

  /**
   * \brief Bounds consistent ternary maximum propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class MaxBnd : public TernaryPropagator<View,PC_INT_BND> {
  protected:
    using TernaryPropagator<View,PC_INT_BND>::x0;
    using TernaryPropagator<View,PC_INT_BND>::x1;
    using TernaryPropagator<View,PC_INT_BND>::x2;

    /// Constructor for cloning \a p
    MaxBnd(Space& home, bool share, MaxBnd& p);
    /// Constructor for posting
    MaxBnd(Home home, View x0, View x1, View x2);
  public:
    /// Constructor for rewriting \a p during cloning
    MaxBnd(Space& home, bool share, Propagator& p, View x0, View x1, View x2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ \max\{x_0,x_1\}=x_2\f$
    static  ExecStatus post(Home home, View x0, View x1, View x2);
  };

  /**
   * \brief Bounds consistent n-ary maximum propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class NaryMaxBnd : public NaryOnePropagator<View,PC_INT_BND> {
  protected:
    using NaryOnePropagator<View,PC_INT_BND>::x;
    using NaryOnePropagator<View,PC_INT_BND>::y;

    /// Constructor for cloning \a p
    NaryMaxBnd(Space& home, bool share, NaryMaxBnd& p);
    /// Constructor for posting
    NaryMaxBnd(Home home, ViewArray<View>& x, View y);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ \max x=y\f$
    static  ExecStatus post(Home home, ViewArray<View>& x, View y);
  };

  /**
   * \brief Domain consistent ternary maximum propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class MaxDom : public TernaryPropagator<View,PC_INT_DOM> {
  protected:
    using TernaryPropagator<View,PC_INT_DOM>::x0;
    using TernaryPropagator<View,PC_INT_DOM>::x1;
    using TernaryPropagator<View,PC_INT_DOM>::x2;

    /// Constructor for cloning \a p
    MaxDom(Space& home, bool share, MaxDom& p);
    /// Constructor for posting
    MaxDom(Home home, View x0, View x1, View x2);
  public:
    /// Constructor for rewriting \a p during cloning
    MaxDom(Space& home, bool share, Propagator& p, View x0, View x1, View x2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If in stage for bounds propagation, the cost is
     * low ternary. Otherwise it is high ternary.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ \max\{x_0,x_1\}=x_2\f$
    static  ExecStatus post(Home home, View x0, View x1, View x2);
  };

  /**
   * \brief Domain consistent n-ary maximum propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class NaryMaxDom : public NaryOnePropagator<View,PC_INT_DOM> {
  protected:
    using NaryOnePropagator<View,PC_INT_DOM>::x;
    using NaryOnePropagator<View,PC_INT_DOM>::y;

    /// Constructor for cloning \a p
    NaryMaxDom(Space& home, bool share, NaryMaxDom& p);
    /// Constructor for posting
    NaryMaxDom(Home home, ViewArray<View>& x, View y);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If in stage for bounds propagation, the cost is
     * low linear. Otherwise it is high linear.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ \max x=y\f$
    static  ExecStatus post(Home home, ViewArray<View>& x, View y);
  };

}}}

#include <gecode/int/arithmetic/max.hpp>

namespace Gecode { namespace Int { namespace Arithmetic {

  /**
   * \brief Operations for square and square-root propagators
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  class SqrOps {
  public:
    /// Return whether exponent is even
    bool even(void) const;
    /// Return exponent
    int exp(void) const;
    /// Set exponent to \a m
    void exp(int m);
    /// Return \f$x^2\f$
    template<class IntType>
    IntType pow(IntType x) const;
    /// Return \f$x^2\f$ truncated to integer limits
    int tpow(int x) const;
    /// Return \f$\lfloor \sqrt{x}\rfloor\f$ where \a x must be non-negative and \f$n>0\f$
    int fnroot(int x) const;
    /// Return \f$\lceil \sqrt{x}\rceil\f$ where \a x must be non-negative and \f$n>0\f$
    int cnroot(int x) const;
  };

  /**
   * \brief Operations for power and nroot propagators
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  class PowOps {
  protected:
    /// The exponent and root index
    int n;
    /// Return whether \a m is even
    static bool even(int m);
    /// Test whether \f$r^n>x\f$
    bool powgr(long long int r, int x) const;
    /// Test whether \f$r^n<x\f$
    bool powle(long long int r, int x) const;
  public:
    /// Initialize with exponent \a n
    PowOps(int n);
    /// Return whether exponent is even
    bool even(void) const;
    /// Return exponent
    int exp(void) const;
    /// Set exponent to \a m
    void exp(int m);
    /// Return \f$x^n\f$ where \f$n>0\f$
    template<class IntType>
    IntType pow(IntType x) const;
    /// Return \f$x^n\f$ where \f$n>0\f$ truncated to integer limits
    int tpow(int x) const;
    /// Return \f$\lfloor \sqrt[n]{x}\rfloor\f$ where \a x must be non-negative and \f$n>0\f$
    int fnroot(int x) const;
    /// Return \f$\lceil \sqrt[n]{x}\rceil\f$ where \a x must be non-negative and \f$n>0\f$
    int cnroot(int x) const;
  };

}}}

#include <gecode/int/arithmetic/pow-ops.hpp>

namespace Gecode { namespace Int { namespace Arithmetic {

  /**
   * \brief Bounds consistent positive power propagator
   *
   * This propagator is for positive views only.
   */
  template<class VA, class VB, class Ops>
  class PowPlusBnd : public MixBinaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND> {
  protected:
    using MixBinaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND>::x0;
    using MixBinaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND>::x1;
    /// Operations
    Ops ops;
    /// Constructor for posting
    PowPlusBnd(Home home, VA x0, VB x1, const Ops& ops);
    /// Constructor for cloning \a p
    PowPlusBnd(Space& home, bool share, PowPlusBnd<VA,VB,Ops>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator
    static ExecStatus post(Home home, VA x0, VB x1, Ops ops);
  };

  /**
   * \brief Bounds consistent power propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Ops>
  class PowBnd : public BinaryPropagator<IntView,PC_INT_BND> {
  protected:
    using BinaryPropagator<IntView,PC_INT_BND>::x0;
    using BinaryPropagator<IntView,PC_INT_BND>::x1;
    /// Operations
    Ops ops;
    /// Constructor for cloning \a p
    PowBnd(Space& home, bool share, PowBnd& p);
    /// Constructor for posting
    PowBnd(Home home, IntView x0, IntView x1, const Ops& ops);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator
    static ExecStatus post(Home home, IntView x0, IntView x1, Ops ops);
  };

  /**
   * \brief Domain consistent positive power propagator
   *
   * This propagator is for positive views only.
   */
  template<class VA, class VB, class Ops>
  class PowPlusDom : public MixBinaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM> {
  protected:
    using MixBinaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM>::x0;
    using MixBinaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM>::x1;
    /// Operations
    Ops ops;
    /// Constructor for posting
    PowPlusDom(Home home, VA x0, VB x1, const Ops& ops);
    /// Constructor for cloning \a p
    PowPlusDom(Space& home, bool share, PowPlusDom<VA,VB,Ops>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If a view has been assigned, the cost is low unary.
     * If in stage for bounds propagation, the cost is
     * low binary. Otherwise it is high binary.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator
    static ExecStatus post(Home home, VA x0, VB x1, Ops ops);
  };

  /**
   * \brief Domain consistent power propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Ops>
  class PowDom : public BinaryPropagator<IntView,PC_INT_DOM> {
  protected:
    using BinaryPropagator<IntView,PC_INT_DOM>::x0;
    using BinaryPropagator<IntView,PC_INT_DOM>::x1;
    /// Operations
    Ops ops;
    /// Constructor for cloning \a p
    PowDom(Space& home, bool share, PowDom<Ops>& p);
    /// Constructor for posting
    PowDom(Home home, IntView x0, IntView x1, const Ops& ops);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Cost function
     *
     * If a view has been assigned, the cost is low unary.
     * If in stage for bounds propagation, the cost is
     * low binary. Otherwise it is high binary.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Post propagator
    static ExecStatus post(Home home, IntView x0, IntView x1, Ops ops);
  };

}}}

#include <gecode/int/arithmetic/pow.hpp>

namespace Gecode { namespace Int { namespace Arithmetic {

  /**
   * \brief Positive bounds consistent n-th root propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Ops, bool minus>
  class NrootPlusBnd : public BinaryPropagator<IntView,PC_INT_BND> {
  protected:
    using BinaryPropagator<IntView,PC_INT_BND>::x0;
    using BinaryPropagator<IntView,PC_INT_BND>::x1;
    /// Operations
    Ops ops;
    /// Constructor for cloning \a p
    NrootPlusBnd(Space& home, bool share, NrootPlusBnd<Ops,minus>& p);
    /// Constructor for posting
    NrootPlusBnd(Home home, IntView x0, IntView x1, const Ops& ops);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator
    static ExecStatus post(Home home, IntView x0, IntView x1, Ops ops);
  };

  /**
   * \brief Bounds consistent n-th root propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Ops>
  class NrootBnd : public BinaryPropagator<IntView,PC_INT_BND> {
  protected:
    using BinaryPropagator<IntView,PC_INT_BND>::x0;
    using BinaryPropagator<IntView,PC_INT_BND>::x1;
    /// Operations
    Ops ops;
    /// Constructor for cloning \a p
    NrootBnd(Space& home, bool share, NrootBnd<Ops>& p);
    /// Constructor for posting
    NrootBnd(Home home, IntView x0, IntView x1, const Ops& ops);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator
    static ExecStatus post(Home home, IntView x0, IntView x1, Ops ops);
  };

  /**
   * \brief Domain consistent n-th root propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Ops, bool minus>
  class NrootPlusDom : public BinaryPropagator<IntView,PC_INT_DOM> {
  protected:
    using BinaryPropagator<IntView,PC_INT_DOM>::x0;
    using BinaryPropagator<IntView,PC_INT_DOM>::x1;
    /// Operations
    Ops ops;
    /// Constructor for cloning \a p
    NrootPlusDom(Space& home, bool share, NrootPlusDom<Ops,minus>& p);
    /// Constructor for posting
    NrootPlusDom(Home home, IntView x0, IntView x1, const Ops& ops);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Cost function
     *
     * If a view has been assigned, the cost is low unary.
     * If in stage for bounds propagation, the cost is
     * low binary. Otherwise it is high binary.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Post propagator
    static ExecStatus post(Home home, IntView x0, IntView x1, Ops ops);
  };

  /**
   * \brief Domain consistent n-th root propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Ops>
  class NrootDom : public BinaryPropagator<IntView,PC_INT_DOM> {
  protected:
    using BinaryPropagator<IntView,PC_INT_DOM>::x0;
    using BinaryPropagator<IntView,PC_INT_DOM>::x1;
    /// Operations
    Ops ops;
    /// Constructor for cloning \a p
    NrootDom(Space& home, bool share, NrootDom<Ops>& p);
    /// Constructor for posting
    NrootDom(Home home, IntView x0, IntView x1, const Ops& ops);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Cost function
     *
     * If a view has been assigned, the cost is low unary.
     * If in stage for bounds propagation, the cost is
     * low binary. Otherwise it is high binary.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Post propagator
    static ExecStatus post(Home home, IntView x0, IntView x1, Ops ops);
  };

}}}

#include <gecode/int/arithmetic/nroot.hpp>

namespace Gecode { namespace Int { namespace Arithmetic {

  /**
   * \brief Bounds or domain consistent propagator for \f$x_0\times x_1=x_0\f$
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, PropCond pc>
  class MultZeroOne : public BinaryPropagator<View,pc> {
  protected:
    using BinaryPropagator<View,pc>::x0;
    using BinaryPropagator<View,pc>::x1;

    /// Constructor for cloning \a p
    MultZeroOne(Space& home, bool share, MultZeroOne<View,pc>& p);
    /// Constructor for posting
    MultZeroOne(Home home, View x0, View x1);
    /// Test whether \a x is equal to \a n
    static RelTest equal(View x, int n);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$x_0\cdot x_1=x_0\f$
    static ExecStatus post(Home home, View x0, View x1);
  };



  /**
   * \brief Bounds consistent positive multiplication propagator
   *
   * This propagator provides multiplication for positive views only.
   */
  template<class VA, class VB, class VC>
  class MultPlusBnd :
    public MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND> {
  protected:
    using MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>::x0;
    using MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>::x1;
    using MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>::x2;
  public:
    /// Constructor for posting
    MultPlusBnd(Home home, VA x0, VB x1, VC x2);
    /// Constructor for cloning \a p
    MultPlusBnd(Space& home, bool share, MultPlusBnd<VA,VB,VC>& p);
    /// Post propagator \f$x_0\cdot x_1=x_2\f$
    static ExecStatus post(Home home, VA x0, VB x1, VC x2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };

  /**
   * \brief Bounds consistent multiplication propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   *
   * \ingroup FuncIntProp
   */
  class MultBnd : public TernaryPropagator<IntView,PC_INT_BND> {
  protected:
    using TernaryPropagator<IntView,PC_INT_BND>::x0;
    using TernaryPropagator<IntView,PC_INT_BND>::x1;
    using TernaryPropagator<IntView,PC_INT_BND>::x2;
    /// Constructor for cloning \a p
    MultBnd(Space& home, bool share, MultBnd& p);
  public:
    /// Constructor for posting
    MultBnd(Home home, IntView x0, IntView x1, IntView x2);
    /// Post propagator \f$x_0\cdot x_1=x_2\f$
    GECODE_INT_EXPORT
    static ExecStatus post(Home home, IntView x0, IntView x1, IntView x2);
    /// Copy propagator during cloning
    GECODE_INT_EXPORT
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    GECODE_INT_EXPORT 
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };



  /**
   * \brief Domain consistent positive multiplication propagator
   *
   * This propagator provides multiplication for positive views only.
   */
  template<class VA, class VB, class VC>
  class MultPlusDom :
    public MixTernaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM,VC,PC_INT_DOM> {
  protected:
    using MixTernaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM,VC,PC_INT_DOM>::x0;
    using MixTernaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM,VC,PC_INT_DOM>::x1;
    using MixTernaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM,VC,PC_INT_DOM>::x2;
  public:
    /// Constructor for posting
    MultPlusDom(Home home, VA x0, VB x1, VC x2);
    /// Constructor for cloning \a p
    MultPlusDom(Space& home, bool share, MultPlusDom<VA,VB,VC>& p);
    /// Post propagator \f$x_0\cdot x_1=x_2\f$
    static ExecStatus post(Home home, VA x0, VB x1, VC x2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If in stage for bounds propagation, the cost is
     * low ternary. Otherwise it is high ternary.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };

  /**
   * \brief Domain consistent multiplication propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   *
   * \ingroup FuncIntProp
   */
  class MultDom : public TernaryPropagator<IntView,PC_INT_DOM> {
  protected:
    using TernaryPropagator<IntView,PC_INT_DOM>::x0;
    using TernaryPropagator<IntView,PC_INT_DOM>::x1;
    using TernaryPropagator<IntView,PC_INT_DOM>::x2;
    /// Constructor for cloning \a p
    MultDom(Space& home, bool share, MultDom& p);
  public:
    /// Constructor for posting
    MultDom(Home home, IntView x0, IntView x1, IntView x2);
    /// Post propagator \f$x_0\cdot x_1=x_2\f$
    GECODE_INT_EXPORT 
    static ExecStatus post(Home home, IntView x0, IntView x1, IntView x2);
    /// Copy propagator during cloning
    GECODE_INT_EXPORT 
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If in stage for bounds propagation, the cost is
     * low ternary. Otherwise it is high ternary.
     */
    GECODE_INT_EXPORT 
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    GECODE_INT_EXPORT 
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };

}}}

#include <gecode/int/arithmetic/mult.hpp>

namespace Gecode { namespace Int { namespace Arithmetic {

  /**
   * \brief Bounds consistent positive division propagator
   *
   * This propagator provides division for positive views only.
   */
  template<class VA, class VB, class VC>
  class DivPlusBnd :
    public MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND> {
  protected:
    using MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>::x0;
    using MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>::x1;
    using MixTernaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND,VC,PC_INT_BND>::x2;
  public:
    /// Constructor for posting
    DivPlusBnd(Home home, VA x0, VB x1, VC x2);
    /// Constructor for cloning \a p
    DivPlusBnd(Space& home, bool share, DivPlusBnd<VA,VB,VC>& p);
    /// Post propagator \f$x_0\mathrm{div} x_1=x_2\f$ (rounding towards 0)
    static ExecStatus post(Home home, VA x0, VB x1, VC x2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };

  /**
   * \brief Bounds consistent division propagator
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   *
   * \ingroup FuncIntProp
   */
  template<class View>
  class DivBnd : public TernaryPropagator<View,PC_INT_BND> {
  protected:
    using TernaryPropagator<View,PC_INT_BND>::x0;
    using TernaryPropagator<View,PC_INT_BND>::x1;
    using TernaryPropagator<View,PC_INT_BND>::x2;

    /// Constructor for cloning \a p
    DivBnd(Space& home, bool share, DivBnd<View>& p);
  public:
    /// Constructor for posting
    DivBnd(Home home, View x0, View x1, View x2);
    /// Post propagator \f$x_0\mathrm{div} x_1=x_2\f$ (rounding towards 0)
    static  ExecStatus post(Home home, View x0, View x1, View x2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };

  /**
   * \brief Integer division/modulo propagator
   *
   * This propagator implements the relation between divisor and
   * modulo of an integer division.
   *
   * Requires \code #include <gecode/int/arithmetic.hh> \endcode
   *
   * \ingroup FuncIntProp
   */
  template<class View>
  class DivMod : public TernaryPropagator<View,PC_INT_BND> {
  protected:
    using TernaryPropagator<View,PC_INT_BND>::x0;
    using TernaryPropagator<View,PC_INT_BND>::x1;
    using TernaryPropagator<View,PC_INT_BND>::x2;

    /// Constructor for cloning \a p
    DivMod(Space& home, bool share, DivMod<View>& p);
  public:
    /// Constructor for posting
    DivMod(Home home, View x0, View x1, View x2);
    /// Post propagator \f$x_1\neq 0 \land (x_2\neq 0\Rightarrow x_0\times x_2>0) \land \mathrm{abs}(x_2)<\mathrm{abs}(x_1)\f$
    static  ExecStatus post(Home home, View x0, View x1, View x2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };

}}}

#include <gecode/int/arithmetic/divmod.hpp>

#endif

// STATISTICS: int-prop

