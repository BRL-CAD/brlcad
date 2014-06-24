/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
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

namespace Gecode { namespace Set {

  forceinline
  SingletonView::SingletonView(void) {}

  forceinline
  SingletonView::SingletonView(Gecode::Int::IntView& y)
    : DerivedView<Gecode::Int::IntView>(y) {}

  forceinline
  SingletonView::SingletonView(const Gecode::IntVar& y)
    : DerivedView<Gecode::Int::IntView>(y) {}

  forceinline PropCond
  SingletonView::pc_settoint(PropCond pc) {
    switch(pc) {
    case PC_SET_VAL:
    case PC_SET_CGLB:
    case PC_SET_CARD:
      return Gecode::Int::PC_INT_VAL;
    default:
      return Gecode::Int::PC_INT_DOM;
    }
  }

  forceinline ModEvent
  SingletonView::me_inttoset(ModEvent me) {
    switch(me) {
    case Gecode::Int::ME_INT_FAILED:
      return ME_SET_FAILED;
    case Gecode::Int::ME_INT_NONE:
      return ME_SET_NONE;
    case Gecode::Int::ME_INT_VAL:
      return ME_SET_VAL;
    case Gecode::Int::ME_INT_DOM:
      return ME_SET_LUB;
    default:
      return ME_SET_LUB;
    }
  }

  forceinline ModEvent
  SingletonView::me_settoint(ModEvent me) {
    switch(me) {
    case ME_SET_FAILED:
      return Gecode::Int::ME_INT_FAILED;
    case ME_SET_NONE:
      return Gecode::Int::ME_INT_NONE;
    case ME_SET_VAL:
      return Gecode::Int::ME_INT_VAL;
    default:
      return Gecode::Int::ME_INT_DOM;
    }
  }

  forceinline unsigned int
  SingletonView::glbSize(void) const { 
    return x.assigned() ? 1U : 0U; 
  }

  forceinline unsigned int
  SingletonView::lubSize(void) const { return x.size(); }

  forceinline unsigned int
  SingletonView::unknownSize(void) const {
    return lubSize() - glbSize();
  }

  forceinline bool
  SingletonView::contains(int n) const { return x.assigned() ?
      (x.val()==n) : false; }

  forceinline bool
  SingletonView::notContains(int n) const { return !x.in(n); }

  forceinline unsigned int
  SingletonView::cardMin() const { return 1; }

  forceinline unsigned int
  SingletonView::cardMax() const { return 1; }

  forceinline int
  SingletonView::lubMin() const { return x.min(); }

  forceinline int
  SingletonView::lubMax() const { return x.max(); }

  forceinline int
  SingletonView::glbMin() const { return x.assigned() ?
      x.val() : BndSet::MIN_OF_EMPTY; }

  forceinline int
  SingletonView::glbMax() const { return x.assigned() ?
      x.val() : BndSet::MAX_OF_EMPTY; }

  forceinline ModEvent
  SingletonView::cardMin(Space&, unsigned int c) {
    return c<=1 ? ME_SET_NONE : ME_SET_FAILED;
  }

  forceinline ModEvent
  SingletonView::cardMax(Space&, unsigned int c) {
    return c<1 ? ME_SET_FAILED : ME_SET_NONE;
  }

  forceinline ModEvent
  SingletonView::include(Space& home,int c) {
    return me_inttoset(x.eq(home,c));
  }

  forceinline ModEvent
  SingletonView::intersect(Space& home,int c) {
    return me_inttoset(x.eq(home,c));
  }

  forceinline ModEvent
  SingletonView::intersect(Space& home,int i, int j) {
    ModEvent me1 = me_inttoset(x.gq(home,i));
    ModEvent me2 = me_inttoset(x.lq(home,j));
    if (me_failed(me1) || me_failed(me2))
      return ME_SET_FAILED;
    switch (me1) {
    case ME_SET_NONE:
    case ME_SET_LUB:
      return me2;
    case ME_SET_VAL:
      return ME_SET_VAL;
    default:
      GECODE_NEVER;
      return ME_SET_VAL;
    }
  }

  forceinline ModEvent
  SingletonView::exclude(Space& home,int c) {
    return me_inttoset(x.nq(home,c));
  }

  forceinline ModEvent
  SingletonView::include(Space& home, int j, int k) {
    return j==k ? me_inttoset(x.eq(home,j)) : ME_SET_FAILED ;
  }

  forceinline ModEvent
  SingletonView::exclude(Space& home, int j, int k) {
    ModEvent me1 = me_inttoset(x.gr(home,j));
    ModEvent me2 = me_inttoset(x.le(home,k));
    if (me_failed(me1) || me_failed(me2))
      return ME_SET_FAILED;
    switch (me1) {
    case ME_SET_NONE:
    case ME_SET_LUB:
      return me2;
    case ME_SET_VAL:
      return ME_SET_VAL;
    default:
      GECODE_NEVER;
      return ME_SET_VAL;
    }
  }

