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

#ifndef __GECODE_INT_CUMULATIVE_HH__
#define __GECODE_INT_CUMULATIVE_HH__

#include <gecode/int/task.hh>
#include <gecode/int/unary.hh>

/**
 * \namespace Gecode::Int::Cumulative
 *
 * The edge-finding and overload-checking algorithms and data structures
 * follow (mostly):
 *   Petr Vilím, Max Energy Filtering Algorithm for Discrete
 *   Cumulative Resources, CP-AI-OR, 2009.
 *   Petr Vilím, Edge Finding Filtering Algorithm for Discrete
 *   Cumulative Resources in O(kn log n), CP, 2009.
 *
 * \brief %Scheduling for cumulative resources
 */

namespace Gecode { namespace Int { namespace Cumulative {

  /// Throw exception if multiplication of \a x and \a y overflows
  void mul_check(long long int x, long long int y);

  /// Throw exception if multiplication of \a x, \a y, and \a z overflows
  void mul_check(long long int x, long long int y, long long int z);

}}}

#include <gecode/int/cumulative/limits.hpp>

namespace Gecode { namespace Int { namespace Cumulative {
  
  /// Cumulative (mandatory) task with fixed processing time
  class ManFixPTask : public Unary::ManFixPTask {
  protected:
    /// Required capacity
    int _c;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ManFixPTask(void);
    /// Initialize task with start time \a s, processing time \a p, and required resource \a c
    ManFixPTask(IntVar s, int p, int c);
    /// Initialize task with start time \a s, processing time \a p, and required resource \a c
    void init(IntVar s, int p, int c);
    /// Initialize from task \a t
    void init(const ManFixPTask& t);
    //@}

    /// \name Value access
    //@{
    /// Return required capacity
    int c(void) const;
    /// Return required energy
    long long int e(void) const;
    //@}

    /// \name Cloning
    //@{
    /// Update this task to be a clone of task \a t
    void update(Space& home, bool share, ManFixPTask& t);
    //@}

  };

  /**
   * \brief Print task in format est:[p,c]:lct
   * \relates ManFixPTask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFixPTask& t);

  /// Cumulative (mandatory) task with fixed processing, start or end time
  class ManFixPSETask : public Unary::ManFixPSETask {
  protected:
    /// Required capacity
    int _c;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ManFixPSETask(void);
    /**
     * \brief Initialize task
     *
     * Depending on \a t, \a s is either the end time (if \a t is FIXS)
     * or the start time of the task, \a p is the fixed parameter,
     * and \a c is the required capacity.
     */
    ManFixPSETask(TaskType t, IntVar s, int p, int c);
    /**
     * \brief Initialize task
     *
     * Depending on \a t, \a s is either the end time (if \a t is FIXS)
     * or the start time of the task, \a p is the fixed parameter,
     * and \a c is the required capacity.
     */
    void init(TaskType t, IntVar s, int p, int c);
    /// Initialize from task \a t
    void init(const ManFixPSETask& t);
    //@}

    /// \name Value access
    //@{
    /// Return required capacity
    int c(void) const;
    /// Return required energy
    long long int e(void) const;
    //@}

    /// \name Cloning
    //@{
    /// Update this task to be a clone of task \a t
    void update(Space& home, bool share, ManFixPSETask& t);
    //@}

  };

