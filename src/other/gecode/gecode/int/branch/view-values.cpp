/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2012
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

#include <gecode/int/branch.hh>

namespace Gecode { namespace Int { namespace Branch {

  PosValuesChoice::PosValuesChoice(const Brancher& b, const Pos& p, IntView x)
    : PosChoice(b,x.size(),p), n(0) {
    for (ViewRanges<IntView> r(x); r(); ++r)
      n++;
    pm = heap.alloc<PosMin>(n+1);
    unsigned int w=0;
    int i=0;
    for (ViewRanges<IntView> r(x); r(); ++r) {
      pm[i].min = r.min();
      pm[i].pos = w;
      w += r.width(); i++;
    }
    pm[i].pos = w;
  }

  PosValuesChoice::PosValuesChoice(const Brancher& b, unsigned int a, Pos p,
                                   Archive& e)
    : PosChoice(b,a,p) {
    e >> n;
    pm = heap.alloc<PosMin>(n+1);
    for (unsigned int i=0; i<n+1; i++) {
      e >> pm[i].pos;
      e >> pm[i].min;
    }
  }

  size_t
  PosValuesChoice::size(void) const {
    return sizeof(PosValuesChoice)+(n+1)*sizeof(PosMin);
  }

  PosValuesChoice::~PosValuesChoice(void) {
    heap.free<PosMin>(pm,n+1);
  }

  forceinline void
  PosValuesChoice::archive(Archive& e) const {
    PosChoice::archive(e);
    e << this->alternatives() << n;
    for (unsigned int i=0; i<n+1; i++) {
      e << pm[i].pos;
      e << pm[i].min;
    }
  }

}}}

// STATISTICS: int-branch
