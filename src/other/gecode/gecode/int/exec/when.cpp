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

#include <gecode/int/exec.hh>

namespace Gecode { namespace Int { namespace Exec {

  ExecStatus
  When::post(Home home, BoolView x, void (*t)(Space&), void (*e)(Space&)) {
    if (x.zero() && (e != NULL)) {
      e(home);
      return home.failed() ? ES_FAILED : ES_OK;
    } else if (x.zero() && (t != NULL)) {
      t(home);
      return home.failed() ? ES_FAILED : ES_OK;
    } else {
      (void) new (home) When(home,x,t,e);
      return ES_OK;
    }
  }

  Actor*
  When::copy(Space& home, bool share) {
    return new (home) When(home,share,*this);
  }

  ExecStatus
  When::propagate(Space& home, const ModEventDelta&) {
    if (x0.zero() && (e != NULL)) {
      e(home);
    } else if (t != NULL) {
      assert(x0.one());
      t(home);
    }
    if (home.failed()) return ES_FAILED;
    return home.ES_SUBSUMED(*this);
  }


}}}

// STATISTICS: int-prop

