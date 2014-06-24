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

#ifndef __GECODE_INT_COUNT_HH__
#define __GECODE_INT_COUNT_HH__

#include <gecode/int.hh>

/**
 * \namespace Gecode::Int::Count
 * \brief Counting propagators
 */

namespace Gecode { namespace Int { namespace Count {

  /**
   * Relations for domain consistent counting
   *
   */
  //@{
  /// Description of view type
  enum ViewTypeDesc {
    VTD_CONSTVIEW, ///< Constant view
    VTD_INTSET,    ///< Integer set
    VTD_VARVIEW    ///< Variable view
  };
  /// Return the view type description of \a y
  template<class VY>
  ViewTypeDesc vtd(VY y);

  /// Subscribe propagator \a p to view \a y
  template<class VY>
  void subscribe(Space& home, Propagator& p, VY y);
  /// Cancel propagator \a p for view \a y
  template<class VY>
  void cancel(Space& home, Propagator& p, VY y);

  /// Test whether \a x and \a y are equal
  template<class VX>
  RelTest holds(VX x, VX y);
  /// Test whether \a x and \a y are equal
  template<class VX>
  RelTest holds(VX x, ConstIntView y);
  /// Test whether \a x and \a y are equal
  template<class VX>
  RelTest holds(VX x, ZeroIntView y);
  /// Test whether \a x and \a y are equal
  template<class VX>
  RelTest holds(VX x, const IntSet& y);

  /// Post that all views in \a x are equal to \a y
  template<class VX>
  ExecStatus post_true(Home home, ViewArray<VX>& x, VX y);
  /// Post that all views in \a x are equal to \a y
  template<class VX>
  ExecStatus post_true(Home home, ViewArray<VX>& x, ConstIntView y);
  /// Post that all views in \a x are equal to \a y
  template<class VX>
  ExecStatus post_true(Home home, ViewArray<VX>& x, ZeroIntView y);
  /// Post that all views in \a x are equal to \a y
  template<class VX>
  ExecStatus post_true(Home home, ViewArray<VX>& x, const IntSet& y);

  /// Post that all views in \a x are not equal to \a y
  template<class VX>
  ExecStatus post_false(Home home, ViewArray<VX>& x, VX y);
  /// Post that all views in \a x are not equal to \a y
  template<class VX>
  ExecStatus post_false(Home home, ViewArray<VX>& x, ConstIntView y);
  /// Post that all views in \a x are not equal to \a y
  template<class VX>
  ExecStatus post_false(Home home, ViewArray<VX>& x, ZeroIntView y);
  /// Post that all views in \a x are not equal to \a y
  template<class VX>
  ExecStatus post_false(Home home, ViewArray<VX>& x, const IntSet& y);

  /// Prune that \a y is the union of \a x
  template<class VX>
  ExecStatus prune(Home home, ViewArray<VX>& x, VX y);
  /// Prune that \a y is the union of \a x
  template<class VX>
  ExecStatus prune(Home home, ViewArray<VX>& x, ConstIntView y);
  /// Prune that \a y is the union of \a x
  template<class VX>
  ExecStatus prune(Home home, ViewArray<VX>& x, ZeroIntView y);
  /// Prune that \a y is the union of \a x
  template<class VX>
  ExecStatus prune(Home home, ViewArray<VX>& x, const IntSet& y);
  //@}

}}}

#include <gecode/int/count/rel.hpp>


namespace Gecode { namespace Int { namespace Count {

