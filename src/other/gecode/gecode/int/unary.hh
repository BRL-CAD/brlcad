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

#ifndef __GECODE_INT_UNARY_HH__
#define __GECODE_INT_UNARY_HH__

#include <gecode/int/task.hh>

/**
 * \namespace Gecode::Int::Unary
 *
 * The algorithms and data structures follow (mostly):
 *   Petr Vilím, Global Constraints in Int, PhD thesis, 
 *   Charles University, Prague, Czech Republic, 2007.
 *
 * \brief %Int for unary resources
 */

namespace Gecode { namespace Int { namespace Unary {

  /// %Unary (mandatory) task with fixed processing time
  class ManFixPTask {
  protected:
    /// Start time
    Int::IntView _s;
    /// Processing time
    int _p;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ManFixPTask(void);
    /// Initialize with start time \a s and processing time \a p
    ManFixPTask(IntVar s, int p);
    /// Initialize with start time \a s and processing time \a p
    void init(IntVar s, int p);
    /// Initialize from task \a t
    void init(const ManFixPTask& t);
    //@}

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
    /// Return start time
    IntVar st(void) const;
    /// Whether task is mandatory
    bool mandatory(void) const;
    /// Whether task is excluded
    bool excluded(void) const;
    /// Whether task can still be optional
    bool optional(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether task is assigned
    bool assigned(void) const;
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
    /// Mark task as mandatory
    ModEvent mandatory(Space& home);
    /// Mark task as excluded
    ModEvent excluded(Space& home);
    //@}

    /// \name Cloning
    //@{
    /// Update this task to be a clone of task \a t
    void update(Space& home, bool share, ManFixPTask& t);
    //@}

    /// \name Dependencies
    //@{
    /// Subscribe propagator \a p to task
    void subscribe(Space& home, Propagator& p, PropCond pc=Int::PC_INT_BND);
    /// Cancel subscription of propagator \a p for task
    void cancel(Space& home, Propagator& p, PropCond pc=Int::PC_INT_BND);
    //@}

  };

  /**
   * \brief Print task in format est:p:lct
   * \relates ManFixPTask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFixPTask& t);

  /// %Unary (mandatory) task with fixed processing, start or end time
  class ManFixPSETask : public ManFixPTask {
  protected:
    /// Task type
    TaskType _t;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ManFixPSETask(void);
    /**
     * \brief Initialize task
     *
     * Depending on \a t, \a s is either the end time (if \a t is TT_FIXS)
     * or the start time of the task, and \a p is the fixed parameter.
     */
    ManFixPSETask(TaskType t, IntVar s, int p);
    /**
     * \brief Initialize task
     *
     * Depending on \a t, \a s is either the end time (if \a t is FIXS)
     * or the start time of the task, and \a p is the fixed parameter.
     */
    void init(TaskType t, IntVar s, int p);
    /// Initialize from task \a t
    void init(const ManFixPSETask& t);
    //@}

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

  /// %Unary optional task with fixed processing time
  class OptFixPTask : public ManToOptTask<ManFixPTask> {
  protected:
    using ManToOptTask<ManFixPTask>::_m;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    OptFixPTask(void);
    /// Initialize with start time \a s, processing time \a p, and mandatory flag \a m
    OptFixPTask(IntVar s, int p, BoolVar m);
    /// Initialize with start time \a s, processing time \a p, and mandatory flag \a m
    void init(IntVar s, int p, BoolVar m);
    //@}
  };

