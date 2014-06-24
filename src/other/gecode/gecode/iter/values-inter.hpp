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
   * \brief Value iterator for the intersection of two value iterators
   *
   * \ingroup FuncIterValues
   */

  template<class I, class J>
  class Inter  {
  protected:
    /// First iterator
    I i;
    /// Second iterator
    J j;
    /// Find next element from intersection
    void next(void);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Inter(void);
    /// Initialize with values from \a i and \a j
    Inter(I& i, J& j);
    /// Initialize with values from \a i and \a j
    void init(I& i, J& j);
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


  template<class I, class J>
  forceinline
  Inter<I,J>::Inter(void) {}

  template<class I, class J>
  forceinline void
  Inter<I,J>::next(void) {
    do {
      while (i() && j() && (i.val() < j.val()))
        ++i;
      while (i() && j() && (j.val() < i.val()))
        ++j;
    } while (i() && j() && (i.val() != j.val()));
  }

  template<class I, class J>
  inline void
  Inter<I,J>::init(I& i0, J& j0) {
    i=i0; j=j0; next();
  }

  template<class I, class J>
  forceinline
  Inter<I,J>::Inter(I& i0, J& j0) : i(i0), j(j0) {
    next();
  }

  template<class I, class J>
  forceinline void
  Inter<I,J>::operator ++(void) {
    ++i; ++j; next();
  }

  template<class I, class J>
  forceinline bool
  Inter<I,J>::operator ()(void) const {
    return i() && j();
  }

  template<class I, class J>
  forceinline int
  Inter<I,J>::val(void) const {
    assert(i.val() == j.val());
    return i.val();
  }

}}}

// STATISTICS: iter-any
