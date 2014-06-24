/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Guido Tack, 2004
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

#ifndef __GECODE_FLOAT_ARITHMETIC_HH__
#define __GECODE_FLOAT_ARITHMETIC_HH__

#include <gecode/int.hh>
#include <gecode/float.hh>
#include <gecode/float/rel.hh>

/**
 * \namespace Gecode::Float::Arithmetic
 * \brief %Arithmetic propagators
 */

namespace Gecode { namespace Float { namespace Arithmetic {

  /**
   * \brief Bounds consistent positive square propagator
   *
   * This propagator provides multiplication for positive views only.
   */
  template<class VA, class VB>
  class SqrPlus : public MixBinaryPropagator<VA,PC_FLOAT_BND,VB,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<VA,PC_FLOAT_BND,VB,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<VA,PC_FLOAT_BND,VB,PC_FLOAT_BND>::x1;
    /// Constructor for posting
    SqrPlus(Home home, VA x0, VB x1);
    /// Constructor for cloning \a p
    SqrPlus(Space& home, bool share, SqrPlus<VA,VB>& p);
  public:
    /// Post propagator \f$x_0\cdot x_0=x_1\f$
    static ExecStatus post(Home home, VA x0, VB x1);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };

  /**
   * \brief %Propagator for bounds consistent square operator
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class View>
  class Sqr : public BinaryPropagator<View,PC_FLOAT_BND> {
  protected:
    using BinaryPropagator<View,PC_FLOAT_BND>::x0;
    using BinaryPropagator<View,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Sqr(Space& home, bool share, Sqr& p);
    /// Constructor for creation
    Sqr(Home home, View x0, View x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$x_0^2 = x_1\f$
    static ExecStatus post(Home home, View x0, View x1);
  };

  /**
   * \brief %Propagator for bounds consistent square root operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class Sqrt : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Sqrt(Space& home, bool share, Sqrt& p);
    /// Constructor for creation
    Sqrt(Home home, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$x_0^2 = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };

  /**
   * \brief %Propagator for bounds consistent absolute operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class Abs : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Abs(Space& home, bool share, Abs& p);
    /// Constructor for creation
    Abs(Home home, A x0, B x1);
  public:
    /// Constructor for rewriting \a p during cloning
    Abs(Space& home, bool share, Propagator& p, A x0, B x1);
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ |x_0| = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };

  /**
   * \brief %Propagator for bounds consistent pow operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class Pow : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;
    int m_n;

    /// Constructor for cloning \a p
    Pow(Space& home, bool share, Pow& p);
    /// Constructor for creation
    Pow(Home home, A x0, B x1, int n);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$x_0^n = x_1\f$
    static ExecStatus post(Home home, A x0, B x1, int n);
  };

  /**
   * \brief %Propagator for bounds consistent nth root operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class NthRoot : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;
    int m_n;

    /// Constructor for cloning \a p
    NthRoot(Space& home, bool share, NthRoot& p);
    /// Constructor for creation
    NthRoot(Home home, A x0, B x1, int n);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$x_0^(1/n) = x_1\f$
    static ExecStatus post(Home home, A x0, B x1, int n);
  };

  /**
   * \brief Bounds or domain consistent propagator for \f$x_0\times x_1=x_0\f$
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class View>
  class MultZeroOne : public BinaryPropagator<View,PC_FLOAT_BND> {
  protected:
    using BinaryPropagator<View,PC_FLOAT_BND>::x0;
    using BinaryPropagator<View,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    MultZeroOne(Space& home, bool share, MultZeroOne<View>& p);
    /// Constructor for posting
    MultZeroOne(Home home, View x0, View x1);
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
  class MultPlus :
    public MixTernaryPropagator<VA,PC_FLOAT_BND,VB,PC_FLOAT_BND,VC,PC_FLOAT_BND> {
  protected:
    using MixTernaryPropagator<VA,PC_FLOAT_BND,VB,PC_FLOAT_BND,VC,PC_FLOAT_BND>::x0;
    using MixTernaryPropagator<VA,PC_FLOAT_BND,VB,PC_FLOAT_BND,VC,PC_FLOAT_BND>::x1;
    using MixTernaryPropagator<VA,PC_FLOAT_BND,VB,PC_FLOAT_BND,VC,PC_FLOAT_BND>::x2;
  public:
    /// Constructor for posting
    MultPlus(Home home, VA x0, VB x1, VC x2);
    /// Constructor for cloning \a p
    MultPlus(Space& home, bool share, MultPlus<VA,VB,VC>& p);
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
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   *
   * \ingroup FuncFloatProp
   */
  template<class View>
  class Mult : public TernaryPropagator<View,PC_FLOAT_BND> {
  protected:
    using TernaryPropagator<View,PC_FLOAT_BND>::x0;
    using TernaryPropagator<View,PC_FLOAT_BND>::x1;
    using TernaryPropagator<View,PC_FLOAT_BND>::x2;

