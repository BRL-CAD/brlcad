/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
 *     Gabor Szokoli, 2004
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

#include <gecode/set.hh>
#include <gecode/int.hh>

namespace Gecode { namespace Set { namespace Int {

  template<class View>
  forceinline
  Card<View>::Card(Home home, View y0, Gecode::Int::IntView y1)
    : MixBinaryPropagator<View,PC_SET_CARD,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND> (home, y0, y1) {}

  template<class View>
  forceinline ExecStatus
  Card<View>::post(Home home, View x0, Gecode::Int::IntView x1) {
    GECODE_ME_CHECK(x1.gq(home,0));
    GECODE_ME_CHECK(x0.cardMax(home, Gecode::Int::Limits::max));
    (void) new (home) Card(home,x0,x1);
    return ES_OK;
  }

  template<class View>
  forceinline
  Card<View>::Card(Space& home, bool share, Card& p)
    : MixBinaryPropagator<View,PC_SET_CARD,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND> (home, share, p) {}

  template<class View>
  Actor*
  Card<View>::copy(Space& home, bool share) {
   return new (home) Card(home,share,*this);
  }

  template<class View>
  ExecStatus
  Card<View>::propagate(Space& home, const ModEventDelta&) {
   int x1min, x1max;
   do {
     x1min = x1.min();
     x1max = x1.max();
     GECODE_ME_CHECK(x0.cardMin(home,static_cast<unsigned int>(x1min)));
     GECODE_ME_CHECK(x0.cardMax(home,static_cast<unsigned int>(x1max)));
     GECODE_ME_CHECK(x1.gq(home,static_cast<int>(x0.cardMin())));
     GECODE_ME_CHECK(x1.lq(home,static_cast<int>(x0.cardMax())));
   } while (x1.min() > x1min || x1.max() < x1max);
   if (x1.assigned())
     return home.ES_SUBSUMED(*this);
   return ES_FIX;
  }

}}}

// STATISTICS: set-prop
