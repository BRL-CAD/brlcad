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

#include <gecode/int/nvalues.hh>
#include <gecode/int/rel.hh>

namespace Gecode {

  void
  nvalues(Home home, const IntVarArgs& x, IntRelType irt, int y,
          IntConLevel) {
    using namespace Int;
    Limits::check(y,"Int::nvalues");
    // Due to the quadratic Boolean matrix used in propagation
    long long int n = x.size();
    Limits::check(n*n,"Int::nvalues");

    if (home.failed()) return;

    ViewArray<IntView> xv(home,x);

    switch (irt) {
    case IRT_EQ:
      {
        ConstIntView yv(y);
        GECODE_ES_FAIL(NValues::EqInt<ConstIntView>::post(home,xv,yv));
      }
      break;
    case IRT_NQ:
      {
        IntVar z(home,0,x.size());
        GECODE_ME_FAIL(IntView(z).nq(home,y));
        GECODE_ES_FAIL(NValues::EqInt<IntView>::post(home,xv,z));
      }
      break;
    case IRT_LE:
      y--;
      // Fall through
    case IRT_LQ:
      {
        ConstIntView yv(y);
        GECODE_ES_FAIL(NValues::LqInt<ConstIntView>::post(home,xv,yv));
      }
      break;
    case IRT_GR:
      y++;
      // Fall through
    case IRT_GQ:
      {
        ConstIntView yv(y);
        GECODE_ES_FAIL(NValues::GqInt<ConstIntView>::post(home,xv,yv));
      }
      break;
    default:
      throw UnknownRelation("Int::nvalues");
    }
  }

  void
  nvalues(Home home, const IntVarArgs& x, IntRelType irt, IntVar y,
          IntConLevel) {
    using namespace Int;
    // Due to the quadratic Boolean matrix used in propagation
    long long int n = x.size();
    Limits::check(n*n,"Int::nvalues");

    if (home.failed()) return;

    if (y.assigned()) {
      nvalues(home, x, irt, y.val());
      return;
    }

    ViewArray<IntView> xv(home,x);

    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL(NValues::EqInt<IntView>::post(home,xv,y));
      break;
    case IRT_NQ:
      {
        IntVar z(home,0,x.size());
        GECODE_ES_FAIL(Rel::Nq<IntView>::post(home,y,z));
        GECODE_ES_FAIL(NValues::EqInt<IntView>::post(home,xv,z));
      }
      break;
    case IRT_LE:
      {
        OffsetView z(y,-1);
        GECODE_ES_FAIL(NValues::LqInt<OffsetView>::post(home,xv,z));
      }
      break;
    case IRT_LQ:
      GECODE_ES_FAIL(NValues::LqInt<IntView>::post(home,xv,y));
      break;
    case IRT_GR:
      {
        OffsetView z(y,1);
        GECODE_ES_FAIL(NValues::GqInt<OffsetView>::post(home,xv,z));
      }
      break;
    case IRT_GQ:
      GECODE_ES_FAIL(NValues::GqInt<IntView>::post(home,xv,y));
      break;
    default:
      throw UnknownRelation("Int::nvalues");
    }
  }

  void
  nvalues(Home home, const BoolVarArgs& x, IntRelType irt, int y,
          IntConLevel) {
    using namespace Int;
    Limits::check(y,"Int::nvalues");

    if (home.failed()) return;

    Region region(home);
    ViewArray<BoolView> xv(region,x);

    switch (irt) {
    case IRT_EQ:
      {
        ConstIntView yv(y);
        GECODE_ES_FAIL(NValues::EqBool<ConstIntView>::post(home,xv,yv));
      }
      break;
    case IRT_NQ:
      {
        IntVar z(home,0,2);
        GECODE_ME_FAIL(IntView(z).nq(home,y));
        GECODE_ES_FAIL(NValues::EqBool<IntView>::post(home,xv,z));
      }
      break;
    case IRT_LE:
      y--;
      // Fall through
    case IRT_LQ:
      {
        ConstIntView yv(y);
        GECODE_ES_FAIL(NValues::LqBool<ConstIntView>::post(home,xv,yv));
      }
      break;
    case IRT_GR:
      y++;
      // Fall through
    case IRT_GQ:
      {
        ConstIntView yv(y);
        GECODE_ES_FAIL(NValues::GqBool<ConstIntView>::post(home,xv,yv));
      }
      break;
    default:
      throw UnknownRelation("Int::nvalues");
    }
  }

  void
  nvalues(Home home, const BoolVarArgs& x, IntRelType irt, IntVar y,
          IntConLevel) {
    using namespace Int;

    if (home.failed()) return;

    if (y.assigned()) {
      nvalues(home, x, irt, y.val());
      return;
    }

    Region region(home);
    ViewArray<BoolView> xv(region,x);

    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL(NValues::EqBool<IntView>::post(home,xv,y));
      break;
    case IRT_NQ:
      {
        IntVar z(home,0,2);
        GECODE_ES_FAIL(Rel::Nq<IntView>::post(home,y,z));
        GECODE_ES_FAIL(NValues::EqBool<IntView>::post(home,xv,z));
      }
      break;
    case IRT_LE:
      {
        OffsetView z(y,-1);
        GECODE_ES_FAIL(NValues::LqBool<OffsetView>::post(home,xv,z));
      }
      break;
    case IRT_LQ:
      GECODE_ES_FAIL(NValues::LqBool<IntView>::post(home,xv,y));
      break;
    case IRT_GR:
      {
        OffsetView z(y,1);
        GECODE_ES_FAIL(NValues::GqBool<OffsetView>::post(home,xv,z));
      }
      break;
    case IRT_GQ:
      GECODE_ES_FAIL(NValues::GqBool<IntView>::post(home,xv,y));
      break;
    default:
      throw UnknownRelation("Int::nvalues");
    }
  }

}

// STATISTICS: int-post

