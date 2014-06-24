/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

#include <algorithm>

namespace Gecode { namespace Int { namespace NValues {

  forceinline int
  SymBitMatrix::pos(int x, int y) const {
    assert(x < y);
    return (x*(2*n-x-1)) / 2 + y - x - 1;
  }

  forceinline
  SymBitMatrix::SymBitMatrix(Region& r, int n0)
    : Support::BitSet<Region>(r,static_cast<unsigned int>((n0*n0-n0)/2)),
      n(n0) {}

  forceinline bool 
  SymBitMatrix::get(int x, int y) const {
    assert(x != y);
    if (x > y) std::swap(x,y);
    return Support::BitSet<Region>::get(static_cast<unsigned int>(pos(x,y)));
  }

  forceinline void
  SymBitMatrix::set(int x, int y) {
    assert(x != y);
    if (x > y) std::swap(x,y);
    Support::BitSet<Region>::set(static_cast<unsigned int>(pos(x,y)));
  }

}}}

// STATISTICS: int-prop
