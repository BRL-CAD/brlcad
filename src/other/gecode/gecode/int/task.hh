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

#ifndef __GECODE_INT_TASK_HH__
#define __GECODE_INT_TASK_HH__

#include <gecode/int.hh>

namespace Gecode { namespace Int {

  /// Class to define an optional from a mandatory task
  template<class ManTask>
  class ManToOptTask : public ManTask {
  protected:
    /// Boolean view whether task is mandatory (= 1) or not
    Int::BoolView _m;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ManToOptTask(void);
    //@}

    /// \name Value access
    //@{
    /// Whether task is mandatory
    bool mandatory(void) const;
    /// Whether task is excluded
    bool excluded(void) const;
    /// Whether task can still be optional
    bool optional(void) const;
    //@}

    //@{
    /// Test whether task is assigned
    bool assigned(void) const;
    //@}

    /// \name Value update
    //@{
    /// Mark task as mandatory
    ModEvent mandatory(Space& home);
    /// Mark task as excluded
    ModEvent excluded(Space& home);
    //@}

    /// \name Cloning
    //@{
    /// Update this task to be a clone of task \a t
    void update(Space& home, bool share, ManToOptTask& t);
    //@}

    /// \name Dependencies
    //@{
    /// Subscribe propagator \a p to task
    void subscribe(Space& home, Propagator& p, PropCond pc);
    /// Cancel subscription of propagator \a p for task
    void cancel(Space& home, Propagator& p, PropCond pc);
    //@}
  };

}}

#include <gecode/int/task/man-to-opt.hpp>

namespace Gecode { namespace Int {

  /// Task mapper: turns a task view into its dual
  template<class TaskView>
  class FwdToBwd : public TaskView {
  public:
    /// \name Value access
    //@{
    /// Return earliest start time
    int est(void) const;
    /// Return earliest completion time
    int ect(void) const;
    /// Return latest start time
    int lst(void) const;
    /// Return latest completion time
    int lct(void) const;
    /// Return minimum processing time
    int pmin(void) const;
    /// Return maximum processing time
    int pmax(void) const;
    //@}

    /// \name Value update
    //@{
    /// Update earliest start time to \a n
    ModEvent est(Space& home, int n);
    /// Update earliest completion time to \a n
    ModEvent ect(Space& home, int n);
    /// Update latest start time to \a n
    ModEvent lst(Space& home, int n);
    /// Update latest completion time to \a n
    ModEvent lct(Space& home, int n);
    /// Update such that task cannot run from \a e to \a l
    ModEvent norun(Space& home, int e, int l);
    //@}
  };

}}

#include <gecode/int/task/fwd-to-bwd.hpp>

namespace Gecode { namespace Int {

  /**
   * \brief Traits class for mapping task views to tasks
   *
   * Each task view must specialize this traits class and add a \code
   * typedef \endcode for the task corresponding to this task view.
   */
  template<class TaskView>
  class TaskViewTraits {};

  /**
   * \brief Traits class for mapping tasks to task views
   *
   * Each task must specialize this traits class and add \code
   * typedef \endcode for the task views corresponding to this task.
   */
  template<class Task>
  class TaskTraits {};

}}

namespace Gecode { namespace Int {

  /// Task array
  template<class Task>
  class TaskArray {
  private:
    /// Number of tasks (size)
    int n;
    /// Tasks
    Task* t;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor (array of size 0)
    TaskArray(void);
    /// Allocate memory for \a n tasks (no initialization)
    TaskArray(Space& home, int n);
    /// Initialize from task array \a a (share elements)
    TaskArray(const TaskArray<Task>& a);
    /// Initialize from task array \a a (share elements)
    const TaskArray<Task>& operator =(const TaskArray<Task>& a);
    //@}

    /// \name Array size
    //@{
    /// Return size of array (number of elements)
    int size(void) const;
    /// Set size of array (number of elements) to \a n, must not be larger
    void size(int n);
    //@}

    /// \name Array elements
    //@{
    /// Return task at position \a i
    Task& operator [](int i);
    /// Return task at position \a i
    const Task& operator [](int i) const;
    //@}

    /// \name Dependencies
    //@{
    /// Subscribe propagator \a p to all tasks
    void subscribe(Space& home, Propagator& p, PropCond pc=Int::PC_INT_BND);
    /// Cancel subscription of propagator \a p for all tasks
    void cancel(Space& home, Propagator& p, PropCond pc=Int::PC_INT_BND);
    //@}

    /// \name Cloning
    //@{
    /// Update array to be a clone of array \a a
    void update(Space&, bool share, TaskArray& a);
    //@}

  private:
    static void* operator new(size_t);
    static void  operator delete(void*,size_t);
  };

  /**
   * \brief Print array elements enclosed in curly brackets
   * \relates TaskArray
   */
  template<class Char, class Traits, class Task>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, 
              const TaskArray<Task>& t);


  /// Task view array
  template<class TaskView>
  class TaskViewArray {
  protected:
    /// The underlying task type
    typedef typename TaskViewTraits<TaskView>::Task Task;
    /// Access to task array
    TaskArray<Task>& t;
  public:
    /// \name Constructors and initialization
    //@{
    /// Initialize from task array \a a
    TaskViewArray(TaskArray<Task>& t);
    //@}

    /// \name Array information
    //@{
    /// Return size of array (number of elements)
    int size(void) const;
    /// Set size of array (number of elements) to \a n, must not be larger
    void size(int n);
    //@}

    /// \name Array elements
    //@{
    /// Return task view at position \a i
    TaskView& operator [](int i);
    /// Return task view at position \a i
    const TaskView& operator [](int i) const;
    //@}
  private:
    static void* operator new(size_t);
    static void  operator delete(void*,size_t);
  };

