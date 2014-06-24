/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
 *    Guido Tack, 2006
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
   * \brief %Range iterator for appending a singleton with a range iterator
   *
   * The singleton is not allowed to be adjacent to the iterator.
   *
   * \ingroup FuncIterRanges
   */

  template<class J>
  class SingletonAppend : public MinMax {
  protected:
    /// Iterator to be appended
    J j;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    SingletonAppend(void);
    /// Initialize with singleton (\a i0, \a i1) and iterator \a j
    SingletonAppend(int i0, int i1, J& j);
    /// Initialize with singleton (\a i0, \a i1) and iterator \a j
    void init(int i0, int i1, J& j);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };


  /*
   * Binary SingletonAppend
   *
   */

  template<class J>
  inline void
  SingletonAppend<J>::operator ++(void) {
    if (j()) {
      mi = j.min();  ma = j.max();
      ++j;
    } else {
      finish();
    }
  }


  template<class J>
  forceinline
  SingletonAppend<J>::SingletonAppend(void) {}

  template<class J>
  forceinline
  SingletonAppend<J>::SingletonAppend(int i0, int i1, J& j0)
    : j(j0) {
    mi=i0; ma=i1;
  }

  template<class J>
  forceinline void
  SingletonAppend<J>::init(int i0, int i1, J& j0) {
    mi=i0; ma=i1; j=j0;
  }

}}}

// STATISTICS: iter-any

