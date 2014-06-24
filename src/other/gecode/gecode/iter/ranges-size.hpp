/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2006
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
   * \brief %Range iterator with size counting
   *
   * Allows to iterate the ranges as defined by the input iterator
   * and access the size of the set already iterated. After complete
   * iteration, the reported size is thus the size of the set.
   *
   * \ingroup FuncIterRanges
   */
  template<class I>
  class Size {
  protected:
    /// Iterator to compute size of
    I i;
    /// Accumulated size
    unsigned int _size;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Size(void);
    /// Initialize with ranges from \a i
    Size(I& i);
    /// Initialize with ranges from \a i
    void init(I& i);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void);
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}

    /// \name %Range access
    //@{
    /// Return smallest value of range
    int min(void) const;
    /// Return largest value of range
    int max(void) const;
    /// Return width of range (distance between minimum and maximum)
    unsigned int width(void) const;
    //@}

    /// \name %Size access
    //@{
    /// Return accumulated size
    unsigned int size(void) const;
    //@}
  };


  template<class I>
  forceinline
  Size<I>::Size(void)
    : _size(0) {}

  template<class I>
  inline void
  Size<I>::init(I& i0) {
    i.init(i0);
    _size = 0;
  }

  template<class I>
  inline
  Size<I>::Size(I& i0) : i(i0), _size(0) {}

  template<class I>
  forceinline void
  Size<I>::operator ++(void) {
    _size += i.width();
    ++i;
  }
  template<class I>
  forceinline bool
  Size<I>::operator ()(void) {
    return i();
  }

  template<class I>
  forceinline int
  Size<I>::min(void) const {
    return i.min();
  }
  template<class I>
  forceinline int
  Size<I>::max(void) const {
    return i.max();
  }
  template<class I>
  forceinline unsigned int
  Size<I>::width(void) const {
    return i.width();
  }

  template<class I>
  forceinline unsigned int
  Size<I>::size(void) const {
    return _size;
  }

}}}

// STATISTICS: iter-any

