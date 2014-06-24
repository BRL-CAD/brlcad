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

#ifndef __GECODE_INT_BOOL_HH__
#define __GECODE_INT_BOOL_HH__

#include <gecode/int.hh>

/**
 * \namespace Gecode::Int::Bool
 * \brief Boolean propagators
 */

namespace Gecode { namespace Int { namespace Bool {

  /*
   * Base Classes
   *
   */

  /// Base-class for binary Boolean propagators
  template<class BVA, class BVB>
  class BoolBinary : public Propagator {
  protected:
    BVA x0; ///< Boolean view
    BVB x1; ///< Boolean view
    /// Constructor for posting
    BoolBinary(Home home, BVA b0, BVB b1);
    /// Constructor for cloning
    BoolBinary(Space& home, bool share, BoolBinary& p);
    /// Constructor for rewriting \a p during cloning
    BoolBinary(Space& home, bool share, Propagator& p,
               BVA b0, BVB b1);
  public:
    /// Cost function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /// Base-class for ternary Boolean propagators
  template<class BVA, class BVB, class BVC>
  class BoolTernary : public Propagator {
  protected:
    BVA x0; ///< Boolean view
    BVB x1; ///< Boolean view
    BVC x2; ///< Boolean view
    /// Constructor for posting
    BoolTernary(Home home, BVA b0, BVB b1, BVC b2);
    /// Constructor for cloning
    BoolTernary(Space& home, bool share, BoolTernary& p);
  public:
    /// Constructor for rewriting \a p during cloning
    BoolTernary(Space& home, bool share, Propagator& p,
                BVA b0, BVB b1, BVC b2);
    /// Cost function (defined as low binary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Boolean equality propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BVA, class BVB>
  class Eq : public BoolBinary<BVA,BVB> {
  protected:
    using BoolBinary<BVA,BVB>::x0;
    using BoolBinary<BVA,BVB>::x1;
    /// Constructor for posting
    Eq(Home home, BVA b0, BVB b1);
    /// Constructor for cloning \a p
    Eq(Space& home, bool share, Eq& p);
  public:
    /// Constructor for rewriting \a p during cloning
    Eq(Space& home, bool share, Propagator& p,
       BVA b0, BVB b1);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x_0 = x_1\f$
    static  ExecStatus post(Home home, BVA x0, BVB x1);
  };


