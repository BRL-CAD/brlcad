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
  Base<Box>::Base(Home home, Box* b0, int n0)
    : Propagator(home), b(b0), n(n0) {
    for (int i=n; i--; )
      b[i].subscribe(home,*this);
  }

  template<class Box>
  forceinline int
  Base<Box>::partition(Box* b, int i, int n) {
    int j = n-1;
    while (true) {
      while (!b[j].mandatory() && (--j >= 0)) {}
      while (b[i].mandatory() && (++i < n)) {}
      if (j <= i) break;
      std::swap(b[i],b[j]);
    }
    return i;
  }

  template<class Box>
  forceinline size_t 
  Base<Box>::dispose(Space& home) {
    for (int i=n; i--; )
      b[i].cancel(home,*this);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }


  template<class Box>
  forceinline
  Base<Box>::Base(Space& home, bool shared, Base<Box>& p, int m) 
    : Propagator(home,shared,p), b(home.alloc<Box>(m)), n(p.n) {
    for (int i=m; i--; )
      b[i].update(home,shared,p.b[i]);
  }

  template<class Box>
  PropCost 
  Base<Box>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::HI,Box::dim()*n);
  }

}}}

// STATISTICS: int-prop

