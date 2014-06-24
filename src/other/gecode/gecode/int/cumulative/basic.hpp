/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2010
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

namespace Gecode { namespace Int { namespace Cumulative {

  /// Event for task
  class Event {
  public:
    /// Event type for task with order in which they are processed
    enum Type {
      LRT = 0, ///< Latest required time of task
      LCT = 1, ///< Latest completion time of task
      EST = 2, ///< Earliest start time of task
      ZRO = 3, ///< Zero-length task start time
      ERT = 4, ///< Earliest required time of task
      END = 5  ///< End marker
    };
    Type e; ///< Type of event
    int t;  ///< Time of event
    int i;  ///< Number of task
    /// Initialize event
    void init(Type e, int t, int i);
    /// Order among events
    bool operator <(const Event& e) const;
  };

  /// Sort order for tasks by decreasing capacity
  template<class Task>
  class TaskByDecCap {
  public:
    /// Sort order
    bool operator ()(const Task& t1, const Task& t2) const;
  };

  forceinline void
  Event::init(Event::Type e0, int t0, int i0) {
    e=e0; t=t0; i=i0;
  }

  forceinline bool
  Event::operator <(const Event& e) const {
    if (this->t == e.t)
      return this->e < e.e;
    return this->t < e.t;
  }

  template<class Task>
  forceinline bool
  TaskByDecCap<Task>::operator ()(const Task& t1, const Task& t2) const {
    return t1.c() > t2.c();
  }


  // Basic propagation
  template<class Task, class Cap>
  ExecStatus
  basic(Space& home, bool& subsumed, Cap c, TaskArray<Task>& t) {
    subsumed = false;
    int ccur = c.max();
    int cmax = ccur;
    int cmin = ccur;
    // Sort tasks by decreasing capacity
    TaskByDecCap<Task> tbdc;
    Support::quicksort(&t[0], t.size(), tbdc);

    Region r(home);

    Event* e = r.alloc<Event>(4*t.size()+1);

    // Initialize events
    bool assigned=true;
    {
      bool required=false;
      int n=0;
      for (int i=t.size(); i--; ) 
        if (t[i].assigned()) {
          // Only add required part
          if (t[i].pmin() > 0) {
            required = true;
            e[n++].init(Event::ERT,t[i].lst(),i);
            e[n++].init(Event::LRT,t[i].ect(),i);
          } else if (t[i].pmax() == 0) {
            required = true;
            e[n++].init(Event::ZRO,t[i].lst(),i);
          }
        } else {
          assigned = false;
          e[n++].init(Event::EST,t[i].est(),i);
          e[n++].init(Event::LCT,t[i].lct(),i);
          // Check whether task has required part
          if (t[i].lst() < t[i].ect()) {
            required = true;
            e[n++].init(Event::ERT,t[i].lst(),i);
            e[n++].init(Event::LRT,t[i].ect(),i);
          }
        }
      
      // Check whether no task has a required part
      if (!required) {
        subsumed = assigned;
        return ES_FIX;
      }
      
      // Write end marker
      e[n++].init(Event::END,Int::Limits::infinity,-1);
      
      // Sort events
      Support::quicksort(e, n);
    }

    // Set of current but not required tasks
    Support::BitSet<Region> tasks(r,static_cast<unsigned int>(t.size()));

    // Process events, use ccur as the capacity that is still free
    while (e->e != Event::END) {
      // Current time
      int time = e->t;

      // Process events for completion of required part
      for ( ; (e->t == time) && (e->e == Event::LRT); e++) 
        if (t[e->i].mandatory()) {
          tasks.set(static_cast<unsigned int>(e->i)); ccur += t[e->i].c();
        }
      // Process events for completion of task
      for ( ; (e->t == time) && (e->e == Event::LCT); e++)
        tasks.clear(static_cast<unsigned int>(e->i));
      // Process events for start of task
      for ( ; (e->t == time) && (e->e == Event::EST); e++)
        tasks.set(static_cast<unsigned int>(e->i));
      // Process events for zero-length task
      for ( ; (e->t == time) && (e->e == Event::ZRO); e++) {
        ccur -= t[e->i].c();
        if (ccur < cmin) cmin=ccur;
        if (ccur < 0)
          return ES_FAILED;
        ccur += t[e->i].c();
      }

      // norun start time for 0-length tasks
      int zltime = time;
      // Process events for start of required part
      for ( ; (e->t == time) && (e->e == Event::ERT); e++) 
        if (t[e->i].mandatory()) {
          tasks.clear(static_cast<unsigned int>(e->i)); 
          ccur -= t[e->i].c();
          if (ccur < cmin) cmin=ccur;
          zltime = time+1;
          if (ccur < 0)
            return ES_FAILED;
        } else if (t[e->i].optional() && (t[e->i].c() > ccur)) {
          GECODE_ME_CHECK(t[e->i].excluded(home));
        }
      
      // Exploit that tasks are sorted according to capacity
      for (Iter::Values::BitSet<Support::BitSet<Region> > j(tasks); 
           j() && (t[j.val()].c() > ccur); ++j) 
        // Task j cannot run from time to next time - 1
        if (t[j.val()].mandatory()) {
          if (t[j.val()].pmin() > 0) {
            GECODE_ME_CHECK(t[j.val()].norun(home, time, e->t - 1));
          } else {
            GECODE_ME_CHECK(t[j.val()].norun(home, zltime, e->t - 1));
          }
        }
    }

    GECODE_ME_CHECK(c.gq(home,cmax-cmin));

    subsumed = assigned;
    return ES_NOFIX;
  }

}}}

// STATISTICS: int-prop
