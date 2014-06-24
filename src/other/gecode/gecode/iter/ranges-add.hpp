/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2005
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
   * \brief Range iterator for adding a single range to a range iterator
   *
   * \ingroup FuncIterRanges
   */
  template<class I>
  class AddRange : public MinMax {
  protected:
    /// Iterator to which the range is to be added
    I i;
    /// Minimum of range to be added
    int r_min;
    /// Maximum of range to be added
    int r_max;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    AddRange(void);
    /// Initialize with ranges \a i and range \a min to \a max
    AddRange(I& i, int min, int max);
    /// Initialize with ranges \a i and range \a min to \a max
    void init(I& i, int min, int max);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };


  /**
   * \brief Range iterator for subtracting a single range from a range iterator
   *
   * \ingroup FuncIterRanges
   */
  template<class I>
  class SubRange : public AddRange<I> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    SubRange(void);
    /// Initialize with ranges \a i and range \a min to \a max
    SubRange(I& i, int min, int max);
    /// Initialize with ranges \a i and range \a min to \a max
    void init(I& i, int min, int max);
    //@}
  };

  template<class I>
  forceinline
  AddRange<I>::AddRange(void) {}

  template<class I>
  forceinline void
  AddRange<I>::operator ++(void) {
    if (i()) {
      mi = r_min + i.min();
      ma = r_max + i.max();
      ++i;
      while (i() && (ma+1 >= r_min+i.min())) {
        ma = r_max + i.max(); ++i;
      }
    } else {
      finish();
    }
  }

  template<class I>
  forceinline
  AddRange<I>::AddRange(I& i0, int r_min0, int r_max0)
    : i(i0), r_min(r_min0), r_max(r_max0) {
    operator ++();
  }

  template<class I>
  forceinline void
  AddRange<I>::init(I& i0, int r_min0, int r_max0) {
    i = i0; r_min = r_min0; r_max = r_max0;
    operator ++();
  }


  template<class I>
  forceinline
  SubRange<I>::SubRange(void) {}

  template<class I>
  forceinline
  SubRange<I>::SubRange(I& i, int r_min, int r_max)
    : AddRange<I>(i,-r_max,-r_min) {}

  template<class I>
  forceinline void
  SubRange<I>::init(I& i, int r_min, int r_max) {
    AddRange<I>::init(i,-r_max,-r_min);
  }

}}}

// STATISTICS: iter-any

