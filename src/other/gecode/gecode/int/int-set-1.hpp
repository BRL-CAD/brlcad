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

#include <sstream>

namespace Gecode {

  /*
   * Integer sets
   *
   */
  forceinline
  IntSet::IntSet(void) {}

  /**
   * \brief Integer set initialization
   *
   * Helper class to allow partial specialization of
   * template member function.
   *
   */
  template<class I>
  class IntSetInit {
  public:
    /// Initialize \a s with iterator \a i
    static void init(IntSet& s, I& i) {
      Support::DynamicArray<IntSet::Range,Heap> d(heap);
      int n=0;
      unsigned int size = 0;
      while (i()) {
        d[n].min = i.min(); d[n].max = i.max(); size += i.width();
        ++n; ++i;
      }
      if (n > 0) {
        IntSet::IntSetObject* o = IntSet::IntSetObject::allocate(n);
        for (int j=n; j--; )
          o->r[j]=d[j];
        o->size = size;
        s.object(o);
      }
    }
  };

  /// Initialize integer set with integer set
  template<>
  class IntSetInit<IntSet> {
  public:
    static void init(IntSet& s, const IntSet& i) {
      s.object(i.object());
    }
  };

  /// Initialize integer set with iterator
  template<class I>
  IntSet::IntSet(I& i) {
    IntSetInit<I>::init(*this,i);
  }

  /// Initialize integer set with iterator
  template<class I>
  IntSet::IntSet(const I& i) {
    IntSetInit<I>::init(*this,i);
  }

  forceinline
  IntSet::IntSet(const int r[][2], int n) {
    init(r,n);
  }

  forceinline
  IntSet::IntSet(const int r[], int n) {
    init(r,n);
  }

  forceinline
  IntSet::IntSet(int n, int m) {
    init(n,m);
  }

  forceinline int
  IntSet::min(int i) const {
    assert(object() != NULL);
    return static_cast<IntSetObject*>(object())->r[i].min;
  }

  forceinline int
  IntSet::max(int i) const {
    assert(object() != NULL);
    return static_cast<IntSetObject*>(object())->r[i].max;
  }

  forceinline unsigned int
  IntSet::width(int i) const {
    assert(object() != NULL);
    IntSetObject* o = static_cast<IntSetObject*>(object());
    return static_cast<unsigned int>(o->r[i].max-o->r[i].min)+1;
  }

  forceinline int
  IntSet::ranges(void) const {
    IntSetObject* o = static_cast<IntSetObject*>(object());
    return (o == NULL) ? 0 : o->n;
  }

  forceinline bool
  IntSet::in(int n) const {
    IntSetObject* o = static_cast<IntSetObject*>(object());
    if ((o == NULL) || (n < o->r[0].min) || (n > o->r[o->n-1].max))
      return false;
    else
      return o->in(n);
  }

  forceinline int
  IntSet::min(void) const {
    IntSetObject* o = static_cast<IntSetObject*>(object());
    return (o == NULL) ? Int::Limits::max : o->r[0].min;
  }

  forceinline int
  IntSet::max(void) const {
    IntSetObject* o = static_cast<IntSetObject*>(object());
    return (o == NULL) ? Int::Limits::min : o->r[o->n-1].max;
  }

  forceinline unsigned int
  IntSet::size(void) const {
    IntSetObject* o = static_cast<IntSetObject*>(object());
    return (o == NULL) ? 0 : o->size;
  }

  forceinline unsigned int
  IntSet::width(void) const {
    return static_cast<unsigned int>(max()-min()+1);
  }


  /*
   * Range iterator for integer sets
   *
   */

  forceinline
  IntSetRanges::IntSetRanges(void) {}
  forceinline
  void
  IntSetRanges::init(const IntSet& s) {
    int n = s.ranges();
    if (n > 0) {
      i = &static_cast<IntSet::IntSetObject*>(s.object())->r[0]; e = i+n;
    } else {
      i = e = NULL;
    }
  }
  forceinline
  IntSetRanges::IntSetRanges(const IntSet& s) { init(s); }


  forceinline void
  IntSetRanges::operator ++(void) {
    i++;
  }
  forceinline bool
  IntSetRanges::operator ()(void) const {
    return i<e;
  }

  forceinline int
  IntSetRanges::min(void) const {
    return i->min;
  }
  forceinline int
  IntSetRanges::max(void) const {
    return i->max;
  }
  forceinline unsigned int
  IntSetRanges::width(void) const {
    return static_cast<unsigned int>(i->max - i->min) + 1;
  }

  /*
   * Value iterator for integer sets
   *
   */
  forceinline
  IntSetValues::IntSetValues(void) {}

  forceinline
  IntSetValues::IntSetValues(const IntSet& s) {
    IntSetRanges r(s);
    Iter::Ranges::ToValues<IntSetRanges>::init(r);
  }

  forceinline void
  IntSetValues::init(const IntSet& s) {
    IntSetRanges r(s);
    Iter::Ranges::ToValues<IntSetRanges>::init(r);
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const IntSet& is) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << '{';
    for (int i = 0; i < is.ranges(); ) {
      int min = is.min(i);
      int max = is.max(i);
      if (min == max)
        s << min;
      else
        s << min << ".." << max;
      i++;
      if (i < is.ranges())
        s << ',';
    }
    s << '}';
    return os << s.str();
  }

}

// STATISTICS: int-var

