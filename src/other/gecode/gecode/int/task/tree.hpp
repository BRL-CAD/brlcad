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

namespace Gecode { namespace Int {

  forceinline int 
  plus(int x, int y) {
    assert(y != -Int::Limits::infinity);
    return (x == -Int::Limits::infinity) ? x : x+y;
  }

  forceinline long long int 
  plus(long long int x, long long int y) {
    assert(y != -Int::Limits::llinfinity);
    return (x == -Int::Limits::llinfinity) ? x : x+y;
  }

  template<class TaskView, class Node>
  forceinline int 
  TaskTree<TaskView,Node>::n_inner(void) const {
    return tasks.size()-1;
  }
  template<class TaskView, class Node>
  forceinline int 
  TaskTree<TaskView,Node>::n_nodes(void) const {
    return 2*tasks.size() - 1;
  }

  template<class TaskView, class Node>
  forceinline bool 
  TaskTree<TaskView,Node>::n_root(int i) {
    return i == 0;
  }
  template<class TaskView, class Node>
  forceinline bool 
  TaskTree<TaskView,Node>::n_leaf(int i) const {
    return i >= n_inner();
  }
  template<class TaskView, class Node>
  forceinline int 
  TaskTree<TaskView,Node>::n_left(int i) {
    return 2*(i+1) - 1;
  }
  template<class TaskView, class Node>
  forceinline bool
  TaskTree<TaskView,Node>::left(int i) {
    assert(!n_root(i));
    // A left node has an odd number
    return (i & 1) != 0;
  }
  template<class TaskView, class Node>
  forceinline int
  TaskTree<TaskView,Node>::n_right(int i) {
    return 2*(i+1);
  }
  template<class TaskView, class Node>
  forceinline bool
  TaskTree<TaskView,Node>::right(int i) {
    assert(!n_root(i));
    // A left node has an even number
    return (i & 1) == 0;
  }
  template<class TaskView, class Node>
  forceinline int
  TaskTree<TaskView,Node>::n_parent(int i) {
    return (i+1)/2 - 1;
  }

  template<class TaskView, class Node>
  forceinline Node&
  TaskTree<TaskView,Node>::leaf(int i) {
    return node[_leaf[i]];
  }

  template<class TaskView, class Node>
  forceinline const Node&
  TaskTree<TaskView,Node>::root(void) const {
    return node[0];
  }

  template<class TaskView, class Node>
  forceinline void
  TaskTree<TaskView,Node>::init(void) {
    for (int i=n_inner(); i--; )
      node[i].init(node[n_left(i)],node[n_right(i)]);
  }

  template<class TaskView, class Node>
  forceinline void
  TaskTree<TaskView,Node>::update(void) {
    for (int i=n_inner(); i--; )
      node[i].update(node[n_left(i)],node[n_right(i)]);
  }

  template<class TaskView, class Node>
  forceinline void
  TaskTree<TaskView,Node>::update(int i, bool l) {
    if (l)
      i = _leaf[i];
    assert(!n_root(i));
    do {
      i = n_parent(i);
      node[i].update(node[n_left(i)],node[n_right(i)]);
    } while (!n_root(i));
  }

  template<class TaskView, class Node>
  forceinline
  TaskTree<TaskView,Node>::TaskTree(Region& r, 
                                    const TaskViewArray<TaskView>& t)
    : tasks(t), 
      node(r.alloc<Node>(n_nodes())),
      _leaf(r.alloc<int>(tasks.size())) {
    // Compute a sorting map to order by non decreasing est
    int* map = r.alloc<int>(tasks.size());
    sort<TaskView,STO_EST,true>(map, tasks);
    // Compute inverse of sorting map
    for (int i=tasks.size(); i--; )
      _leaf[map[i]] = i;
    r.free<int>(map,tasks.size());
    // Compute index of first leaf in tree: the next larger power of two
    int fst = 1;
    while (fst < tasks.size())
      fst <<= 1;
    fst--;
    // Remap task indices to leaf indices
    for (int i=tasks.size(); i--; )
      if (_leaf[i] + fst >= n_nodes())
        _leaf[i] += fst - tasks.size();
      else
        _leaf[i] += fst;
  }

  template<class TaskView, class Node> template<class Node2>
  forceinline
  TaskTree<TaskView,Node>::TaskTree(Region& r, 
                                    const TaskTree<TaskView,Node2>& t)
    : tasks(t.tasks), 
      node(r.alloc<Node>(n_nodes())),
      _leaf(r.alloc<int>(tasks.size())) {
    for (int i=tasks.size(); i--; )
      _leaf[i] = t._leaf[i];
  }

}}

// STATISTICS: int-prop
