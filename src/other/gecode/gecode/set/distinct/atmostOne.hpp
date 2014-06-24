/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
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

namespace Gecode { namespace Set { namespace Distinct {

  /*
   * "AtMostOneIntersection" propagator
   *
   */

  forceinline
  AtmostOne::AtmostOne(Home home, ViewArray<SetView>& x, unsigned int _c)
    : NaryPropagator<SetView, PC_SET_ANY>(home,x), c(_c) {}

  forceinline
  AtmostOne::AtmostOne(Space& home, bool share, AtmostOne& p)
    : NaryPropagator<SetView, PC_SET_ANY>(home,share,p), c(p.c) {}

  forceinline ExecStatus
  AtmostOne::post(Home home, ViewArray<SetView> x, unsigned int c) {
    for (int i=x.size(); i--;) {
      GECODE_ME_CHECK(x[i].cardMin(home, c));
      GECODE_ME_CHECK(x[i].cardMax(home, c));
    }

    (void) new (home) AtmostOne(home,x,c);
    return ES_OK;
  }

}}}

// STATISTICS: set-prop
