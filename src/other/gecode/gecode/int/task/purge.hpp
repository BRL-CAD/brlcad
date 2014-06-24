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

namespace Gecode { namespace Int {

  template<class OptTask, PropCond pc>
  ExecStatus
  purge(Space& home, Propagator& p, TaskArray<OptTask>& t) {
    int n=t.size();
    for (int i=n; i--; )
      if (t[i].excluded()) {
        t[i].cancel(home,p,pc); t[i]=t[--n];
      }
    t.size(n);

    return (t.size() < 2) ? home.ES_SUBSUMED(p) : ES_OK;
  }

  template<class OptTask, PropCond pc, class Cap>
  ExecStatus
  purge(Space& home, Propagator& p, TaskArray<OptTask>& t, Cap c) {
    int n=t.size();
    for (int i=n; i--; )
      if (t[i].excluded()) {
        t[i].cancel(home,p,pc); t[i]=t[--n];
      }
    t.size(n);
    if (t.size() == 1) {
      if (t[0].mandatory())
        GECODE_ME_CHECK(c.gq(home, t[0].c()));
      else if (c.min() < t[0].c())
        return ES_OK;
    }

    return (t.size() < 2) ? home.ES_SUBSUMED(p) : ES_OK;
  }
  
}}

// STATISTICS: int-prop