  /**
   * \brief n-ary Boolean equality propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BV>
  class NaryEq : public NaryPropagator<BV,PC_BOOL_VAL> {
  protected:
    using NaryPropagator<BV,PC_BOOL_VAL>::x;
    /// Constructor for posting
    NaryEq(Home home, ViewArray<BV>& x);
    /// Constructor for cloning \a p
    NaryEq(Space& home, bool share, NaryEq& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x_0 = x_1=\ldots =x_{|x|-1}\f$
    static  ExecStatus post(Home home, ViewArray<BV>& x);
  };


  /**
   * \brief Boolean less or equal propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BV>
  class Lq : public BoolBinary<BV,BV> {
  protected:
    using BoolBinary<BV,BV>::x0;
    using BoolBinary<BV,BV>::x1;
    /// Constructor for posting
    Lq(Home home, BV b0, BV b1);
    /// Constructor for cloning \a p
    Lq(Space& home, bool share, Lq& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ b_0 \leq b_1\f$
    static  ExecStatus post(Home home, BV b0, BV b1);
  };

  /**
   * \brief Nary Boolean less or equal propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX>
  class NaryLq : public NaryPropagator<VX,PC_BOOL_NONE> {
  protected:
    using NaryPropagator<VX,PC_BOOL_NONE>::x;
    /// Whether the propagator is currently running
    bool run;
    /// The number of views assigned to zero in \a x
    int n_zero;
    /// The number of views assigned to one in \a x
    int n_one;
    /// The advisor council
    Council<Advisor> c;
    /// Constructor for posting
    NaryLq(Home home,  ViewArray<VX>& x);
    /// Constructor for cloning \a p
    NaryLq(Space& home, bool share, NaryLq<VX>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Give advice to propagator
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    /// Cost function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x_0 \leq x_1 \leq \cdots \leq x_{|x|-1}\f$
    static  ExecStatus post(Home home, ViewArray<VX>& x);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };




  /**
   * \brief Boolean less propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BV>
  class Le {
  public:
    /// Post propagator \f$ b_0 < b_1\f$
    static  ExecStatus post(Home home, BV b0, BV b1);
  };


  /**
   * \brief Binary Boolean disjunction propagator (true)
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BVA, class BVB>
  class BinOrTrue : public BoolBinary<BVA,BVB> {
  protected:
    using BoolBinary<BVA,BVB>::x0;
    using BoolBinary<BVA,BVB>::x1;
    /// Constructor for posting
    BinOrTrue(Home home, BVA b0, BVB b1);
    /// Constructor for cloning \a p
    BinOrTrue(Space& home, bool share, BinOrTrue& p);
  public:
    /// Constructor for rewriting \a p during cloning
    BinOrTrue(Space& home, bool share, Propagator& p,
              BVA b0, BVB b1);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ b_0 \lor b_1 = 1 \f$
    static  ExecStatus post(Home home, BVA b0, BVB b1);
  };

  /**
   * \brief Ternary Boolean disjunction propagator (true)
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BV>
  class TerOrTrue : public BoolBinary<BV,BV> {
  protected:
    using BoolBinary<BV,BV>::x0;
    using BoolBinary<BV,BV>::x1;
    /// Boolean view without subscription
    BV x2;
    /// Constructor for posting
    TerOrTrue(Home home, BV b0, BV b1, BV b2);
    /// Constructor for cloning \a p
    TerOrTrue(Space& home, bool share, TerOrTrue& p);
  public:
    /// Constructor for rewriting \a p during cloning
    TerOrTrue(Space& home, bool share, Propagator& p,
              BV b0, BV b1, BV b2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ b_0 \lor b_1 \lor b_2 = 1 \f$
    static  ExecStatus post(Home home, BV b0, BV b1, BV b2);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Quarternary Boolean disjunction propagator (true)
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BV>
  class QuadOrTrue : public BoolBinary<BV,BV> {
  protected:
    using BoolBinary<BV,BV>::x0;
    using BoolBinary<BV,BV>::x1;
    /// Boolean view without subscription
    BV x2;
    /// Boolean view without subscription
    BV x3;
    /// Constructor for posting
    QuadOrTrue(Home home, BV b0, BV b1, BV b2, BV b3);
    /// Constructor for cloning \a p
    QuadOrTrue(Space& home, bool share, QuadOrTrue& p);
  public:
    /// Constructor for rewriting \a p during cloning
    QuadOrTrue(Space& home, bool share, Propagator& p,
               BV b0, BV b1, BV b2, BV b3);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ b_0 \lor b_1 \lor b_2 \lor b_3 = 1 \f$
    static  ExecStatus post(Home home, BV b0, BV b1, BV b2, BV b3);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Boolean disjunction propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BVA, class BVB, class BVC>
  class Or : public BoolTernary<BVA,BVB,BVC> {
  protected:
    using BoolTernary<BVA,BVB,BVC>::x0;
    using BoolTernary<BVA,BVB,BVC>::x1;
    using BoolTernary<BVA,BVB,BVC>::x2;
    /// Constructor for posting
    Or(Home home, BVA b0, BVB b1, BVC b2);
    /// Constructor for cloning \a p
    Or(Space& home, bool share, Or& p);
  public:
    /// Constructor for rewriting \a p during cloning
    Or(Space& home, bool share, Propagator& p, BVA b0, BVB b1, BVC b2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ b_0 \lor b_1 = b_2 \f$
    static  ExecStatus post(Home home, BVA b0, BVB b1, BVC b2);
  };

  /**
   * \brief Boolean n-ary disjunction propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX,class VY>
  class NaryOr
    : public MixNaryOnePropagator<VX,PC_BOOL_NONE,VY,PC_BOOL_VAL> {
  protected:
    using MixNaryOnePropagator<VX,PC_BOOL_NONE,VY,PC_BOOL_VAL>::x;
    using MixNaryOnePropagator<VX,PC_BOOL_NONE,VY,PC_BOOL_VAL>::y;
    /// The number of views assigned to zero in \a x
    int n_zero;
    /// The advisor council
    Council<Advisor> c;
    /// Constructor for posting
    NaryOr(Home home,  ViewArray<VX>& x, VY y);
    /// Constructor for cloning \a p
    NaryOr(Space& home, bool share, NaryOr<VX,VY>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Give advice to propagator
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    /// Cost function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ \bigvee_{i=0}^{|x|-1} x_i = y\f$
    static  ExecStatus post(Home home, ViewArray<VX>& x, VY y);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };


  /**
   * \brief Boolean n-ary disjunction propagator (true)
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BV>
  class NaryOrTrue : public BinaryPropagator<BV,PC_BOOL_VAL> {
  protected:
    using BinaryPropagator<BV,PC_BOOL_VAL>::x0;
    using BinaryPropagator<BV,PC_BOOL_VAL>::x1;
    /// Views not yet subscribed to
    ViewArray<BV> x;
    /// Update subscription
    ExecStatus resubscribe(Space& home, BV& x0, BV x1);
    /// Constructor for posting
    NaryOrTrue(Home home, ViewArray<BV>& x);
    /// Constructor for cloning \a p
    NaryOrTrue(Space& home, bool share, NaryOrTrue<BV>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ \bigvee_{i=0}^{|b|-1} b_i = 0\f$
    static  ExecStatus post(Home home, ViewArray<BV>& b);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };


  /**
   * \brief Boolean equivalence propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class BVA, class BVB, class BVC>
  class Eqv : public BoolTernary<BVA,BVB,BVC> {
  protected:
    using BoolTernary<BVA,BVB,BVC>::x0;
    using BoolTernary<BVA,BVB,BVC>::x1;
    using BoolTernary<BVA,BVB,BVC>::x2;
    /// Constructor for cloning \a p
    Eqv(Space& home, bool share, Eqv& p);
    /// Constructor for posting
    Eqv(Home home, BVA b0 ,BVB b1, BVC b2);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ b_0 \Leftrightarrow b_1 = b_2 \f$ (equivalence)
    static  ExecStatus post(Home home, BVA b0, BVB b1, BVC b2);
  };


  /**
   * \brief Boolean n-ary equivalence propagator
   *
   * Enforces that the parity of the views is odd.
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  class NaryEqv : public BinaryPropagator<BoolView,PC_BOOL_VAL> {
  protected:
    using BinaryPropagator<BoolView,PC_BOOL_VAL>::x0;
    using BinaryPropagator<BoolView,PC_BOOL_VAL>::x1;
    /// Views not yet subscribed to
    ViewArray<BoolView> x;
    /// Parity information mod 2
    int pm2;
    /// Update subscription
    void resubscribe(Space& home, BoolView& x0);
    /// Constructor for posting
    NaryEqv(Home home, ViewArray<BoolView>& x, int pm2);
    /// Constructor for cloning \a p
    NaryEqv(Space& home, bool share, NaryEqv& p);
  public:
    /// Copy propagator during cloning
    GECODE_INT_EXPORT 
    virtual Actor* copy(Space& home, bool share);
    /// Cost function (defined as low binary)
    GECODE_INT_EXPORT 
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    GECODE_INT_EXPORT 
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x_0 \Leftrightarrow x_1 \Leftrightarrow \cdots \Leftrightarrow x_{|x|-1}=p\f$
    GECODE_INT_EXPORT 
    static ExecStatus post(Home home, ViewArray<BoolView>& x, int pm2);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };


  /**
   * \brief Boolean clause propagator (disjunctive)
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX, class VY>
  class Clause : public Propagator {
  protected:
    /// Positive views
    ViewArray<VX> x;
    /// Positive views (origin from negative variables)
    ViewArray<VY> y;
    /// Result
    VX z;
    /// The number of views assigned to zero in \a x and \a y
    int n_zero;
    /// %Advisors for views (tagged whether for \a x or \a y)
    class Tagged : public Advisor {
    public:
      /// Whether advises a view for x or y
      const bool x;
      /// Create tagged advisor
      Tagged(Space& home, Propagator& p, Council<Tagged>& c, bool x);
      /// Clone tagged advisor \a a
      Tagged(Space& home, bool share, Tagged& a);
    };
    /// The advisor council
    Council<Tagged> c;
    /// Cancel subscriptions
    void cancel(Space& home);
    /// Constructor for posting
    Clause(Home home,  ViewArray<VX>& x, ViewArray<VY>& y, VX z);
    /// Constructor for cloning \a p
    Clause(Space& home, bool share, Clause<VX,VY>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Give advice to propagator
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    /// Cost function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ \bigvee_{i=0}^{|x|-1} x_i \vee \bigvee_{i=0}^{|x|-1} y_i = z\f$
    static  ExecStatus post(Home home, ViewArray<VX>& x, ViewArray<VY>& y,
                            VX z);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };


  /**
   * \brief Boolean clause propagator (disjunctive, true)
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX, class VY>
  class ClauseTrue
    : public MixBinaryPropagator<VX,PC_BOOL_VAL,VY,PC_BOOL_VAL> {
  protected:
    using MixBinaryPropagator<VX,PC_BOOL_VAL,VY,PC_BOOL_VAL>::x0;
    using MixBinaryPropagator<VX,PC_BOOL_VAL,VY,PC_BOOL_VAL>::x1;
    /// Views not yet subscribed to
    ViewArray<VX> x;
    /// Views not yet subscribed to (origin from negative variables)
    ViewArray<VY> y;
    /// Constructor for posting
    ClauseTrue(Home home, ViewArray<VX>& x, ViewArray<VY>& y);
    /// Constructor for cloning \a p
    ClauseTrue(Space& home, bool share, ClauseTrue<VX,VY>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost function (defined as low binary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ \bigvee_{i=0}^{|x|-1} x_i \vee \bigvee_{i=0}^{|y|-1} y_i = 1\f$
    static  ExecStatus post(Home home, ViewArray<VX>& x, ViewArray<VY>& y);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };


  /**
   * \brief If-then-else propagator base-class
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, PropCond pc>
  class IteBase : public Propagator {
  protected:
    /// View for condition
    BoolView b;
    /// Views
    View x0, x1, x2;
    /// Constructor for cloning \a p
    IteBase(Space& home, bool share, IteBase& p);
    /// Constructor for creation
    IteBase(Home home, BoolView b, View x0, View x1, View x2);
  public:
    /// Cost function (defined as low ternary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief If-then-else bounds-consistent propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class IteBnd : public IteBase<View,PC_INT_BND> {
  protected:
    using IteBase<View,PC_INT_BND>::b;
    using IteBase<View,PC_INT_BND>::x0;
    using IteBase<View,PC_INT_BND>::x1;
    using IteBase<View,PC_INT_BND>::x2;
    /// Constructor for cloning \a p
    IteBnd(Space& home, bool share, IteBnd& p);
    /// Constructor for creation
    IteBnd(Home home, BoolView b, View x0, View x1, View x2);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post if-then-else propagator
    static ExecStatus post(Home home, BoolView b, View x0, View x1, View x2);
  };

  /**
   * \brief If-then-else domain-consistent propagator
   *
   * Requires \code #include <gecode/int/bool.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class IteDom : public IteBase<View,PC_INT_DOM> {
  protected:
    using IteBase<View,PC_INT_DOM>::b;
    using IteBase<View,PC_INT_DOM>::x0;
    using IteBase<View,PC_INT_DOM>::x1;
    using IteBase<View,PC_INT_DOM>::x2;
    /// Constructor for cloning \a p
    IteDom(Space& home, bool share, IteDom& p);
    /// Constructor for creation
    IteDom(Home home, BoolView b, View x0, View x1, View x2);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost function (defined as high ternary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post if-then-else propagator
    static ExecStatus post(Home home, BoolView b, View x0, View x1, View x2);
  };

}}}

#include <gecode/int/bool/base.hpp>
#include <gecode/int/bool/eq.hpp>
#include <gecode/int/bool/lq.hpp>
#include <gecode/int/bool/or.hpp>
#include <gecode/int/bool/eqv.hpp>
#include <gecode/int/bool/clause.hpp>
#include <gecode/int/bool/ite.hpp>

#endif

// STATISTICS: int-prop

