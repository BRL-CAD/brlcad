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
   * \brief Range iterator for singleton range
   *
   * \ingroup FuncIterRanges
   */

  class Singleton : public MinMax {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Singleton(void);
    /// Initialize with range \a min to \a max
    Singleton(int min, int max);
    /// Initialize with range \a min to \a max
    void init(int min, int max);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };


  forceinline
  Singleton::Singleton(void) {}

  forceinline
  Singleton::Singleton(int min, int max)
    : MinMax(min,max) {}

  forceinline void
  Singleton::init(int min, int max) {
    mi = min; ma = max;
  }

  forceinline void
  Singleton::operator ++(void) {
    mi=1; ma=0;
  }

}}}

// STATISTICS: iter-any

