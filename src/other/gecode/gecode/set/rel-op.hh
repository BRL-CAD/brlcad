/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
 *     Gabor Szokoli, 2004
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

#ifndef __GECODE_SET_RELOP_HH__
#define __GECODE_SET_RELOP_HH__

#include <gecode/set.hh>
#include <gecode/set/rel.hh>

namespace Gecode { namespace Set { namespace RelOp {

  /**
   * \namespace Gecode::Set::RelOp
   * \brief Standard set operation propagators
   */

  /**
   * \brief %Propagator for the superset of intersection
   *
   * Requires \code #include <gecode/set/rel-op.hh> \endcode
   * \ingroup FuncSetProp
   */

  template<class View0, class View1, class View2>
  class SuperOfInter :
    public MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                View2,PC_SET_CLUB> {
  protected:
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                               View2,PC_SET_CLUB>::x0;
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                               View2,PC_SET_CLUB>::x1;
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                               View2,PC_SET_CLUB>::x2;
    /// Constructor for cloning \a p
    SuperOfInter(Space& home, bool share,SuperOfInter& p);
    /// Constructor for posting
    SuperOfInter(Home home,View0, View1, View2);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ z \supseteq x \cap y\f$
    static  ExecStatus  post(Home home, View0 x, View1 y, View2 z);
  };

  /**
   * \brief %Propagator for the subset of union
   *
   * Requires \code #include <gecode/set/rel-op.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0, class View1, class View2>
  class SubOfUnion :
    public MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                View2,PC_SET_ANY> {
  protected:
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                 View2,PC_SET_ANY>::x0;
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                 View2,PC_SET_ANY>::x1;
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                 View2,PC_SET_ANY>::x2;
    /// Constructor for cloning \a p
    SubOfUnion(Space& home, bool share,SubOfUnion& p);
    /// Constructor for posting
    SubOfUnion(Home home,View0, View1, View2);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ z \subseteq x \cap y\f$
    static  ExecStatus  post(Home home,View0 x,View1 y,View2 z);
  };


   /**
    * \brief %Propagator for ternary intersection
    *
    * Requires \code #include <gecode/set/rel-op.hh> \endcode
    * \ingroup FuncSetProp
    */
  template<class View0, class View1, class View2>
  class Intersection:
    public MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                View2,PC_SET_ANY> {
  protected:
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                               View2,PC_SET_ANY>::x0;
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                               View2,PC_SET_ANY>::x1;
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                               View2,PC_SET_ANY>::x2;
    /// Constructor for cloning \a p
    Intersection(Space& home, bool share,Intersection& p);
    /// Constructor for posting
    Intersection(Home home,View0,View1,View2);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ z=x\cap y\f$
    static  ExecStatus  post(Home home,View0 x,View1 y,View2 z);
  };

  /**
   * \brief %Propagator for ternary union
   *
   * Requires \code #include <gecode/set/rel-op.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0, class View1, class View2>
  class Union:
    public MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                View2,PC_SET_ANY> {
  protected:
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                 View2,PC_SET_ANY>::x0;
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                 View2,PC_SET_ANY>::x1;
    using MixTernaryPropagator<View0,PC_SET_ANY,View1,PC_SET_ANY,
                                 View2,PC_SET_ANY>::x2;
    /// Constructor for cloning \a p
    Union(Space& home, bool share,Union& p);
    /// Constructor for posting
    Union(Home home,View0,View1,View2);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ z=x\cup y\f$
    static  ExecStatus  post(Home home,View0 x,View1 y,View2 z);
  };

   /**
    * \brief %Propagator for nary intersection
    *
    * Requires \code #include <gecode/set/rel-op.hh> \endcode
    * \ingroup FuncSetProp
    */
  template<class View0, class View1>
  class IntersectionN : 
    public MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY> {
  protected:
    using MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::x;
    using MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::y;
    /// Whether the any views share a variable implementation
    bool shared;
    /// Intersection of the determined \f$x_i\f$ (which are dropped)
    LUBndSet intOfDets;
    /// Constructor for cloning \a p
    IntersectionN(Space& home, bool share,IntersectionN& p);
    /// Constructor for posting
    IntersectionN(Home home,ViewArray<View0>&, View1);
    /// Constructor for posting
    IntersectionN(Home home,ViewArray<View0>&, const IntSet&, View1);
  public:
    virtual PropCost    cost(const Space& home, const ModEventDelta& med) const;
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /// Post propagator \f$ x = \bigcap_{i\in\{0,\dots,n-1\}} y_i \f$
    static  ExecStatus  post(Home home,ViewArray<View0>& y,View1 x);
    /// Post propagator \f$ x = z\cap\bigcap_{i\in\{0,\dots,n-1\}} y_i \f$
    static  ExecStatus  post(Home home,ViewArray<View0>& y,
                             const IntSet& z,View1 x);
  };

