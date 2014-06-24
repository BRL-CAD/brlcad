/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Joseph Scott <joseph.scott@it.uu.se>
 *
 *  Copyright:
 *     Christian Schulte, 2009
 *     Guido Tack, 2010
 *     Joseph Scott, 2011
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

namespace Gecode { namespace Int { namespace Cumulative {

  /// Sort by capacity
  template<class TaskView, bool inc>
  class StoCap {
  public:
    /// Sort order
    bool operator ()(const TaskView& t1, const TaskView& t2) const {
      return inc ? (t1.c() < t2.c()) : (t2.c() < t1.c());
    }
  };

  /// Sort by prec array
  class PrecOrder {
  public:
    int* prec;
    /// Constructor
    PrecOrder(int* prec0) : prec(prec0) {}
    /// Sort order
    bool operator ()(int i, int j) const {
      return prec[i] > prec[j];
    }
  };

  template<class TaskView>
  forceinline ExecStatus
  edgefinding(Space& home, int c, TaskViewArray<TaskView>& t) {
    sort<TaskView,STO_LCT,false>(t);

    Region r(home);

    ///////////////////////
    // Detection

    int* prec = r.alloc<int>(t.size());
    for (int i=t.size(); i--; )
      prec[i] = t[i].ect();

    OmegaLambdaTree<TaskView> ol(r,c,t);

    for (int j=0; j<t.size(); j++) {
      while (!ol.lempty() && 
             (ol.lenv() > static_cast<long long int>(c)*t[j].lct())) {
        int i = ol.responsible();
        prec[i] = std::max(prec[i], t[j].lct());
        ol.lremove(i);
      }
      ol.shift(j);
    }      

    ///////////////////////
    // Propagation
    
    // Compute array of unique capacities and a mapping
    // from the task array to the corresponding entry in
    // the capacity array
    
    int* cap = r.alloc<int>(t.size());
    for (int i=t.size(); i--;)
      cap[i] = i;
    SortMap<TaskView,StoCap,true> o(t);
    Support::quicksort(cap, t.size(), o);

    int* capacities = r.alloc<int>(t.size());
    int* capInv = r.alloc<int>(t.size());
    for (int i=t.size(); i--;) {
      capacities[cap[i]] = t[i].c();
      capInv[cap[i]] = i;
    }
    
    int n_c = 0;
    for (int i=0, cur_c=INT_MIN; i<t.size(); i++) {
      if (capacities[i] != cur_c)
        capacities[n_c++] = cur_c = capacities[i];
      cap[capInv[i]] = n_c-1;
    }
    r.free<int>(capInv, t.size());

    // Compute update values for each capacity and LCut

    int* update = r.alloc<int>(t.size()*n_c);
    for (int i=t.size()*n_c; i--;)
      update[i] = -Int::Limits::infinity;

    ExtOmegaTree<TaskView> eo(r,c,ol);
    for (int i=0; i<n_c; i++) {
      eo.init(capacities[i]);
      int u = -Int::Limits::infinity;
      for (int j=t.size(); j--;) {
        long long int lctj = 
          static_cast<long long int>(c-capacities[i])*t[j].lct();
        long long int eml = plus(eo.env(j), -lctj);
        long long int diff_l;
        if (eml == -Limits::llinfinity)
          diff_l = -Limits::llinfinity;
        else
          diff_l = ceil_div_xx(eml, 
                               static_cast<long long int>(capacities[i]));
        int diff = (diff_l <= -Limits::infinity) ? 
          -Limits::infinity : static_cast<int>(diff_l);
        u = std::max(u,diff);
        update[i*t.size()+j] = u;
      }
    }

    // Update est by iterating in parallel over the prec array
    // and the task array, both sorted by lct

    int* precMap = r.alloc<int>(t.size());
    for (int i=t.size(); i--;)
      precMap[i] = i;
    PrecOrder po(prec);
    Support::quicksort(precMap, t.size(), po);
    
    int curJ = 0;
    for (int i=0; i<t.size(); i++) {
      // discard any curJ with lct > prec[i]:
      while (curJ < t.size() && t[curJ].lct() > prec[precMap[i]])
        curJ++;
      if (curJ >= t.size())
        break;
      // if lct[curJ] == prec[i], then LCut(T,j) <= i, so update est[i]
      int locJ = curJ;
      do {
        if (t[locJ].lct() != t[precMap[i]].lct()) {
          GECODE_ME_CHECK(t[precMap[i]].est(home,update[cap[precMap[i]]*t.size()+locJ]));
          break;
        }
      } while (t[locJ].lct() == prec[precMap[i]] && locJ++ < t.size() - 1);
    }

    return ES_OK;
  }

  template<class Task>
  ExecStatus
  edgefinding(Space& home, int c, TaskArray<Task>& t) {
    TaskViewArray<typename TaskTraits<Task>::TaskViewFwd> f(t);
    GECODE_ES_CHECK(edgefinding(home,c,f));
    TaskViewArray<typename TaskTraits<Task>::TaskViewBwd> b(t);
    GECODE_ES_CHECK(edgefinding(home,c,b));
    return ES_OK;
  }
    
}}}

// STATISTICS: int-prop
