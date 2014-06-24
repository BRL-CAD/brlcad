/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2007
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

#include <gecode/int/channel.hh>

namespace Gecode { namespace Int { namespace Channel {

  forceinline
  LinkSingle::LinkSingle(Space& home, bool share, LinkSingle& p)
    : MixBinaryPropagator<BoolView,PC_BOOL_VAL,IntView,PC_INT_VAL>
  (home,share,p) {}

  Actor*
  LinkSingle::copy(Space& home, bool share) {
    return new (home) LinkSingle(home,share,*this);
  }

  PropCost
  LinkSingle::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::LO);
  }

  ExecStatus
  LinkSingle::propagate(Space& home, const ModEventDelta&) {
    if (x0.zero()) {
      GECODE_ME_CHECK(x1.eq(home,0));
    } else if (x0.one()) {
      GECODE_ME_CHECK(x1.eq(home,1));
    } else {
      assert(x0.none() && x1.assigned());
      if (x1.val() == 0) {
        GECODE_ME_CHECK(x0.zero_none(home));
      } else {
        assert(x1.val() == 1);
        GECODE_ME_CHECK(x0.one_none(home));
      }
    }
    return home.ES_SUBSUMED(*this);
  }

}}}

// STATISTICS: int-prop
