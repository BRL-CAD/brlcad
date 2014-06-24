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

namespace Gecode { namespace Iter { namespace Ranges {

  /**
   * \brief %Range iterator for range lists
   *
   * Allows to iterate the ranges of a RangeList where the
   * ranges must be increasing and non-overlapping.
   *
   * \ingroup FuncIterRanges
   */
  class RangeList  {
  protected:
    /// Current range
    const Gecode::RangeList* c;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    RangeList(void);
    /// Initialize with BndSet \a s
    RangeList(const Gecode::RangeList* s);
    /// Initialize with BndSet \a s
    void init(const Gecode::RangeList* s);
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
    /// Return width of range (distance between minimum and maximum)
    unsigned int width(void) const;
    //@}
  };

  forceinline
  RangeList::RangeList(void) {}

  forceinline
  RangeList::RangeList(const Gecode::RangeList* s) : c(s) {}

  forceinline void
  RangeList::init(const Gecode::RangeList* s) { c = s; }

  forceinline bool
  RangeList::operator ()(void) const { return c != NULL; }

  forceinline void
  RangeList::operator ++(void) {
    c = c->next();
  }

  forceinline int
  RangeList::min(void) const {
    return c->min();
  }
  forceinline int
  RangeList::max(void) const {
    return c->max();
  }
  forceinline unsigned int
  RangeList::width(void) const {
    return c->width();
  }


}}}

// STATISTICS: iter-any
