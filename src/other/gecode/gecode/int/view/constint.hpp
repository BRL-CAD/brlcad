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
  ConstIntView::ConstIntView(void) {}
  forceinline
  ConstIntView::ConstIntView(int n) : x(n) {}

  /*
   * Value access
   *
   */
  forceinline int
  ConstIntView::min(void) const {
    return x;
  }
  forceinline int
  ConstIntView::max(void) const {
    return x;
  }
  forceinline int
  ConstIntView::med(void) const {
    return x;
  }
  forceinline int
  ConstIntView::val(void) const {
    return x;
  }

  forceinline unsigned int
  ConstIntView::size(void) const {
    return 1;
  }
  forceinline unsigned int
  ConstIntView::width(void) const {
    return 1;
  }
  forceinline unsigned int
  ConstIntView::regret_min(void) const {
    return 0;
  }
  forceinline unsigned int
  ConstIntView::regret_max(void) const {
    return 0;
  }


  /*
   * Domain tests
   *
   */
  forceinline bool
  ConstIntView::range(void) const {
    return true;
  }
  forceinline bool
  ConstIntView::in(int n) const {
    return n == x;
  }
  forceinline bool
  ConstIntView::in(long long int n) const {
    return n == x;
  }


  /*
   * Domain update by value
   *
   */
  forceinline ModEvent
  ConstIntView::lq(Space&, int n) {
    return (x <= n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ConstIntView::lq(Space&, long long int n) {
    return (x <= n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ConstIntView::le(Space&, int n) {
    return (x < n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ConstIntView::le(Space&, long long int n) {
    return (x < n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ConstIntView::gq(Space&, int n) {
    return (x >= n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ConstIntView::gq(Space&, long long int n) {
    return (x >= n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ConstIntView::gr(Space&, int n) {
    return (x > n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ConstIntView::gr(Space&, long long int n) {
    return (x > n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ConstIntView::nq(Space&, int n) {
    return (x != n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ConstIntView::nq(Space&, long long int n) {
    return (x != n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ConstIntView::eq(Space&, int n) {
    return (x == n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ConstIntView::eq(Space&, long long int n) {
    return (x == n) ? ME_INT_NONE : ME_INT_FAILED;
  }



  /*
   * Iterator-based domain update
   *
   */
  template<class I>
  forceinline ModEvent
  ConstIntView::narrow_r(Space&, I& i, bool) {
    return i() ? ME_INT_NONE : ME_INT_FAILED;
  }
  template<class I>
  forceinline ModEvent
  ConstIntView::inter_r(Space&, I& i, bool) {
    while (i() && (i.max() < x))
      ++i;
    return (i() && (i.min() <= x)) ? ME_INT_NONE : ME_INT_FAILED;
  }
  template<class I>
  forceinline ModEvent
  ConstIntView::minus_r(Space&, I& i, bool) {
    while (i() && (i.max() < x))
      ++i;
    return (i() && (i.min() <= x)) ? ME_INT_FAILED : ME_INT_NONE;
  }
  template<class I>
  forceinline ModEvent
  ConstIntView::narrow_v(Space&, I& i, bool) {
    return i() ? ME_INT_NONE : ME_INT_FAILED;
  }
  template<class I>
  forceinline ModEvent
  ConstIntView::inter_v(Space&, I& i, bool) {
    while (i() && (i.val() < x))
      ++i;
    return (i() && (i.val() == x)) ? ME_INT_NONE : ME_INT_FAILED;
  }
  template<class I>
  forceinline ModEvent
  ConstIntView::minus_v(Space&, I& i, bool) {
    while (i() && (i.val() < x))
      ++i;
    return (i() && (i.val() == x)) ? ME_INT_FAILED : ME_INT_NONE;
  }


  /*
   * Delta information for advisors
   *
   */
  forceinline int
  ConstIntView::min(const Delta&) const {
    return 1;
  }
  forceinline int
  ConstIntView::max(const Delta&) const {
    return 0;
  }
  forceinline bool
  ConstIntView::any(const Delta&) const {
    return true;
  }



  /*
   * Cloning
   *
   */
  forceinline void
  ConstIntView::update(Space& home, bool share, ConstIntView& y) {
    ConstView<IntView>::update(home,share,y);
    x = y.x;
  }


  /**
   * \brief %Range iterator for constant integer views
   * \ingroup TaskActorIntView
   */
  template<>
  class ViewRanges<ConstIntView> {
  private:
    /// The single integer to iterate
    int n;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const ConstIntView& x);
    /// Initialize with ranges for view \a x
    void init(const ConstIntView& x);
    //@}

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
  ViewRanges<ConstIntView>::ViewRanges(void) {}

  forceinline
  ViewRanges<ConstIntView>::ViewRanges(const ConstIntView& x)
    : n(x.val()) {}

  forceinline bool
  ViewRanges<ConstIntView>::operator ()(void) const {
    return n <= Limits::max;
  }
  forceinline void
  ViewRanges<ConstIntView>::operator ++(void) {
    n = Limits::max+1;
  }

  forceinline int
  ViewRanges<ConstIntView>::min(void) const {
    return n;
  }
  forceinline int
  ViewRanges<ConstIntView>::max(void) const {
    return n;
  }
  forceinline unsigned int
  ViewRanges<ConstIntView>::width(void) const {
    return 1;
  }

  /*
   * View comparison
   *
   */
  forceinline bool
  same(const ConstIntView& x, const ConstIntView& y) {
    return x.min() == y.min();
  }
  forceinline bool
  before(const ConstIntView& x, const ConstIntView& y) {
    return x.min() < y.min();
  }

}}

// STATISTICS: int-var

