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
   * \defgroup FuncIterRangesOp Operations on range iterators
   *
   * \ingroup FuncIterRanges
   */

  //@{
  /// Size of all ranges of range iterator \a i
  template<class I>
  unsigned int size(I& i);

  /// Check whether range iterators \a i and \a j are equal
  template<class I, class J>
  bool equal(I& i, J& j);

  /// Check whether range iterator \a i is subset of range iterator \a j
  template<class I, class J>
  bool subset(I& i, J& j);

  /// Check whether range iterators \a i and \a j are disjoint
  template<class I, class J>
  bool disjoint(I& i, J& j);

  /// Comapre two iterators with each other
  enum CompareStatus {
    CS_SUBSET,   ///< First is subset of second iterator
    CS_DISJOINT, ///< Intersection is empty
    CS_NONE      ///< Neither of the above
  };

  /// Check whether range iterator \a i is a subset of \a j, or whether they are disjoint
  template<class I, class J>
  CompareStatus compare(I& i, J& j);
  //@}


  template<class I>
  inline unsigned int
  size(I& i) {
    unsigned int s = 0;
    while (i()) {
      s += i.width(); ++i;
    }
    return s;
  }

  template<class I, class J>
  forceinline bool
  equal(I& i, J& j) {
    // Are i and j equal?
    while (i() && j())
      if ((i.min() == j.min()) && (i.max() == j.max())) {
        ++i; ++j;
      } else {
        return false;
      }
    return !i() && !j();
  }

  template<class I, class J>
  forceinline bool
  subset(I& i, J& j) {
    // Is i subset of j?
    while (i() && j())
      if (j.max() < i.min()) {
        ++j;
      } else if ((i.min() >= j.min()) && (i.max() <= j.max())) {
        ++i;
      } else {
        return false;
      }
    return !i();
  }

  template<class I, class J>
  forceinline bool
  disjoint(I& i, J& j) {
    // Are i and j disjoint?
    while (i() && j())
      if (j.max() < i.min()) {
        ++j;
      } else if (i.max() < j.min()) {
        ++i;
      } else {
        return false;
      }
    return true;
  }

  template<class I, class J>
  forceinline CompareStatus
  compare(I& i, J& j) {
    bool subset = true;
    bool disjoint = true;
    while (i() && j()) {
      if (j.max() < i.min()) {
        ++j;
      } else if (i.max() < j.min()) {
        ++i; subset = false;
      } else if ((i.min() >= j.min()) && (i.max() <= j.max())) {
        ++i; disjoint = false;
      } else if (i.max() <= j.max()) {
        ++i; disjoint = false; subset = false;
      } else if (j.max() <= i.max()) {
        ++j; disjoint = false; subset = false;
      }
    }
    if (i())
      subset = false;
    if (subset)
      return CS_SUBSET;
    return disjoint ? CS_DISJOINT : CS_NONE;
  }

}}}

// STATISTICS: iter-any

