/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Denys Duchier <denys.duchier@univ-orleans.fr>
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Denys Duchier, 2011
 *     Guido Tack, 2011
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

#ifndef __GECODE_SET_CHANNEL_HH__
#define __GECODE_SET_CHANNEL_HH__

#include <gecode/set.hh>

namespace Gecode { namespace Set { namespace Channel {

  /**
   * \namespace Gecode::Set::Channel
   * \brief Channeling propagators for set variables
   */




  /**
   * \brief %Propagator for the sorted channel constraint
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View>
  class ChannelSorted : public Propagator {
  protected:
    /// SetView for the match
    View x0;
    /// IntViews that together form the set \a x0
    ViewArray<Gecode::Int::IntView> xs;

    /// Constructor for cloning \a p
    ChannelSorted(Space& home, bool share,ChannelSorted& p);
    /// Constructor for posting
    ChannelSorted(Home home, View, ViewArray<Gecode::Int::IntView>&);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home,bool);
    /// Cost function (defined as PC_LINEAR_LO)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete Propagator
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator that propagates that \a s contains the \f$x_i\f$, which are sorted in non-descending order
    static ExecStatus post(Home home, View s,
                           ViewArray<Gecode::Int::IntView>& x);
  };

  /**
   * \brief %Propagator for channelling between variable-value-dual models
   *
   * Implements channelling constraints between IntVars and SetVars.
   * For IntVars \f$x_0,\dots,x_n\f$ and SetVars \f$y_0,\dots,y_m\f$ it
   * propagates the constraint \f$x_i=j \Leftrightarrow i\in y_j\f$.
   *
   * Can be used to implement the "channelling constraints" for disjoint with
   * cardinalities from
   *   "Disjoint, Partition and Intersection Constraints for
   *    Set and Multiset Variables"
   *    Christian Bessiere, Emmanuel Hebrard, Brahim Hnich, Toby Walsh
   *    CP 2004
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View>
  class ChannelInt : public Propagator {
  protected:
    /// IntViews, \f$x_i\f$ reflects which set contains element \f$i\f$
    ViewArray<Gecode::Int::CachedView<Gecode::Int::IntView> > xs;
    /// SetViews that are constrained to be disjoint
    ViewArray<CachedView<View> > ys;

    /// Constructor for cloning \a p
    ChannelInt(Space& home, bool share,ChannelInt& p);
    /// Constructor for posting
    ChannelInt(Home home,
               ViewArray<Gecode::Int::CachedView<Gecode::Int::IntView> >&,
               ViewArray<CachedView<View> >&);
  public:
    /// Copy propagator during cloning
    virtual Actor*   copy(Space& home,bool);
    /// Cost function (defined as PC_QUADRATIC_LO)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$x_i=j \Leftrightarrow i\in y_j\f$
    static ExecStatus post(Home home,
                           ViewArray<Gecode::Int::CachedView<
                            Gecode::Int::IntView> >& x,
                           ViewArray<CachedView<View> >& y);
  };

  /**
   * \brief %Propagator for channelling between set variable and its
   * characteristic function
   *
   * Implements channelling constraints between BoolVar and a SetVar.
   * For BoolVars \f$x_0,\dots,x_n\f$ and SetVar \f$y\f$ it
   * propagates the constraint \f$x_i=1 \Leftrightarrow i\in y\f$.
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View>
  class ChannelBool
    : public MixNaryOnePropagator<Gecode::Int::BoolView,
                                  Gecode::Int::PC_BOOL_VAL,
                                  View,PC_GEN_NONE> {
  protected:
    typedef MixNaryOnePropagator<Gecode::Int::BoolView,
                                 Gecode::Int::PC_BOOL_VAL,
                                 View,PC_GEN_NONE> Super;
    using Super::x;
    using Super::y;

    /// Constructor for cloning \a p
    ChannelBool(Space& home, bool share,ChannelBool& p);
    /// Constructor for posting
    ChannelBool(Home home,ViewArray<Gecode::Int::BoolView>&,
                View);

    /// %Advisor storing a single index
    class IndexAdvisor : public Advisor {
    protected:
      /// The single index
      int idx;
    public:
      /// Constructor for creation
      template<class A>
      IndexAdvisor(Space& home, ChannelBool<View>& p, Council<A>& c,
                   int index);
      /// Constructor for cloning \a a
      IndexAdvisor(Space& home, bool share, IndexAdvisor& a);
      /// Access index
      int index(void) const;
      /// Delete advisor
      template<class A>
      void dispose(Space& home, Council<A>& c);
    };

    /// Council for managing advisors
    Council<IndexAdvisor> co;
    /// Accumulated delta information
    SetDelta delta;
    /// Accumulated zero Booleans
    GLBndSet zeros;
    /// Accumulated one Booleans
    GLBndSet ones;
    /// Flag whether propagation is currently running
    bool running;
  public:
    /// Copy propagator during cloning
    virtual Actor*   copy(Space& home,bool);
    /// Cost function (defined as PC_QUADRATIC_LO)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Give advice to propagator
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    /// Post propagator for \f$x_i=j \Leftrightarrow i\in y_j\f$
    static ExecStatus post(Home home,ViewArray<Gecode::Int::BoolView>& x,
                           View y);
  };

  /**
   * \brief %Propagator for successors/predecessors channelling
   *
   * Implements channelling constraints between 2 sequences of SetVars.
   * For SetVars \f$x_0,\dots,x_n\f$ and SetVars \f$y_0,\dots,y_m\f$ it
   * propagates the constraint \f$j\in x_i \Leftrightarrow i\in y_i\f$.
   * \f$x_i\f$ is the set of successors of \f$i\f$, and \f$y_j\f$ is the
   * set of predecessors of \f$j\f$.
   *
   * Requires \code #include <gecode/set/int.hh> \endcode
   * \ingroup FuncSetProp
   */

  template<typename View>
  class ChannelSet: public Propagator {
  protected:
    /// SetViews, \f$x_i\f$ reflects the successors of \f$i\f$
    ViewArray<CachedView<View> > xs;
    /// SetViews, \f$y_j\f$ reflects the predecessors of \f$j\f$
    ViewArray<CachedView<View> > ys;

    /// Constructor for cloning \a p
    ChannelSet(Space& home, bool share, ChannelSet& p);
    /// Constructor for posting
    ChannelSet(Home home,
               ViewArray<CachedView<View> >&,
               ViewArray<CachedView<View> >&);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool);
    /// Cost function (defined as PC_QUADRATIC_HI)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$j\in x_i \Leftrightarrow i\in y_j\f$
    static ExecStatus post(Home home,
                           ViewArray<CachedView<View> >& x,
                           ViewArray<CachedView<View> >& y);
  };

}}}

#include <gecode/set/channel/sorted.hpp>
#include <gecode/set/channel/int.hpp>
#include <gecode/set/channel/bool.hpp>
#include <gecode/set/channel/set.hpp>

#endif

// STATISTICS: set-prop
