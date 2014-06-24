/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
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

namespace Gecode { namespace Set { namespace Sequence {

  /*
   * "Sequenced union" propagator
   *
   */

  forceinline
  SeqU::SeqU(Home home, ViewArray<SetView>& x, SetView y)
    : NaryOnePropagator<SetView,PC_SET_ANY>(home,x, y) {}

  forceinline
  SeqU::SeqU(Space& home, bool share, SeqU& p)
    : NaryOnePropagator<SetView,PC_SET_ANY>(home,share,p) {
    unionOfDets.update(home, p.unionOfDets);
  }

  forceinline ExecStatus
  SeqU::post(Home home, ViewArray<SetView> x, SetView y) {
    switch (x.size()) {
    case 0:
      GECODE_ME_CHECK(y.cardMax(home, 0));
      return ES_OK;
    case 1:
      return Rel::Eq<SetView,SetView>::post(home, x[0], y);
    default:
      if (x.shared(home) || x.shared(home,y))
        return ES_FAILED;
      (void) new (home) SeqU(home,x,y);
      return ES_OK;
    }
  }

}}}

// STATISTICS: set-prop
