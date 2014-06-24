/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
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

#ifndef __GECODE_SET_INT_HH__
#define __GECODE_SET_INT_HH__

#include <gecode/set.hh>

namespace Gecode { namespace Set { namespace Int {

  /**
   * \namespace Gecode::Set::Int
   * \brief Propagators connecting set and int variables
   */

  /**
   * \brief %Propagator for minimum element
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View>
  class MinElement :
    public MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND> {
  protected:
    using MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND>::x0;
    using MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND>::x1;
    /// Constructor for cloning \a p
    MinElement(Space& home, bool share,MinElement& p);
    /// Constructor for posting
    MinElement(Home home, View, Gecode::Int::IntView);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \a x is the minimal element of \a s
    static ExecStatus post(Home home, View s, Gecode::Int::IntView x);
  };

  /**
   * \brief %Propagator for not minimum element
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View>
  class NotMinElement :
    public MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM> {
  protected:
    using MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM>::x0;
    using MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM>::x1;
    /// Constructor for cloning \a p
    NotMinElement(Space& home, bool share,NotMinElement& p);
    /// Constructor for posting
    NotMinElement(Home home, View, Gecode::Int::IntView);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \a x is not the minimal element of \a s
    static ExecStatus post(Home home, View s, Gecode::Int::IntView x);
  };

  /**
   * \brief %Propagator for reified minimum element
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View, ReifyMode rm>
  class ReMinElement :
    public Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,Gecode::Int::BoolView> {
  protected:
    using Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,Gecode::Int::BoolView>::x0;
    using Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,Gecode::Int::BoolView>::x1;
    using Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,Gecode::Int::BoolView>::b;
    /// Constructor for cloning \a p
    ReMinElement(Space& home, bool share,ReMinElement& p);
    /// Constructor for posting
    ReMinElement(Home home, View, Gecode::Int::IntView,
                 Gecode::Int::BoolView);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post reified propagator for \a b iff \a x is the minimal element of \a s
    static ExecStatus post(Home home, View s, Gecode::Int::IntView x,
                           Gecode::Int::BoolView b);
  };

  /**
   * \brief %Propagator for maximum element
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View>
  class MaxElement :
    public MixBinaryPropagator<View,PC_SET_ANY,Gecode::Int::IntView,Gecode::Int::PC_INT_BND> {
  protected:
    using MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND>::x0;
    using MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND>::x1;
    /// Constructor for cloning \a p
    MaxElement(Space& home, bool share,MaxElement& p);
    /// Constructor for posting
    MaxElement(Home home, View, Gecode::Int::IntView);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \a x is the largest element of \a s
    static ExecStatus post(Home home, View s, Gecode::Int::IntView x);
  };

  /**
   * \brief %Propagator for not maximum element
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View>
  class NotMaxElement :
    public MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM> {
  protected:
    using MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM>::x0;
    using MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM>::x1;
    /// Constructor for cloning \a p
    NotMaxElement(Space& home, bool share,NotMaxElement& p);
    /// Constructor for posting
    NotMaxElement(Home home, View, Gecode::Int::IntView);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \a x is not the largest element of \a s
    static ExecStatus post(Home home, View s, Gecode::Int::IntView x);
  };

  /**
   * \brief %Reified propagator for maximum element
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View, ReifyMode rm>
  class ReMaxElement :
    public Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,Gecode::Int::BoolView> {
  protected:
    using Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,Gecode::Int::BoolView>::x0;
    using Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,Gecode::Int::BoolView>::x1;
    using Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,Gecode::Int::BoolView>::b;
    /// Constructor for cloning \a p
    ReMaxElement(Space& home, bool share,ReMaxElement& p);
    /// Constructor for posting
    ReMaxElement(Home home, View, Gecode::Int::IntView, Gecode::Int::BoolView);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post reified propagator for \a b iff \a x is the largest element of \a s
    static ExecStatus post(Home home, View s, Gecode::Int::IntView x,
                           Gecode::Int::BoolView b);
  };

  /**
   * \brief %Propagator for cardinality
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View>
  class Card :
    public MixBinaryPropagator<View,PC_SET_CARD,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND> {
  protected:
    using MixBinaryPropagator<View,PC_SET_CARD,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND>::x0;
    using MixBinaryPropagator<View,PC_SET_CARD,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND>::x1;
    /// Constructor for cloning \a p
    Card(Space& home, bool share,Card& p);
    /// Constructor for posting
    Card(Home home, View, Gecode::Int::IntView);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$ |s|=x \f$
    static ExecStatus post(Home home, View s, Gecode::Int::IntView x);
  };

  /**
   * \brief %Propagator for weight of a set
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View>
  class Weights : public Propagator {
  protected:
    /// List of elements in the upper bound
    SharedArray<int> elements;
    /// Weights for the elements in the upper bound
    SharedArray<int> weights;

    /// The set view
    View x;
    /// The integer view
    Gecode::Int::IntView y;

    /// Constructor for cloning \a p
    Weights(Space& home, bool share,Weights& p);
    /// Constructor for posting
    Weights(Home home, const SharedArray<int>&, const SharedArray<int>&,
            View, Gecode::Int::IntView);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home,bool);
    /// Cost function (defined as PC_LINEAR_LO)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\sum_{i\in x} weights_i = y \f$
    static ExecStatus post(Home home,
                           const SharedArray<int>& elements,
                           const SharedArray<int>& weights,
                           View x, Gecode::Int::IntView y);
  };

}}}

#include <gecode/set/int/minmax.hpp>
#include <gecode/set/int/card.hpp>
#include <gecode/set/int/weights.hpp>

#endif

// STATISTICS: set-prop
