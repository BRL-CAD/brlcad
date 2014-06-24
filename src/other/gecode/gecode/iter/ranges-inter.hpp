/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
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

#include <algorithm>

namespace Gecode { namespace Iter { namespace Ranges {

  /**
   * \brief Range iterator for computing intersection (binary)
   *
   * \ingroup FuncIterRanges
   */
  template<class I, class J>
  class Inter : public MinMax {
  protected:
    /// First iterator
    I i;
    /// Second iterator
    J j;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Inter(void);
    /// Initialize with iterator \a i and \a j
    Inter(I& i, J& j);
    /// Initialize with iterator \a i and \a j
    void init(I& i, J& j);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };


  /**
   * \brief Range iterator for intersection of iterators
   *
   * \ingroup FuncIterRanges
   */
  class NaryInter : public RangeListIter {
  protected:
    /// Freelist
    RangeList* f;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    NaryInter(void);
    /// Initialize with single iterator \a i
    template<class I>
    NaryInter(Region& r, I& i);
    /// Initialize with iterators \a i and \a j
    template<class I, class J>
    NaryInter(Region& r, I& i, J& j);
    /// Initialize with \a n iterators in \a i
    template<class I>
    NaryInter(Region& r, I* i, int n);
    /// Initialize with single iterator \a i
    template<class I>
    void init(Region& r, I& i);
    /// Initialize with iterators \a i and \a j
    template<class I, class J>
    void init(Region& r, I& i, J& j);
    /// Initialize with \a n iterators in \a i
    template<class I>
    void init(Region& r, I* i, int n);
    /// Add iterator \a i
    template<class I>
    void operator &=(I& i);
    /// Assignment operator (both iterators must be allocated from the same region)
    NaryInter& operator =(const NaryInter& m);
    //@}
  };



  /*
   * Binary intersection
   *
   */

  template<class I, class J>
  inline void
  Inter<I,J>::operator ++(void) {
    if (!i() || !j()) goto done;
    do {
      while (i() && (i.max() < j.min())) ++i;
      if (!i()) goto done;
      while (j() && (j.max() < i.min())) ++j;
      if (!j()) goto done;
    } while (i.max() < j.min());
    // Now the intervals overlap: consume the smaller interval
    ma = std::min(i.max(),j.max());
    mi = std::max(i.min(),j.min());
    if (i.max() < j.max()) ++i; else ++j;
    return;
  done:
    finish();
  }

  template<class I, class J>
  forceinline
  Inter<I,J>::Inter(void) {}

  template<class I, class J>
  forceinline
  Inter<I,J>::Inter(I& i0, J& j0)
    : i(i0), j(j0) {
    operator ++();
  }

  template<class I, class J>
  forceinline void
  Inter<I,J>::init(I& i0, J& j0) {
    i = i0; j = j0;
    operator ++();
  }


  /*
   * Nary intersection
   *
   */

  forceinline
  NaryInter::NaryInter(void) {}

  template<class I>
  forceinline void
  NaryInter::init(Region& r, I& i) {
    RangeListIter::init(r);
    f = NULL;
    set(copy(i));
  }

  template<class I, class J>
  forceinline void
  NaryInter::init(Region& r, I& i, J& j) {
    RangeListIter::init(r);
    f = NULL;
    RangeList*  h;
    RangeList** c = &h;
    while (i() && j()) {
      do {
        while (i() && (i.max() < j.min())) ++i;
        if (!i()) goto done;
        while (j() && (j.max() < i.min())) ++j;
        if (!j()) goto done;
      } while (i.max() < j.min());
      // Now the intervals overlap: consume the smaller interval
      RangeList* t = range(std::max(i.min(),j.min()),
                           std::min(i.max(),j.max()));
      *c = t; c = &t->next;
      if (i.max() < j.max()) ++i; else ++j;
    }
  done:
    *c = NULL;
    set(h);
  }

  template<class I>
  forceinline void
  NaryInter::init(Region& r, I* i, int n) {
    RangeListIter::init(r);
    f = NULL;
    if ((n > 0) && i[0]()) {
      RangeList*  h;
      RangeList** c = &h;

      int min = i[0].min();
      while (i[0]()) {
        // Initialize with last interval
        int max = i[0].max();
        // Intersect with all other intervals
      restart:
        for (int j=n; j--;) {
          // Skip intervals that are too small
          while (i[j]() && (i[j].max() < min))
            ++i[j];
          if (!i[j]())
            goto done;
          if (i[j].min() > max) {
            min=i[j].min();
            max=i[j].max();
            goto restart;
          }
          // Now the intervals overlap
          if (min < i[j].min())
            min = i[j].min();
          if (max > i[j].max())
            max = i[j].max();
        }
        RangeList* t = range(min,max);
        *c = t; c = &t->next;
        // The next interval must be at least two elements away
        min = max + 2;
      }
    done:
      *c = NULL;
      set(h);
    }
  }

  template<class I>
  forceinline
  NaryInter::NaryInter(Region& r, I& i) {
    init(r, i);
  }
  template<class I, class J>
  forceinline
  NaryInter::NaryInter(Region& r, I& i, J& j) {
    init(r, i, j);
  }
  template<class I>
  forceinline
  NaryInter::NaryInter(Region& r, I* i, int n) {
    init(r, i, n);
  }

  template<class I>
  forceinline void
  NaryInter::operator &=(I& i) {
    RangeList* j = get();
    // The new rangelist
    RangeList* h;
    RangeList** c = &h;
    while (i() && (j != NULL)) {
      do {
        while (i() && (i.max() < j->min))
          ++i;
        if (!i()) goto done;
        while ((j != NULL) && (j->max < i.min())) {
          RangeList* t = j->next;
          j->next = f; f = j;
          j = t;
        }
        if (j == NULL) goto done;
      } while (i.max() < j->min);
      // Now the intervals overlap: consume the smaller interval
      RangeList* t = range(std::max(i.min(),j->min),
                           std::min(i.max(),j->max),f);
      *c = t; c = &t->next;
      if (i.max() < j->max) {
        ++i; 
      } else {
        RangeList* t = j->next;
        j->next = f; f = j;
        j = t;
      }
    }
  done:
    // Put remaining elements into freelist
    while (j != NULL) {
      RangeList* t = j->next;
      j->next = f; f = j;
      j = t;
    }
    *c = NULL;
    set(h);
  }

  forceinline NaryInter&
  NaryInter::operator =(const NaryInter& m) {
    f = NULL;
    return static_cast<NaryInter&>(RangeListIter::operator =(m));
  }

}}}

// STATISTICS: iter-any

