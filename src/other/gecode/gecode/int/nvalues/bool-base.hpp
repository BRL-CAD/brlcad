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

namespace Gecode { namespace Int { namespace NValues {

  template<class VY>
  forceinline
  BoolBase<VY>::BoolBase(Home home, 
                         int status0, ViewArray<BoolView>& x, VY y0)
    : Propagator(home), status(status0), c(home), y(y0) {
    y.subscribe(home,*this,PC_INT_BND);
    for (int i=x.size(); i--; ) {
      assert(!x[i].assigned());
      (void) new (home) ViewAdvisor<BoolView>(home, *this, c, x[i]);
    }
  }

  template<class VY>
  forceinline
  BoolBase<VY>::BoolBase(Space& home, bool share, BoolBase<VY>& p)
    : Propagator(home,share,p), status(p.status) {
    c.update(home,share,p.c);
    y.update(home,share,p.y);
  }

  template<class VY>
  PropCost
  BoolBase<VY>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::LO);
  }

  template<class VY>
  ExecStatus
  BoolBase<VY>::advise(Space& home, Advisor& _a, const Delta&) {
    ViewAdvisor<BoolView>& a(static_cast<ViewAdvisor<BoolView>&>(_a));
    ExecStatus es;
    if (status == (VS_ZERO | VS_ONE)) {
      // Everything is already decided
      es = ES_FIX;
    } else {
      if (a.view().zero())
        status |= VS_ZERO;
      else
        status |= VS_ONE;
      es = ES_NOFIX;
    }
    a.dispose(home,c);
    if (c.empty())
      es = ES_NOFIX;
    return es;
  }

  template<class VY>
  forceinline size_t
  BoolBase<VY>::dispose(Space& home) {
    c.dispose(home);
    y.cancel(home,*this,PC_INT_BND);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

}}}

// STATISTICS: int-prop
