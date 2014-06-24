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
   * \brief Value iterator for the union of two value iterators
   *
   * \ingroup FuncIterValues
   */

  template<class I, class J>
  class Union  {
  protected:
    /// First iterator
    I i;
    /// Second iterator
    J j;
    /// Current value
    int v;
    /// Whether iterator is done
    bool done;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Union(void);
    /// Initialize with values from \a i and \a j
    Union(I& i, J& j);
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
  Union<I,J>::Union(void) : v(0) {}

  template<class I, class J>
  forceinline void
  Union<I,J>::operator ++(void) {
    if (i()) {
      if (j()) {
        if (i.val() == j.val()) {
          v=i.val(); ++i; ++j;
        } else if (i.val() < j.val()) {
          v=i.val(); ++i;
        } else {
          v=j.val(); ++j;
        }
      } else {
        v=i.val(); ++i;
      }
    } else if (j()) {
      v=j.val(); ++j;
    } else {
      done=true;
    }
  }

  template<class I, class J>
  inline void
  Union<I,J>::init(I& i0, J& j0) {
    i=i0; j=j0; v=0; done=false;
    operator ++();
  }

  template<class I, class J>
  forceinline
  Union<I,J>::Union(I& i0, J& j0) : i(i0), j(j0), v(0), done(false) {
    operator ++();
  }

  template<class I, class J>
  forceinline bool
  Union<I,J>::operator ()(void) const {
    return !done;
  }

  template<class I, class J>
  forceinline int
  Union<I,J>::val(void) const {
    return v;
  }

}}}

// STATISTICS: iter-any
