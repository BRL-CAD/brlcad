/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
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
   * \brief %Range iterator for array of ranges
   *
   * Allows to iterate the ranges as defined by an array where the
   * ranges must be increasing and non-overlapping.
   * The ranges can be iterated several times provided the iterator
   * is %reset by the reset member function.
   *
   * \ingroup FuncIterRanges
   */
  class Array  {
  public:
    /// %Ranges for array
    class Range {
    public:
      int min; int max;
    };
  protected:
    /// Array for ranges
    Range* r;
    /// Current range
    int c;
    /// Number of ranges in array
    int n;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Array(void);
    /// Initialize with \a n ranges from \a r
    Array(Range* r, int n);
    /// Initialize with \a n ranges from \a r
    void init(Range* r, int n);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
    /// Move iterator to next range (if possible)
    void operator ++(void);
    /// Reset iterator to start from beginning
    void reset(void);
    //@}

    /// \name %Range access
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
  Array::Array(void) {}

  forceinline
  Array::Array(Range* r0, int n0)
    : r(r0), c(0), n(n0) {}

  forceinline void
  Array::init(Range* r0, int n0) {
    r=r0; c=0; n=n0;
  }

  forceinline void
  Array::operator ++(void) {
    c++;
  }
  forceinline bool
  Array::operator ()(void) const {
    return c < n;
  }

  forceinline void
  Array::reset(void) {
    c=0;
  }

  forceinline int
  Array::min(void) const {
    return r[c].min;
  }
  forceinline int
  Array::max(void) const {
    return r[c].max;
  }
  forceinline unsigned int
  Array::width(void) const {
    return static_cast<unsigned int>(r[c].max-r[c].min)+1;
  }

}}}

// STATISTICS: iter-any

