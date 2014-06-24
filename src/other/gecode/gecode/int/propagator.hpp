/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

namespace Gecode { namespace Int {

  /**
   * \defgroup TaskPropRePat Reified propagator patterns
   *
   * \ingroup TaskActor
   */

  //@{
  /**
   * \brief Reified unary propagator
   *
   * Stores single view of type \a View with propagation condition \a pc
   * and a Boolean control view of type \a CtrlView.
   *
   * If the propagation condition \a pc has the value PC_GEN_NONE, no
   * subscriptions are created for \a View.
   *
   */
  template<class View, PropCond pc, class CtrlView>
  class ReUnaryPropagator : public Propagator {
  protected:
    /// Single view
    View x0;
    /// Boolean control view
    CtrlView b;
    /// Constructor for cloning \a p
    ReUnaryPropagator(Space& home, bool share, ReUnaryPropagator& p);
    /// Constructor for rewriting \a p during cloning
    ReUnaryPropagator(Space& home, bool share, Propagator& p,
                      View x0, CtrlView b);
    /// Constructor for creation
    ReUnaryPropagator(Home home, View x0, CtrlView b);
  public:
    /// Cost function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Reified binary propagator
   *
   * Stores two views of type \a View with propagation condition \a pc
   * and a Boolean control view of type \a CtrlView.
   *
   * If the propagation condition \a pc has the value PC_GEN_NONE, no
   * subscriptions are created for \a View.
   *
   */
  template<class View, PropCond pc, class CtrlView>
  class ReBinaryPropagator : public Propagator {
  protected:
    /// Two views
    View x0, x1;
    /// Boolean control view
    CtrlView b;
    /// Constructor for cloning \a p
    ReBinaryPropagator(Space& home, bool share, ReBinaryPropagator& p);
    /// Constructor for rewriting \a p during cloning
    ReBinaryPropagator(Space& home, bool share, Propagator& p,
                       View x0, View x1, CtrlView b);
    /// Constructor for creation
    ReBinaryPropagator(Home home, View x0, View x1, CtrlView b);
  public:
    /// Cost function (defined as low binary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };
  //@}


  /**
   * \brief Reified mixed binary propagator
   *
   * Stores two views of type \a View0 and \a View1 with propagation
   * conditions \a pc0 and \a pc1 and a Boolean control view of type 
   * \a CtrlView.
   *
   * If the propagation conditions \a pc0 or \a pc1 have the values
   * PC_GEN_NONE, no subscriptions are created.
   *
   */
  template<class View0, PropCond pc0, class View1, PropCond pc1,
           class CtrlView>
  class ReMixBinaryPropagator : public Propagator {
  protected:
    /// View of type \a View0
    View0 x0;
    /// View of type \a View1
    View1 x1;
    /// Boolean control view
    CtrlView b;
    /// Constructor for cloning \a p
    ReMixBinaryPropagator(Space& home, bool share, ReMixBinaryPropagator& p);
    /// Constructor for creation
    ReMixBinaryPropagator(Home home, View0 x0, View1 x1, CtrlView b);
    /// Constructor for rewriting \a p during cloning
    ReMixBinaryPropagator(Space& home, bool share, Propagator& p,
                          View0 x0, View1 x1, CtrlView b);
  public:
    /// Cost function (defined as low binary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };


  /*
   * Reified unary propagators
   *
   */
  template<class View, PropCond pc, class CtrlView>
  ReUnaryPropagator<View,pc,CtrlView>::ReUnaryPropagator
  (Home home, View y0, CtrlView b0)
    : Propagator(home), x0(y0), b(b0) {
    if (pc != PC_GEN_NONE)
      x0.subscribe(home,*this,pc);
    b.subscribe(home,*this,Int::PC_INT_VAL);
  }

  template<class View, PropCond pc, class CtrlView>
  forceinline
  ReUnaryPropagator<View,pc,CtrlView>::ReUnaryPropagator
  (Space& home, bool share, ReUnaryPropagator<View,pc,CtrlView>& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    b.update(home,share,p.b);
  }

  template<class View, PropCond pc, class CtrlView>
  forceinline
  ReUnaryPropagator<View,pc,CtrlView>::ReUnaryPropagator
  (Space& home, bool share, Propagator& p, View y0, CtrlView b0)
    : Propagator(home,share,p) {
    x0.update(home,share,y0);
    b.update(home,share,b0);
  }

  template<class View, PropCond pc, class CtrlView>
  PropCost
  ReUnaryPropagator<View,pc,CtrlView>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::LO);
  }

