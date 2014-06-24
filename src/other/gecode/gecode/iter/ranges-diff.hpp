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
   * \brief Range iterator for computing set difference
   *
   * \ingroup FuncIterRanges
   */

  template<class I, class J>
  class Diff : public MinMax {
  protected:
    /// Iterator from which to subtract
    I i;
    /// Iterator to be subtracted
    J j;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Diff(void);
    /// Initialize with iterator \a i and \a j
    Diff(I& i, J& j);
    /// Initialize with iterator \a i and \a j
    void init(I& i, J& j);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };



  template<class I, class J>
  forceinline void
  Diff<I,J>::operator ++(void) {
    // Precondition: mi <= ma
    // Task: find next mi greater than ma
    while (true) {
      if (!i()) break;
      mi = ma+1;
      ma = i.max();
      if (mi > i.max()) {
        ++i;
        if (!i()) break;
        mi = i.min();
        ma = i.max();
      }
      while (j() && (j.max() < mi))
        ++j;
      if (j() && (j.min() <= ma)) {
        // Now the interval [mi ... ma] must be shrunken
        // Is [mi ... ma] completely consumed?
        if ((mi >= j.min()) && (ma <= j.max()))
          continue;
        // Does [mi ... ma] overlap on the left?
        if (j.min() <= mi) {
          mi = j.max()+1;
          // Search for max!
          ++j;
          if (j() && (j.min() <= ma))
            ma = j.min()-1;
        } else {
          ma = j.min()-1;
        }
      }
      return;
    }
    finish();
  }

  template<class I, class J>
  forceinline
  Diff<I,J>::Diff(void) {}

  template<class I, class J>
  forceinline
  Diff<I,J>::Diff(I& i0, J& j0)
    : i(i0), j(j0) {
    if (!i()) {
      finish();
    } else {
      mi = i.min()-1; ma = mi;
      operator ++();
    }
  }

  template<class I, class J>
  forceinline void
  Diff<I,J>::init(I& i0, J& j0) {
    i = i0; j = j0;
    if (!i()) {
      finish();
    } else {
      mi = i.min()-1; ma = mi;
      operator ++();
    }
  }

}}}

// STATISTICS: iter-any

