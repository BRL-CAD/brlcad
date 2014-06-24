/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2005
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
   * \brief Range iterator for computing the complement (described by template arguments)
   *
   * The complement is computed with respect to a given universe set
   * provided as template arguments (\a UMIN ... \a UMAX). The universe
   * must always be a superset!
   *
   * \ingroup FuncIterRanges
   */

  template<int UMIN, int UMAX, class I>
  class Compl : public MinMax {
  protected:
    /// Iterator to compute complement for
    I i;
    /// Initialize
    void start(void);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Compl(void);
    /// Initialize with iterator \a i
    Compl(I& i);
    /// Initialize with iterator \a i
    void init(I& i);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };


  /**
   * \brief Range iterator for computing the complement (described by values)
   *
   * The complement is computed with respect to a given universe set
   * provided as arguments upon initialization. The universe
   * must always be a superset!
   *
   * \ingroup FuncIterRanges
   */

  template<class I>
  class ComplVal : public MinMax {
  protected:
    /// Values describing the universe set
    int UMIN, UMAX;
    /// Iterator to compute complement for
    I i;
    /// Initialize
    void start(void);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ComplVal(void);
    /// Initialize with iterator \a i
    ComplVal(int umin, int umax, I& i);
    /// Initialize with iterator \a i
    void init(int umin, int umax, I& i);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };


  template<int UMIN, int UMAX, class I>
  forceinline void
  Compl<UMIN,UMAX,I>::start(void) {
    if (i()) {
      assert((i.min() >= UMIN) && (i.max() <= UMAX));
      if (i.min() > UMIN) {
        mi = UMIN;
        ma = i.min()-1;
      } else if (i.max() < UMAX) {
        mi = i.max()+1;
        ++i;
        ma = i() ? (i.min()-1) : UMAX;
      } else {
        finish();
      }
    } else {
      mi = UMIN;
      ma = UMAX;
    }
  }

  template<int UMIN, int UMAX, class I>
  forceinline
  Compl<UMIN,UMAX,I>::Compl(void) {}

  template<int UMIN, int UMAX, class I>
  forceinline
  Compl<UMIN,UMAX,I>::Compl(I& i0) : i(i0) {
    start();
  }

  template<int UMIN, int UMAX, class I>
  forceinline void
  Compl<UMIN,UMAX,I>::init(I& i0) {
    i=i0; start();
  }

  template<int UMIN, int UMAX, class I>
  forceinline void
  Compl<UMIN,UMAX,I>::operator ++(void) {
    assert(!i() || (i.max() <= UMAX));
    if (i() && (i.max() < UMAX)) {
      mi = i.max()+1;
      ++i;
      ma = i() ? (i.min()-1) : UMAX;
    } else {
      finish();
    }
  }

  template<class I>
  forceinline void
  ComplVal<I>::start(void) {
    if (i()) {
      assert((i.min() >= UMIN) && (i.max() <= UMAX));
      if (i.min() > UMIN) {
        mi = UMIN;
        ma = i.min()-1;
      } else if (i.max() < UMAX) {
        mi = i.max()+1;
        ++i;
        ma = i() ? (i.min()-1) : UMAX;
      } else {
        finish();
      }
    } else {
      mi = UMIN;
      ma = UMAX;
    }
  }

  template<class I>
  forceinline
  ComplVal<I>::ComplVal(void) {}

  template<class I>
  forceinline
  ComplVal<I>::ComplVal(int umin, int umax, I& i0)
    : UMIN(umin), UMAX(umax), i(i0) {
    start();
  }

  template<class I>
  forceinline void
  ComplVal<I>::init(int umin, int umax, I& i0) {
    UMIN=umin; UMAX=umax; i=i0; start();
  }

  template<class I>
  forceinline void
  ComplVal<I>::operator ++(void) {
    assert(!i() || (i.max() <= UMAX));
    if (i() && (i.max() < UMAX)) {
      mi = i.max()+1;
      ++i;
      ma = i() ? (i.min()-1) : UMAX;
    } else {
      finish();
    }
  }

}}}

// STATISTICS: iter-any

