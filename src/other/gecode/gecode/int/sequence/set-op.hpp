/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
 *
 *  Last modified:
 *     $Date$
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

namespace Gecode { namespace Int { namespace Sequence {

  /// Status of whether a view takes a value from a set
  enum TakesStatus {
    TS_NO,   ///< Definitely not
    TS_YES,  ///< Definitely yes
    TS_MAYBE ///< Maybe or maybe not
  };

  /// Return whether view \a x takes value \a s
  template<class View>
  forceinline TakesStatus
  takes(const View& x, int s) {
    if (x.in(s))
      return x.assigned() ? TS_YES : TS_MAYBE;
    else
      return TS_NO;
  }
  /// Return whether view \a x takes value from \a s
  template<class View>
  forceinline TakesStatus
  takes(const View& x, const IntSet& s) {
    if ((x.max() < s.min()) || (x.min() > s.max()))
      return TS_NO;
    ViewRanges<View> ix(x);
    IntSetRanges is(s);
    switch (Iter::Ranges::compare(ix,is)) {
    case Iter::Ranges::CS_SUBSET: return TS_YES;
    case Iter::Ranges::CS_DISJOINT: return TS_NO;
    case Iter::Ranges::CS_NONE: return TS_MAYBE;
    default: GECODE_NEVER;
    }
    return TS_MAYBE;
  }

  /// Test whether all values of view \a x are included in \a s
  template<class View>
  forceinline bool
  includes(const View& x, int s) {
    return x.assigned() && x.in(s);
  }
  /// Test whether all values of view \a x are included in \a s
  template<class View>
  forceinline bool
  includes(const View& x, const IntSet& s) {
    if ((x.max() < s.min()) || (x.min() > s.max()))
      return false;
    ViewRanges<View> ix(x);
    IntSetRanges is(s);
    return Iter::Ranges::subset(ix,is);
  }

  /// Test whether all values of view \a x are excluded from \a s
  template<class View>
  forceinline bool
  excludes(const View& x, int s) {
    return !x.in(s);
  }
  /// Test whether all values of view \a x are excluded from \a s
  template<class View>
  forceinline bool
  excludes(const View& x, const IntSet& s) {
    if ((x.max() < s.min()) || (x.min() > s.max()))
      return true;
    ViewRanges<View> ix(x);
    IntSetRanges is(s);
    return Iter::Ranges::disjoint(ix,is);
  }

  /// Test whether no decision on inclusion or exclusion of values of view \a x in \a s can be made
  template<class View>
  forceinline bool
  undecided(const View& x, int s) {
    return !x.assigned() && x.in(s);
  }
  /// Test whether no decision on inclusion or exclusion of values of view \a x in \a s can be made
  template<class View>
  forceinline bool
  undecided(const View& x, const IntSet& s) {
    if ((x.max() < s.min()) || (x.min() > s.max()))
      return false;
    ViewRanges<View> ix(x);
    IntSetRanges is(s);
    return Iter::Ranges::compare(ix,is) == Iter::Ranges::CS_NONE;
  }

  /// Prune view \a x to only include values from \a s
  template<class View>
  forceinline ModEvent
  include(Space& home, View& x, int s) {
    return x.eq(home,s);
  }
  /// Prune view \a x to only include values from \a s
  template<class View>
  forceinline ModEvent
  include(Space& home, View& x, const IntSet& s) {
    IntSetRanges is(s);
    return x.inter_r(home,is,false);
  }

  /// Prune view \a x to exclude all values from \a s
  template<class View>
  forceinline ModEvent
  exclude(Space& home, View& x, int s) {
    return x.nq(home,s);
  }
  /// Prune view \a x to exclude all values from \a s
  template<class View>
  forceinline ModEvent
  exclude(Space& home, View& x, const IntSet& s) {
    IntSetRanges is(s);
    return x.minus_r(home,is,false);
  }

}}}

// STATISTICS: int-prop
