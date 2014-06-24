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

  /*
   * Task array
   */

  template<class Task>
  forceinline
  TaskArray<Task>::TaskArray(void) 
    : n(0), t(NULL) {}
  template<class Task>
  forceinline
  TaskArray<Task>::TaskArray(Space& home, int n0)
    : n(n0), t(home.alloc<Task>(n)) {
    assert(n > 0);
  }
  template<class Task>
  forceinline
  TaskArray<Task>::TaskArray(const TaskArray<Task>& a)
    : n(a.n), t(a.t) {}
  template<class Task>
  forceinline const TaskArray<Task>& 
  TaskArray<Task>::operator =(const TaskArray<Task>& a) {
    n=a.n; t=a.t;
    return *this;
  }

  template<class Task>
  forceinline int 
  TaskArray<Task>::size(void) const {
    return n;
  }
  template<class Task>
  forceinline void
  TaskArray<Task>::size(int n0) {
    n = n0;
  }

  template<class Task>
  forceinline Task& 
  TaskArray<Task>::operator [](int i) {
    assert((i >= 0) && (i < n));
    return t[i];
  }
  template<class Task>
  forceinline const Task& 
  TaskArray<Task>::operator [](int i) const {
    assert((i >= 0) && (i < n));
    return t[i];
  }

  template<class Task>
  inline void 
  TaskArray<Task>::subscribe(Space& home, Propagator& p, PropCond pc) {
    for (int i=n; i--; )
      t[i].subscribe(home,p,pc);
  }

  template<class Task>
  inline void 
  TaskArray<Task>::cancel(Space& home, Propagator& p, PropCond pc) {
    for (int i=n; i--; )
      t[i].cancel(home,p,pc);
  }

  template<class Task>
  forceinline void 
  TaskArray<Task>::update(Space& home, bool share, TaskArray& a) {
    n=a.n; t=home.alloc<Task>(n);
    for (int i=n; i--; )
      t[i].update(home,share,a.t[i]);
  }


  template<class Char, class Traits, class Task>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, 
              const TaskArray<Task>& t) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << '{';
    if (t.size() > 0) {
      s << t[0];
      for (int i=1; i<t.size(); i++)
        s << ", " << t[i];
    }
    s << '}';
    return os << s.str();
  }


  /*
   * Task view array
   */
  template<class TaskView>
  forceinline
  TaskViewArray<TaskView>::TaskViewArray(TaskArray<Task>& t0)
    : t(t0) {}

  template<class TaskView>
  forceinline int 
  TaskViewArray<TaskView>::size(void) const {
    return t.size();
  }

  template<class TaskView>
  forceinline void
  TaskViewArray<TaskView>::size(int n) {
    t.size(n);
  }

  template<class TaskView>
  forceinline TaskView&
  TaskViewArray<TaskView>::operator [](int i) {
    return static_cast<TaskView&>(t[i]);
  }
  template<class TaskView>
  forceinline const TaskView&
  TaskViewArray<TaskView>::operator [](int i) const {
    return static_cast<const TaskView&>(t[i]);
  }

  template<class Char, class Traits, class TaskView>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, 
              const TaskViewArray<TaskView>& t) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << '{';
    if (t.size() > 0) {
      s << t[0];
      for (int i=1; i<t.size(); i++)
        s << ", " << t[i];
    }
    s << '}';
    return os << s.str();
  }


}}

// STATISTICS: int-other
