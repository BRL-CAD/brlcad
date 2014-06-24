/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
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

#ifndef __GECODE_FLOAT_TRIGONOMETRIC_HH__
#define __GECODE_FLOAT_TRIGONOMETRIC_HH__
#ifdef GECODE_HAS_MPFR

#include <gecode/float.hh>

/**
 * \namespace Gecode::Float::Trigonometric
 * \brief %Trigonometric propagators
 */

namespace Gecode { namespace Float { namespace Trigonometric {

  /**
   * \brief %Propagator for bounds consistent sinus operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/trigonometric.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class Sin : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Sin(Space& home, bool share, Sin& p);
    /// Constructor for creation
    Sin(Home home, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$sin(x_0) = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };


  /**
   * \brief %Propagator for bounds consistent cosinus operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/trigonometric.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class Cos : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Cos(Space& home, bool share, Cos& p);
    /// Constructor for creation
    Cos(Home home, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$cos(x_0) = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };

  /**
   * \brief %Propagator for bounds consistent arc sinus operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/trigonometric.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class ASin : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    ASin(Space& home, bool share, ASin& p);
    /// Constructor for creation
    ASin(Home home, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$asin(x_0) = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };


  /**
   * \brief %Propagator for bounds consistent arc cosinus operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/trigonometric.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class ACos : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    ACos(Space& home, bool share, ACos& p);
    /// Constructor for creation
    ACos(Home home, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$acos(x_0) = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };

  /**
   * \brief %Propagator for bounds consistent tangent operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/trigonometric.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class Tan : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Tan(Space& home, bool share, Tan& p);
    /// Constructor for creation
    Tan(Home home, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$tan(x_0) = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };

  /**
   * \brief %Propagator for bounds consistent arc tangent operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/trigonometric.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class ATan : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    ATan(Space& home, bool share, ATan& p);
    /// Constructor for creation
    ATan(Home home, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$atan(x_0) = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };
}}}

#include <gecode/float/trigonometric/sincos.hpp>
#include <gecode/float/trigonometric/asinacos.hpp>
#include <gecode/float/trigonometric/tanatan.hpp>
#endif
#endif

// STATISTICS: float-prop
