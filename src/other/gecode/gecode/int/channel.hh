/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
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

#ifndef __GECODE_INT_CHANNEL_HH__
#define __GECODE_INT_CHANNEL_HH__

#include <gecode/int.hh>
#include <gecode/int/distinct.hh>

/**
 * \namespace Gecode::Int::Channel
 * \brief %Channel propagators
 */

namespace Gecode { namespace Int { namespace Channel {

  /// Processing stack
  typedef Support::StaticStack<int,Region> ProcessStack;

  /**
   * \brief Base-class for channel propagators
   *
   */
  template<class Info, class Offset, PropCond pc>
  class Base : public Propagator {
  protected:
    /// Number of views (actually twice as many for both x and y)
    int n;
    /// Total number of not assigned views (not known to be assigned)
    int n_na;
    /// Offset transformation for x variables
    Offset ox;
    /// Offset transformation for y variables
    Offset oy;
    /// View and information for both \a x and \a y
    Info* xy;
    /// Constructor for cloning \a p
    Base(Space& home, bool share, Base<Info,Offset,pc>& p);
    /// Constructor for posting
    Base(Home home, int n, Info* xy, Offset& ox, Offset& oy);
  public:
    /// Propagation cost (defined as low quadratic)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };


  /**
   * \brief Combine view with information for value propagation
   *
   */
  template<class View> class ValInfo;

  /**
   * \brief Naive channel propagator
   *
   * Only propagates if views become assigned. If \a shared is true,
   * the same views can be contained in both \a x and \a y.
   *
   * Requires \code #include <gecode/int/channel.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, class Offset, bool shared>
  class Val : public Base<ValInfo<View>,Offset,PC_INT_VAL> {
 protected:
    using Base<ValInfo<View>,Offset,PC_INT_VAL>::n;
    using Base<ValInfo<View>,Offset,PC_INT_VAL>::n_na;
    using Base<ValInfo<View>,Offset,PC_INT_VAL>::xy;
    using Base<ValInfo<View>,Offset,PC_INT_VAL>::ox;
    using Base<ValInfo<View>,Offset,PC_INT_VAL>::oy;
    /// Constructor for cloning \a p
    Val(Space& home, bool share, Val& p);
    /// Constructor for posting
    Val(Home home, int n, ValInfo<View>* xy, Offset& ox, Offset& oy);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for channeling
    static  ExecStatus post(Home home, int n, ValInfo<View>* xy,
                            Offset& ox, Offset& oy);
  };

  /**
   * \brief Combine view with information for domain propagation
   *
   */
  template<class View, class Offset> class DomInfo;

  /**
   * \brief Domain consistent channel propagator
   *
   * If \a shared is true, the same views can be contained in both
   * \a x and \a y.
   *
   * Requires \code #include <gecode/int/channel.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, class Offset, bool shared>
  class Dom : public Base<DomInfo<View,Offset>,Offset,PC_INT_DOM> {
  protected:
    using Base<DomInfo<View,Offset>,Offset,PC_INT_DOM>::n;
    using Base<DomInfo<View,Offset>,Offset,PC_INT_DOM>::n_na;
    using Base<DomInfo<View,Offset>,Offset,PC_INT_DOM>::xy;
    using Base<DomInfo<View,Offset>,Offset,PC_INT_DOM>::ox;
    using Base<DomInfo<View,Offset>,Offset,PC_INT_DOM>::oy;
    /// Propagation controller for propagating distinct
    Distinct::DomCtrl<View> dc;
    /// Constructor for cloning \a p
    Dom(Space& home, bool share, Dom& p);
    /// Constructor for posting
    Dom(Home home, int n, DomInfo<View,Offset>* xy, Offset& ox, Offset& oy);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If in stage for naive value propagation, the cost is
     * low quadratic. Otherwise it is high quadratic.
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for channeling on \a xy
    static  ExecStatus post(Home home, int n, DomInfo<View,Offset>* xy,
                            Offset& ox, Offset& oy);
  };

  /**
   * \brief Link propagator for a single Boolean view
   *
   * Requires \code #include <gecode/int/channel.hh> \endcode
   * \ingroup FuncIntProp
   */
  class LinkSingle :
    public MixBinaryPropagator<BoolView,PC_BOOL_VAL,IntView,PC_INT_VAL> {
  private:
    using MixBinaryPropagator<BoolView,PC_BOOL_VAL,IntView,PC_INT_VAL>::x0;
    using MixBinaryPropagator<BoolView,PC_BOOL_VAL,IntView,PC_INT_VAL>::x1;

    /// Constructor for cloning \a p
    LinkSingle(Space& home, bool share, LinkSingle& p);
    /// Constructor for posting
    LinkSingle(Home home, BoolView x0, IntView x1);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ x_0 = x_1\f$
    static  ExecStatus post(Home home, BoolView x0, IntView x1);
  };

  /**
   * \brief Link propagator for multiple Boolean views
   *
   * Requires \code #include <gecode/int/channel.hh> \endcode
   * \ingroup FuncIntProp
   */
  class LinkMulti :
    public MixNaryOnePropagator<BoolView,PC_BOOL_NONE,IntView,PC_INT_DOM> {
  private:
    using MixNaryOnePropagator<BoolView,PC_BOOL_NONE,IntView,PC_INT_DOM>::x;
    using MixNaryOnePropagator<BoolView,PC_BOOL_NONE,IntView,PC_INT_DOM>::y;
    /// The advisor council
    Council<Advisor> c;
    /// Value for propagator being idle
    static const int S_NONE = 0;
    /// Value for propagator having detected a one
    static const int S_ONE  = 1;
    /// Value for propagator currently running
    static const int S_RUN  = 2;
    /// Propagator status
    int status;
    /// Offset value
    int o;
    /// Constructor for cloning \a p
    LinkMulti(Space& home, bool share, LinkMulti& p);
    /// Constructor for posting
    LinkMulti(Home home, ViewArray<BoolView>& x, IntView y, int o0);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost function (low unary if \a y is assigned, low linear otherwise)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Give advice to propagator
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ x_i = 1\leftrightarrow y=i+o\f$
    GECODE_INT_EXPORT
    static  ExecStatus post(Home home,
                            ViewArray<BoolView>& x, IntView y, int o);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

}}}

#include <gecode/int/channel/base.hpp>
#include <gecode/int/channel/val.hpp>
#include <gecode/int/channel/dom.hpp>

#include <gecode/int/channel/link-single.hpp>
#include <gecode/int/channel/link-multi.hpp>

#endif

// STATISTICS: int-prop

