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
   * \brief Base for range iterators with explicit min and max
   *
   * The iterator provides members \a mi and \a ma for storing the
   * limits of the currently iterated range. The iterator
   * continues until \a mi becomes greater than \a ma. The member function
   * finish does exactly that.
   *
   * \ingroup FuncIterRanges
   */

  class MinMax {
  protected:
    /// Minimum of current range
    int mi;
    /// Maximum of current range
    int ma;
    /// %Set range such that iteration stops
    void finish(void);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    MinMax(void);
    /// Initialize with range \a min to \a max
    MinMax(int min, int max);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
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

  forceinline void
  MinMax::finish(void) {
    mi = 1; ma = 0;
  }

  forceinline
  MinMax::MinMax(void) {}

  forceinline
  MinMax::MinMax(int min, int max)
    : mi(min), ma(max) {}

  forceinline bool
  MinMax::operator ()(void) const {
    return mi <= ma;
  }

  forceinline int
  MinMax::min(void) const {
    return mi;
  }
  forceinline int
  MinMax::max(void) const {
    return ma;
  }
  forceinline unsigned int
  MinMax::width(void) const {
    return static_cast<unsigned int>(ma-mi)+1;
  }

}}}

// STATISTICS: iter-any

