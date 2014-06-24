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

  /// Sort by earliest start times
  template<class TaskView, bool inc>
  class StoEst {
  public:
    /// Sort order
    bool operator ()(const TaskView& t1, const TaskView& t2) const;
  };

  /// Sort by earliest completion times
  template<class TaskView, bool inc>
  class StoEct {
  public:
    /// Sort order
    bool operator ()(const TaskView& t1, const TaskView& t2) const;
  };

  /// Sort by latest start times
  template<class TaskView, bool inc>
  class StoLst {
  public:
    /// Sort order
    bool operator ()(const TaskView& t1, const TaskView& t2) const;
  };

  /// Sort by latest completion times
  template<class TaskView, bool inc>
  class StoLct {
  public:
    /// Sort order
    bool operator ()(const TaskView& t1, const TaskView& t2) const;
  };

  /// Sorting maps rather than tasks
  template<class TaskView, template<class,bool> class STO, bool inc>
  class SortMap {
  private:
    /// The tasks
    const TaskViewArray<TaskView>& tasks;
    /// The sorting order for tasks
    STO<TaskView,inc> sto;
  public:
    /// Initialize
    SortMap(const TaskViewArray<TaskView>& t);
    /// Sort order
    bool operator ()(int& i, int& j) const;
  };

  template<class TaskView, bool inc>
  forceinline bool
  StoEst<TaskView,inc>::operator ()
    (const TaskView& t1, const TaskView& t2) const {
    return inc ?
      (t1.est() < t2.est() || (t1.est()==t2.est() && t1.lct() < t2.lct()))
    : (t2.est() < t1.est() || (t2.est()==t1.est() && t2.lct() < t1.lct()));
  }
  
  template<class TaskView, bool inc>
  forceinline bool
  StoEct<TaskView,inc>::operator ()
    (const TaskView& t1, const TaskView& t2) const {
    return inc ?
      (t1.ect() < t2.ect() || (t1.ect()==t2.ect() && t1.lst() < t2.lst()))
    : (t2.ect() < t1.ect() || (t2.ect()==t1.ect() && t2.lst() < t1.lst()));
  }
  
  template<class TaskView, bool inc>
  forceinline bool
  StoLst<TaskView,inc>::operator ()
    (const TaskView& t1, const TaskView& t2) const {
    return inc ?
      (t1.lst() < t2.lst() || (t1.lst()==t2.lst() && t1.ect() < t2.ect()))
    : (t2.lst() < t1.lst() || (t2.lst()==t1.lst() && t2.ect() < t1.ect()));
  }
  
  template<class TaskView, bool inc>
  forceinline bool
  StoLct<TaskView,inc>::operator ()
    (const TaskView& t1, const TaskView& t2) const {
    return inc ?
      (t1.lct() < t2.lct() || (t1.lct()==t2.lct() && t1.est() < t2.est()))
    : (t2.lct() < t1.lct() || (t2.lct()==t1.lct() && t2.est() < t1.est()));
  }

  template<class TaskView, template<class,bool> class STO, bool inc>
  forceinline
  SortMap<TaskView,STO,inc>::SortMap(const TaskViewArray<TaskView>& t) 
    : tasks(t) {}
  template<class TaskView, template<class,bool> class STO, bool inc>
  forceinline bool 
  SortMap<TaskView,STO,inc>::operator ()(int& i, int& j) const {
    return sto(tasks[i],tasks[j]);
  }

  template<class TaskView, SortTaskOrder sto, bool inc>
  forceinline void 
  sort(TaskViewArray<TaskView>& t) {
    switch (sto) {
    case STO_EST:
      {
        StoEst<TaskView,inc> o; Support::quicksort(&t[0], t.size(), o);
      }
      break;
    case STO_ECT:
      {
        StoEct<TaskView,inc> o; Support::quicksort(&t[0], t.size(), o);
      }
      break;
    case STO_LST:
      {
        StoLst<TaskView,inc> o; Support::quicksort(&t[0], t.size(), o);
      }
      break;
    case STO_LCT:
      {
        StoLct<TaskView,inc> o; Support::quicksort(&t[0], t.size(), o);
      }
      break;
    default:
      GECODE_NEVER;
    }
  }

  template<class TaskView, SortTaskOrder sto, bool inc>
  forceinline void
  sort(int* map, const TaskViewArray<TaskView>& t) {
    for (int i=t.size(); i--; )
      map[i]=i;
    switch (sto) {
    case STO_EST:
      {
        SortMap<TaskView,StoEst,inc> o(t); 
        Support::quicksort(map, t.size(), o);
      }
      break;
    case STO_ECT:
      {
        SortMap<TaskView,StoEct,inc> o(t); 
        Support::quicksort(map, t.size(), o);
      }
      break;
    case STO_LST:
      {
        SortMap<TaskView,StoLst,inc> o(t); 
        Support::quicksort(map, t.size(), o);
      }
      break;
    case STO_LCT:
      {
        SortMap<TaskView,StoLct,inc> o(t); 
        Support::quicksort(map, t.size(), o);
      }
      break;
    default:
      GECODE_NEVER;
    }
  }

  template<class TaskView, SortTaskOrder sto, bool inc>
  forceinline void
  sort(int* map, int n, const TaskViewArray<TaskView>& t) {
    switch (sto) {
    case STO_EST:
      {
        SortMap<TaskView,StoEst,inc> o(t); 
        Support::quicksort(map, n, o);
      }
      break;
    case STO_ECT:
      {
        SortMap<TaskView,StoEct,inc> o(t); 
        Support::quicksort(map, n, o);
      }
      break;
    case STO_LST:
      {
        SortMap<TaskView,StoLst,inc> o(t); 
        Support::quicksort(map, n, o);
      }
      break;
    case STO_LCT:
      {
        SortMap<TaskView,StoLct,inc> o(t); 
        Support::quicksort(map, n, o);
      }
      break;
    default:
      GECODE_NEVER;
    }
  }

}}

// STATISTICS: int-other
