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
   * \brief Range iterator for computing union (binary)
   *
   * \ingroup FuncIterRanges
   */
  template<class I, class J>
  class Union : public MinMax {
  protected:
    /// First iterator
    I i;
    /// Second iterator
    J j;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Union(void);
    /// Initialize with iterator \a i and \a j
    Union(I& i, J& j);
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
   * \brief Range iterator for union of iterators
   *
   * \ingroup FuncIterRanges
   */
  class NaryUnion : public RangeListIter {
  protected:
    /// Freelist used for allocation
    RangeList* f;
    /// Return range list for union of two iterators
    template<class I, class J>
    RangeList* two(I& i, J& j);
    /// Insert ranges from \a i into \a u
    template<class I>
    void insert(I& i, RangeList*& u);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    NaryUnion(void);
    /// Initialize with single iterator \a i
    template<class I>
    NaryUnion(Region& r, I& i);
    /// Initialize with iterators \a i and \a j
    template<class I, class J>
    NaryUnion(Region& r, I& i, J& j);
    /// Initialize with \a n iterators in \a i
    template<class I>
    NaryUnion(Region& r, I* i, int n);
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
    void operator |=(I& i);
    /// Assignment operator (both iterators must be allocated from the same region)
    NaryUnion& operator =(const NaryUnion& m);
    //@}
  };



  /*
   * Binary union
   *
   */

  template<class I, class J>
  inline void
  Union<I,J>::operator ++(void) {
    if (!i() && !j()) {
      finish(); return;
    }

    if (!i() || (j() && (j.max()+1 < i.min()))) {
      mi = j.min(); ma = j.max(); ++j; return;
    }
    if (!j() || (i() && (i.max()+1 < j.min()))) {
      mi = i.min(); ma = i.max(); ++i; return;
    }

    mi = std::min(i.min(),j.min());
    ma = std::max(i.max(),j.max());

    ++i; ++j;

  next:
    if (i() && (i.min() <= ma+1)) {
      ma = std::max(ma,i.max()); ++i;
      goto next;
    }
    if (j() && (j.min() <= ma+1)) {
      ma = std::max(ma,j.max()); ++j;
      goto next;
    }
  }


  template<class I, class J>
  forceinline
  Union<I,J>::Union(void) {}

  template<class I, class J>
  forceinline
  Union<I,J>::Union(I& i0, J& j0)
    : i(i0), j(j0) {
    operator ++();
  }

  template<class I, class J>
  forceinline void
  Union<I,J>::init(I& i0, J& j0) {
    i = i0; j = j0;
    operator ++();
  }



  /*
   * Nary union
   *
   */

  template<class I, class J>
  RangeListIter::RangeList*
  NaryUnion::two(I& i, J& j) {
    RangeList*  h;
    RangeList** c = &h;
    
    while (i() && j())
      if (i.max()+1 < j.min()) {
        RangeList* t = range(i); ++i;
        *c = t; c = &t->next;
      } else if (j.max()+1 < i.min()) {
        RangeList* t = range(j); ++j;
        *c = t; c = &t->next;
      } else {
        int min = std::min(i.min(),j.min());
        int max = std::max(i.max(),j.max());
        ++i; ++j;

      nexta:
        if (i() && (i.min() <= max+1)) {
          max = std::max(max,i.max()); ++i;
          goto nexta;
        }
        if (j() && (j.min() <= max+1)) {
          max = std::max(max,j.max()); ++j;
          goto nexta;
        }
 
        RangeList* t = range(min,max);
        *c = t; c = &t->next;
      }
    for ( ; i(); ++i) {
      RangeList* t = range(i);
      *c = t; c = &t->next;
    }
    for ( ; j(); ++j) {
      RangeList* t = range(j);
      *c = t; c = &t->next;
    }
    *c = NULL;
    return h;
  }

  template<class I>
  void
  NaryUnion::insert(I& i, RangeList*& u) {
    // The current rangelist
    RangeList** c = &u;
        
    while ((*c != NULL) && i())
      if ((*c)->max+1 < i.min()) {
        // Keep range from union
        c = &(*c)->next;
      } else if (i.max()+1 < (*c)->min) {
        // Copy range from iterator
        RangeList* t = range(i,f); ++i;
        // Insert
        t->next = *c; *c = t; c = &t->next;
      } else {
        // Ranges overlap
        // Compute new minimum
        (*c)->min = std::min((*c)->min,i.min());
        // Compute new maximum
        int max = std::max((*c)->max,i.max());
        
        // Scan from the next range in the union
        RangeList* s = (*c)->next; 
        ++i;
        
      nextb:
        if ((s != NULL) && (s->min <= max+1)) {
          max = std::max(max,s->max);
          RangeList* t = s;
          s = s->next;
          // Put deleted element into freelist
          t->next = f; f = t;
          goto nextb;
        }
        if (i() && (i.min() <= max+1)) {
          max = std::max(max,i.max()); ++i;
          goto nextb;
        }
        // Store computed max and shunt skipped ranges from union
        (*c)->max = max; (*c)->next = s;
      }
    if (*c == NULL) {
      // Copy remaining ranges from iterator
      for ( ; i(); ++i) {
        RangeList* t = range(i,f);
        *c = t; c = &t->next;
      }
      *c = NULL;
    }
  }


  forceinline
  NaryUnion::NaryUnion(void) 
    : f(NULL) {}

  template<class I>
  forceinline void
  NaryUnion::init(Region& r, I& i) {
    RangeListIter::init(r);
    f = NULL;
    set(copy(i));
  }

  template<class I, class J>
  forceinline void
  NaryUnion::init(Region& r, I& i, J& j) {
    RangeListIter::init(r);
    f = NULL;
    set(two(i,j));
  }

  template<class I>
  forceinline void
  NaryUnion::init(Region& r, I* i, int n) {
    f = NULL;
    RangeListIter::init(r);

    int m = 0;
    while ((m < n) && !i[m]())
      m++;

    // Union is empty
    if (m >= n)
      return;

    n--;
    while (!i[n]())
      n--;

    if (m == n) {
      // Union is just a single iterator
      set(copy(i[m]));
    } else {
      // At least two iterators
      RangeList* u = two(i[m++],i[n--]);
      // Insert the remaining iterators
      for ( ; m<=n; m++)
        insert(i[m], u);
      set(u);
    }
  }

  template<class I>
  forceinline
  NaryUnion::NaryUnion(Region& r, I& i) {
    init(r, i);
  }
  template<class I, class J>
  forceinline
  NaryUnion::NaryUnion(Region& r, I& i, J& j) {
    init(r, i, j);
  }
  template<class I>
  forceinline
  NaryUnion::NaryUnion(Region& r, I* i, int n) {
    init(r, i, n);
  }

  template<class I>
  forceinline void
  NaryUnion::operator |=(I& i) {
    RangeList* u = get();
    insert(i, u);
    set(u);
  }

  forceinline NaryUnion&
  NaryUnion::operator =(const NaryUnion& m) {
    f = NULL;
    return static_cast<NaryUnion&>(RangeListIter::operator =(m));
  }

}}}

// STATISTICS: iter-any

