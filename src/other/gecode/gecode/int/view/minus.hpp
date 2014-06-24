/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

  /*
   * Constructors and initialization
   *
   */
  forceinline
  MinusView::MinusView(void) {}
  forceinline
  MinusView::MinusView(const IntView& y)
    : DerivedView<IntView>(y) {}


  /*
   * Value access
   *
   */
  forceinline int
  MinusView::min(void) const {
    return -x.max();
  }
  forceinline int
  MinusView::max(void) const {
    return -x.min();
  }
  forceinline int
  MinusView::val(void) const {
    return -x.val();
  }

  forceinline unsigned int
  MinusView::width(void) const {
    return x.width();
  }
  forceinline unsigned int
  MinusView::size(void) const {
    return x.size();
  }
  forceinline unsigned int
  MinusView::regret_min(void) const {
    return x.regret_max();
  }
  forceinline unsigned int
  MinusView::regret_max(void) const {
    return x.regret_min();
  }


  /*
   * Domain tests
   *
   */
  forceinline bool
  MinusView::range(void) const {
    return x.range();
  }
  forceinline bool
  MinusView::in(int n) const {
    return x.in(-n);
  }
  forceinline bool
  MinusView::in(long long int n) const {
    return x.in(-n);
  }


  /*
   * Domain update by value
   *
   */
  forceinline ModEvent
  MinusView::lq(Space& home, int n) {
    return x.gq(home,-n);
  }
  forceinline ModEvent
  MinusView::lq(Space& home, long long int n) {
    return x.gq(home,-n);
  }

  forceinline ModEvent
  MinusView::le(Space& home, int n) {
    return x.gr(home,-n);
  }
  forceinline ModEvent
  MinusView::le(Space& home, long long int n) {
    return x.gr(home,-n);
  }

  forceinline ModEvent
  MinusView::gq(Space& home, int n) {
    return x.lq(home,-n);
  }
  forceinline ModEvent
  MinusView::gq(Space& home, long long int n) {
    return x.lq(home,-n);
  }

  forceinline ModEvent
  MinusView::gr(Space& home, int n) {
    return x.le(home,-n);
  }
  forceinline ModEvent
  MinusView::gr(Space& home, long long int n) {
    return x.le(home,-n);
  }

  forceinline ModEvent
  MinusView::nq(Space& home, int n) {
    return x.nq(home,-n);
  }
  forceinline ModEvent
  MinusView::nq(Space& home, long long int n) {
    return x.nq(home,-n);
  }

  forceinline ModEvent
  MinusView::eq(Space& home, int n) {
    return x.eq(home,-n);
  }
  forceinline ModEvent
  MinusView::eq(Space& home, long long int n) {
    return x.eq(home,-n);
  }


  /*
   * Iterator-based domain update
   *
   */
  template<class I>
  forceinline ModEvent
  MinusView::narrow_r(Space& home, I& i, bool) {
    Region r(home);
    Iter::Ranges::Minus mi(r,i);
    return x.narrow_r(home,mi,false);
  }
  template<class I>
  forceinline ModEvent
  MinusView::inter_r(Space& home, I& i, bool) {
    Region r(home);
    Iter::Ranges::Minus mi(r,i);
    return x.inter_r(home,mi,false);
  }
  template<class I>
  forceinline ModEvent
  MinusView::minus_r(Space& home, I& i, bool) {
    Region r(home);
    Iter::Ranges::Minus mi(r,i);
    return x.minus_r(home,mi,false);
  }
  template<class I>
  forceinline ModEvent
  MinusView::narrow_v(Space& home, I& i, bool) {
    Region r(home);
    Iter::Values::Minus mi(r,i);
    return x.narrow_v(home,mi,false);
  }
  template<class I>
  forceinline ModEvent
  MinusView::inter_v(Space& home, I& i, bool) {
    Region r(home);
    Iter::Values::Minus mi(r,i);
    return x.inter_v(home,mi,false);
  }
  template<class I>
  forceinline ModEvent
  MinusView::minus_v(Space& home, I& i, bool) {
    Region r(home);
    Iter::Values::Minus mi(r,i);
    return x.minus_v(home,mi,false);
  }


  /*
   * Propagator modification events
   *
   */
  forceinline ModEventDelta
  MinusView::med(ModEvent me) {
    return IntView::med(me);
  }


  /*
   * Delta information for advisors
   *
   */
  forceinline int
  MinusView::min(const Delta& d) const {
    return -x.max(d);
  }
  forceinline int
  MinusView::max(const Delta& d) const {
    return -x.min(d);
  }
  forceinline bool
  MinusView::any(const Delta& d) const {
    return x.any(d);
  }


  /**
   * \brief %Range iterator for minus integer views
   * \ingroup TaskActorIntView
   */
  template<>
  class ViewRanges<MinusView> : public IntVarImpBwd {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const MinusView& x);
    /// Initialize with ranges for view \a x
    void init(const MinusView& x);
    //@}

    /// \name Range access
    //@{
    /// Return smallest value of range
    int min(void) const;
    /// Return largest value of range
    int max(void) const;
    //@}
  };

  forceinline
  ViewRanges<MinusView>::ViewRanges(void) {}

  forceinline
  ViewRanges<MinusView>::ViewRanges(const MinusView& x)
    : IntVarImpBwd(x.base().varimp()) {}

  forceinline void
  ViewRanges<MinusView>::init(const MinusView& x) {
    IntVarImpBwd::init(x.base().varimp());
  }

  forceinline int
  ViewRanges<MinusView>::min(void) const {
    return -IntVarImpBwd::max();
  }
  forceinline int
  ViewRanges<MinusView>::max(void) const {
    return -IntVarImpBwd::min();
  }

  inline int
  MinusView::med(void) const {
    if (x.range())
      return (min()+max())/2 - ((min()+max())%2 < 0 ? 1 : 0);

    unsigned int i = x.size() / 2;
    if (size() % 2 == 0)
      i--;
    ViewRanges<MinusView> r(*this);
    while (i >= r.width()) {
      i -= r.width();
      ++r;
    }
    return r.min() + static_cast<int>(i);
  }

}}

// STATISTICS: int-var

