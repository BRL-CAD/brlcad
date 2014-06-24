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
   * \brief Value iterator for pointwise offset (by some constant)
   *
   * \ingroup FuncIterValues
   */
  template<class I>
  class Offset {
  protected:
    /// Input iterator
    I i;
    /// Offset for ranges
    int c;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Offset(void);
    /// Initialize with values from \a i and offset \a c
    Offset(I& i, int c);
    /// Initialize with values from \a i and offset \a c
    void init(I& i, int c);
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


  template<class I>
  forceinline
  Offset<I>::Offset(void) {}

  template<class I>
  forceinline void
  Offset<I>::init(I& i0, int c0) {
    i = i0; c = c0;
  }

  template<class I>
  forceinline
  Offset<I>::Offset(I& i0, int c0) : i(i0), c(c0) {}

  template<class I>
  forceinline void
  Offset<I>::operator ++(void) {
    ++i;
  }
  template<class I>
  forceinline bool
  Offset<I>::operator ()(void) const {
    return i();
  }

  template<class I>
  forceinline int
  Offset<I>::val(void) const {
    return i.val()+c;
  }

}}}

// STATISTICS: iter-any

