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
  ZeroIntView::ZeroIntView(void) {}

  /*
   * Value access
   *
   */
  forceinline int
  ZeroIntView::min(void) const {
    return 0;
  }
  forceinline int
  ZeroIntView::max(void) const {
    return 0;
  }
  forceinline int
  ZeroIntView::med(void) const {
    return 0;
  }
  forceinline int
  ZeroIntView::val(void) const {
    return 0;
  }

  forceinline unsigned int
  ZeroIntView::size(void) const {
    return 1;
  }
  forceinline unsigned int
  ZeroIntView::width(void) const {
    return 1;
  }
  forceinline unsigned int
  ZeroIntView::regret_min(void) const {
    return 0;
  }
  forceinline unsigned int
  ZeroIntView::regret_max(void) const {
    return 0;
  }


  /*
   * Domain tests
   *
   */
  forceinline bool
  ZeroIntView::range(void) const {
    return true;
  }
  forceinline bool
  ZeroIntView::in(int n) const {
    return n == 0;
  }
  forceinline bool
  ZeroIntView::in(long long int n) const {
    return n == 0;
  }


  /*
   * Domain update by value
   *
   */
  forceinline ModEvent
  ZeroIntView::lq(Space&, int n) {
    return (0 <= n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ZeroIntView::lq(Space&, long long int n) {
    return (0 <= n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ZeroIntView::le(Space&, int n) {
    return (0 < n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ZeroIntView::le(Space&, long long int n) {
    return (0 < n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ZeroIntView::gq(Space&, int n) {
    return (0 >= n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ZeroIntView::gq(Space&, long long int n) {
    return (0 >= n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ZeroIntView::gr(Space&, int n) {
    return (0 > n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ZeroIntView::gr(Space&, long long int n) {
    return (0 > n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ZeroIntView::nq(Space&, int n) {
    return (0 != n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ZeroIntView::nq(Space&, long long int n) {
    return (0 != n) ? ME_INT_NONE : ME_INT_FAILED;
  }

  forceinline ModEvent
  ZeroIntView::eq(Space&, int n) {
    return (0 == n) ? ME_INT_NONE : ME_INT_FAILED;
  }
  forceinline ModEvent
  ZeroIntView::eq(Space&, long long int n) {
    return (0 == n) ? ME_INT_NONE : ME_INT_FAILED;
  }



  /*
   * Iterator-based domain update
   *
   */
  template<class I>
  forceinline ModEvent
  ZeroIntView::narrow_r(Space&, I& i, bool) {
    return i() ? ME_INT_NONE : ME_INT_FAILED;
  }
  template<class I>
  forceinline ModEvent
  ZeroIntView::inter_r(Space&, I& i, bool) {
    while (i() && (i.max() < 0))
      ++i;
    return (i() && (i.min() <= 0)) ? ME_INT_NONE : ME_INT_FAILED;
  }
  template<class I>
  forceinline ModEvent
  ZeroIntView::minus_r(Space&, I& i, bool) {
    while (i() && (i.max() < 0))
      ++i;
    return (i() && (i.min() <= 0)) ? ME_INT_FAILED : ME_INT_NONE;
  }
  template<class I>
  forceinline ModEvent
  ZeroIntView::narrow_v(Space&, I& i, bool) {
    return i() ? ME_INT_NONE : ME_INT_FAILED;
  }
  template<class I>
  forceinline ModEvent
  ZeroIntView::inter_v(Space&, I& i, bool) {
    while (i() && (i.val() < 0))
      ++i;
    return (i() && (i.val() == 0)) ? ME_INT_NONE : ME_INT_FAILED;
  }
  template<class I>
  forceinline ModEvent
  ZeroIntView::minus_v(Space&, I& i, bool) {
    while (i() && (i.val() < 0))
      ++i;
    return (i() && (i.val() == 0)) ? ME_INT_FAILED : ME_INT_NONE;
  }

  /*
   * Delta information for advisors
   *
   */
  forceinline int
  ZeroIntView::min(const Delta&) const {
    return 1;
  }
  forceinline int
  ZeroIntView::max(const Delta&) const {
    return 0;
  }
  forceinline bool
  ZeroIntView::any(const Delta&) const {
    return true;
  }


  /**
   * \brief %Range iterator for constant integer views
   * \ingroup TaskActorIntView
   */
  template<>
  class ViewRanges<ZeroIntView> {
  private:
    /// Whether the iterator is done
    bool done;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const ZeroIntView& x);
    /// Initialize with ranges for view \a x
    void init(const ZeroIntView& x);
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
  ViewRanges<ZeroIntView>::ViewRanges(void) {}

  forceinline
  ViewRanges<ZeroIntView>::ViewRanges(const ZeroIntView&)
    : done(false) {}

  forceinline bool
  ViewRanges<ZeroIntView>::operator ()(void) const {
    return !done;
  }
  forceinline void
  ViewRanges<ZeroIntView>::operator ++(void) {
    done=true;
  }

  forceinline int
  ViewRanges<ZeroIntView>::min(void) const {
    return 0;
  }
  forceinline int
  ViewRanges<ZeroIntView>::max(void) const {
    return 0;
  }
  forceinline unsigned int
  ViewRanges<ZeroIntView>::width(void) const {
    return 1;
  }

  /*
   * View comparison
   *
   */
  forceinline bool
  same(const ZeroIntView&, const ZeroIntView&) {
    return true;
  }

}}

// STATISTICS: int-var