  template<class I> ModEvent
  SingletonView::excludeI(Space& home, I& iter) {
    return me_inttoset(x.minus_r(home,iter));
  }

  template<class I> ModEvent
  SingletonView::includeI(Space& home, I& iter) {
    if (!iter())
      return ME_SET_NONE;

    if (iter.min()!=iter.max())
      return ME_SET_FAILED;

    int val = iter.min();
    ++iter;
    if ( iter() )
      return ME_SET_FAILED;

    return me_inttoset(x.eq(home, val));
  }

  template<class I> ModEvent
  SingletonView::intersectI(Space& home, I& iter) {
    return me_inttoset(x.inter_r(home,iter));
  }

  forceinline void
  SingletonView::subscribe(Space& home, Propagator& p, PropCond pc,
                           bool schedule) {
    x.subscribe(home,p,pc_settoint(pc),schedule);
  }
  forceinline void
  SingletonView::cancel(Space& home, Propagator& p, PropCond pc) {
    x.cancel(home,p,pc_settoint(pc));
  }

  forceinline void
  SingletonView::subscribe(Space& home, Advisor& a) {
    x.subscribe(home,a);
  }
  forceinline void
  SingletonView::cancel(Space& home, Advisor& a) {
    x.cancel(home,a);
  }


  forceinline void
  SingletonView::schedule(Space& home, Propagator& p, ModEvent me) {
    return Gecode::Int::IntView::schedule(home,p,me_settoint(me));
  }
  forceinline ModEvent
  SingletonView::me(const ModEventDelta& med) {
    return me_inttoset(Int::IntView::me(med));
  }
  forceinline ModEventDelta
  SingletonView::med(ModEvent me) {
    return SetView::med(me_settoint(me));
  }


  /*
   * Delta information for advisors
   *
   * For SingletonViews, a glb change means that the view is assigned.
   * Thus, the delta for the glb is always equal to the delta for the lub.
   *
   */

  forceinline ModEvent
  SingletonView::modevent(const Delta& d) {
    return me_inttoset(Int::IntView::modevent(d));
  }

  forceinline int
  SingletonView::glbMin(const Delta& d) const { return x.min(d); }

  forceinline int
  SingletonView::glbMax(const Delta& d) const { return x.max(d); }

  forceinline bool
  SingletonView::glbAny(const Delta& d) const { return x.any(d); }

  forceinline int
  SingletonView::lubMin(const Delta& d) const { return x.min(d); }

  forceinline int
  SingletonView::lubMax(const Delta& d) const { return x.max(d); }

  forceinline bool
  SingletonView::lubAny(const Delta& d) const { return x.any(d); }

  /*
   * Iterators
   *
   */

  /**
   * \brief %Range iterator for least upper bound of singleton set view
   * \ingroup TaskActorSetView
   */
  template<>
  class LubRanges<SingletonView> : public Gecode::Int::IntVarImpFwd {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    LubRanges(void);
    /// Initialize with ranges for view \a x
    LubRanges(const SingletonView& x);
    /// Initialize with ranges for view \a x
    void init(const SingletonView& x);
    //@}
  };

  forceinline
  LubRanges<SingletonView>::LubRanges(void) {}

  forceinline
  LubRanges<SingletonView>::LubRanges(const SingletonView& s) :
    Gecode::Int::IntVarImpFwd(s.base().varimp()) {}

  forceinline void
  LubRanges<SingletonView>::init(const SingletonView& s) {
    Gecode::Int::IntVarImpFwd::init(s.base().varimp());
  }

  /**
   * \brief %Range iterator for greatest lower bound of singleton set view
   * \ingroup TaskActorSetView
   */
  template<>
  class GlbRanges<SingletonView> {
  private:
    int  val;
    bool flag;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    GlbRanges(void);
    /// Initialize with ranges for view \a x
    GlbRanges(const SingletonView& x);
    /// Initialize with ranges for view \a x
    void init(const SingletonView& x);

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}

    /// \name Range access
    //@{
    /// Return smallest value of range
    int min(void) const;
    /// Return largest value of range
    int max(void) const;
    /// Return width of ranges (distance between minimum and maximum)
    unsigned int width(void) const;
    //@}
  };

  forceinline
  GlbRanges<SingletonView>::GlbRanges(void) {}

  forceinline void
  GlbRanges<SingletonView>::init(const SingletonView& s) {
    if (s.base().assigned()) {
      val = s.base().val();
      flag = true;
    } else {
      val = 0;
      flag = false;
    }
  }

  forceinline
  GlbRanges<SingletonView>::GlbRanges(const SingletonView& s) {
    init(s);
  }

  forceinline bool
  GlbRanges<SingletonView>::operator ()(void) const { return flag; }

  forceinline void
  GlbRanges<SingletonView>::operator ++(void) { flag=false; }

  forceinline int
  GlbRanges<SingletonView>::min(void) const { return val; }
  forceinline int
  GlbRanges<SingletonView>::max(void) const { return val; }
  forceinline unsigned int
  GlbRanges<SingletonView>::width(void) const { return 1; }

}}

// STATISTICS: set-var

