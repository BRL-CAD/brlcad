/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

#include <gecode/int.hh>

namespace Gecode {

  IntSet::IntSetObject*
  IntSet::IntSetObject::allocate(int n) {
    IntSetObject* o = new IntSetObject;
    o->n = n;
    o->r = heap.alloc<Range>(n);
    return o;
  }

  SharedHandle::Object*
  IntSet::IntSetObject::copy(void) const {
    IntSetObject* o = allocate(n);
    o->size = size;
    for (int i=n; i--; )
      o->r[i]=r[i];
    return o;
  }

  bool
  IntSet::IntSetObject::in(int n) const {
    int l = 0;
    int r = this->n - 1;

    while (l <= r) {
      int m = l + (r - l) / 2;
      if ((this->r[m].min <= n) && (n <= this->r[m].max)) {
        return true;
      } else if (l == r) {
        return false;
      } else if (n < this->r[m].min) {
        r=m-1;
      } else {
        l=m+1;
      }
    }
    return false;
  }

  IntSet::IntSetObject::~IntSetObject(void) {
    heap.free<Range>(r,n);
  }

  /// Sort ranges according to increasing minimum
  class IntSet::MinInc {
  public:
    bool operator ()(const Range &x, const Range &y);
  };

  forceinline bool
  IntSet::MinInc::operator ()(const Range &x, const Range &y) {
    return x.min < y.min;
  }

  void
  IntSet::normalize(Range* r, int n) {
    if (n > 0) {
      // Sort ranges
      {
        MinInc lt_mi;
        Support::quicksort<Range>(r, n, lt_mi);
      }
      // Conjoin continuous ranges
      {
        int min = r[0].min;
        int max = r[0].max;
        int i = 1;
        int j = 0;
        while (i < n) {
          if (max+1 < r[i].min) {
            r[j].min = min; r[j].max = max; j++;
            min = r[i].min; max = r[i].max; i++;
          } else {
            max = std::max(max,r[i].max); i++;
          }
        }
        r[j].min = min; r[j].max = max;
        n=j+1;
      }
      IntSetObject* o = IntSetObject::allocate(n);
      unsigned int s = 0;
      for (int i=n; i--; ) {
        s += static_cast<unsigned int>(r[i].max-r[i].min+1);
        o->r[i]=r[i];
      }
      o->size = s;
      object(o);
    }
  }

  void
  IntSet::init(const int r[], int n) {
    Range* dr = heap.alloc<Range>(n);
    for (int i=n; i--; ) {
      dr[i].min=r[i]; dr[i].max=r[i];
    }
    normalize(&dr[0],n);
    heap.free(dr,n);
  }

  void
  IntSet::init(const int r[][2], int n) {
    Range* dr = heap.alloc<Range>(n);
    int j = 0;
    for (int i=n; i--; )
      if (r[i][0] <= r[i][1]) {
        dr[j].min=r[i][0]; dr[j].max=r[i][1]; j++;
      }
    normalize(&dr[0],j);
    heap.free(dr,n);
  }

  void
  IntSet::init(int n, int m) {
    if (n <= m) {
      IntSetObject* o = IntSetObject::allocate(1);
      o->r[0].min = n; o->r[0].max = m;
      o->size = static_cast<unsigned int>(m - n + 1);
      object(o);
    }
  }

  const IntSet IntSet::empty;

}

// STATISTICS: int-var

