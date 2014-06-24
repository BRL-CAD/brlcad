/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
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

#include <gecode/int/rel.hh>

namespace Gecode { namespace Int { namespace Element {

  forceinline
  Pair::Pair(Home home, IntView x0, IntView x1, IntView x2, int w0)
    : TernaryPropagator<IntView,PC_INT_DOM>(home,x0,x1,x2), w(w0) {}

  inline ExecStatus
  Pair::post(Home home, IntView x0, IntView x1, IntView x2,
             int w, int h) {
    GECODE_ME_CHECK(x0.gq(home,0)); GECODE_ME_CHECK(x0.le(home,w));
    GECODE_ME_CHECK(x1.gq(home,0)); GECODE_ME_CHECK(x1.le(home,h));
    GECODE_ME_CHECK(x2.gq(home,0)); GECODE_ME_CHECK(x2.le(home,w*h));
    if (x0.assigned() && x1.assigned()) {
      GECODE_ME_CHECK(x2.eq(home,x0.val()+w*x1.val()));
    } else if (x1.assigned()) {
      OffsetView x0x1w(x0,x1.val()*w);
      return Rel::EqDom<OffsetView,IntView>::post(home,x0x1w,x2);
    } else if (x2.assigned()) {
      GECODE_ME_CHECK(x0.eq(home,x2.val() % w));
      GECODE_ME_CHECK(x1.eq(home,static_cast<int>(x2.val() / w)));
    } else {
      assert(!shared(x0,x2) && !shared(x1,x2));
      (void) new (home) Pair(home,x0,x1,x2,w);
    }
    return ES_OK;
  }

  forceinline
  Pair::Pair(Space& home, bool share, Pair& p)
    : TernaryPropagator<IntView,PC_INT_DOM>(home,share,p), w(p.w) {}

}}}

// STATISTICS: int-prop

