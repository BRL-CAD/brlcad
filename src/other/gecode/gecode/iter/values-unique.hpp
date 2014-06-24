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
   * \brief Remove duplicate values from from value iterator
   *
   * The iterator only requires that the value iterator
   * iterates values in increasing order. Duplicates in the
   * iterated value sequence are removed.
   *
   * \ingroup FuncIterValues
   */
  template<class I>
  class Unique {
  protected:
    /// Value iterator used
    I i;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Unique(void);
    /// Initialize with value iterator \a i
    Unique(I& i);
    /// Initialize with value iterator \a i
    void init(I& i);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a value or done
    bool operator ()(void) const;
    /// Move iterator to next unique value (if possible)
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
  Unique<I>::Unique(void) {}

  template<class I>
  forceinline
  Unique<I>::Unique(I& i0)
    : i(i0) {}

  template<class I>
  forceinline void
  Unique<I>::init(I& i0) {
    i = i0;
  }

  template<class I>
  forceinline void
  Unique<I>::operator ++(void) {
    int n=i;
    do {
      ++i;
    } while (i() && (i.val() == n));
  }
  template<class I>
  forceinline bool
  Unique<I>::operator ()(void) const {
    return i();
  }

  template<class I>
  forceinline int
  Unique<I>::val(void) const {
    return i.val();
  }

}}}

// STATISTICS: iter-any

