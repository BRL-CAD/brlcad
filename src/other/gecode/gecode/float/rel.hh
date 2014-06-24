/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Guido Tack, 2004
 *     Gabor Szokoli, 2003
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

#ifndef __GECODE_FLOAT_REL_HH__
#define __GECODE_FLOAT_REL_HH__

#include <gecode/int.hh>
#include <gecode/float.hh>

/**
 * \namespace Gecode::Float::Rel
 * \brief Simple relation propagators
 */
namespace Gecode { namespace Float { namespace Rel {

  /*
   * Equality propagators
   *
   */

  /**
   * \brief Binary bounds consistent equality propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class View0, class View1>
  class Eq :
    public MixBinaryPropagator<View0,PC_FLOAT_BND,View1,PC_FLOAT_BND> {
  protected:
    using MixBinaryPropagator<View0,PC_FLOAT_BND,View1,PC_FLOAT_BND>::x0;
    using MixBinaryPropagator<View0,PC_FLOAT_BND,View1,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Eq(Space& home, bool share, Eq<View0,View1>& p);
  public:
    /// Constructor for posting
    Eq(Home home, View0 x0, View1 x1);
    /// Constructor for rewriting \a p during cloning
    Eq(Space& home, bool share, Propagator& p, View0 x0, View1 x1);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post bounds consistent propagator \f$ x_0 = x_1\f$
    static  ExecStatus post(Home home, View0 x0, View1 x1);
  };

  /**
   * \brief n-ary bounds consistent equality propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class View>
  class NaryEq : public NaryPropagator<View,PC_FLOAT_BND> {
  protected:
    using NaryPropagator<View,PC_FLOAT_BND>::x;

    /// Constructor for cloning \a p
    NaryEq(Space& home, bool share, NaryEq<View>& p);
    /// Constructor for posting
    NaryEq(Home home, ViewArray<View>&);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If a view has been assigned, the cost is low unary.
     * Otherwise it is low linear.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post bounds consistent propagator \f$ x_0 = x_1=\ldots =x_{|x|-1}\f$
    static  ExecStatus post(Home home, ViewArray<View>& x);
  };

  /**
   * \brief Reified binary bounds consistent equality propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class View, class CtrlView, ReifyMode rm>
  class ReEq : public Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView> {
  protected:
    using Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>::x0;
    using Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>::x1;
    using Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>::b;

    /// Constructor for cloning \a p
    ReEq(Space& home, bool share, ReEq& p);
    /// Constructor for posting
    ReEq(Home home, View x0, View x1, CtrlView b);
  public:
    /// Copy propagator during cloning
    virtual Actor*     copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post bounds consistent propagator \f$ (x_0 = x_1)\Leftrightarrow b\f$
    static  ExecStatus post(Home home, View x0, View x1, CtrlView b);
  };

  /**
   * \brief Reified bounds consistent equality with float propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class View, class CtrlView, ReifyMode rm>
  class ReEqFloat : public Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView> {
  protected:
    using Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>::x0;
    using Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>::b;

    /// Float constant to check
    FloatVal c;
    /// Constructor for cloning \a p
    ReEqFloat(Space& home, bool share, ReEqFloat& p);
    /// Constructor for posting
    ReEqFloat(Home home, View x, FloatVal c, CtrlView b);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post bounds consistent propagator \f$ (x = c)\Leftrightarrow b\f$
    static  ExecStatus post(Home home, View x, FloatVal c, CtrlView b);
  };


  /**
   * \brief Binary bounds consistent disequality propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class View0, class View1>
  class Nq :
    public MixBinaryPropagator<View0,PC_FLOAT_VAL,View1,PC_FLOAT_VAL> {
  protected:
    using MixBinaryPropagator<View0,PC_FLOAT_VAL,View1,PC_FLOAT_VAL>::x0;
    using MixBinaryPropagator<View0,PC_FLOAT_VAL,View1,PC_FLOAT_VAL>::x1;

    /// Constructor for cloning \a p
    Nq(Space& home, bool share, Nq<View0,View1>& p);
  public:
    /// Constructor for posting
    Nq(Home home, View0 x0, View1 x1);
    /// Constructor for rewriting \a p during cloning
    Nq(Space& home, bool share, Propagator& p, View0 x0, View1 x1);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post bounds consistent propagator \f$ x_0 \neq x_1\f$
    static  ExecStatus post(Home home, View0 x0, View1 x1);
  };

  /**
   * \brief Binary bounds consistent disequality propagator with float value
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */
  template<class View>
  class NqFloat :
    public UnaryPropagator<View,PC_FLOAT_VAL> {
  protected:
    using UnaryPropagator<View,PC_FLOAT_VAL>::x0;

    /// Float constant to check
    FloatVal c;
    /// Constructor for cloning \a p
    NqFloat(Space& home, bool share, NqFloat<View>& p);
  public:
    /// Constructor for posting
    NqFloat(Home home, View x, FloatVal c);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post bounds consistent propagator \f$ x_0 \neq c\f$
    static  ExecStatus post(Home home, View x0, FloatVal c);
  };