    /// Constructor for cloning \a p
    Mult(Space& home, bool share, Mult<View>& p);
  public:
    /// Constructor for posting
    Mult(Home home, View x0, View x1, View x2);
    /// Post propagator \f$x_0\cdot x_1=x_2\f$
    static  ExecStatus post(Home home, View x0, View x1, View x2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };


  /**
   * \brief %Propagator for bounds multiplication operator
   *
   * The types \a A, \a B and \a C give the types of the views.
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  /*
  template<class A, class B, class C>
  class Mult : public MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND> {
  protected:
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x0;
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x1;
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x2;
    /// Constructor for cloning \a p
    Mult(Space& home, bool share, Mult& p);
    /// Constructor for creation
    Mult(Home home, A x0, B x1, C x2);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ x_0 * x_1 = x_2\f$
    static ExecStatus post(Home home, A x0, B x1, C x2);
  };
  */

  /**
   * \brief %Propagator for bounds division operator
   *
   * The types \a A, \a B and \a C give the types of the views.
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B, class C>
  class Div : public MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND> {
  protected:
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x0;
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x1;
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x2;
    /// Constructor for cloning \a p
    Div(Space& home, bool share, Div& p);
    /// Constructor for creation
    Div(Home home, A x0, B x1, C x2);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ x_0 / x_1 = x_2\f$
    static ExecStatus post(Home home, A x0, B x1, C x2);
  };

  /**
   * \brief %Propagator for bounds consistent min operator
   *
   * The types \a A, \a B and \a C give the types of the views.
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B, class C>
  class Min : public MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND> {
  protected:
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x0;
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x1;
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x2;
    /// Constructor for cloning \a p
    Min(Space& home, bool share, Min& p);
    /// Constructor for creation
    Min(Home home, A x0, B x1, C x2);
  public:
    /// Constructor for rewriting \a p during cloning
    Min(Space& home, bool share, Propagator& p, A x0, B x1, C x2);
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ min(x_0, x_1) = x_2\f$
    static ExecStatus post(Home home, A x0, B x1, C x2);
  };

  /**
   * \brief %Propagator for bounds consistent max operator
   *
   * The types \a A, \a B and \a C give the types of the views.
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B, class C>
  class Max : public MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND> {
  protected:
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x0;
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x1;
    using MixTernaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND,C,PC_FLOAT_BND>::x2;
    /// Constructor for cloning \a p
    Max(Space& home, bool share, Max& p);
    /// Constructor for creation
    Max(Home home, A x0, B x1, C x2);
  public:
    /// Constructor for rewriting \a p during cloning
    Max(Space& home, bool share, Propagator& p, A x0, B x1, C x2);
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ max(x_0, x_1) = x_2\f$
    static ExecStatus post(Home home, A x0, B x1, C x2);
  };

  /**
   * \brief Bounds consistent n-ary maximum propagator
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class View>
  class NaryMax : public NaryOnePropagator<View,PC_FLOAT_BND> {
  protected:
    using NaryOnePropagator<View,PC_FLOAT_BND>::x;
    using NaryOnePropagator<View,PC_FLOAT_BND>::y;

    /// Constructor for cloning \a p
    NaryMax(Space& home, bool share, NaryMax& p);
    /// Constructor for posting
    NaryMax(Home home, ViewArray<View>& x, View y);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ \max x=y\f$
    static  ExecStatus post(Home home, ViewArray<View>& x, View y);
  };

  /**
   * \brief %Propagator for bounds consistent integer part operator
   *
   * Requires \code #include <gecode/float/arithmetic.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class Channel : 
    public MixBinaryPropagator<A,PC_FLOAT_BND,B,Gecode::Int::PC_INT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,Gecode::Int::PC_INT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,Gecode::Int::PC_INT_BND>::x1;

    /// Constructor for cloning \a p
    Channel(Space& home, bool share, Channel& p);
    /// Constructor for creation
    Channel(Home home, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ int(x_0) = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };

}}}

#include <gecode/float/arithmetic/sqr-sqrt-abs.hpp>
#include <gecode/float/arithmetic/pow-nroot.hpp>
#include <gecode/float/arithmetic/mult.hpp>
#include <gecode/float/arithmetic/div.hpp>
#include <gecode/float/arithmetic/min-max-channel.hpp>

#endif

// STATISTICS: float-prop
