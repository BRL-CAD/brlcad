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

#include <gecode/set/branch.hh>

namespace Gecode { namespace Set { namespace Branch {

  ViewSel<SetView>*
  viewsel(Space& home, const SetVarBranch& svb) {
    switch (svb.select()) {
    case SetVarBranch::SEL_NONE:
      return new (home) ViewSelNone<SetView>(home,svb);
    case SetVarBranch::SEL_RND:
      return new (home) ViewSelRnd<SetView>(home,svb);
    default: break;
    }
    if (svb.tbl() != NULL) {
      switch (svb.select()) {
      case SetVarBranch::SEL_MERIT_MIN:
        return new (home) ViewSelMinTbl<MeritFunction<SetView> >(home,svb);
      case SetVarBranch::SEL_MERIT_MAX:
        return new (home) ViewSelMaxTbl<MeritFunction<SetView> >(home,svb);
      case SetVarBranch::SEL_MIN_MIN:
        return new (home) ViewSelMinTbl<MeritMin>(home,svb);
      case SetVarBranch::SEL_MIN_MAX:
        return new (home) ViewSelMaxTbl<MeritMin>(home,svb);
      case SetVarBranch::SEL_MAX_MIN:
        return new (home) ViewSelMinTbl<MeritMax>(home,svb);
      case SetVarBranch::SEL_MAX_MAX:
        return new (home) ViewSelMaxTbl<MeritMax>(home,svb);
      case SetVarBranch::SEL_SIZE_MIN:
        return new (home) ViewSelMinTbl<MeritSize>(home,svb);
      case SetVarBranch::SEL_SIZE_MAX:
        return new (home) ViewSelMaxTbl<MeritSize>(home,svb);
      case SetVarBranch::SEL_DEGREE_MIN:
        return new (home) ViewSelMinTbl<MeritDegree<SetView> >(home,svb);
      case SetVarBranch::SEL_DEGREE_MAX:
        return new (home) ViewSelMaxTbl<MeritDegree<SetView> >(home,svb);
      case SetVarBranch::SEL_AFC_MIN:
        return new (home) ViewSelMinTbl<MeritAFC<SetView> >(home,svb);
      case SetVarBranch::SEL_AFC_MAX:
        return new (home) ViewSelMaxTbl<MeritAFC<SetView> >(home,svb);
      case SetVarBranch::SEL_ACTIVITY_MIN:
        return new (home) ViewSelMinTbl<MeritActivity<SetView> >(home,svb);
      case SetVarBranch::SEL_ACTIVITY_MAX:
        return new (home) ViewSelMaxTbl<MeritActivity<SetView> >(home,svb);
      case SetVarBranch::SEL_DEGREE_SIZE_MIN:
        return new (home) ViewSelMinTbl<MeritDegreeSize>(home,svb);
      case SetVarBranch::SEL_DEGREE_SIZE_MAX:
        return new (home) ViewSelMaxTbl<MeritDegreeSize>(home,svb);
      case SetVarBranch::SEL_AFC_SIZE_MIN:
        return new (home) ViewSelMinTbl<MeritAFCSize>(home,svb);
      case SetVarBranch::SEL_AFC_SIZE_MAX:
        return new (home) ViewSelMaxTbl<MeritAFCSize>(home,svb);
      case SetVarBranch::SEL_ACTIVITY_SIZE_MIN:
        return new (home) ViewSelMinTbl<MeritActivitySize>(home,svb);
      case SetVarBranch::SEL_ACTIVITY_SIZE_MAX:
        return new (home) ViewSelMaxTbl<MeritActivitySize>(home,svb);
      default:
        throw UnknownBranching("Set::branch");
      }
    } else {
      switch (svb.select()) {
      case SetVarBranch::SEL_MERIT_MIN:
        return new (home) ViewSelMin<MeritFunction<SetView> >(home,svb);
      case SetVarBranch::SEL_MERIT_MAX:
        return new (home) ViewSelMax<MeritFunction<SetView> >(home,svb);
      case SetVarBranch::SEL_MIN_MIN:
        return new (home) ViewSelMin<MeritMin>(home,svb);
      case SetVarBranch::SEL_MIN_MAX:
        return new (home) ViewSelMax<MeritMin>(home,svb);
      case SetVarBranch::SEL_MAX_MIN:
        return new (home) ViewSelMin<MeritMax>(home,svb);
      case SetVarBranch::SEL_MAX_MAX:
        return new (home) ViewSelMax<MeritMax>(home,svb);
      case SetVarBranch::SEL_SIZE_MIN:
        return new (home) ViewSelMin<MeritSize>(home,svb);
      case SetVarBranch::SEL_SIZE_MAX:
        return new (home) ViewSelMax<MeritSize>(home,svb);
      case SetVarBranch::SEL_DEGREE_MIN:
        return new (home) ViewSelMin<MeritDegree<SetView> >(home,svb);
      case SetVarBranch::SEL_DEGREE_MAX:
        return new (home) ViewSelMax<MeritDegree<SetView> >(home,svb);
      case SetVarBranch::SEL_AFC_MIN:
        return new (home) ViewSelMin<MeritAFC<SetView> >(home,svb);
      case SetVarBranch::SEL_AFC_MAX:
        return new (home) ViewSelMax<MeritAFC<SetView> >(home,svb);
      case SetVarBranch::SEL_ACTIVITY_MIN:
        return new (home) ViewSelMin<MeritActivity<SetView> >(home,svb);
      case SetVarBranch::SEL_ACTIVITY_MAX:
        return new (home) ViewSelMax<MeritActivity<SetView> >(home,svb);
      case SetVarBranch::SEL_DEGREE_SIZE_MIN:
        return new (home) ViewSelMin<MeritDegreeSize>(home,svb);
      case SetVarBranch::SEL_DEGREE_SIZE_MAX:
        return new (home) ViewSelMax<MeritDegreeSize>(home,svb);
      case SetVarBranch::SEL_AFC_SIZE_MIN:
        return new (home) ViewSelMin<MeritAFCSize>(home,svb);
      case SetVarBranch::SEL_AFC_SIZE_MAX:
        return new (home) ViewSelMax<MeritAFCSize>(home,svb);
      case SetVarBranch::SEL_ACTIVITY_SIZE_MIN:
        return new (home) ViewSelMin<MeritActivitySize>(home,svb);
      case SetVarBranch::SEL_ACTIVITY_SIZE_MAX:
        return new (home) ViewSelMax<MeritActivitySize>(home,svb);
      default:
        throw UnknownBranching("Set::branch");
      }
    }
    GECODE_NEVER;
    return NULL;
  }

}}}


// STATISTICS: set-branch

