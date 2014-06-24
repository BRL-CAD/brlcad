/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Gabor Szokoli, 2003
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

#include <gecode/int/distinct.hh>

namespace Gecode {

  using namespace Int;

  void
  distinct(Home home, const IntVarArgs& x, IntConLevel icl) {
    if (x.same(home))
      throw ArgumentSame("Int::distinct");
    if (home.failed()) return;
    ViewArray<IntView> xv(home,x);
    switch (icl) {
    case ICL_BND:
      GECODE_ES_FAIL(Distinct::Bnd<IntView>::post(home,xv));
      break;
    case ICL_DOM:
      GECODE_ES_FAIL(Distinct::Dom<IntView>::post(home,xv));
      break;
    default:
      GECODE_ES_FAIL(Distinct::Val<IntView>::post(home,xv));
    }
  }

  void
  distinct(Home home, const IntArgs& c, const IntVarArgs& x,
           IntConLevel icl) {
    if (x.same(home))
      throw ArgumentSame("Int::distinct");
    if (c.size() != x.size())
      throw ArgumentSizeMismatch("Int::distinct");
    if (home.failed()) return;
    ViewArray<OffsetView> cx(home,x.size());
    for (int i = c.size(); i--; ) {
      long long int cx_min = (static_cast<long long int>(c[i]) +
                              static_cast<long long int>(x[i].min()));
      long long int cx_max = (static_cast<long long int>(c[i]) +
                              static_cast<long long int>(x[i].max()));
      Limits::check(c[i],"Int::distinct");
      Limits::check(cx_min,"Int::distinct");
      Limits::check(cx_max,"Int::distinct");
      cx[i] = OffsetView(x[i],c[i]);
    }
    switch (icl) {
    case ICL_BND:
      GECODE_ES_FAIL(Distinct::Bnd<OffsetView>::post(home,cx));
      break;
    case ICL_DOM:
      GECODE_ES_FAIL(Distinct::Dom<OffsetView>::post(home,cx));
      break;
    default:
      GECODE_ES_FAIL(Distinct::Val<OffsetView>::post(home,cx));
    }
  }

}

// STATISTICS: int-post