  /**
   * \brief Print optional task in format est:p:lct:m
   * \relates OptFixPTask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFixPTask& t);

  /// Unary optional task with fixed processing, start or end time
  class OptFixPSETask : public ManToOptTask<ManFixPSETask> {
  protected:
    using ManToOptTask<ManFixPSETask>::_m;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    OptFixPSETask(void);
    /// Initialize with start time \a s, processing time \a p, and mandatory flag \a m
    OptFixPSETask(TaskType t, IntVar s, int p, BoolVar m);
    /// Initialize with start time \a s, processing time \a p, and mandatory flag \a m
    void init(TaskType t, IntVar s, int p, BoolVar m);
    //@}
  };

  /**
   * \brief Print optional task in format est:p:lct:m
   * \relates OptFixPSETask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFixPSETask& t);

  /// %Unary (mandatory) task with flexible processing time
  class ManFlexTask {
  protected:
    /// Start time
    Int::IntView _s;
    /// Processing time
    Int::IntView _p;
    /// End time
    Int::IntView _e;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ManFlexTask(void);
    /// Initialize with start time \a s, processing time \a p, end time \a e
    ManFlexTask(IntVar s, IntVar p, IntVar e);
    /// Initialize with start time \a s, processing time \a p, end time \a e
    void init(IntVar s, IntVar p, IntVar e);
    /// Initialize from task \a t
    void init(const ManFlexTask& t);
    //@}

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
    /// Return start time
    IntVar st(void) const;
    /// Return processing time
    IntVar p(void) const;
    /// Return end time
    IntVar e(void) const;
    /// Whether task is mandatory
    bool mandatory(void) const;
    /// Whether task is excluded
    bool excluded(void) const;
    /// Whether task can still be optional
    bool optional(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether task is assigned
    bool assigned(void) const;
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
    /// Mark task as mandatory
    ModEvent mandatory(Space& home);
    /// Mark task as excluded
    ModEvent excluded(Space& home);
    //@}

    /// \name Cloning
    //@{
    /// Update this task to be a clone of task \a t
    void update(Space& home, bool share, ManFlexTask& t);
    //@}

    /// \name Dependencies
    //@{
    /// Subscribe propagator \a p to task
    void subscribe(Space& home, Propagator& p, PropCond pc=Int::PC_INT_BND);
    /// Cancel subscription of propagator \a p for task
    void cancel(Space& home, Propagator& p, PropCond pc=Int::PC_INT_BND);
    //@}

  };

  /**
   * \brief Print task in format est:lst:pmin:pmax:ect:lct
   * \relates ManFlexTask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFlexTask& t);

  /// %Unary optional task with flexible processing time
  class OptFlexTask : public ManToOptTask<ManFlexTask> {
  protected:
    using ManToOptTask<ManFlexTask>::_m;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    OptFlexTask(void);
    /// Initialize with start time \a s, processing time \a p, end time \a e, and mandatory flag \a m
    OptFlexTask(IntVar s, IntVar p, IntVar e, BoolVar m);
    /// Initialize with start time \a s, processing time \a p, end time \a e, and mandatory flag \a m
    void init(IntVar s, IntVar p, IntVar e, BoolVar m);
    //@}
  };

  /**
   * \brief Print optional task in format est:lst:pmin:pmax:ect:lct:m
   * \relates OptFlexTask
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFlexTask& t);

}}}

#include <gecode/int/unary/task.hpp>

namespace Gecode { namespace Int { namespace Unary {

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

  /**
   * \brief Print backward task view in format est:lst:pmin:pmax:ect:lct
   * \relates ManFlexTaskBwd
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFlexTaskBwd& t);

  /**
   * \brief Print optional backward task view in format 
   * est:lst:pmin:pmax:ect:lct:m
   *
   * \relates OptFlexTaskBwd
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFlexTaskBwd& t);

}}}

#include <gecode/int/unary/task-view.hpp>

namespace Gecode { namespace Int {

  /// Task view traits for forward task views
  template<>
  class TaskViewTraits<Unary::ManFixPTaskFwd> {
  public:
    /// The task type
    typedef Unary::ManFixPTask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Unary::ManFixPTaskBwd> {
  public:
    /// The task type
    typedef Unary::ManFixPTask Task;
  };

  /// Task view traits for forward task views
  template<>
  class TaskViewTraits<Unary::ManFixPSETaskFwd> {
  public:
    /// The task type
    typedef Unary::ManFixPSETask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Unary::ManFixPSETaskBwd> {
  public:
    /// The task type
    typedef Unary::ManFixPSETask Task;
  };

  /// Task view traits for forward optional task views
  template<>
  class TaskViewTraits<Unary::OptFixPTaskFwd> {
  public:
    /// The task type
    typedef Unary::OptFixPTask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Unary::OptFixPTaskBwd> {
  public:
    /// The task type
    typedef Unary::OptFixPTask Task;
  };

  /// Task view traits for forward optional task views
  template<>
  class TaskViewTraits<Unary::OptFixPSETaskFwd> {
  public:
    /// The task type
    typedef Unary::OptFixPSETask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Unary::OptFixPSETaskBwd> {
  public:
    /// The task type
    typedef Unary::OptFixPSETask Task;
  };

  /// Task view traits for forward task views
  template<>
  class TaskViewTraits<Unary::ManFlexTaskFwd> {
  public:
    /// The task type
    typedef Unary::ManFlexTask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Unary::ManFlexTaskBwd> {
  public:
    /// The task type
    typedef Unary::ManFlexTask Task;
  };

  /// Task view traits for forward optional task views
  template<>
  class TaskViewTraits<Unary::OptFlexTaskFwd> {
  public:
    /// The task type
    typedef Unary::OptFlexTask Task;
  };

  /// Task view traits for backward task views
  template<>
  class TaskViewTraits<Unary::OptFlexTaskBwd> {
  public:
    /// The task type
    typedef Unary::OptFlexTask Task;
  };


  /// Task traits for mandatory fixed tasks
  template<>
  class TaskTraits<Unary::ManFixPTask> {
  public:
    /// The forward task view type
    typedef Unary::ManFixPTaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Unary::ManFixPTaskBwd TaskViewBwd;
  };

  /// Task traits for mandatory fixed tasks
  template<>
  class TaskTraits<Unary::ManFixPSETask> {
  public:
    /// The forward task view type
    typedef Unary::ManFixPSETaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Unary::ManFixPSETaskBwd TaskViewBwd;
  };

  /// Task traits for optional fixed tasks
  template<>
  class TaskTraits<Unary::OptFixPTask> {
  public:
    /// The forward task view type
    typedef Unary::OptFixPTaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Unary::OptFixPTaskBwd TaskViewBwd;
    /// The corresponding mandatory task
    typedef Unary::ManFixPTask ManTask;
  };

  /// Task traits for optional fixed tasks
  template<>
  class TaskTraits<Unary::OptFixPSETask> {
  public:
    /// The forward task view type
    typedef Unary::OptFixPSETaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Unary::OptFixPSETaskBwd TaskViewBwd;
    /// The corresponding mandatory task
    typedef Unary::ManFixPTask ManTask;
  };

  /// Task traits for mandatory flexible tasks
  template<>
  class TaskTraits<Unary::ManFlexTask> {
  public:
    /// The forward task view type
    typedef Unary::ManFlexTaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Unary::ManFlexTaskBwd TaskViewBwd;
  };

  /// Task traits for optional flexible tasks
  template<>
  class TaskTraits<Unary::OptFlexTask> {
  public:
    /// The forward task view type
    typedef Unary::OptFlexTaskFwd TaskViewFwd;
    /// The backward task view type
    typedef Unary::OptFlexTaskBwd TaskViewBwd;
    /// The corresponding mandatory task
    typedef Unary::ManFlexTask ManTask;
  };

}}

namespace Gecode { namespace Int { namespace Unary {

  /// Node for an omega tree
  class OmegaNode {
  public:
    /// Processing time for subtree
    int p;
    /// Earliest completion time for subtree
    int ect;
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
  public:
    /// Initialize tree for tasks \a t
    OmegaTree(Region& r, const TaskViewArray<TaskView>& t);
    /// Insert task with index \a i
    void insert(int i);
    /// Remove task with index \a i
    void remove(int i);
    /// Return earliest completion time of all tasks
    int ect(void) const;
    /// Return earliest completion time of all tasks but \a i
    int ect(int i) const;
  };

  /// Node for an omega lambda tree
  class OmegaLambdaNode : public OmegaNode {
  public:
    /// Undefined task
    static const int undef = -1;
    /// Processing times for subtree
    int lp;
    /// Earliest completion times for subtree
    int lect;
    /// Node which is responsible for lect
    int resEct;
    /// Node which is responsible for lp
    int resLp;
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
  public:
    /// Initialize tree for tasks \a t with all tasks included, if \a inc is true
    OmegaLambdaTree(Region& r, const TaskViewArray<TaskView>& t, 
                    bool inc=true);
    /// Shift task with index \a i from omega to lambda
    void shift(int i);
    /// Insert task with index \a i to omega
    void oinsert(int i);
    /// Insert task with index \a i to lambda
    void linsert(int i);
    /// Remove task with index \a i from lambda
    void lremove(int i);
    /// Whether has responsible task
    bool lempty(void) const;
    /// Return responsible task
    int responsible(void) const;
    /// Return earliest completion time of all tasks
    int ect(void) const;
    /// Return earliest completion time of all tasks excluding lambda tasks
    int lect(void) const;
  };

}}}

#include <gecode/int/unary/tree.hpp>

namespace Gecode { namespace Int { namespace Unary {

  /// Check mandatory tasks \a t for overload
  template<class ManTask>
  ExecStatus overload(Space& home, TaskArray<ManTask>& t);
  /// Check optional tasks \a t for overload
  template<class OptTask>
  ExecStatus overload(Space& home, Propagator& p, TaskArray<OptTask>& t);

  /// Check tasks \a t for subsumption
  template<class Task>
  ExecStatus subsumed(Space& home, Propagator& p, TaskArray<Task>& t);

  /// Propagate detectable precedences
  template<class ManTask>
  ExecStatus detectable(Space& home, TaskArray<ManTask>& t);
  /// Propagate detectable precedences
  template<class OptTask>
  ExecStatus detectable(Space& home, Propagator& p, TaskArray<OptTask>& t);

  /// Propagate not-first and not-last
  template<class ManTask>
  ExecStatus notfirstnotlast(Space& home, TaskArray<ManTask>& t);
  /// Propagate not-first and not-last
  template<class OptTask>
  ExecStatus notfirstnotlast(Space& home, Propagator& p, TaskArray<OptTask>& t);

  /// Propagate by edge finding
  template<class Task>
  ExecStatus edgefinding(Space& home, TaskArray<Task>& t);


  /**
   * \brief %Scheduling propagator for unary resource with mandatory tasks
   *
   * Requires \code #include <gecode/int/unary.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class ManTask>
  class ManProp : public TaskProp<ManTask,Int::PC_INT_BND> {
  protected:
    using TaskProp<ManTask,Int::PC_INT_BND>::t;
    /// Constructor for creation
    ManProp(Home home, TaskArray<ManTask>& t);
    /// Constructor for cloning \a p
    ManProp(Space& home, bool shared, ManProp& p);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator that schedules tasks on unary resource
    static ExecStatus post(Home home, TaskArray<ManTask>& t);
  };

  /**
   * \brief %Scheduling propagator for unary resource with optional tasks
   *
   * Requires \code #include <gecode/int/unary.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class OptTask>
  class OptProp : public TaskProp<OptTask,Int::PC_INT_BND> {
  protected:
    using TaskProp<OptTask,Int::PC_INT_BND>::t;
    /// Constructor for creation
    OptProp(Home home, TaskArray<OptTask>& t);
    /// Constructor for cloning \a p
    OptProp(Space& home, bool shared, OptProp& p);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator that schedules tasks on unary resource
    static ExecStatus post(Home home, TaskArray<OptTask>& t);
  };

}}}

#include <gecode/int/unary/overload.hpp>
#include <gecode/int/unary/subsumption.hpp>
#include <gecode/int/unary/detectable.hpp>
#include <gecode/int/unary/not-first-not-last.hpp>
#include <gecode/int/unary/edge-finding.hpp>

#include <gecode/int/unary/man-prop.hpp>
#include <gecode/int/unary/opt-prop.hpp>

#endif

// STATISTICS: int-prop
