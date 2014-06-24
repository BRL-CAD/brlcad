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
   * \brief Value iterator for mapping values of a value iterator
   *
   * If \a strict is true, the values obtained by mapping must be
   * strictly increasing (that is, no duplicates).
   *
   * \ingroup FuncIterValues
   */
  template<class I, class M, bool strict=false>
  class Map {
  protected:
    /// Input iterator
    I i;
    /// Mapping object
    M m;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Map(void);
    /// Initialize with values from \a i
    Map(I& i);
    /// Initialize with values from \a i and map \a m
    Map(I& i, const M& m);
    /// Initialize with values from \a i
    void init(I& i);
    /// Initialize with values from \a i and map \a m
    void init(I& i, const M& m);
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


  template<class I, class M, bool strict>
  forceinline
  Map<I,M,strict>::Map(void) {}

  template<class I, class M, bool strict>
  forceinline
  Map<I,M,strict>::Map(I& i0) : i(i0) {}

  template<class I, class M, bool strict>
  forceinline
  Map<I,M,strict>::Map(I& i0, const M& m0) : i(i0), m(m0) {}

  template<class I, class M, bool strict>
  forceinline void
  Map<I,M,strict>::init(I& i0) {
    i=i0;
  }

  template<class I, class M, bool strict>
  forceinline void
  Map<I,M,strict>::init(I& i0, const M& m0) {
    i=i0; m=m0;
  }

  template<class I, class M, bool strict>
  forceinline void
  Map<I,M,strict>::operator ++(void) {
    if (strict) {
      ++i;
    } else {
      int n=m.val(i.val());
      do {
        ++i;
      } while (i() && (n == m.val(i.val())));
    }
  }
  template<class I, class M, bool strict>
  forceinline bool
  Map<I,M,strict>::operator ()(void) const {
    return i();
  }

  template<class I, class M, bool strict>
  forceinline int
  Map<I,M,strict>::val(void) const {
    return m.val(i.val());
  }

}}}

// STATISTICS: iter-any
