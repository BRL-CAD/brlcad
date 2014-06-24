/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
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

#ifndef __GECODE_KERNEL_WAIT_HH__
#define __GECODE_KERNEL_WAIT_HH__

#include <gecode/kernel.hh>

namespace Gecode { namespace Kernel {

  /**
   * \brief Wait propagator for single view
   *
   * Requires \code #include <gecode/kernel/wait.hh> \endcode
   * \ingroup FuncKernelProp
   */
  template<class View>
  class UnaryWait : public Propagator {
  protected:
    /// View to wait for becoming assigned
    View x;
    /// Continuation to execute
    void (*c)(Space&);
    /// Constructor for creation
    UnaryWait(Home home, View x, void (*c0)(Space&));
    /// Constructor for cloning \a p
    UnaryWait(Space& home, bool shared, UnaryWait& p);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Const function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator that waits until \a x becomes assigned and then executes \a c
    static ExecStatus post(Space& home, View x, void (*c)(Space&));
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Wait propagator for several views
   *
   * Requires \code #include <gecode/kernel/wait.hh> \endcode
   * \ingroup FuncKernelProp
   */
  template<class View>
  class NaryWait : public Propagator {
  protected:
    /// Views to wait for becoming assigned
    ViewArray<View> x;
    /// Continuation to execute
    void (*c)(Space&);
    /// Constructor for creation
    NaryWait(Home home, ViewArray<View>& x, void (*c0)(Space&));
    /// Constructor for cloning \a p
    NaryWait(Space& home, bool shared, NaryWait& p);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Const function (defined as high unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator that waits until \a x becomes assigned and then executes \a c
    static ExecStatus post(Space& home, ViewArray<View>& x, void (*c)(Space&));
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };


  /*
   * Wait propagator for single view
   *
   */
  template<class View>
  forceinline
  UnaryWait<View>::UnaryWait(Home home, View x0, void (*c0)(Space&))
    : Propagator(home), x(x0), c(c0) {
    x.subscribe(home,*this,PC_GEN_ASSIGNED);
  }
  template<class View>
  forceinline
  UnaryWait<View>::UnaryWait(Space& home, bool shared, UnaryWait& p) 
    : Propagator(home,shared,p), c(p.c) {
    x.update(home,shared,p.x);
  }
  template<class View>
  Actor* 
  UnaryWait<View>::copy(Space& home, bool share) {
    return new (home) UnaryWait<View>(home,share,*this);
  }
  template<class View>
  PropCost 
  UnaryWait<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::LO);
  }
  template<class View>
  ExecStatus 
  UnaryWait<View>::propagate(Space& home, const ModEventDelta&) {
    assert(x.assigned());
    c(home);
    return home.failed() ? ES_FAILED : home.ES_SUBSUMED(*this);
  }
  template<class View>
  ExecStatus 
  UnaryWait<View>::post(Space& home, View x, void (*c)(Space&)) {
    if (x.assigned()) {
      c(home);
      return home.failed() ? ES_FAILED : ES_OK;
    } else {
      (void) new (home) UnaryWait<View>(home,x,c);
      return ES_OK;
    }
  }
  template<class View>
  size_t 
  UnaryWait<View>::dispose(Space& home) {
    x.cancel(home,*this,PC_GEN_ASSIGNED);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }


  /*
   * Wait propagator for several views
   *
   */
  template<class View>
  forceinline
  NaryWait<View>::NaryWait(Home home, ViewArray<View>& x0, 
                           void (*c0)(Space&))
    : Propagator(home), x(x0), c(c0) {
    assert(!x[0].assigned());
    x[0].subscribe(home,*this,PC_GEN_ASSIGNED);
  }
  template<class View>
  forceinline
  NaryWait<View>::NaryWait(Space& home, bool shared, NaryWait& p) 
    : Propagator(home,shared,p), c(p.c) {
    x.update(home,shared,p.x);
  }
  template<class View>
  Actor* 
  NaryWait<View>::copy(Space& home, bool share) {
    assert(!x[0].assigned());
    for (int i=x.size()-1; i>0; i--)
      if (x[i].assigned())
        x.move_lst(i);
    assert(x.size() > 0);
    return new (home) NaryWait<View>(home,share,*this);
  }
  template<class View>
  PropCost 
  NaryWait<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::HI);
  }
  template<class View>
  ExecStatus 
  NaryWait<View>::propagate(Space& home, const ModEventDelta& ) {
    assert(x[0].assigned());
    for (int i=x.size()-1; i>0; i--)
      if (x[i].assigned())
        x.move_lst(i);
    assert(x.size() > 0);
    if (x.size() == 1) {
      x.size(0);
      c(home);
      return home.failed() ? ES_FAILED : home.ES_SUBSUMED(*this);
    } else {
      // Create new subscription
      x.move_lst(0);
      assert(!x[0].assigned());
      x[0].subscribe(home,*this,PC_GEN_ASSIGNED,false);
      return ES_OK;
    }
  }
  template<class View>
  ExecStatus 
  NaryWait<View>::post(Space& home, ViewArray<View>& x, void (*c)(Space&)) {
    for (int i=x.size(); i--; )
      if (x[i].assigned())
        x.move_lst(i);
    if (x.size() == 0) {
      c(home);
      return home.failed() ? ES_FAILED : ES_OK;
    } else {
      x.unique(home);
      if (x.size() == 1) {
        return UnaryWait<View>::post(home,x[0],c);
      } else {
        (void) new (home) NaryWait<View>(home,x,c);
        return ES_OK;
      }
    }
  }
  template<class View>
  size_t 
  NaryWait<View>::dispose(Space& home) {
    if (x.size() > 0) 
      x[0].cancel(home,*this,PC_GEN_ASSIGNED);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

}}

#endif

// STATISTICS: kernel-prop
