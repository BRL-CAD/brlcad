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

#ifndef __GECODE_FLOAT_TRANSCENDENTAL_HH__
#define __GECODE_FLOAT_TRANSCENDENTAL_HH__

#ifdef GECODE_HAS_MPFR

#include <gecode/float.hh>

/**
 * \namespace Gecode::Float::Transcendental
 * \brief %Transcendental propagators
 */

namespace Gecode { namespace Float { namespace Transcendental {

  /**
   * \brief %Propagator for bounds consistent exp operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/transcendental.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class Exp : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Exp(Space& home, bool share, Exp& p);
    /// Constructor for creation
    Exp(Home home, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$e^{x_0} = x_1\f$
    static ExecStatus post(Home home, A x0, B x1);
  };


  /**
   * \brief %Propagator for bounds consistent pow operator
   *
   * The types \a A and \a B give the types of the views.
   *
   * Requires \code #include <gecode/float/transcendental.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class A, class B>
  class Pow : public MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<A,PC_FLOAT_BND,B,PC_FLOAT_BND>::x1;
    FloatNum base;

    /// Constructor for cloning \a p
    Pow(Space& home, bool share, Pow& p);
    /// Constructor for creation
    Pow(Home home, FloatNum base, A x0, B x1);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\mathit{base}^{x_0} = x_1\f$
    static ExecStatus post(Home home, FloatNum base, A x0, B x1);
  };

}}}

#include <gecode/float/transcendental/exp-log.hpp>
#endif

#endif

// STATISTICS: float-prop
