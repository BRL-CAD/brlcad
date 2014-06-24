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
  
  template<class OptTask>
  forceinline
  OptProp<OptTask>::OptProp(Home home, TaskArray<OptTask>& t)
    : TaskProp<OptTask,Int::PC_INT_BND>(home,t) {}

  template<class OptTask>
  forceinline
  OptProp<OptTask>::OptProp(Space& home, bool shared, OptProp<OptTask>& p) 
    : TaskProp<OptTask,Int::PC_INT_BND>(home,shared,p) {}

  template<class OptTask>
  forceinline ExecStatus 
  OptProp<OptTask>::post(Home home, TaskArray<OptTask>& t) {
    int m=0, o=0;
    for (int i=t.size(); i--; ) {
      if (t[i].mandatory())
        m++;
      else if (t[i].optional())
        o++;
    }
    if (m == t.size()) {
      TaskArray<typename TaskTraits<OptTask>::ManTask> mt(home,m);
      for (int i=m; i--; )
        mt[i].init(t[i]);
      return ManProp<typename TaskTraits<OptTask>::ManTask>::post(home,mt);
    }
    if (o+m > 1)
      (void) new (home) OptProp<OptTask>(home,t);
    return ES_OK;
  }

  template<class OptTask>
  Actor* 
  OptProp<OptTask>::copy(Space& home, bool share) {
    return new (home) OptProp<OptTask>(home,share,*this);
  }

  template<class OptTask>
  ExecStatus 
  OptProp<OptTask>::propagate(Space& home, const ModEventDelta& med) {
    // Did one of the Boolean views change?
    if (Int::BoolView::me(med) == Int::ME_BOOL_VAL)
      GECODE_ES_CHECK((purge<OptTask,Int::PC_INT_BND>(home,*this,t)));

    GECODE_ES_CHECK(overload(home,*this,t));

    GECODE_ES_CHECK(detectable(home,*this,t));
    GECODE_ES_CHECK(notfirstnotlast(home,*this,t));

    // Partition into mandatory and optional activities
    int n = t.size();
    int i=0, j=n-1;
    while (true) {
      while ((i < n) && t[i].mandatory()) i++;
      while ((j >= 0) && !t[j].mandatory()) j--;
      if (i >= j) break;
      std::swap(t[i],t[j]);
    }

    if (i > 1) {
      // Truncate array to only contain mandatory tasks
      t.size(i);
      GECODE_ES_CHECK(edgefinding(home,t));
      // Restore to also include optional tasks
      t.size(n);
    }

    GECODE_ES_CHECK(subsumed(home,*this,t));

    return ES_NOFIX;
  }

}}}

// STATISTICS: int-prop
