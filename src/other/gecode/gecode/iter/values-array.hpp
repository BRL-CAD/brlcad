/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2007
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

namespace Gecode { namespace Iter { namespace Values {

  /**
   * \brief %Value iterator for array of integers
   *
   * Allows to iterate the integers as defined by an array where the
   * values in the array must be sorted in increasing order.
   * The integers can be iterated several times provided the iterator
   * is %reset by the reset member function.
   *
   * \ingroup FuncIterValues
   */
  class Array  {
  protected:
    /// Array for values
    int* v;
    /// Current value
    int c;
    /// Number of ranges in array
    int n;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Array(void);
    /// Initialize with \a n values from \a v
    Array(int* v, int n);
    /// Initialize with \a n ranges from \a v
    void init(int* v, int n);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a value or done
    bool operator ()(void) const;
    /// Move iterator to next value (if possible)
    void operator ++(void);
    /// Reset iterator to start from beginning
    void reset(void);
    //@}

    /// \name %Value access
    //@{
    /// Return current value
    int val(void) const;
    //@}
  };


  forceinline
  Array::Array(void) {}

  forceinline
  Array::Array(int* v0, int n0)
    : v(v0), c(0), n(n0) {}

  forceinline void
  Array::init(int* v0, int n0) {
    v=v0; c=0; n=n0;
  }

  forceinline void
  Array::operator ++(void) {
    c++;
  }
  forceinline bool
  Array::operator ()(void) const {
    return c<n;
  }
  forceinline void
  Array::reset(void) {
    c=0;
  }

  forceinline int
  Array::val(void) const {
    return v[c];
  }

}}}

// STATISTICS: iter-any