  template<class View, PropCond pc, class CtrlView>
  forceinline size_t
  ReUnaryPropagator<View,pc,CtrlView>::dispose(Space& home) {
    if (pc != PC_GEN_NONE)
      x0.cancel(home,*this,pc);
    b.cancel(home,*this,Int::PC_INT_VAL);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  /*
   * Reified binary propagators
   *
   */
  template<class View, PropCond pc, class CtrlView>
  ReBinaryPropagator<View,pc,CtrlView>::ReBinaryPropagator
  (Home home, View y0, View y1, CtrlView b1)
    : Propagator(home), x0(y0), x1(y1), b(b1) {
    if (pc != PC_GEN_NONE) {
      x0.subscribe(home,*this,pc);
      x1.subscribe(home,*this,pc);
    }
    b.subscribe(home,*this,Int::PC_INT_VAL);
  }

  template<class View, PropCond pc, class CtrlView>
  forceinline
  ReBinaryPropagator<View,pc,CtrlView>::ReBinaryPropagator
  (Space& home, bool share, ReBinaryPropagator<View,pc,CtrlView>& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    b.update(home,share,p.b);
  }

  template<class View, PropCond pc, class CtrlView>
  forceinline
  ReBinaryPropagator<View,pc,CtrlView>::ReBinaryPropagator
  (Space& home, bool share, Propagator& p, View y0, View y1, CtrlView b0)
    : Propagator(home,share,p) {
    x0.update(home,share,y0);
    x1.update(home,share,y1);
    b.update(home,share,b0);
  }

  template<class View, PropCond pc, class CtrlView>
  PropCost
  ReBinaryPropagator<View,pc,CtrlView>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::binary(PropCost::LO);
  }

  template<class View, PropCond pc, class CtrlView>
  forceinline size_t
  ReBinaryPropagator<View,pc,CtrlView>::dispose(Space& home) {
    if (pc != PC_GEN_NONE) {
      x0.cancel(home,*this,pc);
      x1.cancel(home,*this,pc);
    }
    b.cancel(home,*this,Int::PC_INT_VAL);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  /*
   * Reified mixed binary propagator
   *
   */
  template<class View0, PropCond pc0, class View1, PropCond pc1,
            class CtrlView>
  ReMixBinaryPropagator<View0,pc0,View1,pc1,CtrlView>
  ::ReMixBinaryPropagator(Home home, View0 y0, View1 y1, CtrlView b1)
    : Propagator(home), x0(y0), x1(y1), b(b1) {
    if (pc0 != PC_GEN_NONE)
      x0.subscribe(home,*this,pc0);
    if (pc1 != PC_GEN_NONE)
      x1.subscribe(home,*this,pc1);
    b.subscribe(home,*this,Int::PC_INT_VAL);
  }

  template<class View0, PropCond pc0, class View1, PropCond pc1,
            class CtrlView>
  forceinline
  ReMixBinaryPropagator<View0,pc0,View1,pc1,CtrlView>::ReMixBinaryPropagator
  (Space& home, bool share, 
   ReMixBinaryPropagator<View0,pc0,View1,pc1,CtrlView>& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    b.update(home,share,p.b);
  }

  template<class View0, PropCond pc0, class View1, PropCond pc1,
            class CtrlView>
  forceinline
  ReMixBinaryPropagator<View0,pc0,View1,pc1,CtrlView>
  ::ReMixBinaryPropagator
  (Space& home, bool share, Propagator& p, View0 y0, View1 y1, CtrlView b0)
    : Propagator(home,share,p) {
    x0.update(home,share,y0);
    x1.update(home,share,y1);
    b.update(home,share,b0);
  }

  template<class View0, PropCond pc0, class View1, PropCond pc1,
            class CtrlView>
  PropCost
  ReMixBinaryPropagator<View0,pc0,View1,pc1,CtrlView>
  ::cost(const Space&, const ModEventDelta&) const {
    return PropCost::binary(PropCost::LO);
  }

  template<class View0, PropCond pc0, class View1, PropCond pc1,
            class CtrlView>
  forceinline size_t
  ReMixBinaryPropagator<View0,pc0,View1,pc1,CtrlView>::dispose(Space& home) {
    if (pc0 != PC_GEN_NONE)
      x0.cancel(home,*this,pc0);
    if (pc1 != PC_GEN_NONE)
      x1.cancel(home,*this,pc1);
    b.cancel(home,*this,Int::PC_INT_VAL);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

}}

// STATISTICS: int-prop

