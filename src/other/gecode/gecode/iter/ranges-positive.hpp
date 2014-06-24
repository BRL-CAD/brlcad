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

#include <algorithm>

namespace Gecode { namespace Iter { namespace Ranges {

  /**
   * \brief Range iterator for positive part of a range iterator
   *
   * If \a strict is true, zero is excluded.
   *
   * \ingroup FuncIterRanges
   */
  template<class I, bool strict=false>
  class Positive {
  protected:
    /// Input iterator
    I i;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Positive(void);
    /// Initialize with ranges from \a i
    Positive(I& i);
    /// Initialize with ranges from \a i
    void init(I& i);
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
    int min(void) const;
    /// Return largest value of range
    int max(void) const;
    /// Return width of range (distance between minimum and maximum)
    unsigned int width(void) const;
    //@}
  };


  template<class I, bool strict>
  forceinline
  Positive<I,strict>::Positive(void) {}

  template<class I, bool strict>
  forceinline void
  Positive<I,strict>::init(I& i0) {
    i=i0;
    if (strict) {
      while (i() && (i.max() < 0)) ++i;
    } else {
      while (i() && (i.max() <= 0)) ++i;
    }
  }

  template<class I, bool strict>
  forceinline
  Positive<I,strict>::Positive(I& i) {
    init(i);
  }

  template<class I, bool strict>
  forceinline void
  Positive<I,strict>::operator ++(void) {
    ++i;
  }
  template<class I, bool strict>
  forceinline bool
  Positive<I,strict>::operator ()(void) const {
    return i();
  }

  template<class I, bool strict>
  forceinline int
  Positive<I,strict>::min(void) const {
    if (strict) {
      return std::max(i.min(),1);
    } else {
      return std::max(i.min(),0);
    }
  }
  template<class I, bool strict>
  forceinline int
  Positive<I,strict>::max(void) const {
    return i.max();
  }
  template<class I, bool strict>
  forceinline unsigned int
  Positive<I,strict>::width(void) const {
    return static_cast<unsigned int>(max()-min()+1);
  }

}}}

// STATISTICS: iter-any