  /*
   * Order propagators
   *
   */

  /**
   * \brief Less or equal propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */

  template<class View>
  class Lq : public BinaryPropagator<View,PC_FLOAT_BND> {
  protected:
    using BinaryPropagator<View,PC_FLOAT_BND>::x0;
    using BinaryPropagator<View,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Lq(Space& home, bool share, Lq& p);
    /// Constructor for posting
    Lq(Home home, View x0, View x1);
  public:
    /// Copy propagator during cloning
    virtual Actor*     copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$x_0 \leq x_1\f$
    static  ExecStatus post(Home home, View x0, View x1);
  };

  /**
   * \brief Less propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */

  template<class View>
  class Le : public BinaryPropagator<View,PC_FLOAT_BND> {
  protected:
    using BinaryPropagator<View,PC_FLOAT_BND>::x0;
    using BinaryPropagator<View,PC_FLOAT_BND>::x1;

    /// Constructor for cloning \a p
    Le(Space& home, bool share, Le& p);
    /// Constructor for posting
    Le(Home home, View x0, View x1);
  public:
    /// Copy propagator during cloning
    virtual Actor*     copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$x_0 \le x_1\f$
    static  ExecStatus post(Home home, View x0, View x1);
  };

  /*
   * Reified order propagators
   *
   */

  /**
   * \brief Reified less or equal with float propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */

  template<class View, class CtrlView, ReifyMode rm>
  class ReLqFloat : public Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView> {
  protected:
    using Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>::x0;
    using Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>::b;

    /// Float constant to check
    FloatVal c;
    /// Constructor for cloning \a p
    ReLqFloat(Space& home, bool share, ReLqFloat& p);
    /// Constructor for posting
    ReLqFloat(Home home, View x, FloatVal c, CtrlView b);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ (x \leq c)\Leftrightarrow b\f$
    static  ExecStatus post(Home home, View x, FloatVal c, CtrlView b);
   };

  /**
   * \brief Reified less with float propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */

  template<class View, class CtrlView, ReifyMode rm>
  class ReLeFloat : public Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView> {
  protected:
    using Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>::x0;
    using Int::ReUnaryPropagator<View,PC_FLOAT_BND,CtrlView>::b;

    /// Float constant to check
    FloatVal c;
    /// Constructor for cloning \a p
    ReLeFloat(Space& home, bool share, ReLeFloat& p);
    /// Constructor for posting
    ReLeFloat(Home home, View x, FloatVal c, CtrlView b);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ (x < c)\Leftrightarrow b\f$
    static  ExecStatus post(Home home, View x, FloatVal c, CtrlView b);
   };

  /**
   * \brief Reified less or equal propagator
   *
   * Requires \code #include <gecode/float/rel.hh> \endcode
   * \ingroup FuncFloatProp
   */

  template<class View, class CtrlView, ReifyMode rm>
  class ReLq : public Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView> {
  protected:
    using Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>::x0;
    using Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>::x1;
    using Int::ReBinaryPropagator<View,PC_FLOAT_BND,CtrlView>::b;

    /// Constructor for cloning \a p
    ReLq(Space& home, bool share, ReLq& p);
    /// Constructor for posting
    ReLq(Home home, View x0, View x1, CtrlView b);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ (x_0 \leq x_1)\Leftrightarrow b\f$
    static  ExecStatus post(Home home, View x0, View x1, CtrlView b);
  };

}}}

#include <gecode/float/rel/eq.hpp>
#include <gecode/float/rel/nq.hpp>
#include <gecode/float/rel/lq-le.hpp>

#endif


// STATISTICS: float-prop

