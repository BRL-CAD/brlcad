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

namespace Gecode { namespace Iter { namespace Ranges {

  /**
   * \brief %Range iterator cache
   *
   * Allows to iterate the ranges as defined by the input iterator
   * several times provided the Cache is %reset by the reset member
   * function.
   *
   * \ingroup FuncIterRanges
   */
  class Cache : public RangeListIter {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Cache(void);
    /// Copy constructor
    Cache(const Cache& m);
    /// Initialize with ranges from \a i
    template<class I>
    Cache(Region& r, I& i);
    /// Initialize with ranges from \a i
    template<class I>
    void init(Region& r, I& i);
    /// Assignment operator (both caches must be allocated from the same region)
    Cache& operator =(const Cache& m);
    //@}
  };


  forceinline
  Cache::Cache(void) {}

  forceinline
  Cache::Cache(const Cache& m) 
    : RangeListIter(m) {}

  template<class I>
  void
  Cache::init(Region& r, I& i) {
    RangeListIter::init(r);
    RangeList* h = NULL;
    RangeList** p = &h;
    for (; i(); ++i) {
      RangeList* t = new (*rlio) RangeList;
      *p = t; p = &t->next;
      t->min = i.min();
      t->max = i.max();
    }
    *p = NULL;
    RangeListIter::set(h);
  }

  template<class I>
  forceinline
  Cache::Cache(Region& r, I& i) {
    init(r,i);
  }

  forceinline Cache&
  Cache::operator =(const Cache& m) {
    return static_cast<Cache&>(RangeListIter::operator =(m));
  }

}}}

// STATISTICS: iter-any

