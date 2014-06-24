/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     David Rijsman <David.Rijsman@quintiq.com>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     David Rijsman, 2009
 *     Christian Schulte, 2009
 *
 *  Last modified:
 *     $Date$
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

#include <gecode/int/sequence.hh>

#include <algorithm>

namespace Gecode {

  using namespace Int;

  void
  sequence(Home home, const IntVarArgs& x, const IntSet &s, 
           int q, int l, int u, IntConLevel) {
    Limits::check(s.min(),"Int::sequence");
    Limits::check(s.max(),"Int::sequence");

    if (x.size() == 0)
      throw TooFewArguments("Int::sequence");

    Limits::check(q,"Int::sequence");
    Limits::check(l,"Int::sequence");
    Limits::check(u,"Int::sequence");

    if (x.same(home))
      throw ArgumentSame("Int::sequence");

    if ((q < 1) || (q > x.size()))
      throw OutOfLimits("Int::sequence");

    if (home.failed())
      return;

    // Normalize l and u
    l=std::max(0,l); u=std::min(q,u);

    // Lower bound of values taken can never exceed upper bound
    if (u < l) {
      home.fail(); return;
    }

    // Already subsumed as any number of values taken is okay
    if ((0 == l) && (q == u)) 
      return;

    // All variables must take a value in s
    if (l == q) {
      for (int i=x.size(); i--; ) {
        IntView xv(x[i]);
        IntSetRanges ris(s);
        GECODE_ME_FAIL(xv.inter_r(home,ris,false));
      }
      return;
    }

    // No variable can take a value in s
    if (0 == u) {
      for (int i=x.size(); i--; ) {
        IntView xv(x[i]);
        IntSetRanges ris(s);
        GECODE_ME_FAIL(xv.minus_r(home,ris,false));
      }
      return;
    }

    ViewArray<IntView> xv(home,x);
    if (s.size() == 1) {
      GECODE_ES_FAIL(
                     (Sequence::Sequence<IntView,int>::post
                      (home,xv,s.min(),q,l,u)));
    } else {
      GECODE_ES_FAIL(
                     (Sequence::Sequence<IntView,IntSet>::post
                      (home,xv,s,q,l,u)));
    }
  }

  void
  sequence(Home home, const BoolVarArgs& x, const IntSet& s, 
           int q, int l, int u, IntConLevel) {
    if ((s.min() < 0) || (s.max() > 1))
      throw NotZeroOne("Int::sequence");

    if (x.size() == 0)
      throw TooFewArguments("Int::sequence");

    Limits::check(q,"Int::sequence");
    Limits::check(l,"Int::sequence");
    Limits::check(u,"Int::sequence");

    if (x.same(home))
      throw ArgumentSame("Int::sequence");

    if ((q < 1) || (q > x.size()))
      throw OutOfLimits("Int::sequence");

    if (home.failed())
      return;

    // Normalize l and u
    l=std::max(0,l); u=std::min(q,u);

    // Lower bound of values taken can never exceed upper bound
    if (u < l) {
      home.fail(); return;
    }

    // Already subsumed as any number of values taken is okay
    if ((0 == l) && (q == u)) 
      return;

    // Check whether the set is {0,1}, then the number of values taken is q
    if ((s.min() == 0) && (s.max() == 1)) {
      if ((l > 0) || (u < q))
        home.failed();
      return;
    }
    assert(s.min() == s.max());

    // All variables must take a value in s
    if (l == q) {
      if (s.min() == 0) {
        for (int i=x.size(); i--; ) {
          BoolView xv(x[i]); GECODE_ME_FAIL(xv.zero(home));
        }
      } else {
        assert(s.min() == 1);
        for (int i=x.size(); i--; ) {
          BoolView xv(x[i]); GECODE_ME_FAIL(xv.one(home));
        }
      }
      return;
    }

    // No variable can take a value in s
    if (0 == u) {
      if (s.min() == 0) {
        for (int i=x.size(); i--; ) {
          BoolView xv(x[i]); GECODE_ME_FAIL(xv.one(home));
        }
      } else {
        assert(s.min() == 1);
        for (int i=x.size(); i--; ) {
          BoolView xv(x[i]); GECODE_ME_FAIL(xv.zero(home));
        }
      }
      return;
    }

    ViewArray<BoolView> xv(home,x);

    GECODE_ES_FAIL(
                   (Sequence::Sequence<BoolView,int>::post
                    (home,xv,s.min(),q,l,u)));
  }

}

// STATISTICS: int-post
