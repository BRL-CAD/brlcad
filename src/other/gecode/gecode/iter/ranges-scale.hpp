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

#include <cmath>

namespace Gecode { namespace Iter { namespace Ranges {

  /**
   * \brief Range iterator for pointwise product with a positive integer
   *
   * Note that this iterator has a different interface as it can be used
   * for both integer precision as well as double precision (depending
   * on the type \a Val (\c int or \c double) and
   * on the type \a UnsVal (\c unsigned \c int or \c double).
   *
   * \ingroup FuncIterRanges
   */
  template<class Val, class UnsVal, class I>
  class ScaleUp {
  protected:
    /// Iterator to be scaled
    I i;
    /// Scale-factor
    int a;
    /// Current value of range
    Val cur;
    /// Last value of scaled range of \a i
    Val end;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ScaleUp(void);
    /// Initialize with ranges from \a i and scale factor \a a
    ScaleUp(I& i, int a);
    /// Initialize with ranges from \a i and scale factor \a a
    void init(I& i, int a);
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
    Val min(void) const;
    /// Return largest value of range
    Val max(void) const;
    /// Return width of range (distance between minimum and maximum)
    UnsVal width(void) const;
    //@}
  };

  /**
   * \brief Range iterator for pointwise division by a positive integer
   *
   * \ingroup FuncIterRanges
   */
  template<class I>
  class ScaleDown : public MinMax {
  protected:
    /// Iterator to be scaled down
    I i;
    /// Divide by this factor
    int a;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ScaleDown(void);
    /// Initialize with ranges from \a i and scale factor \a a
    ScaleDown(I& i, int a);
    /// Initialize with ranges from \a i and scale factor \a a
    void init(I& i, int a);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };



  template<class Val, class UnsVal, class I>
  forceinline
  ScaleUp<Val,UnsVal,I>::ScaleUp(void) {}

  template<class Val, class UnsVal, class I>
  inline void
  ScaleUp<Val,UnsVal,I>::init(I& i0, int a0) {
    i = i0; a = a0;
    if (i()) {
      cur = a * i.min();
      end = a * i.max();
    } else {
      cur = 1;
      end = 0;
    }
  }

  template<class Val, class UnsVal, class I>
  inline
  ScaleUp<Val,UnsVal,I>::ScaleUp(I& i0, int a0) : i(i0), a(a0) {
    if (i()) {
      cur = a * i.min();
      end = a * i.max();
    } else {
      cur = 1;
      end = 0;
    }
  }

  template<class Val, class UnsVal, class I>
  forceinline void
  ScaleUp<Val,UnsVal,I>::operator ++(void) {
    if (a == 1) {
      ++i;
    } else {
      cur += a;
      if (cur > end) {
        ++i;
        if (i()) {
          cur = a * i.min();
          end = a * i.max();
        }
      }
    }
  }
  template<class Val, class UnsVal, class I>
  forceinline bool
  ScaleUp<Val,UnsVal,I>::operator ()(void) const {
    return (a == 1) ? i() : (cur <= end);
  }

  template<class Val, class UnsVal, class I>
  forceinline Val
  ScaleUp<Val,UnsVal,I>::min(void) const {
    return (a == 1) ? static_cast<Val>(i.min()) : cur;
  }
  template<class Val, class UnsVal, class I>
  forceinline Val
  ScaleUp<Val,UnsVal,I>::max(void) const {
    return (a == 1) ? static_cast<Val>(i.max()) : cur;
  }
  template<class Val, class UnsVal, class I>
  forceinline UnsVal
  ScaleUp<Val,UnsVal,I>::width(void) const {
    return (a == 1) ?
      static_cast<UnsVal>(i.width()) :
      static_cast<UnsVal>(1);
  }



  template<class I>
  forceinline void
  ScaleDown<I>::operator ++(void) {
    finish();
    while ((mi > ma) && i()) {
      mi = static_cast<int>(ceil(static_cast<double>(i.min())/a));
      ma = static_cast<int>(floor(static_cast<double>(i.max())/a));
      ++i;
    }
    while (i()) {
      int n_mi = static_cast<int>(ceil(static_cast<double>(i.min())/a));
      if (n_mi-ma > 1)
        break;
      int n_ma = static_cast<int>(floor(static_cast<double>(i.max())/a));
      if (n_mi <= n_ma) {
        ma = n_ma;
      }
      ++i;
    }
  }

  template<class I>
  forceinline
  ScaleDown<I>::ScaleDown(void) {}

  template<class I>
  inline void
  ScaleDown<I>::init(I& i0, int a0) {
    i = i0; a = a0;
    operator ++();
  }

  template<class I>
  inline
  ScaleDown<I>::ScaleDown(I& i0, int a0) : i(i0), a(a0) {
    i = i0; a = a0;
    operator ++();
  }

}}}

// STATISTICS: iter-any

