/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
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
   * \brief Value iterator for selecting only negative values
   *
   * If \a strict is true, zero is excluded.
   * \ingroup FuncIterValues
   */
  template<class I, bool strict=false>
  class Negative {
  protected:
    /// Input iterator
    I i;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Negative(void);
    /// Initialize with values from \a i
    Negative(I& i);
    /// Initialize with values from \a i
    void init(I& i);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a value or done
    bool operator ()(void) const;
    /// Move iterator to next value (if possible)
    void operator ++(void);
    //@}

    /// \name Value access
    //@{
    /// Return current value
    int val(void) const;
    //@}
  };


  template<class I, bool strict>
  forceinline
  Negative<I,strict>::Negative(void) {}

  template<class I, bool strict>
  forceinline void
  Negative<I,strict>::init(I& i0) {
    i=i0;
  }

  template<class I, bool strict>
  forceinline
  Negative<I,strict>::Negative(I& i0) : i(i0) {}

  template<class I, bool strict>
  forceinline void
  Negative<I,strict>::operator ++(void) {
    ++i;
  }
  template<class I, bool strict>
  forceinline bool
  Negative<I,strict>::operator ()(void) const {
    if (strict) {
      return i() && (i.val() < 0);
    } else {
      return i() && (i.val() <= 0);
    }
  }

  template<class I, bool strict>
  forceinline int
  Negative<I,strict>::val(void) const {
    return i.val();
  }

}}}

// STATISTICS: iter-any

