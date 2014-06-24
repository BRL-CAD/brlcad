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

  template<class ManTaskView>
  forceinline ExecStatus
  notlast(Space& home, TaskViewArray<ManTaskView>& t) {
    sort<ManTaskView,STO_LCT,true>(t);

    Region r(home);

    OmegaTree<ManTaskView> o(r,t);
    TaskViewIter<ManTaskView,STO_LST,true> q(r,t);
    int* lct = r.alloc<int>(t.size());

    for (int i=t.size(); i--; )
      lct[i] = t[i].lct();

    for (int i=0; i<t.size(); i++) {
      int j = -1;
      while (q() && (t[i].lct() > t[q.task()].lst())) {
        if ((j >= 0) && (o.ect() > t[q.task()].lst()))
          lct[q.task()] = std::min(lct[q.task()],t[j].lst());
        j = q.task();
        o.insert(j); ++q;
      }
      if ((j >= 0) && (o.ect(i) > t[i].lst()))
        lct[i] = std::min(lct[i],t[j].lst());
    }

    for (int i=t.size(); i--; )
      GECODE_ME_CHECK(t[i].lct(home,lct[i]));
      
    return ES_OK;
  }

  template<class ManTask>
  ExecStatus
  notfirstnotlast(Space& home, TaskArray<ManTask>& t) {
    TaskViewArray<typename TaskTraits<ManTask>::TaskViewFwd> f(t);
    GECODE_ES_CHECK(notlast(home,f));
    TaskViewArray<typename TaskTraits<ManTask>::TaskViewBwd> b(t);
    return notlast(home,b);
  }
  
  template<class OptTaskView>
  forceinline ExecStatus
  notlast(Space& home, Propagator& p, TaskViewArray<OptTaskView>& t) {
    sort<OptTaskView,STO_LCT,true>(t);

    Region r(home);

    OmegaTree<OptTaskView> o(r,t);
    ManTaskViewIter<OptTaskView,STO_LST,true> q(r,t);
    int* lct = r.alloc<int>(t.size());

    for (int i=t.size(); i--; )
      lct[i] = t[i].lct();

    for (int i=0; i<t.size(); i++) {
      int j = -1;
      while (q() && (t[i].lct() > t[q.task()].lst())) {
        if ((j >= 0) && (o.ect() > t[q.task()].lst()))
          lct[q.task()] = std::min(lct[q.task()],t[j].lst());
        j = q.task();
        o.insert(j); ++q;
      }
      if ((j >= 0) && (o.ect(i) > t[i].lst()))
        lct[i] = std::min(lct[i],t[j].lst());
    }

    int n = t.size();
    for (int i=n; i--; )
      if (t[i].mandatory()) {
        GECODE_ME_CHECK(t[i].lct(home,lct[i]));
      } else if (lct[i] < t[i].ect()) {
        //        GECODE_ME_CHECK(t[i].excluded(home));
        //        t[i].cancel(home,p); t[i]=t[--n];
      }
    t.size(n);

    return (t.size() < 2) ? home.ES_SUBSUMED(p) : ES_OK;
  }

  template<class OptTask>
  ExecStatus
  notfirstnotlast(Space& home, Propagator& p, TaskArray<OptTask>& t) {
    TaskViewArray<typename TaskTraits<OptTask>::TaskViewFwd> f(t);
    GECODE_ES_CHECK(notlast(home,p,f));
    TaskViewArray<typename TaskTraits<OptTask>::TaskViewBwd> b(t);
    return notlast(home,p,b);
  }
  
}}}

// STATISTICS: int-prop
