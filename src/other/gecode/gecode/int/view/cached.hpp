/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2011
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
  template<class View>
  forceinline
  CachedView<View>::CachedView(void) : _size(0) {}
  template<class View>
  forceinline
  CachedView<View>::CachedView(const View& y)
    : DerivedView<View>(y), _firstRange(NULL), _lastRange(NULL), 
      _size(0) {}


  /*
   * Value access
   *
   */
  template<class View>
  forceinline int
  CachedView<View>::min(void) const {
    return x.min();
  }
  template<class View>
  forceinline int
  CachedView<View>::max(void) const {
    return x.max();
  }
  template<class View>
  forceinline int
  CachedView<View>::med(void) const {
    return x.med();
  }
  template<class View>
  forceinline int
  CachedView<View>::val(void) const {
    return x.val();
  }

  template<class View>
  forceinline unsigned int
  CachedView<View>::width(void) const {
    return x.width();
  }
  template<class View>
  forceinline unsigned int
  CachedView<View>::size(void) const {
    return x.size();
  }
  template<class View>
  forceinline unsigned int
  CachedView<View>::regret_min(void) const {
    return x.regret_min();
  }
  template<class View>
  forceinline unsigned int
  CachedView<View>::regret_max(void) const {
    return x.regret_max();
  }

  /*
   * Domain tests
   *
   */
  template<class View>
  forceinline bool
  CachedView<View>::range(void) const {
    return x.range();
  }
  template<class View>
  forceinline bool
  CachedView<View>::in(int n) const {
    return x.in(n);
  }
  template<class View>
  forceinline bool
  CachedView<View>::in(long long int n) const {
    return x.in(n);
  }


  /*
   * Domain update by value
   *
   */
  template<class View>
  forceinline ModEvent
  CachedView<View>::lq(Space& home, int n) {
    return x.lq(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::lq(Space& home, long long int n) {
    return x.lq(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::le(Space& home, int n) {
    return x.le(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::le(Space& home, long long int n) {
    return x.le(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::gq(Space& home, int n) {
    return x.gq(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::gq(Space& home, long long int n) {
    return x.gq(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::gr(Space& home, int n) {
    return x.gr(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::gr(Space& home, long long int n) {
    return x.gr(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::nq(Space& home, int n) {
    return x.nq(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::nq(Space& home, long long int n) {
    return x.nq(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::eq(Space& home, int n) {
    return x.eq(home,n);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::eq(Space& home, long long int n) {
    return x.eq(home,n);
  }


  /*
   * Iterator-based domain update
   *
   */
  template<class View>
  template<class I>
  forceinline ModEvent
  CachedView<View>::narrow_r(Space& home, I& i, bool depend) {
    return x.narrow_r(home,i,depend);
  }
  template<class View>
  template<class I>
  forceinline ModEvent
  CachedView<View>::inter_r(Space& home, I& i, bool depend) {
    return x.inter_r(home,i,depend);
  }
  template<class View>
  template<class I>
  forceinline ModEvent
  CachedView<View>::minus_r(Space& home, I& i, bool depend) {
    return x.minus_r(home,i,depend);
  }
  template<class View>
  template<class I>
  forceinline ModEvent
  CachedView<View>::narrow_v(Space& home, I& i, bool depend) {
    return x.narrow_v(home,i,depend);
  }
  template<class View>
  template<class I>
  forceinline ModEvent
  CachedView<View>::inter_v(Space& home, I& i, bool depend) {
    return x.inter_v(home,i,depend);
  }
  template<class View>
  template<class I>
  forceinline ModEvent
  CachedView<View>::minus_v(Space& home, I& i, bool depend) {
    return x.minus_v(home,i,depend);
  }



  /*
   * Propagator modification events
   *
   */
  template<class View>
  forceinline ModEventDelta
  CachedView<View>::med(ModEvent me) {
    return View::med(me);
  }


  /*
   * Delta information for advisors
   *
   */
  template<class View>
  forceinline int
  CachedView<View>::min(const Delta& d) const {
    return x.min(d);
  }
  template<class View>
  forceinline int
  CachedView<View>::max(const Delta& d) const {
    return x.max(d);
  }
  template<class View>
  forceinline bool
  CachedView<View>::any(const Delta& d) const {
    return x.any(d);
  }



  /*
   * Cloning
   *
   */
  template<class View>
  void
  CachedView<View>::update(Space& home, bool share, CachedView<View>& y) {
    DerivedView<View>::update(home,share,y);
    if (y._firstRange) {
      _firstRange = new (home) RangeList(y._firstRange->min(),
                                         y._firstRange->max(),NULL);
      RangeList* cur = _firstRange;
      
      for (RangeList* y_cur = y._firstRange->next(); y_cur != NULL;
           y_cur = y_cur->next()) {
        RangeList* next =
          new (home) RangeList(y_cur->min(),y_cur->max(),NULL);
        cur->next(next);
        cur = next;
      }
      _lastRange = cur;
      _size = y._size;
    }
  }

  /*
   * Cache operations
   *
   */

  template<class View>
  void
  CachedView<View>::initCache(Space& home, const IntSet& s) {
    _firstRange = NULL;
    for (int i=s.ranges(); i--;) {
      _firstRange = new (home) RangeList(s.min(i),s.max(i),_firstRange);
      if (i==s.ranges()-1)
        _lastRange = _firstRange;
    }
    _size = s.size();
  }

  template<class View>
  void
  CachedView<View>::cache(Space& home) {
    _firstRange->dispose(home,_lastRange);
    ViewRanges<View> xr(x);
    _firstRange = new (home) RangeList(xr.min(),xr.max(),NULL);
    ++xr;
    RangeList* cur = _firstRange;
    for (; xr(); ++xr) {
      RangeList* next = new (home) RangeList(xr.min(),xr.max(),NULL);
      cur->next(next);
      cur = next;
    }
    _lastRange = cur;
    _size = x.size();
  }

  template<class View>
  forceinline bool
  CachedView<View>::modified(void) const {
    return x.size() != _size;
  }


  /**
   * \brief %Range iterator for offset integer views
   * \ingroup TaskActorIntView
   */
  template<class View>
  class ViewRanges<CachedView<View> >
    : public ViewRanges<View> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const CachedView<View>& x);
    /// Initialize with ranges for view \a x
    void init(const CachedView<View>& x);
    //@}
  };

  template<class View>
  forceinline
  ViewRanges<CachedView<View> >::ViewRanges(void) {}

  template<class View>
  forceinline
  ViewRanges<CachedView<View> >::ViewRanges(const CachedView<View>& x) {
    ViewRanges<IntView>::init(x.base());
  }

  template<class View>
  forceinline void
  ViewRanges<CachedView<View> >::init(const CachedView<View>& x) {
    ViewRanges<View>::init(x.base());
  }

  template<class View>
  forceinline
  ViewDiffRanges<View>::ViewDiffRanges(void) {}

  template<class View>
  forceinline
  ViewDiffRanges<View>::ViewDiffRanges(const CachedView<View>& x)
    : cr(x._firstRange), dr(x.base()) {
    Super::init(cr,dr);
  }

  template<class View>
  forceinline void
  ViewDiffRanges<View>::init(const CachedView<View>& x) {
    cr.init(x._firstRange);
    dr.init(x.base());
    Super::init(cr,dr);
  }

  /*
   * View comparison
   *
   */
  template<class View>
  forceinline bool
  same(const CachedView<View>& x, const CachedView<View>& y) {
    return same(x.base(),y.base()) && (x.offset() == y.offset());
  }
  template<class View>
  forceinline bool
  before(const CachedView<View>& x, const CachedView<View>& y) {
    return before(x.base(),y.base())
      || (same(x.base(),y.base()) && (x.offset() < y.offset()));
  }

}}

// STATISTICS: int-var

