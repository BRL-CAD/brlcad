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

namespace Gecode { namespace Iter { namespace Ranges {

  /**
   * \brief Value iterator from range iterator
   *
   * \ingroup FuncIterValues
   */
  template<class I>
  class ToValues {
  protected:
    /// Range iterator used
    I i;
    /// Current value
    int cur;
    /// End of current range
    int max;
    /// Initialize iterator
    void start(void);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ToValues(void);
    /// Initialize with values from range iterator \a i
    ToValues(I& i);
    /// Initialize with values from range iterator \a i
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
    int  val(void) const;
    //@}
  };



  template<class I>
  forceinline
  ToValues<I>::ToValues(void) {}

  template<class I>
  forceinline void
  ToValues<I>::start(void) {
    if (i()) {
      cur = i.min(); max = i.max();
    } else {
      cur = 1;       max = 0;
    }
  }

  template<class I>
  forceinline
  ToValues<I>::ToValues(I& i0)
    : i(i0) {
    start();
  }

  template<class I>
  forceinline void
  ToValues<I>::init(I& i0) {
    i = i0;
    start();
  }


  template<class I>
  forceinline bool
  ToValues<I>::operator ()(void) const {
    return (cur <= max);
  }

  template<class I>
  forceinline void
  ToValues<I>::operator ++(void) {
    ++cur;
    if (cur > max) {
      ++i;
      if (i()) {
        cur = i.min(); max = i.max();
      }
    }
  }

  template<class I>
  forceinline int
  ToValues<I>::val(void) const {
    return cur;
  }

}}}

// STATISTICS: iter-any

