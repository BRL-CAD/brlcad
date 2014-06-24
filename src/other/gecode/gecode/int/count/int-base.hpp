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

namespace Gecode { namespace Int { namespace Count {

  template<class VX, class VY>
  forceinline
  IntBase<VX,VY>::IntBase(Home home,
                          ViewArray<VX>& x0, int n_s0, VY y0, int c0)
    : Propagator(home), x(x0), n_s(n_s0), y(y0), c(c0) {
    if (vtd(y) == VTD_INTSET)
      home.notice(*this,AP_DISPOSE);
    for (int i=n_s; i--; )
      x[i].subscribe(home,*this,PC_INT_DOM);
    subscribe(home,*this,y);
  }

  template<class VX, class VY>
  forceinline size_t
  IntBase<VX,VY>::dispose(Space& home) {
    if (vtd(y) == VTD_INTSET)
      home.ignore(*this,AP_DISPOSE);
    for (int i=n_s; i--; )
      x[i].cancel(home,*this,PC_INT_DOM);
    cancel(home,*this,y);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class VX, class VY>
  forceinline
  IntBase<VX,VY>::IntBase(Space& home, bool share, IntBase<VX,VY>& p)
    : Propagator(home,share,p), n_s(p.n_s), c(p.c) {
    x.update(home,share,p.x);
    y.update(home,share,p.y);
  }

  template<class VX, class VY>
  PropCost
  IntBase<VX,VY>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO,x.size());
  }

}}}

// STATISTICS: int-prop