  /**
   * \brief Baseclass for count propagators (integer)
   *
   */
  template<class VX, class VY>
  class IntBase : public Propagator {
  protected:
    /// Views still to count
    ViewArray<VX> x;
    /// Views from x[0] ... x[n_s-1] have subscriptions
    int n_s;
    /// View to compare to
    VY y;
    /// Number of views which are equal and have been eliminated
    int c;
    /// Constructor for cloning \a p
    IntBase(Space& home, bool share, IntBase& p);
    /// Constructor for creation
    IntBase(Home home, ViewArray<VX>& x, int n_s, VY y, int c);
  public:
    /// Cost function (defined as low linear)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief %Propagator for counting views (equal integer to number of equal views)
   *
   * Not all combinations of views are possible. The types \a VX
   * and \a VY must be either equal, or \a VY must be ConstIntView, 
   * ZeroIntView, or IntSet.
   *
   * Requires \code #include <gecode/int/count.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX, class VY>
  class EqInt : public IntBase<VX,VY> {
  protected:
    using IntBase<VX,VY>::x;
    using IntBase<VX,VY>::n_s;
    using IntBase<VX,VY>::y;
    using IntBase<VX,VY>::c;
    /// Constructor for cloning \a p
    EqInt(Space& home, bool share, EqInt& p);
    /// Constructor for creation
    EqInt(Home home, ViewArray<VX>& x, int n_s, VY y, int c);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}=c\f$
    static ExecStatus post(Home home, ViewArray<VX>& x, VY y, int c);
  };

  /**
   * \brief %Propagator for counting views (greater or equal integer to number of equal views)
   *
   * Not all combinations of views are possible. The types \a VX
   * and \a VY must be either equal, or \a VY must be ConstIntView, 
   * ZeroIntView, or IntSet.
   *
   * Requires \code #include <gecode/int/count.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX, class VY>
  class GqInt : public IntBase<VX,VY> {
  protected:
    using IntBase<VX,VY>::x;
    using IntBase<VX,VY>::n_s;
    using IntBase<VX,VY>::y;
    using IntBase<VX,VY>::c;
    /// Constructor for cloning \a p
    GqInt(Space& home, bool share, GqInt& p);
    /// Constructor for creation
    GqInt(Home home, ViewArray<VX>& x, int n_s, VY y, int c);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}\geq c\f$
    static ExecStatus post(Home home, ViewArray<VX>& x, VY y, int c);
  };

  /**
   * \brief %Propagator for counting views (less or equal integer to number of equal views)
   *
   * Not all combinations of views are possible. The types \a VX
   * and \a VY must be either equal, or \a VY must be ConstIntView, 
   * ZeroIntView, or IntSet.
   *
   * Requires \code #include <gecode/int/count.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX, class VY>
  class LqInt : public IntBase<VX,VY> {
  protected:
    using IntBase<VX,VY>::x;
    using IntBase<VX,VY>::n_s;
    using IntBase<VX,VY>::y;
    using IntBase<VX,VY>::c;
    /// Constructor for cloning \a p
    LqInt(Space& home, bool share, LqInt& p);
    /// Constructor for creation
    LqInt(Home home, ViewArray<VX>& x, int n_s, VY y, int c);
  public:
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}\leq c\f$
    static ExecStatus post(Home home, ViewArray<VX>& x, VY y, int c);
  };

}}}

#include <gecode/int/count/int-base.hpp>
#include <gecode/int/count/int-eq.hpp>
#include <gecode/int/count/int-gq.hpp>
#include <gecode/int/count/int-lq.hpp>


namespace Gecode { namespace Int { namespace Count {

  /**
   * \brief Base-class for count propagators (view)
   *
   */
  template<class VX, class VY, class VZ>
  class ViewBase : public Propagator {
  protected:
    /// Views still to count
    ViewArray<VX> x;
    /// View to compare to
    VY y;
    /// View which yields result of counting
    VZ z;
    /// Number of views which are equal and have been eliminated
    int   c;
    /// Constructor for cloning \a p
    ViewBase(Space& home, bool share, ViewBase& p);
    /// Constructor for creation
    ViewBase(Home home, ViewArray<VX>& x, VY y, VZ z, int c);
  public:
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Cost function (defined as low linear)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
  protected:
    /// Count how many views are equal now
    void count(Space& home);
    /// How many views are at least equal
    int atleast(void) const;
    /// How many views are at most equal
    int atmost(void) const;
    /// Test whether there is sharing of \a z with \a x or \a y
    static bool sharing(const ViewArray<VX>& x, const VY& y, const VZ& z);
  };

