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

namespace Gecode { namespace Int { namespace Channel {

  forceinline
  LinkSingle::LinkSingle(Home home, BoolView x0, IntView x1)
    : MixBinaryPropagator<BoolView,PC_BOOL_VAL,IntView,PC_INT_VAL>
  (home,x0,x1) {}

  forceinline ExecStatus
  LinkSingle::post(Home home, BoolView x0, IntView x1) {
    if (x1.assigned()) {
      switch (x1.val()) {
      case 0:
        GECODE_ME_CHECK(x0.zero(home)); break;
      case 1:
        GECODE_ME_CHECK(x0.one(home)); break;
      default:
        return ES_FAILED;
      }
    } else if (x0.zero()) {
      GECODE_ME_CHECK(x1.eq(home,0));
    } else if (x0.one()) {
      GECODE_ME_CHECK(x1.eq(home,1));
    } else {
      GECODE_ME_CHECK(x1.gq(home,0));
      GECODE_ME_CHECK(x1.lq(home,1));
      (void) new (home) LinkSingle(home,x0,x1);
    }
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

