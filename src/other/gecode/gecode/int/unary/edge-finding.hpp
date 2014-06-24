/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
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

namespace Gecode { namespace Int { namespace Unary {

  template<class TaskView>
  forceinline ExecStatus
  edgefinding(Space& home, TaskViewArray<TaskView>& t) {
    Region r(home);

    OmegaLambdaTree<TaskView> ol(r,t);
    TaskViewIter<TaskView,STO_LCT,false> q(r,t);

    int j = q.task();
    while (q.left() > 1) {
      if (ol.ect() > t[j].lct())
        return ES_FAILED;
      ol.shift(j);
      ++q;
      j = q.task();
      while (!ol.lempty() && (ol.lect() > t[j].lct())) {
        int i = ol.responsible();
        GECODE_ME_CHECK(t[i].est(home,ol.ect()));
        ol.lremove(i);
      }
    }

    return ES_OK;
  }

  template<class Task>
  ExecStatus
  edgefinding(Space& home, TaskArray<Task>& t) {
    TaskViewArray<typename TaskTraits<Task>::TaskViewFwd> f(t);
    GECODE_ES_CHECK(edgefinding(home,f));
    TaskViewArray<typename TaskTraits<Task>::TaskViewBwd> b(t);
    return edgefinding(home,b);
  }
    
}}}

// STATISTICS: int-prop