  /**
   * \brief %Propagator for counting views (equal to number of equal views)
   *
   * Not all combinations of views are possible. The types \a VX
   * and \a VY must be either equal, or \a VY must be ConstIntView, 
   * ZeroIntView, or IntSet.
   *
   * Requires \code #include <gecode/int/count.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX, class VY, class VZ, bool shr, bool dom>
  class EqView : public ViewBase<VX,VY,VZ> {
  protected:
    using ViewBase<VX,VY,VZ>::x;
    using ViewBase<VX,VY,VZ>::z;
    using ViewBase<VX,VY,VZ>::c;
    using ViewBase<VX,VY,VZ>::y;
    using ViewBase<VX,VY,VZ>::count;
    using ViewBase<VX,VY,VZ>::atleast;
    using ViewBase<VX,VY,VZ>::atmost;
    using ViewBase<VX,VY,VZ>::sharing;

    /// Constructor for cloning \a p
    EqView(Space& home, bool share, EqView& p);
  public:
    /// Constructor for creation
    EqView(Home home, ViewArray<VX>& x, VY y, VZ z, int c);
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}=z+c\f$
    static ExecStatus post(Home home, ViewArray<VX>& x, VY y, VZ z, int c);
  };

  /**
   * \brief %Propagator for counting views (less or equal to number of equal views)
   *
   * Not all combinations of views are possible. The types \a VX
   * and \a VY must be either equal, or \a VY must be ConstIntView, 
   * ZeroIntView, or IntSet.
   *
   * Requires \code #include <gecode/int/count.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX, class VY, class VZ, bool shr>
  class LqView : public ViewBase<VX,VY,VZ> {
  protected:
    using ViewBase<VX,VY,VZ>::x;
    using ViewBase<VX,VY,VZ>::z;
    using ViewBase<VX,VY,VZ>::c;
    using ViewBase<VX,VY,VZ>::y;
    using ViewBase<VX,VY,VZ>::count;
    using ViewBase<VX,VY,VZ>::atleast;
    using ViewBase<VX,VY,VZ>::atmost;
    using ViewBase<VX,VY,VZ>::sharing;

    /// Constructor for cloning \a p
    LqView(Space& home, bool share, LqView& p);
  public:
    /// Constructor for creation
    LqView(Home home, ViewArray<VX>& x, VY y, VZ z, int c);
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}\leq z+c\f$
    static ExecStatus post(Home home, ViewArray<VX>& x, VY y, VZ z, int c);
  };

  /**
   * \brief %Propagator for counting views (greater or equal to number of equal views)
   *
   * Not all combinations of views are possible. The types \a VX
   * and \a VY must be either equal, or \a VY must be ConstIntView, 
   * ZeroIntView, or IntSet.
   *
   * Requires \code #include <gecode/int/count.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VX, class VY, class VZ, bool shr, bool dom>
  class GqView : public ViewBase<VX,VY,VZ> {
  protected:
    using ViewBase<VX,VY,VZ>::x;
    using ViewBase<VX,VY,VZ>::z;
    using ViewBase<VX,VY,VZ>::c;
    using ViewBase<VX,VY,VZ>::y;
    using ViewBase<VX,VY,VZ>::count;
    using ViewBase<VX,VY,VZ>::atleast;
    using ViewBase<VX,VY,VZ>::atmost;
    using ViewBase<VX,VY,VZ>::sharing;

    /// Constructor for cloning \a p
    GqView(Space& home, bool share, GqView& p);
  public:
    /// Constructor for creation
    GqView(Home home, ViewArray<VX>& x, VY y, VZ z, int c);
    /// Create copy during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\#\{i\in\{0,\ldots,|x|-1\}\;|\;x_i=y\}\geq z+c\f$
    static ExecStatus post(Home home, ViewArray<VX>& x, VY y, VZ z, int c);
  };

}}}

#include <gecode/int/count/view-base.hpp>
#include <gecode/int/count/view-eq.hpp>
#include <gecode/int/count/view-gq.hpp>
#include <gecode/int/count/view-lq.hpp>

#endif

// STATISTICS: int-prop

