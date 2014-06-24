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

#include <gecode/set/convex.hh>

namespace Gecode { namespace Set { namespace Convex {

  Actor*
  Convex::copy(Space& home, bool share) {
    return new (home) Convex(home,share,*this);
  }

  ExecStatus
  Convex::propagate(Space& home, const ModEventDelta&) {
    //I, drop ranges from UB that are shorter than cardMin
    //II, add range LB.smallest to LB.largest to LB
    //III, Drop ranges from UB that do not contain all elements of LB
    //that is: range.min()>LB.smallest or range.max()<LB.largest
    //This leaves only one range.
    //II
    if (x0.glbSize()>0) {
      GECODE_ME_CHECK( x0.include(home,x0.glbMin(),x0.glbMax()) );
    } else {
      //If lower bound is empty, we can still constrain the cardinality
      //maximum with the width of the longest range in UB.
      //No need to do this if there is anything in LB because UB
      //becomes a single range then.
       LubRanges<SetView> ubRangeIt(x0);
       unsigned int maxWidth = 0;
       for (;ubRangeIt();++ubRangeIt) {
         assert(ubRangeIt());
         maxWidth = std::max(maxWidth, ubRangeIt.width());
       }
       GECODE_ME_CHECK( x0.cardMax(home,maxWidth) );
    }


    //I + III

    Region r(home);
    LubRanges<SetView> ubRangeIt(x0);
    Iter::Ranges::Cache ubRangeItC(r,ubRangeIt);

    for (; ubRangeItC(); ++ubRangeItC) {
      if (ubRangeItC.width() < (unsigned int) x0.cardMin()
          || ubRangeItC.min() > x0.glbMin() //No need to test for empty lb.
          || ubRangeItC.max() < x0.glbMax()
          ) {
        GECODE_ME_CHECK( x0.exclude(home,ubRangeItC.min(), ubRangeItC.max()) );
      }
    }
    if (x0.assigned()) 
      return home.ES_SUBSUMED(*this);
    return ES_FIX;
  }

}}}

// STATISTICS: set-prop
