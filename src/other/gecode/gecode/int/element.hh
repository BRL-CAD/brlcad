/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
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

#ifndef __GECODE_INT_ELEMENT_HH__
#define __GECODE_INT_ELEMENT_HH__

#include <gecode/int.hh>
#include <gecode/int/rel.hh>

/**
 * \namespace Gecode::Int::Element
 * \brief %Element propagators
 */

namespace Gecode { namespace Int { namespace Element {

  /**
   * \brief %Element propagator for array of integers
   *
   * Requires \code #include <gecode/int/element.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class V0, class V1, class Idx, class Val>
  class Int : public Propagator {
  protected:
    /**
     * \brief Linked index-value pairs
     *
     * Data structure linking pairs of index and value (index,value)
     * where pairs are linked in order of both index and
     * value (to allow for easy removal while keeping both index and
     * value sorted).
     */
    class IdxVal  {
    public:
      Idx idx_next; ///< The position of the next pair in index order
      Idx val_next; ///< The position of the next pair in value order
      Idx idx; ///< The index
      Val val; ///< The value
      /// Mark that this pair should be removed
      void mark(void);
      /// Return whether this pair is marked for removal
      bool marked(void) const;
    };
    /**
     * \brief Value iterator for indices in index-value map
     *
     * The iterator also removes marked index-value pairs.
     *
     */
    class IterIdxUnmark {
    private:
      IdxVal* iv; ///< The index value data structure
      Idx i; ///< Current index value pair
    public:
      /// Initialize with start
      IterIdxUnmark(IdxVal* iv);
      /// Test whether more pairs to be iterated
      bool operator ()(void) const;
      /// Move to next index value pair (next index)
      void operator ++(void);
      /// Return index of current index value pair
      Idx val(void) const;
    };
    /**
     * \brief Value iterator for values in index-value map
     *
     * Note that the iterated value sequence is not strictly
     * increasing (might contain duplicates).
     */
    class IterVal {
    private:
      IdxVal* iv; ///< The index value data structure
      Idx i; ///< Current index value pair
    public:
      /// Initialize with start
      IterVal(IdxVal* iv);
      /// Test whether more pairs to be iterated
      bool operator ()(void) const;
      /// Move to next index value pair (next value)
      void operator ++(void);
      /// Return value of current index value pair
      Val val(void) const;
    };
    /**
     * \brief Value iterator for values in index-value map
     *
     * Note that the iterated value sequence is not strictly
     * increasing (might contain duplicates).
     *
     * The iterator also removes marked index-value pairs.
     */
    class IterValUnmark {
    private:
      IdxVal* iv; ///< The index value data structure
      Idx i; ///< Current index value pair
    public:
      /// Initialize with start
      IterValUnmark(IdxVal* iv);
      /// Test whether more pairs to be iterated
      bool operator ()(void) const;
      /// Move to next index value pair (next value)
      void operator ++(void);
      /// Return value of current index value pair
      Val val(void) const;
    };
    /// Sorting pointers to (index,value) pairs in value order
    class ByVal {
    protected:
      const IdxVal* iv; ///< Index-value pairs
    public:
      /// Initialize with index value pairs
      ByVal(const IdxVal* iv);
      /// Compare pairs at positions \a i and \a j
      bool operator ()(Idx& i, Idx& j);
    };

