/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
 *     Guido Tack, 2010
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
#include <cmath>

namespace Gecode { namespace Int { namespace Cumulative {

  /*
   * Omega tree
   */

  forceinline void
  OmegaNode::init(const OmegaNode&, const OmegaNode&) {
    e = 0; env = -Limits::llinfinity;
  }

  forceinline void
  OmegaNode::update(const OmegaNode& l, const OmegaNode& r) {
    e = l.e + r.e; env = std::max(plus(l.env,r.e), r.env);
  }

  template<class TaskView>
  OmegaTree<TaskView>::OmegaTree(Region& r, int c0,
                                 const TaskViewArray<TaskView>& t)
    : TaskTree<TaskView,OmegaNode>(r,t), c(c0) {
    for (int i=tasks.size(); i--; ) {
      leaf(i).e = 0; leaf(i).env = -Limits::llinfinity;
    }
    init();
  }

  template<class TaskView>
  forceinline void 
  OmegaTree<TaskView>::insert(int i) {
    leaf(i).e = tasks[i].e(); 
    leaf(i).env = 
      static_cast<long long int>(c)*tasks[i].est()+tasks[i].e();
    update(i);
  }

  template<class TaskView>
  forceinline void
  OmegaTree<TaskView>::remove(int i) {
    leaf(i).e = 0; leaf(i).env = -Limits::llinfinity;
    update(i);
  }

  template<class TaskView>
  forceinline long long int
  OmegaTree<TaskView>::env(void) const {
    return root().env;
  }
  
  /*
   * Extended Omega tree
   */

  forceinline void
  ExtOmegaNode::init(const ExtOmegaNode& l, const ExtOmegaNode& r) {
    OmegaNode::init(l,r);
    cenv = -Limits::llinfinity;
  }

  forceinline void
  ExtOmegaNode::update(const ExtOmegaNode& l, const ExtOmegaNode& r) {
    OmegaNode::update(l,r);
    cenv = std::max(plus(l.cenv,r.e), r.cenv);
  }

  template<class TaskView> void
  ExtOmegaTree<TaskView>::init(int ci0) {
    ci = ci0;
    for (int i=tasks.size(); i--; ) {
      leaf(i).e = 0; 
      leaf(i).env = leaf(i).cenv = -Limits::llinfinity;
    }
    init();
  }

  template<class TaskView> template<class Node>
  ExtOmegaTree<TaskView>::ExtOmegaTree(Region& r, int c0,
                                       const TaskTree<TaskView,Node>& t)
    : TaskTree<TaskView,ExtOmegaNode>(r,t), c(c0) {}

  template<class TaskView>
  forceinline long long int
  ExtOmegaTree<TaskView>::env(int i) {
    // Enter task i
    leaf(i).e = tasks[i].e(); 
    leaf(i).env = 
      static_cast<long long int>(c)*tasks[i].est()+tasks[i].e();
    leaf(i).cenv = 
      static_cast<long long int>(c-ci)*tasks[i].est()+tasks[i].e();
    TaskTree<TaskView,ExtOmegaNode>::update(i);

    // Perform computation of node for task with minest
    int met = 0;
    {
      long long int e = 0;
      while (!n_leaf(met)) {
        if (plus(node[n_right(met)].cenv,e) > 
            static_cast<long long int>(c-ci) * tasks[i].lct()) {
          met = n_right(met);
        } else {
          e += node[n_right(met)].e; met = n_left(met);
        }
      }
    }

    /*
     * The following idea to compute the cut in one go is taken from:
     * Joseph Scott, Filtering Algorithms for Discrete Resources, 
     * Master Thesis, Uppsala University, 2010 (in preparation).
     */

    // Now perform split from leaf met upwards
    long long int a_e = node[met].e;
    long long int a_env = node[met].env;
    long long int b_e = 0;

    while (!n_root(met)) {
      if (left(met)) {
        b_e += node[n_right(n_parent(met))].e;
      } else {
        a_env = std::max(a_env, plus(node[n_left(n_parent(met))].env,a_e));
        a_e += node[n_left(n_parent(met))].e;
      }
      met = n_parent(met);
    }

    return plus(a_env,b_e);
  }

  

