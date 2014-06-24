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

namespace Gecode { namespace Int { namespace Count {

  template<class VX, class VY, class VZ>
  forceinline
  ViewBase<VX,VY,VZ>::ViewBase(Home home,
                               ViewArray<VX>& x0, VY y0, VZ z0, int c0)
    : Propagator(home), x(x0), y(y0), z(z0), c(c0) {
    if (vtd(y) == VTD_INTSET)
      home.notice(*this,AP_DISPOSE);
    x.subscribe(home,*this,PC_INT_DOM);
    subscribe(home,*this,y);
    z.subscribe(home,*this,PC_INT_BND);
  }

  template<class VX, class VY, class VZ>
  forceinline
  ViewBase<VX,VY,VZ>::ViewBase(Space& home, bool share, ViewBase<VX,VY,VZ>& p)
    : Propagator(home,share,p), c(p.c) {
    x.update(home,share,p.x);
    y.update(home,share,p.y);
    z.update(home,share,p.z);
  }

  template<class VX, class VY, class VZ>
  PropCost
  ViewBase<VX,VY,VZ>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO,x.size()+1);
  }

  template<class VX, class VY, class VZ>
  forceinline size_t
  ViewBase<VX,VY,VZ>::dispose(Space& home) {
    if (vtd(y) == VTD_INTSET)
      home.ignore(*this,AP_DISPOSE);
    x.cancel(home,*this,PC_INT_DOM);
    cancel(home,*this,y);
    z.cancel(home,*this,PC_INT_BND);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class VX, class VY, class VZ>
  forceinline void
  ViewBase<VX,VY,VZ>::count(Space& home) {
    int n = x.size();
    for (int i=n; i--; )
      switch (holds(x[i],y)) {
      case RT_FALSE:
        x[i].cancel(home,*this,PC_INT_DOM); x[i]=x[--n];
        break;
      case RT_TRUE:
        x[i].cancel(home,*this,PC_INT_DOM); x[i]=x[--n];
        c--;
        break;
      case RT_MAYBE:
        break;
      default:
        GECODE_NEVER;
      }
    x.size(n);
  }

  template<class VX, class VY, class VZ>
  forceinline int
  ViewBase<VX,VY,VZ>::atleast(void) const {
    return -c;
  }

  template<class VX, class VY, class VZ>
  forceinline int
  ViewBase<VX,VY,VZ>::atmost(void) const {
    return x.size()-c;
  }

  template<class VX>
  forceinline bool
  shared(const IntSet&, VX) {
    return false;
  }
  template<class VX, class VY, class VZ>
  forceinline bool
  ViewBase<VX,VY,VZ>::sharing(const ViewArray<VX>& x, 
                              const VY& y, const VZ& z) {
    if (shared(y,z))
      return true;
    for (int i = x.size(); i--; )
      if (shared(x[i],z))
        return true;
    return false;
  }

}}}

// STATISTICS: int-prop
