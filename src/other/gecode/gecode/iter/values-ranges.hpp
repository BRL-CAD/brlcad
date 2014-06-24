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

namespace Gecode { namespace Iter { namespace Values {

  /**
   * \brief Range iterator from value iterator
   *
   * The iterator only requires that the value iterator
   * iterates values in increasing order. Duplicates in the
   * iterated value sequence are okay.
   *
   * \ingroup FuncIterValues
   */
  template<class I>
  class ToRanges : public Ranges::MinMax {
  protected:
    /// Value iterator used
    I i;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ToRanges(void);
    /// Initialize with value iterator \a i
    ToRanges(I& i);
    /// Initialize with value iterator \a i
    void init(I& i);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };

  template<class I>
  forceinline
  ToRanges<I>::ToRanges(void) {}

  template<class I>
  forceinline void
  ToRanges<I>::operator ++(void) {
    if (!i()) {
      finish(); return;
    }
    mi = i.val(); ma = i.val();
    ++i;
    while (i() && (ma+1 >= i.val())) {
      ma = i.val(); ++i;
    }
  }

  template<class I>
  forceinline
  ToRanges<I>::ToRanges(I& i0)
    : i(i0) {
    operator ++();
  }

  template<class I>
  forceinline void
  ToRanges<I>::init(I& i0) {
    i = i0;
    operator ++();
  }

}}}

// STATISTICS: iter-any

