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

  inline ExecStatus
  propagateSeq(Space& home, bool& modified, bool& assigned,
               ViewArray<SetView>& x) {
    int lastElem = x.size()-1;
    int cur_max = BndSet::MAX_OF_EMPTY;
    int cur_min = BndSet::MIN_OF_EMPTY;

    Region r(home);
    Support::DynamicArray<int,Region> ub(r);

    for (int i=0; i<lastElem; i++) {
      if (x[i].glbSize() > 0)
        cur_max = std::max(cur_max, x[i].glbMax());
      if (x[i].cardMin() > 0)
        cur_max = std::max(cur_max, x[i].lubMinN(x[i].cardMin()-1));
      if (cur_max >= Limits::min)
        GECODE_SET_ME_CHECK_VAL_B(modified,
                                  x[i+1].exclude(home, Limits::min,
                                                 cur_max),
                                  assigned);

      if (x[lastElem-i].lubSize() > 0) {
        cur_min = std::min(cur_min, x[lastElem-i].glbMin());
        if (x[lastElem-i].cardMin() > 0) {
          // Compute n-th largest element in x[lastElem-i].lub
          // for n = x[lastElem-i].cardMin()-1
          int maxN = BndSet::MAX_OF_EMPTY;
          int j=0;
          for (LubRanges<SetView> ubr(x[lastElem-i]); ubr(); ++ubr, ++j) {
            ub[2*j]=ubr.min(); ub[2*j+1]=ubr.max();
          }
          unsigned int xcm = x[lastElem-i].cardMin()-1;
          while (j--) {
            unsigned int width = static_cast<unsigned int>(ub[2*j+1]-ub[2*j]+1);
            if (width > xcm) {
              maxN = static_cast<int>(ub[2*j+1]-xcm);
              break;
            }
            xcm -= width;
          }
          cur_min = std::min(cur_min, maxN);
        }
      }
      if (Limits::max>=cur_min)
        GECODE_SET_ME_CHECK_VAL_B(modified,
                                  x[lastElem-i-1].exclude(home, cur_min,
                                                          Limits::max),
                                  assigned);
    }
    return ES_NOFIX;
  }

}}}

// STATISTICS: set-prop
