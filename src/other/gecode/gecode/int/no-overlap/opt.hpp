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

  template<class Box>
  forceinline
  OptProp<Box>::OptProp(Home home, Box* b, int n, int m0)
    : Base<Box>(home,b,n), m(m0) {
    for (int i=m; i--; )
      b[n+i].subscribe(home, *this);
  }

  template<class Box>
  ExecStatus
  OptProp<Box>::post(Home home, Box* b, int n) {
    // Partition into mandatory and optional boxes
    if (n > 1) {
      int p = Base<Box>::partition(b, 0, n);
      (void) new (home) OptProp<Box>(home,b,p,n-p);
    }
    return ES_OK;
  }

  template<class Box>
  forceinline size_t 
  OptProp<Box>::dispose(Space& home) {
    for (int i=m; i--; )
      b[n+i].cancel(home, *this);
    (void) Base<Box>::dispose(home);
    return sizeof(*this);
  }


  template<class Box>
  forceinline
  OptProp<Box>::OptProp(Space& home, bool shared, OptProp<Box>& p) 
    : Base<Box>(home, shared, p, p.n + p.m), m(p.m) {}

  template<class Box>
  Actor* 
  OptProp<Box>::copy(Space& home, bool share) {
    return new (home) OptProp<Box>(home,share,*this);
  }

  template<class Box>
  ExecStatus 
  OptProp<Box>::propagate(Space& home, const ModEventDelta& med) {
    Region r(home);

    if (BoolView::me(med) == ME_BOOL_VAL) {
      // Eliminate excluded boxes
      for (int i=m; i--; )
        if (b[n+i].excluded()) {
          b[n+i].cancel(home,*this);
          b[n+i] = b[n+(--m)];
        }
      // Reconsider optional boxes
      if (m > 0) {
        int p = Base<Box>::partition(b+n, 0, m);
        n += p; m -= p;
      }
    }

    // Number of disjoint boxes
    int* db = r.alloc<int>(n);
    for (int i=n; i--; )
      db[i] = n-1;

    // Number of boxes to be eliminated
    int e = 0;
    for (int i=n; i--; ) {
      assert(b[i].mandatory());
      for (int j=i; j--; ) 
        if (b[i].nooverlap(b[j])) {
          assert(db[i] > 0); assert(db[j] > 0);
          if (--db[i] == 0) e++;
          if (--db[j] == 0) e++;
          continue;
        } else {
          GECODE_ES_CHECK(b[i].nooverlap(home,b[j]));
        }
    }

    if (m == 0) {
      if (e == n)
        return home.ES_SUBSUMED(*this);
      int i = n-1;
      while (e > 0) {
        // Eliminate boxes that do not overlap
        while (db[i] > 0)
          i--;
        b[i].cancel(home, *this);
        b[i] = b[--n]; b[n] = b[n+m];
        e--; i--;
      }
      if (n < 2)
        return home.ES_SUBSUMED(*this);
    }

    // Check whether some optional boxes must be excluded
    for (int i=m; i--; ) {
      if (b[n+i].optional()) {
        for (int j=n; j--; )
          if (b[n+i].overlap(b[j])) {
            GECODE_ES_CHECK(b[n+i].exclude(home));
            b[n+i].cancel(home,*this);
            b[n+i] = b[n+(--m)];
            break;
          }
      } else {
        // This might be the case if the same Boolean view occurs
        // several times and has already been excluded
        assert(b[n+i].excluded());
        b[n+i].cancel(home,*this);
        b[n+i] = b[n+(--m)];
      }
    }

    return ES_NOFIX;
  }

}}}

// STATISTICS: int-prop

