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

#include <gecode/int/div.hh>

namespace Gecode { namespace Int {

  /*
   * Constructors and initialization
   *
   */
  template<class Val, class UnsVal>
  forceinline
  ScaleView<Val,UnsVal>::ScaleView(void) {}

  template<class Val, class UnsVal>
  forceinline
  ScaleView<Val,UnsVal>::ScaleView(int b, const IntView& y)
    : DerivedView<IntView>(y), a(b) {}


  /*
   * Value access
   *
   */
  template<class Val, class UnsVal>
  forceinline int
  ScaleView<Val,UnsVal>::scale(void) const {
    return a;
  }
  template<class Val, class UnsVal>
  forceinline Val
  ScaleView<Val,UnsVal>::min(void) const {
    return static_cast<Val>(x.min()) * a;
  }

  template<class Val, class UnsVal>
  forceinline Val
  ScaleView<Val,UnsVal>::max(void) const {
    return static_cast<Val>(x.max()) * a;
  }

  template<class Val, class UnsVal>
  forceinline Val
  ScaleView<Val,UnsVal>::med(void) const {
    return static_cast<Val>(x.med()) * a;
  }

  template<class Val, class UnsVal>
  forceinline Val
  ScaleView<Val,UnsVal>::val(void) const {
    return static_cast<Val>(x.val()) * a;
  }

  template<class Val, class UnsVal>
  forceinline UnsVal
  ScaleView<Val,UnsVal>::size(void) const {
    return static_cast<UnsVal>(x.size());
  }

  template<class Val, class UnsVal>
  forceinline UnsVal
  ScaleView<Val,UnsVal>::width(void) const {
    return static_cast<UnsVal>(x.width()) * a;
  }

  template<class Val, class UnsVal>
  forceinline UnsVal
  ScaleView<Val,UnsVal>::regret_min(void) const {
    return static_cast<UnsVal>(x.regret_min()) * a;
  }

  template<class Val, class UnsVal>
  forceinline UnsVal
  ScaleView<Val,UnsVal>::regret_max(void) const {
    return static_cast<UnsVal>(x.regret_max()) * a;
  }


  /*
   * Domain tests
   *
   */
  template<class Val, class UnsVal>
  forceinline bool
  ScaleView<Val,UnsVal>::range(void) const {
    return x.range();
  }
  template<class Val, class UnsVal>
  forceinline bool
  ScaleView<Val,UnsVal>::in(Val n) const {
    return ((n % a) == 0) && x.in(n / a);
  }




  /*
   * Domain update by value
   *
   */
  template<class Val, class UnsVal>
  forceinline ModEvent
  ScaleView<Val,UnsVal>::lq(Space& home, Val n) {
    return (n >= max()) ? ME_INT_NONE : 
      x.lq(home,floor_div_xp(n,static_cast<Val>(a)));
  }

  template<class Val, class UnsVal>
  forceinline ModEvent
  ScaleView<Val,UnsVal>::le(Space& home, Val n) {
    return (n > max()) ? ME_INT_NONE : 
      x.le(home,floor_div_xp(n,static_cast<Val>(a)));
  }

  template<class Val, class UnsVal>
  forceinline ModEvent
  ScaleView<Val,UnsVal>::gq(Space& home, Val n) {
    return (n <= min()) ? ME_INT_NONE : 
      x.gq(home,ceil_div_xp(n,static_cast<Val>(a)));
  }
  template<class Val, class UnsVal>
  forceinline ModEvent
  ScaleView<Val,UnsVal>::gr(Space& home, Val n) {
    return (n < min()) ? ME_INT_NONE : 
      x.gr(home,ceil_div_xp(n,static_cast<Val>(a)));
  }

  template<class Val, class UnsVal>
  forceinline ModEvent
  ScaleView<Val,UnsVal>::nq(Space& home, Val n) {
    return ((n % a) == 0) ? x.nq(home,n/a) :  ME_INT_NONE;
  }

