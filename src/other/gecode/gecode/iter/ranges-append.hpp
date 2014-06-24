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
   * \brief %Range iterator for appending two range iterators
   *
   * The iterators are allowed to be adjacent but not to
   * overlap (in this case, use Gecode::Iter::Union).
   *
   * \ingroup FuncIterRanges
   */

  template<class I, class J>
  class Append : public MinMax {
  protected:
    /// First iterator
    I i;
    /// Iterator to be appended
    J j;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Append(void);
    /// Initialize with iterator \a i and \a j
    Append(I& i, J& j);
    /// Initialize with iterator \a i and \a j
    void init(I& i, J& j);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };


  /**
   * \brief Range iterator for appending arbitrarily many iterators
   *
   * The iterators are allowed to be adjacent but not to
   * overlap (in this case, use Gecode::Iter::NaryUnion)
   *
   * \ingroup FuncIterRanges
   */

  template<class I>
  class NaryAppend  :  public MinMax {
  protected:
    /// The array of iterators to be appended
    I* r;
    /// Number of iterators
    int n;
    /// Number of current iterator being processed
    int active;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    NaryAppend(void);
    /// Initialize with \a n iterators in \a i
    NaryAppend(I* i, int n);
    /// Initialize with \a n iterators in \a i
    void init(I* i, int n);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };


  /*
   * Binary Append
   *
   */

  template<class I, class J>
  inline void
  Append<I,J>::operator ++(void) {
    if (i()) {
      mi = i.min(); ma = i.max();
      ++i;
      if (!i() && j() && (j.min() == ma+1)) {
        ma = j.max();
        ++j;
      }
    } else if (j()) {
      mi = j.min();  ma = j.max();
      ++j;
    } else {
      finish();
    }
  }


  template<class I, class J>
  forceinline
  Append<I,J>::Append(void) {}

  template<class I, class J>
  forceinline
  Append<I,J>::Append(I& i0, J& j0)
    : i(i0), j(j0) {
    if (i() || j())
      operator ++();
    else
      finish();
  }

  template<class I, class J>
  forceinline void
  Append<I,J>::init(I& i0, J& j0) {
    i = i0; j = j0;
    if (i() || j())
      operator ++();
    else
      finish();
  }


  /*
   * Nary Append
   *
   */

  template<class I>
  inline void
  NaryAppend<I>::operator ++(void) {
    mi = r[active].min();
    ma = r[active].max();
    ++r[active];
    while (!r[active]()) {
      //Skip empty iterators:
      do {
        active++;
        if (active >= n) {
          finish(); return;
        }
      } while (!r[active]());
      if (r[active].min() == ma+1){
        ma = r[active].max();
        ++r[active];
      } else {
        return;
      }
    }
  }

  template<class I>
  forceinline
  NaryAppend<I>::NaryAppend(void) {}

  template<class I>
  inline
  NaryAppend<I>::NaryAppend(I* r0, int n0)
    : r(r0), n(n0), active(0) {
    while (active < n && !r[active]())
      active++;
    if (active < n){
      operator ++();
    } else {
      finish();
    }
  }

  template<class I>
  inline void
  NaryAppend<I>::init(I* r0, int n0) {
    r = r0; n = n0; active = 0;
    while (active < n && !r[active]())
      active++;
    if (active < n){
      operator ++();
    } else {
      finish();
    }
  }

}}}

// STATISTICS: iter-any

