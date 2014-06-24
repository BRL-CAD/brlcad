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
   * \brief Range iterator for pointwise minus of a range iterator
   *
   * This iterator in effect changes the order of how ranges
   * are iterated: the first range of the input iterator defines
   * the last range of the Minus iterator. Upon initialization
   * all ranges of the input iterator are stored 
   * which later allows iteration in inverse direction.
   *
   * \ingroup FuncIterRanges
   */
  class Minus : public RangeListIter {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Minus(void);
    /// Copy constructor
    Minus(const Minus& m);
    /// Initialize with ranges from \a i
    template<class I>
    Minus(Region& r, I& i);
    /// Initialize with ranges from \a i
    template<class I>
    void init(Region& r, I& i);
    /// Assignment operator (both iterators must be allocated from the same region)
    Minus& operator =(const Minus& m);
    //@}
  };


  forceinline
  Minus::Minus(void) {}

  forceinline
  Minus::Minus(const Minus& m) 
    : RangeListIter(m) {}

  template<class I>
  void
  Minus::init(Region& r, I& i) {
    RangeListIter::init(r);
    RangeList* p = NULL;
    for (; i(); ++i) {
      RangeList* t = new (*rlio) RangeList;
      t->next = p;
      t->min = -i.max();
      t->max = -i.min();
      p = t;
    }
    RangeListIter::set(p);
  }

  template<class I>
  forceinline
  Minus::Minus(Region& r, I& i) {
    init(r,i);
  }

  forceinline Minus&
  Minus::operator =(const Minus& m) {
    return static_cast<Minus&>(RangeListIter::operator =(m));
  }

}}}

// STATISTICS: iter-any