  /**
   * \brief Print task in format est:[p,c]:lct
   * \relates ManFixPSETask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFixPSETask& t);

  /// Cumulative (mandatory) task with flexible processing time
  class ManFlexTask : public Unary::ManFlexTask {
  protected:
    /// Required capacity
    int _c;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ManFlexTask(void);
    /// Initialize with start time \a s, processing time \a p, end time \a e
    ManFlexTask(IntVar s, IntVar p, IntVar e, int c);
    /// Initialize with start time \a s, processing time \a p, end time \a e
    void init(IntVar s, IntVar p, IntVar e, int c);
    /// Initialize from task \a t
    void init(const ManFlexTask& t);
    //@}

    /// \name Value access
    //@{
    /// Return required capacity
    int c(void) const;
    /// Return required energy
    long long int e(void) const;
    //@}

    /// \name Cloning
    //@{
    /// Update this task to be a clone of task \a t
    void update(Space& home, bool share, ManFlexTask& t);
    //@}

  };

  /**
   * \brief Print task in format est:[p,c]:lct
   * \relates ManFlexTask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFlexTask& t);


  /// Cumulative optional task with fixed processing time
  class OptFixPTask : public ManToOptTask<ManFixPTask> {
  protected:
    using ManToOptTask<ManFixPTask>::_m;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    OptFixPTask(void);
    /// Initialize with start time \a s, processing time \a p, required capacity \a c, and mandatory flag \a m
    OptFixPTask(IntVar s, int p, int c, BoolVar m);
    /// Initialize with start time \a s, processing time \a p, required capacity \a c, and mandatory flag \a m
    void init(IntVar s, int p, int c, BoolVar m);
    //@}
    /// Cast to corresponding unary task
    operator Unary::OptFixPTask (void);
  };

  /**
   * \brief Print optional task in format est:[p,c]:lct:m
   * \relates OptFixPTask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFixPTask& t);

  /// Cumulative optional task with fixed processing, start or end time
  class OptFixPSETask : public ManToOptTask<ManFixPSETask> {
  protected:
    using ManToOptTask<ManFixPSETask>::_m;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    OptFixPSETask(void);
    /// Initialize with start time \a s, processing time \a p, required capacity \a c, and mandatory flag \a m
    OptFixPSETask(TaskType t, IntVar s, int p, int c, BoolVar m);
    /// Initialize with start time \a s, processing time \a p, required capacity \a c, and mandatory flag \a m
    void init(TaskType t, IntVar s, int p, int c, BoolVar m);
    //@}
    /// Cast to corresponding unary task
    operator Unary::OptFixPSETask (void);
  };

  /**
   * \brief Print optional task in format est:[p,c]:lct:m
   * \relates OptFixPSETask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFixPSETask& t);

  /// %Cumulative optional task with flexible processing time
  class OptFlexTask : public ManToOptTask<ManFlexTask> {
  protected:
    using ManToOptTask<ManFlexTask>::_m;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    OptFlexTask(void);
    /// Initialize with start time \a s, processing time \a p, end time \a e, and mandatory flag \a m
    OptFlexTask(IntVar s, IntVar p, IntVar e, int c, BoolVar m);
    /// Initialize with start time \a s, processing time \a p, end time \a e, and mandatory flag \a m
    void init(IntVar s, IntVar p, IntVar e, int c, BoolVar m);
    //@}
    /// Cast to corresponding unary task
    operator Unary::OptFlexTask (void);
  };

  /**
   * \brief Print optional task in format est:lst:pmin:pmax:c:ect:lct:m
   * \relates OptFlexTask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFlexTask& t);

}}}

#include <gecode/int/cumulative/task.hpp>

namespace Gecode { namespace Int { namespace Cumulative {

  /// Forward mandatory fixed task view
  typedef ManFixPTask ManFixPTaskFwd;

  /// Backward (dual) mandatory fixed task view
  typedef FwdToBwd<ManFixPTaskFwd> ManFixPTaskBwd;

  /// Forward mandatory fixed task view
  typedef ManFixPSETask ManFixPSETaskFwd;

  /// Backward (dual) mandatory fixed task view
  typedef FwdToBwd<ManFixPSETaskFwd> ManFixPSETaskBwd;

  /// Forward optional fixed task view
  typedef OptFixPTask OptFixPTaskFwd;

  /// Backward (dual) optional fixed task view
  typedef FwdToBwd<OptFixPTaskFwd> OptFixPTaskBwd;

  /// Forward optional fixed task view
  typedef OptFixPSETask OptFixPSETaskFwd;

  /// Backward (dual) optional fixed task view
  typedef FwdToBwd<OptFixPSETaskFwd> OptFixPSETaskBwd;

  /// Forward mandatory flexible task view
  typedef ManFlexTask ManFlexTaskFwd;

  /// Backward (dual) mandatory flexible task view
  typedef FwdToBwd<ManFlexTaskFwd> ManFlexTaskBwd;

  /// Forward optional flexible task view
  typedef OptFlexTask OptFlexTaskFwd;

  /// Backward (dual) optional flexible task view
  typedef FwdToBwd<OptFlexTaskFwd> OptFlexTaskBwd;


  /**
   * \brief Print backward task view in format est:p:lct
   * \relates ManFixPTaskBwd
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFixPTaskBwd& t);

  /**
   * \brief Print backward task view in format est:p:lct
   * \relates ManFixPSETaskBwd
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFixPSETaskBwd& t);

  /**
   * \brief Print optional backward task view in format est:p:lct:m
   * \relates OptFixPTaskBwd
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFixPTaskBwd& t);

  /**
   * \brief Print optional backward task view in format est:p:lct:m
   * \relates OptFixPSETaskBwd
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFixPSETaskBwd& t);

}}}

#include <gecode/int/cumulative/task-view.hpp>

namespace Gecode { namespace Int {

  /// Task view traits for forward task views
  template<>
  class TaskViewTraits<Cumulative::ManFixPTaskFwd> {
  public:
    /// The task type
    typedef Cumulative::ManFixPTask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Cumulative::ManFixPTaskBwd> {
  public:
    /// The task type
    typedef Cumulative::ManFixPTask Task;
  };

  /// Task view traits for forward task views
  template<>
  class TaskViewTraits<Cumulative::ManFixPSETaskFwd> {
  public:
    /// The task type
    typedef Cumulative::ManFixPSETask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Cumulative::ManFixPSETaskBwd> {
  public:
    /// The task type
    typedef Cumulative::ManFixPSETask Task;
  };

  /// Task view traits for forward optional task views
  template<>
  class TaskViewTraits<Cumulative::OptFixPTaskFwd> {
  public:
    /// The task type
    typedef Cumulative::OptFixPTask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Cumulative::OptFixPTaskBwd> {
  public:
    /// The task type
    typedef Cumulative::OptFixPTask Task;
  };

  /// Task view traits for forward optional task views
  template<>
  class TaskViewTraits<Cumulative::OptFixPSETaskFwd> {
  public:
    /// The task type
    typedef Cumulative::OptFixPSETask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Cumulative::OptFixPSETaskBwd> {
  public:
    /// The task type
    typedef Cumulative::OptFixPSETask Task;
  };

  /// Task view traits for forward task views
  template<>
  class TaskViewTraits<Cumulative::ManFlexTaskFwd> {
  public:
    /// The task type
    typedef Cumulative::ManFlexTask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Cumulative::ManFlexTaskBwd> {
  public:
    /// The task type
    typedef Cumulative::ManFlexTask Task;
  };

  /// Task view traits for forward optional task views
  template<>
  class TaskViewTraits<Cumulative::OptFlexTaskFwd> {
  public:
    /// The task type
    typedef Cumulative::OptFlexTask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Cumulative::OptFlexTaskBwd> {
  public:
    /// The task type
    typedef Cumulative::OptFlexTask Task;
  };


  /// Task traits for mandatory fixed tasks
  template<>
  class TaskTraits<Cumulative::ManFixPTask> {
  public:
    /// The forward task view type
    typedef Cumulative::ManFixPTaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Cumulative::ManFixPTaskBwd TaskViewBwd;
    /// The corresponding unary task type
    typedef Unary::ManFixPTask UnaryTask;
  };

  /// Task traits for mandatory fixed tasks
  template<>
  class TaskTraits<Cumulative::ManFixPSETask> {
  public:
    /// The forward task view type
    typedef Cumulative::ManFixPSETaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Cumulative::ManFixPSETaskBwd TaskViewBwd;
    /// The corresponding unary task type
    typedef Unary::ManFixPSETask UnaryTask;
  };

  /// Task traits for optional fixed tasks
  template<>
  class TaskTraits<Cumulative::OptFixPTask> {
  public:
    /// The forward task view type
    typedef Cumulative::OptFixPTaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Cumulative::OptFixPTaskBwd TaskViewBwd;
    /// The corresponding mandatory task
    typedef Cumulative::ManFixPTask ManTask;
    /// The corresponding unary task type
    typedef Unary::OptFixPTask UnaryTask;
  };

  /// Task traits for optional fixed tasks
  template<>
  class TaskTraits<Cumulative::OptFixPSETask> {
  public:
    /// The forward task view type
    typedef Cumulative::OptFixPSETaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Cumulative::OptFixPSETaskBwd TaskViewBwd;
    /// The corresponding mandatory task
    typedef Cumulative::ManFixPSETask ManTask;
    /// The corresponding unary task type
    typedef Unary::OptFixPSETask UnaryTask;
  };

  /// Task traits for mandatory flexible tasks
  template<>
  class TaskTraits<Cumulative::ManFlexTask> {
  public:
    /// The forward task view type
    typedef Cumulative::ManFlexTaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Cumulative::ManFlexTaskBwd TaskViewBwd;
    /// The corresponding unary task type
    typedef Unary::ManFlexTask UnaryTask;
  };

  /// Task traits for optional flexible tasks
  template<>
  class TaskTraits<Cumulative::OptFlexTask> {
  public:
    /// The forward task view type
    typedef Cumulative::OptFlexTaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Cumulative::OptFlexTaskBwd TaskViewBwd;
    /// The corresponding mandatory task
    typedef Cumulative::ManFlexTask ManTask;
    /// The corresponding unary task type
    typedef Unary::OptFlexTask UnaryTask;
  };

}}

namespace Gecode { namespace Int { namespace Cumulative {

  /// Node for an omega tree
  class OmegaNode {
  public:
    /// Energy for subtree
    long long int e;
    /// Energy envelope for subtree
    long long int env;
    /// Initialize node from left child \a l and right child \a r
    void init(const OmegaNode& l, const OmegaNode& r);
    /// Update node from left child \a l and right child \a r
    void update(const OmegaNode& l, const OmegaNode& r);
  };

  /// Omega trees for computing ect of task sets
  template<class TaskView>
  class OmegaTree : public TaskTree<TaskView,OmegaNode> {
  protected:
    using TaskTree<TaskView,OmegaNode>::tasks;
    using TaskTree<TaskView,OmegaNode>::leaf;
    using TaskTree<TaskView,OmegaNode>::root;
    using TaskTree<TaskView,OmegaNode>::init;
    using TaskTree<TaskView,OmegaNode>::update;
    /// Capacity
    int c;
  public:
    /// Initialize tree for tasks \a t and capacity \a c
    OmegaTree(Region& r, int c, const TaskViewArray<TaskView>& t);
    /// Insert task with index \a i
    void insert(int i);
    /// Remove task with index \a i
    void remove(int i);
    /// Return energy envelope of all tasks
    long long int env(void) const;
  };

  /// Node for an extended omega tree
  class ExtOmegaNode : public OmegaNode {
  public:
    /// Energy envelope for subtree
    long long int cenv;
    /// Initialize node from left child \a l and right child \a r
    void init(const ExtOmegaNode& l, const ExtOmegaNode& r);
    /// Update node from left child \a l and right child \a r
    void update(const ExtOmegaNode& l, const ExtOmegaNode& r);
  };

  /// Omega trees for computing ect of task sets
  template<class TaskView>
  class ExtOmegaTree : public TaskTree<TaskView,ExtOmegaNode> {
  protected:
    using TaskTree<TaskView,ExtOmegaNode>::tasks;
    using TaskTree<TaskView,ExtOmegaNode>::leaf;
    using TaskTree<TaskView,ExtOmegaNode>::root;
    using TaskTree<TaskView,ExtOmegaNode>::init;
    using TaskTree<TaskView,ExtOmegaNode>::update;
    using TaskTree<TaskView,ExtOmegaNode>::node;
    using TaskTree<TaskView,ExtOmegaNode>::n_leaf;
    using TaskTree<TaskView,ExtOmegaNode>::n_left;
    using TaskTree<TaskView,ExtOmegaNode>::left;
    using TaskTree<TaskView,ExtOmegaNode>::n_right;
    using TaskTree<TaskView,ExtOmegaNode>::right;
    using TaskTree<TaskView,ExtOmegaNode>::n_root;
    using TaskTree<TaskView,ExtOmegaNode>::n_parent;
    using TaskTree<TaskView,ExtOmegaNode>::n_nodes;
    using TaskTree<TaskView,ExtOmegaNode>::_leaf;
    /// Capacity
    int c, ci;
  public:
    /// Initialize tree for tasks \a t and capacity \a c
    template<class Node>
    ExtOmegaTree(Region& r, int c, const TaskTree<TaskView,Node>& t);
    /// Initialize tasks for current capacity \a ci
    void init(int ci);
    /// Compute update for task with index \a i
    long long int env(int i);
  };


  /// Node for an omega lambda tree
  class OmegaLambdaNode : public OmegaNode {
  public:
    /// Undefined task
    static const int undef = -1;
    /// Energy for subtree
    long long int le;
    /// Energy envelope for subtree
    long long int lenv;
    /// Node which is responsible for le
    int resLe;
    /// Node which is responsible for lenv
    int resLenv;
    /// Initialize node from left child \a l and right child \a r
    void init(const OmegaLambdaNode& l, const OmegaLambdaNode& r);
    /// Update node from left child \a l and right child \a r
    void update(const OmegaLambdaNode& l, const OmegaLambdaNode& r);
  };

  /// Omega-lambda trees for computing ect of task sets
  template<class TaskView>
  class OmegaLambdaTree : public TaskTree<TaskView,OmegaLambdaNode> {
  protected:
    using TaskTree<TaskView,OmegaLambdaNode>::tasks;
    using TaskTree<TaskView,OmegaLambdaNode>::leaf;
    using TaskTree<TaskView,OmegaLambdaNode>::root;
    using TaskTree<TaskView,OmegaLambdaNode>::init;
    using TaskTree<TaskView,OmegaLambdaNode>::update;
    /// Capacity
    int c;
  public:
    /// Initialize tree for tasks \a t and capcity \a c with all tasks included in omega
    OmegaLambdaTree(Region& r, int c, const TaskViewArray<TaskView>& t);
    /// Shift task with index \a i from omega to lambda
    void shift(int i);
    /// Remove task with index \a i from lambda
    void lremove(int i);
    /// Whether has responsible task
    bool lempty(void) const;
    /// Return responsible task
    int responsible(void) const;
    /// Return energy envelope of all tasks
    long long int env(void) const;
    /// Return energy envelope of all tasks excluding lambda tasks
    long long int lenv(void) const;
  };

}}}

#include <gecode/int/cumulative/tree.hpp>

namespace Gecode { namespace Int { namespace Cumulative {

  /// Perform basic propagation
  template<class Task, class Cap>
  ExecStatus basic(Space& home, bool& subsumed, Cap c, TaskArray<Task>& t);

  /// Check mandatory tasks \a t for overload
  template<class ManTask>
  ExecStatus overload(Space& home, int c, TaskArray<ManTask>& t);

  /// Propagate by edge finding
  template<class Task>
  ExecStatus edgefinding(Space& home, int c, TaskArray<Task>& t);

  /**
   * \brief Scheduling propagator for cumulative resource with mandatory tasks
   *
   * Requires \code #include <gecode/int/cumulative.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class ManTask, class Cap>
  class ManProp : public TaskProp<ManTask,Int::PC_INT_DOM> {
  protected:
    using TaskProp<ManTask,Int::PC_INT_DOM>::t;
    /// Resource capacity
    Cap c;
    /// Constructor for creation
    ManProp(Home home, Cap c, TaskArray<ManTask>& t);
    /// Constructor for cloning \a p
    ManProp(Space& home, bool shared, ManProp& p);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator that schedules tasks on cumulative resource
    static ExecStatus post(Home home, Cap c, TaskArray<ManTask>& t);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  /**
   * \brief Scheduling propagator for cumulative resource with optional tasks
   *
   * Requires \code #include <gecode/int/cumulative.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class OptTask, class Cap>
  class OptProp : public TaskProp<OptTask,Int::PC_INT_DOM> {
  protected:
    using TaskProp<OptTask,Int::PC_INT_DOM>::t;
    /// Resource capacity
    Cap c;
    /// Constructor for creation
    OptProp(Home home, Cap c, TaskArray<OptTask>& t);
    /// Constructor for cloning \a p
    OptProp(Space& home, bool shared, OptProp& p);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator that schedules tasks on cumulative resource
    static ExecStatus post(Home home, Cap c, TaskArray<OptTask>& t);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

}}}

#include <gecode/int/cumulative/basic.hpp>
#include <gecode/int/cumulative/overload.hpp>
#include <gecode/int/cumulative/edge-finding.hpp>
#include <gecode/int/cumulative/man-prop.hpp>
#include <gecode/int/cumulative/opt-prop.hpp>

#endif

// STATISTICS: int-prop