  /*
   * Omega lambda tree
   */

  forceinline void
  OmegaLambdaNode::init(const OmegaLambdaNode& l, const OmegaLambdaNode& r) {
    OmegaNode::init(l,r);
    le = 0; lenv = -Limits::llinfinity;
    resLe = undef; resLenv = undef;
  }

  forceinline void
  OmegaLambdaNode::update(const OmegaLambdaNode& l, const OmegaLambdaNode& r) {
    OmegaNode::update(l,r);
    if (l.le + r.e > l.e + r.le) {
      le = l.le + r.e;
      resLe = l.resLe;
    } else {
      le = l.e + r.le;
      resLe = r.resLe;
    }
    if ((r.lenv >= plus(l.env,r.le)) && (r.lenv >= plus(l.lenv,r.e))) {
      lenv = r.lenv; resLenv = r.resLenv;
    } else if (plus(l.env,r.le) >= plus(l.lenv,r.e)) {
      assert(plus(l.env,r.le) > r.lenv);
      lenv = plus(l.env,r.le); resLenv = r.resLe;
    } else {
      assert((plus(l.lenv,r.e) > r.lenv) &&
             (plus(l.lenv,r.e) > plus(l.env,r.le)));
      lenv = plus(l.lenv,r.e); resLenv = l.resLenv;
    }
  }


  template<class TaskView>
  OmegaLambdaTree<TaskView>::OmegaLambdaTree(Region& r, int c0,
                                             const TaskViewArray<TaskView>& t)
    : TaskTree<TaskView,OmegaLambdaNode>(r,t), c(c0) {
    // Enter all tasks into tree (omega = all tasks, lambda = empty)
    for (int i=tasks.size(); i--; ) {
      leaf(i).e = tasks[i].e();
      leaf(i).le = 0;
      leaf(i).env = static_cast<long long int>(c)*tasks[i].est()+tasks[i].e();
      leaf(i).lenv = -Limits::llinfinity;
      leaf(i).resLe = OmegaLambdaNode::undef;
      leaf(i).resLenv = OmegaLambdaNode::undef;
    }
    update();
  }

  template<class TaskView>
  forceinline void 
  OmegaLambdaTree<TaskView>::shift(int i) {
    // i is in omega
    assert(leaf(i).env > -Limits::llinfinity);
    leaf(i).le = leaf(i).e;
    leaf(i).e = 0;
    leaf(i).lenv = leaf(i).env;
    leaf(i).env = -Limits::llinfinity;
    leaf(i).resLe = i;
    leaf(i).resLenv = i;
    update(i);
  }

  template<class TaskView>
  forceinline void
  OmegaLambdaTree<TaskView>::lremove(int i) {
    // i not in omega but in lambda
    assert(leaf(i).env == -Limits::llinfinity);
    assert(leaf(i).lenv > -Limits::llinfinity);
    leaf(i).le = 0; 
    leaf(i).lenv = -Limits::llinfinity;
    leaf(i).resLe = OmegaLambdaNode::undef;
    leaf(i).resLenv = OmegaLambdaNode::undef;
    update(i);
  }

  template<class TaskView>
  forceinline bool
  OmegaLambdaTree<TaskView>::lempty(void) const {
    return root().resLenv < 0;
  }
  
  template<class TaskView>
  forceinline int 
  OmegaLambdaTree<TaskView>::responsible(void) const {
    return root().resLenv;
  }
  
  template<class TaskView>
  forceinline long long int
  OmegaLambdaTree<TaskView>::env(void) const {
    return root().env;
  }
  
  template<class TaskView>
  forceinline long long int
  OmegaLambdaTree<TaskView>::lenv(void) const {
    return root().lenv;
  }
  
}}}

// STATISTICS: int-prop