  template<class Val, class UnsVal>
  forceinline ModEvent
  ScaleView<Val,UnsVal>::eq(Space& home, Val n) {
    return ((n % a) == 0) ? x.eq(home,n/a) : ME_INT_FAILED;
  }


  /*
   * Propagator modification events
   *
   */
  template<class Val, class UnsVal>
  forceinline ModEventDelta
  ScaleView<Val,UnsVal>::med(ModEvent me) {
    return IntView::med(me);
  }



  /*
   * Delta information for advisors
   *
   */
  template<class Val, class UnsVal>
  forceinline Val
  ScaleView<Val,UnsVal>::min(const Delta& d) const {
    return static_cast<Val>(x.min(d)) * a;
  }
  template<class Val, class UnsVal>
  forceinline Val
  ScaleView<Val,UnsVal>::max(const Delta& d) const {
    return static_cast<Val>(x.max(d)) * a;
  }
  template<class Val, class UnsVal>
  forceinline bool
  ScaleView<Val,UnsVal>::any(const Delta& d) const {
    return x.any(d);
  }



  /*
   * Cloning
   *
   */
  template<class Val, class UnsVal>
  forceinline void
  ScaleView<Val,UnsVal>::update(Space& home, bool share,
                                ScaleView<Val,UnsVal>& y) {
    DerivedView<IntView>::update(home,share,y);
    a=y.a;
  }



  /**
   * \brief %Range iterator for integer-precision scale integer views
   * \ingroup TaskActorIntView
   */
  template<>
  class ViewRanges<IntScaleView>
    : public Iter::Ranges::ScaleUp<int,unsigned int,ViewRanges<IntView> > {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const IntScaleView& x);
    /// Initialize with ranges for view \a x
    void init(const IntScaleView& x);
    //@}
  };

  forceinline
  ViewRanges<IntScaleView>::ViewRanges(void) {}
  forceinline
  ViewRanges<IntScaleView>::ViewRanges(const IntScaleView& x) {
    ViewRanges<IntView> xi(x.base());
    Iter::Ranges::ScaleUp<int,unsigned int,ViewRanges<IntView> >::init
      (xi,x.scale());
  }
  forceinline void
  ViewRanges<IntScaleView>::init(const IntScaleView& x) {
    ViewRanges<IntView> xi(x.base());
    Iter::Ranges::ScaleUp<int,unsigned int,ViewRanges<IntView> >::init
      (xi,x.scale());
  }


  /**
   * \brief %Range iterator for long long int-precision scale integer views
   * \ingroup TaskActorIntView
   */
  template<>
  class ViewRanges<LLongScaleView>
    : public Iter::Ranges::ScaleUp<long long int,unsigned long long int,
                                   ViewRanges<IntView> > {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const LLongScaleView& x);
    /// Initialize with ranges for view \a x
    void init(const LLongScaleView& x);
    //@}
  };

  forceinline
  ViewRanges<LLongScaleView>::ViewRanges(void) {}
  forceinline
  ViewRanges<LLongScaleView>::ViewRanges(const LLongScaleView& x) {
    ViewRanges<IntView> xi(x.base());
    Iter::Ranges::ScaleUp<long long int,unsigned long long int,
      ViewRanges<IntView> >::init(xi,x.scale());
  }
  forceinline void
  ViewRanges<LLongScaleView>::init(const LLongScaleView& x) {
    ViewRanges<IntView> xi(x.base());
    Iter::Ranges::ScaleUp<long long int,unsigned long long int,
      ViewRanges<IntView> >::init(xi,x.scale());
  }


  /*
   * View comparison
   *
   */
  template<class Val, class UnsVal>
  forceinline bool
  same(const ScaleView<Val,UnsVal>& x, const ScaleView<Val,UnsVal>& y) {
    return same(x.base(),y.base()) && (x.scale() == y.scale());
  }
  template<class Val, class UnsVal>
  forceinline bool
  before(const ScaleView<Val,UnsVal>& x, const ScaleView<Val,UnsVal>& y) {
    return before(x.base(),y.base())
      || (same(x.base(),y.base()) && (x.scale() < y.scale()));
  }

}}

// STATISTICS: int-var

