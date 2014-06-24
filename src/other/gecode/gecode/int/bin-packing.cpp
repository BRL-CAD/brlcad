/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2010
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

#include <gecode/int/bin-packing.hh>

namespace Gecode {

  void
  binpacking(Home home, 
             const IntVarArgs& l, 
             const IntVarArgs& b, const IntArgs& s,
             IntConLevel) {
    using namespace Int;
    if (l.same(home,b))
      throw ArgumentSame("Int::binpacking");
    if (b.size() != s.size())
      throw ArgumentSizeMismatch("Int::binpacking");      
    for (int i=s.size(); i--; )
      Limits::nonnegative(s[i],"Int::binpacking");
    if (home.failed()) return;

    ViewArray<OffsetView> lv(home,l.size());
    for (int i=l.size(); i--; )
      lv[i] = OffsetView(l[i],0);

    if (b.size() == 0) {
      for (int i=l.size(); i--; )
        GECODE_ME_FAIL(lv[i].eq(home,0));
      return;
    }

    ViewArray<BinPacking::Item> bs(home,b.size());
    for (int i=bs.size(); i--; )
      bs[i] = BinPacking::Item(b[i],s[i]);

    Support::quicksort(&bs[0], bs.size());

    GECODE_ES_FAIL(Int::BinPacking::Pack::post(home,lv,bs));
  }

}

// STATISTICS: int-post