  /**
   * \brief Print array elements enclosed in curly brackets
   * \relates TaskViewArray
   */
  template<class Char, class Traits, class TaskView>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, 
              const TaskViewArray<TaskView>& t);

}}

#include <gecode/int/task/array.hpp>

namespace Gecode { namespace Int {

  /// How to sort tasks
  enum SortTaskOrder {
    STO_EST, ///< Sort by earliest start times
    STO_ECT, ///< Sort by earliest completion times
    STO_LST, ///< Sort by latest start times
    STO_LCT  ///< Sort by latest completion times
  };

  /// Sort task view array \a t according to \a sto and \a inc (increasing or decreasing)
  template<class TaskView, SortTaskOrder sto, bool inc>
  void sort(TaskViewArray<TaskView>& t);

  /// Initialize and sort \a map for task view array \a t according to \a sto and \a inc (increasing or decreasing)
  template<class TaskView, SortTaskOrder sto, bool inc>
  void sort(int* map, const TaskViewArray<TaskView>& t);

  /// Sort \a map with size \a n for task view array \a t according to \a sto and \a inc (increasing or decreasing)
  template<class TaskView, SortTaskOrder sto, bool inc>
  void sort(int* map, int n, const TaskViewArray<TaskView>& t);

}}

#include <gecode/int/task/sort.hpp>

namespace Gecode { namespace Int {

  /// Allows to iterate over task views according to a specified order
  template<class TaskView, SortTaskOrder sto, bool inc>
  class TaskViewIter {
  protected:
    /// Map for iteration order
    int* map;
    /// Current position
    int i;
    /// Default constructor (no initialization)
    TaskViewIter(void);
  public:
    /// Initialize iterator
    TaskViewIter(Region& r, const TaskViewArray<TaskView>& t);
    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a task
    bool operator ()(void) const;
    /// How many tasks are left to be iterated
    int left(void) const;
    /// Move iterator to next task
    void operator ++(void);
    //@}

    /// \name %Task access
    //@{
    /// Return current task position
    int task(void) const;
    //@}
  };

  /// Allows to iterate over mandatory task views according to a specified order
  template<class OptTaskView, SortTaskOrder sto, bool inc>
  class ManTaskViewIter : public TaskViewIter<OptTaskView,sto,inc> {
  protected:
    using TaskViewIter<OptTaskView,sto,inc>::map;
    using TaskViewIter<OptTaskView,sto,inc>::i;
  public:
    /// Initialize iterator with mandatory tasks
    ManTaskViewIter(Region& r, const TaskViewArray<OptTaskView>& t);
  };

}}

#include <gecode/int/task/iter.hpp>

namespace Gecode { namespace Int {

  /// Safe addition in case \a x is -Int::Limits::infinity
  int plus(int x, int y);

  /// Safe addition in case \a x is -Int::Limits::llinfinity
  long long int plus(long long int x, long long int y);

  /// Safe addition in case \a x is -Int::Limits::double_infinity
  double plus(double x, double y);
  
  /// Task trees for task views with node type \a Node
  template<class TaskView, class Node>
  class TaskTree {
    template<class,class> friend class TaskTree;
  protected:
    /// The tasks from which the tree is computed
    const TaskViewArray<TaskView>& tasks;
    /// Task nodes
    Node* node;
    /// Map task number to leaf node number in right order
    int* _leaf;

    /// Return number of inner nodes
    int n_inner(void) const;
    /// Return number of nodes for balanced binary tree
    int n_nodes(void) const;
    /// Whether node \a i is index of root
    static bool n_root(int i);
    /// Whether node \a i is leaf
    bool n_leaf(int i) const;
    /// Return index of left child of node \a i
    static int n_left(int i);
    /// Test whether node \a i is a left child
    static bool left(int i);
    /// Return index of right child of node \a i
    static int n_right(int i);
    /// Test whether node \a i is a right child
    static bool right(int i);
    /// Return index of parent of node \a i
    static int n_parent(int i);
  protected:
    /// Return leaf for task \a i
    Node& leaf(int i);
    /// Return root node
    const Node& root(void) const;
    /// Update tree after leaf for task \a i has changed (\a l whether i refers to a leaf)
    void update(int i, bool l=true);
    /// Initialize tree after leaves have been initialized
    void init(void);
    /// Update all inner nodes of tree after leaves have been initialized
    void update(void);
    /// Initialize tree for tasks \a t
    TaskTree(Region& r, const TaskViewArray<TaskView>& t);
    /// Initialize tree using tree \a t
    template<class Node2> TaskTree(Region& r,
                                   const TaskTree<TaskView,Node2>& t);
  };

}}

#include <gecode/int/task/tree.hpp>

namespace Gecode { namespace Int {

  /**
   * \brief %Propagator for tasks
   *
   * Requires \code #include <gecode/int/task.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Task, PropCond pc>
  class TaskProp : public Propagator {
  protected:
    /// Tasks
    TaskArray<Task> t;
    /// Constructor for creation
    TaskProp(Home home, TaskArray<Task>& t);
    /// Constructor for cloning \a p
    TaskProp(Space& home, bool shared, TaskProp<Task,pc>& p);
  public:
    /// Cost function (defined as high linear)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /// Purge optional tasks that are excluded and possibly rewrite propagator
  template<class OptTask,PropCond pc>
  ExecStatus purge(Space& home, Propagator& p, TaskArray<OptTask>& t);

  /// Purge optional tasks that are excluded and possibly rewrite propagator
  template<class OptTask,PropCond pc,class Cap>
  ExecStatus purge(Space& home, Propagator& p, TaskArray<OptTask>& t, Cap c);

}}

#include <gecode/int/task/prop.hpp>
#include <gecode/int/task/purge.hpp>

#endif

// STATISTICS: int-prop
