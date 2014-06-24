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

namespace Gecode { namespace Int { namespace Unary {

  // Overload checking for mandatory tasks
  template<class ManTask>
  ExecStatus
  overload(Space& home, TaskArray<ManTask>& t) {
    TaskViewArray<typename TaskTraits<ManTask>::TaskViewFwd> f(t);
    sort<typename TaskTraits<ManTask>::TaskViewFwd,STO_LCT,true>(f);

    Region r(home);
    OmegaTree<typename TaskTraits<ManTask>::TaskViewFwd> o(r,f);

    for (int i=0; i<f.size(); i++) {
      o.insert(i);
      if (o.ect() > f[i].lct())
        return ES_FAILED;
    }
    return ES_OK;
  }
  
  // Overload checking for optional tasks
  template<class OptTask>
  ExecStatus
  overload(Space& home, Propagator& p, TaskArray<OptTask>& t) {
    TaskViewArray<typename TaskTraits<OptTask>::TaskViewFwd> f(t);
    sort<typename TaskTraits<OptTask>::TaskViewFwd,STO_LCT,true>(f);

    Region r(home);
    OmegaLambdaTree<typename TaskTraits<OptTask>::TaskViewFwd> ol(r,f,false);

    bool to_purge = false;

    for (int i=0; i<f.size(); i++) {
      if (f[i].optional()) {
        ol.linsert(i);
      } else if (f[i].mandatory()) {
        ol.oinsert(i);
        if (ol.ect() > f[i].lct())
          return ES_FAILED;
      }
      while (!ol.lempty() && (ol.lect() > f[i].lct())) {
        int j = ol.responsible();
        GECODE_ME_CHECK(f[j].excluded(home));
        ol.lremove(j);
        to_purge = true;
      }
    }

    if (to_purge)
      GECODE_ES_CHECK((purge<OptTask,Int::PC_INT_BND>(home,p,t)));
    return ES_OK;
  }
  
}}}

// STATISTICS: int-prop
