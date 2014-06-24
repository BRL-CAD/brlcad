/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
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

namespace Gecode { namespace Int { namespace Channel {

  template<class Info, class Offset, PropCond pc>
  forceinline
  Base<Info,Offset,pc>::Base(Home home, int n0, Info* xy0,
                             Offset& ox0, Offset& oy0)
    : Propagator(home), n(n0), n_na(2*n), ox(ox0), oy(oy0), xy(xy0) {
    for (int i=2*n; i--; )
      xy[i].view.subscribe(home,*this,pc);
  }

  template<class Info, class Offset, PropCond pc>
  forceinline
  Base<Info,Offset,pc>::Base(Space& home, bool share, Base<Info,Offset,pc>& p)
    : Propagator(home,share,p), n(p.n), n_na(p.n_na),
      xy(home.alloc<Info>(2*n)) {
    ox.update(p.ox);
    oy.update(p.oy);
    for (int i=2*n; i--; )
      xy[i].update(home,share,p.xy[i]);
  }

  template<class Info, class Offset, PropCond pc>
  PropCost
  Base<Info,Offset,pc>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::LO, 2*n);
  }

  template<class Info, class Offset, PropCond pc>
  forceinline size_t
  Base<Info,Offset,pc>::dispose(Space& home) {
    for (int i=2*n; i--; )
      xy[i].view.cancel(home,*this,pc);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

}}}

// STATISTICS: int-prop

