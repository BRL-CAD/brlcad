/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Patrick Pekczynski <pekczynski@ps.uni-sb.de>
 *
 *  Copyright:
 *     Patrick Pekczynski, 2004
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

#include <gecode/int/sorted.hh>

namespace Gecode {

  void
  sorted(Home home, const IntVarArgs& x, const IntVarArgs& y,
         const IntVarArgs& z, IntConLevel) {
    using namespace Int;
    if ((x.size() != y.size()) || (x.size() != z.size()))
      throw ArgumentSizeMismatch("Int::Sorted");
    if (x.same(home,y) || x.same(home,z) || y.same(home,z))
      throw ArgumentSame("Int::Sorted");

    if (home.failed()) return;

    if (x.size()==0) return;

    ViewArray<IntView> x0(home,x), y0(home,y), z0(home,z);

    GECODE_ES_FAIL(
                   (Sorted::Sorted<IntView,true>::post(home,x0,y0,z0)));
  }

  void
  sorted(Home home, const IntVarArgs& x, const IntVarArgs& y,
         IntConLevel) {
    using namespace Int;
    if (x.size() != y.size())
      throw ArgumentSizeMismatch("Int::Sorted");
    if (x.same(home,y))
      throw ArgumentSame("Int::Sorted");

    if (home.failed()) return;

    if (x.size()==0) return;

    ViewArray<IntView> x0(home,x), y0(home,y), z0(home,0);

    GECODE_ES_FAIL(
                   (Sorted::Sorted<IntView,false>::post(home,x0,y0,z0)));
  }

}

// STATISTICS: int-post