  /**
   * \brief %Propagator for nary union
   *
   * Requires \code #include <gecode/set/rel-op.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0, class View1>
  class UnionN : public MixNaryOnePropagator<View0,PC_SET_ANY,
                                             View1,PC_SET_ANY> {
  protected:
    using MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::x;
    using MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::y;
    /// Whether the any views share a variable implementation
    bool shared;
    /// Union of the determined \f$x_i\f$ (which are dropped)
    GLBndSet unionOfDets;
    /// Constructor for cloning \a p
    UnionN(Space& home, bool share,UnionN& p);
    /// Constructor for posting
    UnionN(Home home,ViewArray<View0>&,View1);
    /// Constructor for posting
    UnionN(Home home,ViewArray<View0>&,const IntSet&,View1);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home, bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    virtual PropCost    cost(const Space& home, const ModEventDelta& med) const;
    /// Post propagator \f$ x = \bigcup_{i\in\{0,\dots,n-1\}} y_i \f$
    static  ExecStatus  post(Home home,ViewArray<View0>& y,View1 x);
    /// Post propagator \f$ x = z\cup\bigcup_{i\in\{0,\dots,n-1\}} y_i \f$
    static  ExecStatus  post(Home home,ViewArray<View0>& y,
                             const IntSet& z,View1 x);
  };


  /**
   * \brief %Propagator for nary partition
   *
   * Requires \code #include <gecode/set/rel-op.hh> \endcode
   * \ingroup FuncSetProp
   */
  template<class View0, class View1>
  class PartitionN : public MixNaryOnePropagator<View0,PC_SET_ANY,
                                                 View1,PC_SET_ANY> {
  protected:
    using MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::x;
    using MixNaryOnePropagator<View0,PC_SET_ANY,View1,PC_SET_ANY>::y;
    /// Whether the any views share a variable implementation
    bool shared;
    /// Union of the determined \f$x_i\f$ (which are dropped)
    GLBndSet unionOfDets;
    /// Constructor for cloning \a p
    PartitionN(Space& home, bool share,PartitionN& p);
    /// Constructor for posting
    PartitionN(Home home,ViewArray<View0>&, View1);
    /// Constructor for posting
    PartitionN(Home home,ViewArray<View0>&, const IntSet&, View1);
  public:
    /// Copy propagator during cloning
    virtual Actor*      copy(Space& home,bool);
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    virtual PropCost    cost(const Space& home, const ModEventDelta& med) const;
    /// Post propagator \f$ x = \biguplus_{i\in\{0,\dots,n-1\}} y_i \f$
    static  ExecStatus  post(Home home,ViewArray<View0>& y,View1 x);
    /// Post propagator \f$ x = z\uplus\biguplus_{i\in\{0,\dots,n-1\}} y_i \f$
    static  ExecStatus  post(Home home,ViewArray<View0>& y,
                             const IntSet& z,View1 x);
  };

}}}

#include <gecode/set/rel-op/common.hpp>
#include <gecode/set/rel-op/superofinter.hpp>
#include <gecode/set/rel-op/subofunion.hpp>
#include <gecode/set/rel-op/inter.hpp>
#include <gecode/set/rel-op/union.hpp>
#include <gecode/set/rel-op/partition.hpp>
#include <gecode/set/rel-op/post.hpp>

#endif

// STATISTICS: set-prop
