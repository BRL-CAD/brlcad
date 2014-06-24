/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

namespace Gecode { namespace Int { namespace NoOverlap {

  /*
   * Mandatory boxes
   *
   */
  template<class Dim, int n>
  forceinline const Dim&
  ManBox<Dim,n>::operator [](int i) const {
    assert((i >= 0) && (i < n));
    return d[i];
  }
  template<class Dim, int n>
  forceinline Dim&
  ManBox<Dim,n>::operator [](int i) {
    assert((i >= 0) && (i < n));
    return d[i];
  }
  template<class Dim, int n>
  forceinline int
  ManBox<Dim,n>::dim(void) {
    return n;
  }

  template<class Dim, int n>
  forceinline bool
  ManBox<Dim,n>::mandatory(void) const {
    return true;
  }
  template<class Dim, int n>
  forceinline bool
  ManBox<Dim,n>::excluded(void) const {
    return false;
  }
  template<class Dim, int n>
  forceinline bool
  ManBox<Dim,n>::optional(void) const {
    return false;
  }

  template<class Dim, int n>
  forceinline ExecStatus
  ManBox<Dim,n>::exclude(Space&) {
    return ES_FAILED;
  }

  template<class Dim, int n>
  forceinline bool
  ManBox<Dim,n>::nooverlap(const ManBox<Dim,n>& box) const {
    for (int i=0; i<n; i++)
      if ((d[i].lec() <= box.d[i].ssc()) || (box.d[i].lec() <= d[i].ssc()))
        return true;
    return false;
  }

  template<class Dim, int n>
  forceinline bool
  ManBox<Dim,n>::overlap(const ManBox<Dim,n>& box) const {
    for (int i=0; i<n; i++)
      if ((d[i].sec() <= box.d[i].lsc()) || (box.d[i].sec() <= d[i].lsc()))
        return false;
    return true;
  }

  template<class Dim, int n>
  forceinline ExecStatus
  ManBox<Dim,n>::nooverlap(Space& home, ManBox<Dim,n>& box) {
    for (int i=0; i<n; i++)
      if ((d[i].sec() <= box.d[i].lsc()) || 
          (box.d[i].sec() <= d[i].lsc())) {
        // Does not overlap for dimension i
        for (int j=i+1; j<n; j++)
          if ((d[j].sec() <= box.d[j].lsc()) || 
              (box.d[j].sec() <= d[j].lsc()))
            return ES_OK;
        // Does not overlap for only dimension i, hence propagate
        d[i].nooverlap(home, box.d[i]);
        box.d[i].nooverlap(home, d[i]);
        return ES_OK;
      }
    // Overlaps in all dimensions
    return ES_FAILED;
  }

  template<class Dim, int n>
  forceinline void
  ManBox<Dim,n>::update(Space& home, bool share, ManBox<Dim,n>& b) {
    for (int i=0; i<n; i++)
      d[i].update(home,share,b.d[i]);
  }

  template<class Dim, int n>
  forceinline void
  ManBox<Dim,n>::subscribe(Space& home, Propagator& p) {
    for (int i=0; i<n; i++)
      d[i].subscribe(home,p);
  }
  template<class Dim, int n>
  forceinline void
  ManBox<Dim,n>::cancel(Space& home, Propagator& p) {
    for (int i=0; i<n; i++)
      d[i].cancel(home,p);
  }


  /*
   * Optional boxes
   *
   */
  template<class Dim, int n>
  forceinline void
  OptBox<Dim,n>::optional(BoolView o0) {
    o = o0;
  }
  template<class Dim, int n>
  forceinline bool
  OptBox<Dim,n>::mandatory(void) const {
    return o.one();
  }
  template<class Dim, int n>
  forceinline bool
  OptBox<Dim,n>::excluded(void) const {
    return o.zero();
  }
  template<class Dim, int n>
  forceinline bool
  OptBox<Dim,n>::optional(void) const {
    return o.none();
  }

  template<class Dim, int n>
  forceinline ExecStatus
  OptBox<Dim,n>::exclude(Space& home) {
    GECODE_ME_CHECK(o.zero(home));
    return ES_OK;
  }

  template<class Dim, int n>
  forceinline void
  OptBox<Dim,n>::update(Space& home, bool share, OptBox<Dim,n>& b) {
    ManBox<Dim,n>::update(home, share, b);
    o.update(home, share, b.o);
  }

  template<class Dim, int n>
  forceinline void
  OptBox<Dim,n>::subscribe(Space& home, Propagator& p) {
    ManBox<Dim,n>::subscribe(home,p);
    o.subscribe(home, p, PC_BOOL_VAL);
  }
  template<class Dim, int n>
  forceinline void
  OptBox<Dim,n>::cancel(Space& home, Propagator& p) {
    ManBox<Dim,n>::cancel(home,p);
    o.cancel(home, p, PC_BOOL_VAL);
  }

}}}

// STATISTICS: int-prop