    /// View for index
    V0 x0;
    /// Type for index size
    typedef typename Gecode::Support::IntTypeTraits<Idx>::utype IdxSize;
    /// Size of \a x0 at last execution
    IdxSize s0;
    /// View for result
    V1 x1;
    /// Type for value size
    typedef typename Gecode::Support::IntTypeTraits<Val>::utype ValSize;
    /// Size of \a x1 at last execution
    ValSize s1;
    /// Shared array of integer values
    IntSharedArray c;
    /// The index-value data structure
    IdxVal* iv;
    /// Prune index according to \a x0
    void prune_idx(void);
    /// Prune values according to \a x1
    void prune_val(void);
    /// Prune when \a x1 is assigned
    static ExecStatus assigned_val(Space& home, IntSharedArray& c, 
                                   V0 x0, V1 x1);
    /// Constructor for cloning \a p
    Int(Space& home, bool shared, Int& p);
    /// Constructor for creation
    Int(Home home, IntSharedArray& i, V0 x0, V1 x1);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Const function (defined as high binary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$i_{x_0}=x_1\f$
    static  ExecStatus post(Home home, IntSharedArray& i, V0 x0, V1 x1);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /// Post propagator with apropriate index and value types
  template<class V0, class V1>
  ExecStatus post_int(Home home, IntSharedArray& c, V0 x0, V1 x1);


  /**
   * \brief Class for index-view map
   *
   */
  template<class ViewB> class IdxView;

  /// Class to get VarArg type for view
  template<class View>
  class ViewToVarArg {};

  /**
   * \brief An array of IndexView pairs
   *
   */
  template<class View>
  class IdxViewArray {
  private:
    /// The actual array
    IdxView<View>* xs;
    /// The size of the array
    int n;
  public:
    /// Default constructor
    IdxViewArray(void);
    /// Copy constructor
    IdxViewArray(const IdxViewArray<View>&);
    /// Construct an IdxViewArray from \a x
    IdxViewArray(Space& home, const typename ViewToVarArg<View>::argtype& x);
    /// Construct an IdxViewArray of size \a n
    IdxViewArray(Space& home, int n);

    /// Return the current size
    int size(void) const;
    /// Set the size to \a n
    void size(int n);

    /// Access element \a n
    IdxView<View>& operator [](int n);
    /// Access element \a n
    const IdxView<View>& operator [](int) const;

    /**
     * Subscribe propagator \a p with propagation condition \a pc
     * to all elements of the array.
     */
    void subscribe(Space& home, Propagator& p, PropCond pc, bool process=true);
    /**
     * Cancel subscription of propagator \a p with propagation condition \a pc
     * for all elements of the array.
     */
    void cancel(Space& home, Propagator& p, PropCond pc);

    /// Cloning
    void update(Space& home, bool share, IdxViewArray<View>& x);
  };

  /**
   * \brief Base-class for element propagator for array of views
   *
   */
  template<class VA, class VB, class VC, PropCond pc_ac>
  class View : public Propagator {
  protected:
    /// Current index-view map
    IdxViewArray<VA> iv;
    /// View for index
    VB x0;
    /// View for result
    VC x1;
    /// Constructor for cloning \a p
    View(Space& home, bool share, View& p);
    /// Constructor for creation
    View(Home home, IdxViewArray<VA>& iv, VB x0, VC x1);
  public:
    // Cost function (defined as low linear)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };


  /**
   * \brief Bounds consistent element propagator for array of views
   *
   * Requires \code #include <gecode/int/element.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VA, class VB, class VC>
  class ViewBnd : public View<VA,VB,VC,PC_INT_BND> {
  protected:
    using View<VA,VB,VC,PC_INT_BND>::iv;
    using View<VA,VB,VC,PC_INT_BND>::x0;
    using View<VA,VB,VC,PC_INT_BND>::x1;

    /// Constructor for cloning \a p
    ViewBnd(Space& home, bool share, ViewBnd& p);
    /// Constructor for creation
    ViewBnd(Home home, IdxViewArray<VA>& iv, VB x0, VC x1);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$iv_{x_0}=x_1\f$
    static  ExecStatus post(Home home, IdxViewArray<VA>& iv, VB x0, VC x1);
  };

  /**
   * \brief Domain consistent element propagator for array of views
   *
   * The propagator uses staging: first it uses
   * bounds-propagation on the array of views and the uses
   * domain-propagation on the array of views.
   *
   * Requires \code #include <gecode/int/element.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class VA, class VB, class VC>
  class ViewDom : public View<VA,VB,VC,PC_INT_DOM> {
  protected:
    using View<VA,VB,VC,PC_INT_DOM>::iv;
    using View<VA,VB,VC,PC_INT_DOM>::x0;
    using View<VA,VB,VC,PC_INT_DOM>::x1;

    /// Constructor for cloning \a p
    ViewDom(Space& home, bool share, ViewDom& p);
    /// Constructor for creation
    ViewDom(Home home, IdxViewArray<VA>& iv, VB x0, VC x1);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * If in stage for bounds-propagation defined as low linear,
     * otherwise as high linear.
     *
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$iv_{x_0}=x_1\f$
    static  ExecStatus post(Home home, IdxViewArray<VA>& iv,
                            VB x0, VC x1);
  };

  /**
   * \brief Domain consistent pair propagator
   *
   * Requires \code #include <gecode/int/element.hh> \endcode
   *
   * \ingroup FuncIntProp
   */
  class GECODE_VTABLE_EXPORT Pair 
    : public TernaryPropagator<IntView,PC_INT_DOM> {
  protected:
    using TernaryPropagator<IntView,PC_INT_DOM>::x0;
    using TernaryPropagator<IntView,PC_INT_DOM>::x1;
    using TernaryPropagator<IntView,PC_INT_DOM>::x2;
    /// Width
    int w;
    /// Constructor for cloning \a p
    Pair(Space& home, bool share, Pair& p);
  public:
    /// Constructor for posting
    Pair(Home home, IntView x0, IntView x1, IntView x2, int w);
    /// Post propagator \f$x_0+x_1\cdot w=x_2\f$
    static ExecStatus post(Home home, IntView x0, IntView x1, IntView x2, 
                           int w, int h);
    /// Copy propagator during cloning
    GECODE_INT_EXPORT virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    GECODE_INT_EXPORT virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };

}}}

#include <gecode/int/element/int.hpp>
#include <gecode/int/element/view.hpp>
#include <gecode/int/element/pair.hpp>

#endif


// STATISTICS: int-prop

