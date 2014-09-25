/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Contributing authors:
 *     Filip Konvicka <filip.konvicka@logis.cz>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Guido Tack, 2003
 *     Mikael Lagerkvist, 2006
 *     LOGIS, s.r.o., 2009
 *
 *  Bugfixes provided by:
 *     Alexander Samoilov <alexander_samoilov@yahoo.com>
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

#include <iostream>

namespace Gecode {

  class Space;

  /**
   * \defgroup FuncSupportShared Support for shared objects and handles
   *
   * Shared handles provide access to reference-counted objects. In
   * particular, they support updates with and without sharing.
   * An update with sharing just updates the handle, while a non-shared
   * update creates a single copy per space.
   *
   * \ingroup FuncSupport
   */

  /**
   * \brief The shared handle
   *
   * A shared handle provides access to an object that lives outside a space,
   * and is shared between entities that possibly reside inside different
   * spaces. The handle has an update mechanism that supports updates with and
   * without sharing. An update without sharing makes sure that a
   * single copy of the object is created when the space is copied.
   *
   * This is the base class that all shared handles must inherit from.
   *
   * \ingroup FuncSupportShared
   */
  class SharedHandle {
  public:
    /**
     * \brief The shared object
     *
     * Shared objects must inherit from this base class.
     *
     * \ingroup FuncSupportShared
     */
    class Object {
      friend class Space;
      friend class SharedHandle;
    private:
      /// The next object collected during copying
      Object* next;
      /// The forwarding pointer
      Object* fwd;
      /// The counter used for reference counting
      unsigned int use_cnt;
    public:
      /// Initialize
      Object(void);
      /// Return fresh copy for update
      virtual Object* copy(void) const = 0;
      /// Delete shared object
      virtual ~Object(void);
      /// Allocate memory from heap
      static void* operator new(size_t s);
      /// Free memory allocated from heap
      static void  operator delete(void* p);
    };
  private:
    /// The shared object
    Object* o;
    /// Subscribe handle to object
    void subscribe(void);
    /// Cancel subscription of handle to object
    void cancel(void);
  public:
    /// Create shared handle with no object pointing to
    SharedHandle(void);
    /// Create shared handle that points to shared object \a so
    SharedHandle(SharedHandle::Object* so);
    /// Copy constructor maintaining reference count
    SharedHandle(const SharedHandle& sh);
    /// Assignment operator maintaining reference count
    SharedHandle& operator =(const SharedHandle& sh);
    /// Updating during cloning
    void update(Space& home, bool share, SharedHandle& sh);
    /// Destructor that maintains reference count
    ~SharedHandle(void);
  protected:
    /// Access to the shared object
    SharedHandle::Object* object(void) const;
    /// Modify shared object
    void object(SharedHandle::Object* n);
  };

  /**
   * \defgroup TaskVarMEPC Generic modification events and propagation conditions
   *
   * Predefined modification events must be taken into account
   * by variable types.
   * \ingroup TaskVar
   */
  //@{
  /// Type for modification events
  typedef int ModEvent;

  /// Generic modification event: failed variable
  const ModEvent ME_GEN_FAILED   = -1;
  /// Generic modification event: no modification
  const ModEvent ME_GEN_NONE     =  0;
  /// Generic modification event: variable is assigned a value
  const ModEvent ME_GEN_ASSIGNED =  1;

  /// Type for propagation conditions
  typedef int PropCond;
  /// Propagation condition to be ignored (convenience)
  const PropCond PC_GEN_NONE     = -1;
  /// Propagation condition for an assigned variable
  const PropCond PC_GEN_ASSIGNED = 0;
  //@}

  /**
   * \brief Modification event deltas
   *
   * Modification event deltas are used by propagators. A
   * propagator stores a modification event for each variable type.
   * They can be accessed through a variable or a view from a given
   * propagator. They can be constructed from a given modevent by
   * a variable or view.
   * \ingroup TaskActor
   */
  typedef int ModEventDelta;

}

#include <gecode/kernel/var-type.hpp>

namespace Gecode {

  /// Configuration class for variable implementations without index structure
  class NoIdxVarImpConf {
  public:
    /// Index for update
    static const int idx_c = -1;
    /// Index for disposal
    static const int idx_d = -1;
    /// Maximal propagation condition
    static const PropCond pc_max = PC_GEN_ASSIGNED;
    /// Freely available bits
    static const int free_bits = 0;
    /// Start of bits for modification event delta
    static const int med_fst = 0;
    /// End of bits for modification event delta
    static const int med_lst = 0;
    /// Bitmask for modification event delta
    static const int med_mask = 0;
    /// Combine modification events \a me1 and \a me2
    static Gecode::ModEvent me_combine(ModEvent me1, ModEvent me2);
    /// Update modification even delta \a med by \a me, return true on change
    static bool med_update(ModEventDelta& med, ModEvent me);
  };

  forceinline ModEvent
  NoIdxVarImpConf::me_combine(ModEvent, ModEvent) {
    GECODE_NEVER; return 0;
  }
  forceinline bool
  NoIdxVarImpConf::med_update(ModEventDelta&, ModEvent) {
    GECODE_NEVER; return false;
  }


  /*
   * These are the classes of interest
   *
   */
  class ActorLink;
  class Actor;
  class Propagator;
  class LocalObject;
  class Advisor;
  class AFC;
  class Brancher;
  class BrancherHandle;
  template<class A> class Council;
  template<class A> class Advisors;
  template<class VIC> class VarImp;


  /*
   * Variable implementations
   *
   */

  /**
   * \brief Base-class for variable implementations
   *
   * Serves as base-class that can be used without having to know any
   * template arguments.
   * \ingroup TaskVar
   */
  class VarImpBase {};

  /**
   * \brief Base class for %Variable type disposer
   *
   * Controls disposal of variable implementations.
   * \ingroup TaskVar
   */
  class GECODE_VTABLE_EXPORT VarImpDisposerBase {
  public:
    /// Dispose list of variable implementations starting at \a x
    GECODE_KERNEL_EXPORT virtual void dispose(Space& home, VarImpBase* x);
    /// Destructor (not used)
    GECODE_KERNEL_EXPORT virtual ~VarImpDisposerBase(void);
  };

  /**
   * \brief %Variable implementation disposer
   *
   * Controls disposal of variable implementations.
   * \ingroup TaskVar
   */
  template<class VarImp>
  class VarImpDisposer : public VarImpDisposerBase {
  public:
    /// Constructor (registers disposer with kernel)
    VarImpDisposer(void);
    /// Dispose list of variable implementations starting at \a x
    virtual void dispose(Space& home, VarImpBase* x);
  };

  /// Generic domain change information to be supplied to advisors
  class Delta {
    template<class VIC> friend class VarImp;
  private:
    /// Modification event
    ModEvent me;
  };

  /**
   * \brief Base-class for variable implementations
   *
   * Implements variable implementation for variable implementation
   * configuration of type \a VIC.
   * \ingroup TaskVar
   */
  template<class VIC>
  class VarImp : public VarImpBase {
    friend class Space;
    friend class Propagator;
    template<class VarImp> friend class VarImpDisposer;
  private:
    union {
      /**
       * \brief Subscribed actors
       *
       * The base pointer of the array of subscribed actors.
       *
       * This pointer must be first to avoid padding on 64 bit machines.
       */
      ActorLink** base;
      /**
       * \brief Forwarding pointer
       *
       * During cloning, this is used as the forwarding pointer for the
       * variable. The original value is saved in the copy and restored after
       * cloning.
       *
       */
      VarImp<VIC>* fwd;
    } b;

    /// Index for update
    static const int idx_c = VIC::idx_c;
    /// Index for disposal
    static const int idx_d = VIC::idx_d;
    /// Number of freely available bits
    static const int free_bits = VIC::free_bits;
    /// Number of used subscription entries
    unsigned int entries;
    /// Number of free subscription entries
    unsigned int free_and_bits;
    /// Maximal propagation condition
    static const Gecode::PropCond pc_max = VIC::pc_max;

    union {
      /**
       * \brief Indices of subscribed actors
       *
       * The entries from base[0] to base[idx[pc_max]] are propagators,
       * where the entries between base[idx[pc-1]] and base[idx[pc]] are
       * the propagators that have subscribed with propagation condition pc.
       *
       * The entries between base[idx[pc_max]] and base[idx[pc_max+1]] are the
       * advisors subscribed to the variable implementation.
       */
      unsigned int idx[pc_max+1];
      /// During cloning, points to the next copied variable
      VarImp<VIC>* next;
    } u;

    /// Return subscribed actor at index \a pc
    ActorLink** actor(PropCond pc);
    /// Return subscribed actor at index \a pc, where \a pc is non-zero
    ActorLink** actorNonZero(PropCond pc);
    /// Return reference to index \a pc, where \a pc is non-zero
    unsigned int& idx(PropCond pc);
    /// Return index \a pc, where \a pc is non-zero
    unsigned int idx(PropCond pc) const;

    /**
     * \brief Update copied variable \a x
     *
     * The argument \a sub gives the memory area where subscriptions are
     * to be stored.
     */
    void update(VarImp* x, ActorLink**& sub);
    /**
     * \brief Update all copied variables of this type
     *
     * The argument \a sub gives the memory area where subscriptions are
     * to be stored.
     */
    static void update(Space& home, ActorLink**& sub);

    /// Enter propagator to subscription array
    void enter(Space& home, Propagator* p, PropCond pc);
    /// Enter advisor to subscription array
    void enter(Space& home, Advisor* a);
    /// Resize subscription array
    void resize(Space& home);
    /// Remove propagator from subscription array
    void remove(Space& home, Propagator* p, PropCond pc);
    /// Remove advisor from subscription array
    void remove(Space& home, Advisor* a);


  protected:
    /// Cancel all subscriptions when variable implementation is assigned
    void cancel(Space& home);
    /**
     * \brief Run advisors when variable implementation has been modified with modification event \a me and domain change \a d
     *
     * Returns false if an advisor has failed.
     */
    bool advise(Space& home, ModEvent me, Delta& d);
#ifdef GECODE_HAS_VAR_DISPOSE
    /// Return reference to variables (dispose)
    static VarImp<VIC>* vars_d(Space& home);
    /// %Set reference to variables (dispose)
    static void vars_d(Space& home, VarImp<VIC>* x);
#endif

  public:
    /// Creation
    VarImp(Space& home);
    /// Creation of static instances
    VarImp(void);

    /// \name Dependencies
    //@{
    /** \brief Subscribe propagator \a p with propagation condition \a pc
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     *
     * In case the variable is assigned (that is, \a assigned is
     * true), the subscribing propagator is scheduled for execution.
     * Otherwise, the propagator subscribes and is scheduled for execution
     * with modification event \a me provided that \a pc is different
     * from \a PC_GEN_ASSIGNED.
     */
    void subscribe(Space& home, Propagator& p, PropCond pc,
                   bool assigned, ModEvent me, bool schedule);
    /** \brief Cancel subscription of propagator \a p with propagation condition \a pc
     *
     * If the variable is assigned, \a assigned must be true.
     *
     */
    void cancel(Space& home, Propagator& p, PropCond pc,
                bool assigned);
    /** \brief Subscribe advisor \a a to variable
     *
     * The advisor \a a is only subscribed if \a assigned is false.
     *
     */
    void subscribe(Space& home, Advisor& a, bool assigned);
    /** \brief Cancel subscription of advisor \a a
     *
     * If the variable is assigned, \a assigned must be true.
     *
     */
    void cancel(Space& home, Advisor& a, bool assigned);

    /**
     * \brief Return degree (number of subscribed propagators and advisors)
     *
     * Note that the degree of a variable implementation is not available
     * during cloning.
     */
    unsigned int degree(void) const;
    /**
     * \brief Return accumulated failure count (plus degree)
     *
     * Note that the accumulated failure count of a variable implementation
     * is not available during cloning.
     */
    double afc(const Space& home) const;
    //@}

    /// \name Cloning variables
    //@{
    /// Constructor for cloning
    VarImp(Space& home, bool share, VarImp& x);
    /// Is variable already copied
    bool copied(void) const;
    /// Use forward pointer if variable already copied
    VarImp* forward(void) const;
    /// Return next copied variable
    VarImp* next(void) const;
    //@}

    /// \name Variable implementation-dependent propagator support
    //@{
    /**
     * \brief Schedule propagator \a p with modification event \a me
     *
     * If \a force is true, the propagator is re-scheduled (including
     * cost computation) even though its modification event delta has
     * not changed.
     */
    static void schedule(Space& home, Propagator& p, ModEvent me,
                         bool force = false);
    /// Project modification event for this variable type from \a med
    static ModEvent me(const ModEventDelta& med);
    /// Translate modification event \a me into modification event delta
    static ModEventDelta med(ModEvent me);
    /// Combine modifications events \a me1 and \a me2
    static ModEvent me_combine(ModEvent me1, ModEvent me2);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return modification event
    static ModEvent modevent(const Delta& d);
    //@}

    /// \name Bit management
    //@{
    /// Provide access to free bits
    unsigned int bits(void) const;
    /// Provide access to free bits
    unsigned int& bits(void);
    //@}

  protected:
    /// Schedule subscribed propagators
    void schedule(Space& home, PropCond pc1, PropCond pc2, ModEvent me);

  public:
    /// \name Memory management
    //@{
    /// Allocate memory from space
    static void* operator new(size_t,Space&);
    /// Return memory to space
    static void  operator delete(void*,Space&);
    /// Needed for exceptions
    static void  operator delete(void*);
    //@}
  };

  /**
   * \defgroup TaskActorStatus Status of constraint propagation and branching commit
   * Note that the enum values starting with a double underscore should not
   * be used directly. Instead, use the provided functions with the same
   * name without leading underscores.
   *
   * \ingroup TaskActor
   */
  enum ExecStatus {
    __ES_SUBSUMED       = -2, ///< Internal: propagator is subsumed, do not use
    ES_FAILED           = -1, ///< Execution has resulted in failure
    ES_NOFIX            =  0, ///< Propagation has not computed fixpoint
    ES_OK               =  0, ///< Execution is okay
    ES_FIX              =  1, ///< Propagation has computed fixpoint
    ES_NOFIX_FORCE      =  2, ///< Advisor forces rescheduling of propagator 
    __ES_PARTIAL        =  2  ///< Internal: propagator has computed partial fixpoint, do not use
  };

  /**
   * \brief Propagation cost
   * \ingroup TaskActor
   */
  class PropCost {
    friend class Space;
  public:
    /// The actual cost values that are used
    enum ActualCost {
      AC_CRAZY_LO     = 0, ///< Exponential complexity, cheap
      AC_CRAZY_HI     = 0, ///< Exponential complexity, expensive
      AC_CUBIC_LO     = 1, ///< Cubic complexity, cheap
      AC_CUBIC_HI     = 1, ///< Cubic complexity, expensive
      AC_QUADRATIC_LO = 2, ///< Quadratic complexity, cheap
      AC_QUADRATIC_HI = 2, ///< Quadratic complexity, expensive
      AC_LINEAR_HI    = 3, ///< Linear complexity, expensive
      AC_LINEAR_LO    = 4, ///< Linear complexity, cheap
      AC_TERNARY_HI   = 4, ///< Three variables, expensive
      AC_BINARY_HI    = 5, ///< Two variables, expensive
      AC_TERNARY_LO   = 5, ///< Three variables, cheap
      AC_BINARY_LO    = 6, ///< Two variables, cheap
      AC_UNARY_LO     = 6, ///< Only single variable, cheap
      AC_UNARY_HI     = 6, ///< Only single variable, expensive
      AC_MAX          = 6  ///< Maximal cost value
    };
    /// Actual cost
    ActualCost ac;
  public:
    /// Propagation cost modifier
    enum Mod {
      LO, ///< Cheap
      HI  ///< Expensive
    };
  private:
    /// Compute dynamic cost for cost \a lo, expensive cost \a hi, and size measure \a n
    static PropCost cost(Mod m, ActualCost lo, ActualCost hi, unsigned int n);
    /// Constructor for automatic coercion of \a ac
    PropCost(ActualCost ac);
  public:
    /// Exponential complexity for modifier \a m and size measure \a n
    static PropCost crazy(PropCost::Mod m, unsigned int n);
    /// Exponential complexity for modifier \a m and size measure \a n
    static PropCost crazy(PropCost::Mod m, int n);
    /// Cubic complexity for modifier \a m and size measure \a n
    static PropCost cubic(PropCost::Mod m, unsigned int n);
    /// Cubic complexity for modifier \a m and size measure \a n
    static PropCost cubic(PropCost::Mod m, int n);
    /// Quadratic complexity for modifier \a m and size measure \a n
    static PropCost quadratic(PropCost::Mod m, unsigned int n);
    /// Quadratic complexity for modifier \a m and size measure \a n
    static PropCost quadratic(PropCost::Mod m, int n);
    /// Linear complexity for modifier \a pcm and size measure \a n
    static PropCost linear(PropCost::Mod m, unsigned int n);
    /// Linear complexity for modifier \a pcm and size measure \a n
    static PropCost linear(PropCost::Mod m, int n);
    /// Three variables for modifier \a pcm
    static PropCost ternary(PropCost::Mod m);
    /// Two variables for modifier \a pcm
    static PropCost binary(PropCost::Mod m);
    /// Single variable for modifier \a pcm
    static PropCost unary(PropCost::Mod m);
  };


  /**
   * \brief Actor properties
   * \ingroup TaskActor
   */
  enum ActorProperty {
    /**
     * \brief Actor must always be disposed
     *
     * Normally, a propagator will not be disposed if its home space
     * is deleted. However, if an actor uses external resources,
     * this property can be used to make sure that the actor
     * will always be disposed.
     */
    AP_DISPOSE = (1 << 0),
    /**
     * Propagator is only weakly monotonic, that is, the propagator
     * is only monotonic on assignments.
     *
     */
    AP_WEAKLY  = (1 << 1)
  };


  /**
   * \brief Double-linked list for actors
   *
   * Used to maintain which actors belong to a space and also
   * (for propagators) to organize actors in the queue of
   * waiting propagators.
   */
  class ActorLink {
    friend class Actor;
    friend class Propagator;
    friend class Advisor;
    friend class Brancher;
    friend class LocalObject;
    friend class Space;
    template<class VIC> friend class VarImp;
  private:
    ActorLink* _next; ActorLink* _prev;
  public:
    //@{
    /// Routines for double-linked list
    ActorLink* prev(void) const; void prev(ActorLink*);
    ActorLink* next(void) const; void next(ActorLink*);
    ActorLink** next_ref(void);
    //@}

    /// Initialize links (self-linked)
    void init(void);
    /// Remove from predecessor and successor
    void unlink(void);
    /// Insert \a al directly after this
    void head(ActorLink* al);
    /// Insert \a al directly before this
    void tail(ActorLink* al);
    /// Test whether actor link is empty (points to itself)
    bool empty(void) const;
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    template<class T> static ActorLink* cast(T* a);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    template<class T> static const ActorLink* cast(const T* a);
  };


  /**
   * \brief Base-class for both propagators and branchers
   * \ingroup TaskActor
   */
  class GECODE_VTABLE_EXPORT Actor : private ActorLink {
    friend class ActorLink;
    friend class Space;
    friend class Propagator;
    friend class Advisor;
    friend class Brancher;
    friend class LocalObject;
    template<class VIC> friend class VarImp;
    template<class A> friend class Council;
  private:
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static Actor* cast(ActorLink* al);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static const Actor* cast(const ActorLink* al);
    /// Static member to test against during space cloning
    GECODE_KERNEL_EXPORT static Actor* sentinel;
  public:
    /// Create copy
    virtual Actor* copy(Space& home, bool share) = 0;

    /// \name Memory management
    //@{
    /// Delete actor and return its size
    GECODE_KERNEL_EXPORT
    virtual size_t dispose(Space& home);
    /// Allocate memory from space
    static void* operator new(size_t s, Space& home);
    /// No-op for exceptions
    static void  operator delete(void* p, Space& home);
  private:
#ifndef __GNUC__
    /// Not used (uses dispose instead)
    static void  operator delete(void* p);
#endif
    /// Not used
    static void* operator new(size_t s);
    //@}
#ifdef __GNUC__
  public:
    /// To avoid warnings from GCC
    GECODE_KERNEL_EXPORT virtual ~Actor(void);
    /// Not used (uses dispose instead)
    static void  operator delete(void* p);
#endif
  };



  /**
   * \brief %Home class for posting propagators
   */
  class Home {
  protected:
    /// The space where the propagator is to be posted
    Space& s;
    /// A propagator (possibly) that is currently being rewritten
    Propagator* p;
  public:
    /// \name Conversion
    //@{
    /// Initialize the home with space \a s and propagator \a p
    Home(Space& s, Propagator* p=NULL);
    /// Assignment operator
    Home& operator =(const Home& h);
    /// Retrieve the space of the home
    operator Space&(void);
    //@}
    /// \name Extended information
    //@{
    /// Return a home extended by propagator to be rewritten
    Home operator ()(Propagator& p);
    /// Return propagator (or NULL) for currently rewritten propagator
    Propagator* propagator(void) const;
    //@}
    /// \name Forwarding of common space operations
    //@{
    /// Check whether corresponding space is failed
    bool failed(void) const;
    /// Mark space as failed
    void fail(void);
    ///  Notice actor property
    void notice(Actor& a, ActorProperty p, bool duplicate=false);
    //@}
  };

  /**
   * \brief Base-class for propagators
   * \ingroup TaskActor
   */
  class GECODE_VTABLE_EXPORT Propagator : public Actor {
    friend class ActorLink;
    friend class Space;
    template<class VIC> friend class VarImp;
    friend class Advisor;
    template<class A> friend class Council;
  private:
    union {
      /// A set of modification events (used during propagation)
      ModEventDelta med;
      /// The size of the propagator (used during subsumption)
      size_t size;
      /// A list of advisors (used during cloning)
      Gecode::ActorLink* advisors;
    } u;
    /// A reference to a counter for afc
    GlobalAFC::Counter& gafc;
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static Propagator* cast(ActorLink* al);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static const Propagator* cast(const ActorLink* al);
  protected:
    /// Constructor for posting
    Propagator(Home home);
    /// Constructor for cloning \a p
    Propagator(Space& home, bool share, Propagator& p);
    /// Return forwarding pointer during copying
    Propagator* fwd(void) const;

  public:
    /// \name Propagation
    //@{
    /**
     * \brief Propagation function
     *
     * The propagation function must return an execution status as
     * follows:
     *  - ES_FAILED: the propagator has detected failure
     *  - ES_NOFIX: the propagator has done propagation
     *  - ES_FIX: the propagator has done propagation and has computed
     *    a fixpoint. That is, running the propagator immediately
     *    again will do nothing.
     *
     * Apart from the above values, a propagator can return
     * the result from calling one of the functions defined by a space:
     *  - ES_SUBSUMED: the propagator is subsumed and has been already
     *    deleted.
     *  - ES_NOFIX_PARTIAL: the propagator has consumed some of its
     *    propagation events.
     *  - ES_FIX_PARTIAL: the propagator has consumed some of its
     *    propagation events and with respect to these events is
     *    at fixpoint
     * For more details, see the individual functions.
     *
     */
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med) = 0;
    /// Cost function
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const = 0;
    /**
     * \brief Return the modification event delta
     * 
     * This function returns the modification event delta of the currently
     * executing propagator and hence can only be called within the
     * propagate function of a propagator.
     */
    ModEventDelta modeventdelta(void) const;
    /**
     * \brief Advise function
     *
     * The advisor is passed as argument \a a.
     *
     * A propagator must specialize this advise function, if it
     * uses advisors. The advise function must return an execution
     * status as follows:
     *  - ES_FAILED: the advisor has detected failure.
     *  - ES_FIX: the advisor's propagator (that is, this propagator)
     *    does not need to be run.
     *  - ES_NOFIX: the advisor's propagator (that is, this propagator)
     *    must be run.
     *  - ES_NOFIX_FORCE: the advisor's propagator (that is, this propagator)
     *    must be run and it must forcefully be rescheduled (including
     *    recomputation of cost).
     *
     * Apart from the above values, an advisor can return
     * the result from calling the function defined by a space:
     *  - ES_FIX_DISPOSE: the advisor's propagator does not need to be run
     *    and the advisor will be disposed.
     *  - ES_NOFIX_DISPOSE: the advisor's propagator must be run and the
     *    advisor will be disposed.
     *  - ES_NOFIX_FORCE_DISPOSE: the advisor's propagator must be run 
     *    , it must forcefully be rescheduled (including recomputation 
     *    of cost), and the adviser will be disposed.
     * For more details, see the function documentation.
     *
     * The delta \a d describes how the variable has been changed
     * by an operation on the advisor's variable. Typically,
     * the delta information can only be utilized by either
     * static or member functions of views as the actual delta
     * information is both domain and view dependent.
     *
     */
    GECODE_KERNEL_EXPORT
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    //@}
    /// \name Information
    //@{
    /// Return the accumlated failure count
    double afc(const Space& home) const;
    //@}
  };


  /**
   * \brief %Council of advisors
   *
   * If a propagator uses advisors, it must maintain its advisors
   * through a council.
   * \ingroup TaskActor
   */
  template<class A>
  class Council {
    friend class Advisor;
    friend class Advisors<A>;
  private:
    /// Starting point for a linked list of advisors
    mutable ActorLink* advisors;
  public:
    /// Default constructor
    Council(void);
    /// Construct advisor council
    Council(Space& home);
    /// Test whether council has advisor left
    bool empty(void) const;
    /// Update during cloning (copies all advisors)
    void update(Space& home, bool share, Council<A>& c);
    /// Dispose council
    void dispose(Space& home);
  };


  /**
   * \brief Class to iterate over advisors of a council
   * \ingroup TaskActor
   */
  template<class A>
  class Advisors {
  private:
    /// The current advisor
    ActorLink* a;
  public:
    /// Initialize
    Advisors(const Council<A>& c);
    /// Test whether there advisors left
    bool operator ()(void) const;
    /// Move iterator to next advisor
    void operator ++(void);
    /// Return advisor
    A& advisor(void) const;
  };


  /**
   * \brief Base-class for advisors
   *
   * Advisors are typically subclassed for each propagator that
   * wants to use advisors. The actual member function that
   * is executed when a variable is changed, must be implemented
   * by the advisor's propagator.
   *
   * \ingroup TaskActor
   */
  class Advisor : private ActorLink {
    template<class VIC> friend class VarImp;
    template<class A> friend class Council;
    template<class A> friend class Advisors;
  private:
    /// Is the advisor disposed?
    bool disposed(void) const;
    /// Static cast
    static Advisor* cast(ActorLink* al);
    /// Static cast
    static const Advisor* cast(const ActorLink* al);
  protected:
    /// Return the advisor's propagator
    Propagator& propagator(void) const;
  public:
    /// Constructor for creation
    template<class A>
    Advisor(Space& home, Propagator& p, Council<A>& c);
    /// Copying constructor
    Advisor(Space& home, bool share, Advisor& a);

    /// \name Memory management
    //@{
    /// Dispose the advisor
    template<class A>
    void dispose(Space& home, Council<A>& c);
    /// Allocate memory from space
    static void* operator new(size_t s, Space& home);
    /// No-op for exceptions
    static void  operator delete(void* p, Space& home);
    //@}
  private:
#ifndef __GNUC__
    /// Not used (uses dispose instead)
    static void  operator delete(void* p);
#endif
    /// Not used
    static void* operator new(size_t s);
  };


  /**
   * \brief No-good literal recorded during search
   *
   */
  class GECODE_VTABLE_EXPORT NGL {
  private:
    /// Combines pointer to next literal and mark whether it is a leaf
    void* nl;
  public:
    /// The status of a no-good literal
    enum Status {
      FAILED,   ///< The literal is failed
      SUBSUMED, ///< The literal is subsumed
      NONE      ///< The literal is neither failed nor subsumed
    };
    /// Constructor for creation
    NGL(void);
    /// Constructor for creation
    NGL(Space& home);
    /// Constructor for cloning \a ngl
    NGL(Space& home, bool share, NGL& ngl);
    /// Subscribe propagator \a p to all views of the no-good literal
    virtual void subscribe(Space& home, Propagator& p) = 0;
    /// Cancel propagator \a p from all views of the no-good literal
    virtual void cancel(Space& home, Propagator& p) = 0;
    /// Test the status of the no-good literal
    virtual NGL::Status status(const Space& home) const = 0;
    /// Propagate the negation of the no-good literal
    virtual ExecStatus prune(Space& home) = 0;
    /// Create copy
    virtual NGL* copy(Space& home, bool share) = 0;
    /// Whether dispose must always be called (returns false)
    GECODE_KERNEL_EXPORT
    virtual bool notice(void) const;
    /// Dispose
    virtual size_t dispose(Space& home);
    /// \name Internal management routines
    //@{
    /// Test whether literal is a leaf
    bool leaf(void) const;
    /// Return pointer to next literal
    NGL* next(void) const;
    /// Mark literal as leaf or not
    void leaf(bool l);
    /// %Set pointer to next literal
    void next(NGL* n);
    /// Add node \a n and mark it as leaf \a l and return \a n
    NGL* add(NGL* n, bool l);
    //@}
    /// \name Memory management
    //@{
    /// Allocate memory from space
    static void* operator new(size_t s, Space& home);
    /// Return memory to space
    static void  operator delete(void* s, Space& home);
    /// Needed for exceptions
    static void  operator delete(void* p);
    //@}
  };

  /**
   * \brief %Choice for performing commit
   *
   * Must be refined by inheritance such that the information stored
   * inside a choice is sufficient to redo a commit performed by a
   * particular brancher.
   *
   * \ingroup TaskActor
   */
  class GECODE_VTABLE_EXPORT Choice {
    friend class Space;
  private:
    unsigned int _id;  ///< Identity to match creating brancher
    unsigned int _alt; ///< Number of alternatives

    /// Return id of the creating brancher
    unsigned int id(void) const;
  protected:
    /// Initialize for particular brancher \a b and alternatives \a a
    Choice(const Brancher& b, const unsigned int a);
  public:
    /// Return number of alternatives
    unsigned int alternatives(void) const;
    /// Destructor
    GECODE_KERNEL_EXPORT virtual ~Choice(void);
    /// Report size occupied by choice
    virtual size_t size(void) const = 0;
    /// Allocate memory from heap
    static void* operator new(size_t);
    /// Return memory to heap
    static void  operator delete(void*);
    /// Archive into \a e
    GECODE_KERNEL_EXPORT virtual void archive(Archive& e) const;
  };

  /**
   * \brief Base-class for branchers
   *
   * Note that branchers cannot be created inside a propagator
   * (no idea why one would like to do that anyway). If you do that
   * the system will explode in a truly interesting way.
   *
   * \ingroup TaskActor
   */
  class GECODE_VTABLE_EXPORT Brancher : public Actor {
    friend class ActorLink;
    friend class Space;
    friend class Choice;
  private:
    /// Unique identity
    unsigned int _id;
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static Brancher* cast(ActorLink* al);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static const Brancher* cast(const ActorLink* al);
  protected:
    /// Constructor for creation
    Brancher(Home home);
    /// Constructor for cloning \a b
    Brancher(Space& home, bool share, Brancher& b);
  public:
    /// \name Brancher
    //@{
    /// Return unsigned brancher id
    unsigned int id(void) const;
    /**
     * \brief Check status of brancher, return true if alternatives left
     *
     * This method is called when Space::status is called, it determines
     * whether to continue branching with this brancher or move on to
     * the (possibly) next brancher.
     *
     */
    virtual bool status(const Space& home) const = 0;
    /**
     * \brief Return choice
     *
     * Note that this method relies on the fact that it is called
     * immediately after a previous call to status. Moreover, the
     * member function can only be called once.
     */
    virtual const Choice* choice(Space& home) = 0;
    /// Return choice from \a e
    virtual const Choice* choice(const Space& home, Archive& e) = 0;
    /**
     * \brief Commit for choice \a c and alternative \a a
     *
     * The current brancher in the space \a home performs a commit from
     * the information provided by the choice \a c and the alternative \a a.
     */
    virtual ExecStatus commit(Space& home, const Choice& c, 
                              unsigned int a) = 0;
    /**
     * \brief Create no-good literal for choice \a c and alternative \a a
     *
     * The current brancher in the space \a home creates a no-good literal
     * from the information provided by the choice \a c and the alternative
     * \a a. The brancher has the following options:
     *  - it returns NULL for all alternatives \a a. This means that the
     *    brancher does not support no-good literals (default).
     *  - it returns NULL for the last alternative \a a. This means that
     *    this alternative is equivalent to the negation of the disjunction
     *    of all other alternatives.
     *
     */
    GECODE_KERNEL_EXPORT 
    virtual NGL* ngl(Space& home, const Choice& c, unsigned int a) const;
    /**
     * \brief Print branch for choice \a c and alternative \a a
     *
     * Prints an explanation of the alternative \a a of choice \a c
     * on the stream \a o.
     *
     * The default function prints nothing.
     *
     */
    GECODE_KERNEL_EXPORT 
    virtual void print(const Space& home, const Choice& c, unsigned int a,
                       std::ostream& o) const;
    //@}
  };

  /**
   * \brief Handle for brancher
   *
   * Supports few operations on a brancher, in particular to kill
   * a brancher.
   *
   * \ingroup TaskActor
   */
  class BrancherHandle {
  private:
    /// Id of the brancher
    unsigned int _id;
  public:
    /// Create handle as unitialized
    BrancherHandle(void);
    /// Create handle for brancher \a b
    BrancherHandle(const Brancher& b);
    /// Update during cloning
    void update(Space& home, bool share, BrancherHandle& bh);
    /// Return brancher id
    unsigned int id(void) const;
    /// Check whether brancher is still active
    bool operator ()(const Space& home) const;
    /// Kill the brancher
    void kill(Space& home);
  };

  /**
   * \brief Local (space-shared) object
   *
   * Local objects must inherit from this base class.
   *
   * \ingroup FuncSupportShared
   */
  class LocalObject : public Actor {
    friend class ActorLink;
    friend class Space;
    friend class LocalHandle;
  protected:
    /// Constructor for creation
    LocalObject(Home home);
    /// Copy constructor
    LocalObject(Space& home, bool share, LocalObject& l);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static LocalObject* cast(ActorLink* al);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static const LocalObject* cast(const ActorLink* al);
  private:
    /// Make copy and set forwarding pointer
    GECODE_KERNEL_EXPORT void fwdcopy(Space& home, bool share);
  public:
    /// Return forwarding pointer
    LocalObject* fwd(Space& home, bool share);
  };

  /**
   * \brief Handles for local (space-shared) objects
   *
   * \ingroup FuncSupportShared
   *
   */
  class LocalHandle {
  private:
    /// The local object
    LocalObject* o;
  protected:
    /// Create local handle pointing to NULL object
    LocalHandle(void);
    /// Create local handle that points to local object \a lo
    LocalHandle(LocalObject* lo);
    /// Copy constructor
    LocalHandle(const LocalHandle& lh);
  public:
    /// Assignment operator
    LocalHandle& operator =(const LocalHandle& lh);
    /// Updating during cloning
    void update(Space& home, bool share, LocalHandle& lh);
    /// Destructor
    ~LocalHandle(void);
  protected:
    /// Access to the local object
    LocalObject* object(void) const;
    /// Modify local object
    void object(LocalObject* n);
  };


  /**
   * \brief No-goods recorded from restarts
   *
   */
  class GECODE_VTABLE_EXPORT NoGoods {
  protected:
    /// Number of no-goods
    unsigned long int n;
  public:
    /// Initialize
    NoGoods(void);
    /// Post no-goods
    GECODE_KERNEL_EXPORT 
    virtual void post(Space& home);
    /// Return number of no-goods posted
    unsigned long int ng(void) const;
    /// %Set number of no-goods posted to \a n
    void ng(unsigned long int n);
    /// Destructor
    virtual ~NoGoods(void);
  };


  /**
   * \brief %Space status
   * \ingroup TaskSearch
   */
  enum SpaceStatus {
    SS_FAILED, ///< %Space is failed
    SS_SOLVED, ///< %Space is solved (no brancher left)
    SS_BRANCH  ///< %Space must be branched (at least one brancher left)
  };

  /**
   * \brief %Statistics for execution of status
   *
   */
  class StatusStatistics {
  public:
    /// Number of propagator executions
    unsigned long int propagate;
    /// Whether a weakly monotonic propagator might have been executed
    bool wmp;
    /// Initialize
    StatusStatistics(void);
    /// Reset information
    void reset(void);
    /// Return sum with \a s
    StatusStatistics operator +(const StatusStatistics& s);
    /// Increment by statistics \a s
    StatusStatistics& operator +=(const StatusStatistics& s);
  };

  /**
   * \brief %Statistics for execution of clone
   *
   */
  class CloneStatistics {
  public:
    /// Initialize
    CloneStatistics(void);
    /// Reset information
    void reset(void);
    /// Return sum with \a s
    CloneStatistics operator +(const CloneStatistics& s);
    /// Increment by statistics \a s
    CloneStatistics& operator +=(const CloneStatistics& s);
  };

  /**
   * \brief %Statistics for execution of commit
   *
   */
  class CommitStatistics {
  public:
    /// Initialize
    CommitStatistics(void);
    /// Reset information
    void reset(void);
    /// Return sum with \a s
    CommitStatistics operator +(const CommitStatistics& s);
    /// Increment by statistics \a s
    CommitStatistics& operator +=(const CommitStatistics& s);
  };


  /**
   * \brief Computation spaces
   */
  class GECODE_VTABLE_EXPORT Space {
    friend class Actor;
    friend class Propagator;
    friend class Brancher;
    friend class Advisor;
    template<class VIC> friend class VarImp;
    template<class VarImp> friend class VarImpDisposer;
    friend class SharedHandle;
    friend class LocalObject;
    friend class Region;
    friend class AFC;
    friend class BrancherHandle;
  private:
    /// Manager for shared memory areas
    SharedMemory* sm;
    /// Performs memory management for space
    MemoryManager mm;
    /// Global AFC information
    GlobalAFC gafc;
    /// Doubly linked list of all propagators
    ActorLink pl;
    /// Doubly linked list of all branchers
    ActorLink bl;
    /**
     * \brief Points to the first brancher to be used for status
     *
     * If equal to &bl, no brancher does exist.
     */
    Brancher* b_status;
    /**
     * \brief Points to the first brancher to be used for commit
     *
     * Note that \a b_commit can point to an earlier brancher
     * than \a b_status. This reflects the fact that the earlier
     * brancher is already done (that is, status on that brancher
     * returns false) but there might be still choices
     * referring to the earlier brancher.
     *
     * If equal to &bl, no brancher does exist.
     */
    Brancher* b_commit;
    /// Find brancher with identity \a id
    Brancher* brancher(unsigned int id);
    /// Kill brancher with identity \a id
    GECODE_KERNEL_EXPORT
    void kill_brancher(unsigned int id);
    /// Reserved brancher id (never created)
    static const unsigned reserved_branch_id = 0U;
    union {
      /// Data only available during propagation
      struct {
        /**
         * \brief Cost level with next propagator to be executed
         *
         * This maintains the following invariant (but only if the
         * space does not perform propagation):
         *  - If active points to a queue, this queue might contain
         *    a propagator. However, there will be at least one queue
         *    containing a propagator.
         *  - Otherwise, active is smaller than the first queue
         *    or larger than the last queue. Then, the space is stable.
         *  - If active is larger than the last queue, the space is failed.
         */
        ActorLink* active;
        /// Scheduled propagators according to cost
        ActorLink queue[PropCost::AC_MAX+1];
        /// Id of next brancher to be created
        unsigned int branch_id;
        /// Number of subscriptions
        unsigned int n_sub;
      } p;
      /// Data available only during copying
      struct {
        /// Entries for updating variables
        VarImpBase* vars_u[AllVarConf::idx_c];
        /// Keep variables during copying without index structure
        VarImpBase* vars_noidx;
        /// Linked list of shared objects
        SharedHandle::Object* shared;
        /// Linked list of local objects
        LocalObject* local;
      } c;
    } pc;
    /// Put propagator \a p into right queue
    void enqueue(Propagator* p);
    /**
     * \name update, and dispose variables
     */
    //@{
#ifdef GECODE_HAS_VAR_DISPOSE
    /// Registered variable type disposers
    GECODE_KERNEL_EXPORT static VarImpDisposerBase* vd[AllVarConf::idx_d];
    /// Entries for disposing variables
    VarImpBase* _vars_d[AllVarConf::idx_d];
    /// Return reference to variables (dispose)
    template<class VIC> VarImpBase* vars_d(void) const;
    /// %Set reference to variables (dispose)
    template<class VIC> void vars_d(VarImpBase* x);
#endif
    /// Update all cloned variables
    void update(ActorLink** sub);
    //@}

    /// First actor for forced disposal
    Actor** d_fst;
    /// Current actor for forced disposal
    Actor** d_cur;
    /// Last actor for forced disposal
    Actor** d_lst;

    /**
     * \brief Number of weakly monotonic propagators and AFC flag
     *
     * The least significant bit encodes whether AFC information
     * must be collected, the remaining bits encode counting for
     * weakly monotonic propagators as follows. If zero, none
     * exists. If one, then none exists right now but there has
     * been one since the last fixpoint computed. Otherwise, it
     * gives the number of weakly monotoning propagators minus
     * one.
     */
    unsigned int _wmp_afc;
    /// %Set that AFC information must be recorded
    void afc_enable(void);
    /// Whether AFC information must be recorded
    bool afc_enabled(void) const;
    /// %Set number of wmp propagators to \a n
    void wmp(unsigned int n);
    /// Return number of wmp propagators
    unsigned int wmp(void) const;

    /// Used for default argument
    GECODE_KERNEL_EXPORT static StatusStatistics unused_status;
    /// Used for default argument
    GECODE_KERNEL_EXPORT static CloneStatistics unused_clone;
    /// Used for default argument
    GECODE_KERNEL_EXPORT static CommitStatistics unused_commit;

    /**
     * \brief Clone space
     *
     * Assumes that the space is stable and not failed. If the space is
     * failed, an exception of type SpaceFailed is thrown. If the space
     * is not stable, an exception of SpaceNotStable is thrown.
     *
     * Otherwise, a clone of the space is returned. If \a share is true,
     * sharable datastructures are shared among the clone and the original
     * space. If \a share is false, independent copies of the shared
     * datastructures must be created. This means that a clone with no
     * sharing can be used in a different thread without any interaction
     * with the original space.
     *
     * Throws an exception of type SpaceNotCloned when the copy constructor
     * of the Space class is not invoked during cloning.
     *
     */
    GECODE_KERNEL_EXPORT Space* _clone(bool share=true);

    /**
     * \brief Commit choice \a c for alternative \a a
     *
     * The current brancher in the space performs a commit from
     * the information provided by the choice \a c
     * and the alternative \a a. The statistics information \a stat is
     * updated.
     *
     * Note that no propagation is perfomed (to support path
     * recomputation), in order to perform propagation the member
     * function status must be used.
     *
     * Committing with choices must be carried
     * out in the same order as the choices have been
     * obtained by the member function Space::choice().
     *
     * It is perfectly okay to add constraints interleaved with
     * choices (provided they are in the right order).
     * However, if propagation is performed by calling the member
     * function status and then new choices are
     * computed, these choices are different.
     *
     * Only choices can be used that are up-to-date in the following
     * sense: if a new choice is created (via the choice member
     * function), no older choices can be used.
     *
     * Committing throws the following exceptions:
     *  - SpaceNoBrancher, if the space has no current brancher (it is
     *    already solved).
     *  - SpaceIllegalAlternative, if \a a is not smaller than the number
     *    of alternatives supported by the choice \a c.
     */
    GECODE_KERNEL_EXPORT
    void _commit(const Choice& c, unsigned int a);

    /**
     * \brief Commit choice \a c for alternative \a a if possible
     *
     * The current brancher in the space performs a commit from
     * the information provided by the choice \a c
     * and the alternative \a a. The statistics information \a stat is
     * updated.
     *
     * Note that no propagation is perfomed (to support path
     * recomputation), in order to perform propagation the member
     * function status must be used.
     *
     * Committing with choices must be carried
     * out in the same order as the choices have been
     * obtained by the member function Space::choice().
     *
     * It is perfectly okay to add constraints interleaved with
     * choices (provided they are in the right order).
     * However, if propagation is performed by calling the member
     * function status and then new choices are
     * computed, these choices are different.
     *
     * Only choices can be used that are up-to-date in the following
     * sense: if a new choice is created (via the choice member
     * function), no older choices can be used.
     *
     * Committing throws the following exceptions:
     *  - SpaceIllegalAlternative, if \a a is not smaller than the number
     *    of alternatives supported by the choice \a c.
     */
    GECODE_KERNEL_EXPORT
    void _trycommit(const Choice& c, unsigned int a);

  public:
    /**
     * \brief Default constructor
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT Space(void);
    /**
     * \brief Destructor
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT virtual ~Space(void);
    /**
     * \brief Constructor for cloning
     *
     * Must copy and update all data structures (such as variables
     * and variable arrays) required by the subclass of Space.
     *
     * If \a share is true, share all data structures among copies.
     * Otherwise, make independent copies.
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT Space(bool share, Space& s);
    /**
     * \brief Copying member function
     *
     * Must create a new object using the constructor for cloning.
     * \ingroup TaskModelScript
     */
    virtual Space* copy(bool share) = 0;
    /**
     * \brief Constrain function for best solution search
     *
     * Must constrain this space to be better than the so far best
     * solution \a best.
     *
     * The default function does nothing.
     *
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT virtual void constrain(const Space& best);
    /**
     * \brief Master configuration function for restart meta search engine
     *
     * A restart meta search engine calls this function on its
     * master space whenever it finds a solution or exploration
     * restarts. \a i is the number of the restart. \a s is 
     * either the solution space or NULL. \a ng are nogoods recorded
     * from the last restart (only if \a s is not a solution).
     *
     * The default function does nothing.
     *
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT 
    virtual void master(unsigned long int i, const Space* s,
                        NoGoods& ng);
    /**
     * \brief Slave configuration function for restart meta search engine
     *
     * A restart meta search engine calls this function on its
     * slave space whenever it finds a solution or exploration
     * restarts. \a i is the number of the restart. \a s is 
     * either the solution space or NULL.
     *
     * The default function does nothing.
     *
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT 
    virtual void slave(unsigned long int i, const Space* s);
    /**
     * \brief Allocate memory from heap for new space
     * \ingroup TaskModelScript
     */
    static void* operator new(size_t);
    /**
     * \brief Free memory allocated from heap
     * \ingroup TaskModelScript
     */
    static void  operator delete(void*);


    /*
     * Member functions for search engines
     *
     */

    /**
     * \brief Query space status
     *
     * Propagates the space until fixpoint or failure;
     * updates the statistics information \a stat; and:
     *  - if the space is failed, SpaceStatus::SS_FAILED is returned.
     *  - if the space is not failed but the space has no brancher left,
     *    SpaceStatus::SS_SOLVED is returned.
     *  - otherwise, SpaceStatus::SS_BRANCH is returned.
     * \ingroup TaskSearch
     */
    GECODE_KERNEL_EXPORT
    SpaceStatus status(StatusStatistics& stat=unused_status);

    /**
     * \brief Create new choice for current brancher
     *
     * This member function can only be called after the member function
     * Space::status on the same space has been called and in between
     * no non-const member function has been called on this space.
     *
     * Moreover, the member function can only be called at most once
     * (otherwise, it might generate conflicting choices).
     *
     * Note that the above invariant only pertains to calls of member
     * functions of the same space. If the invariant is violated, the
     * system is likely to crash (hopefully it does). In particular, if
     * applied to a space with no current brancher, the system will
     * crash.
     *
     * After a new choice has been created, no older choices
     * can be used on the space.
     *
     * If the status() member function has returned that the space has
     * no more branchers (that is, the result was either SS_FAILED or
     * SS_SOLVED), a call to choice() will return NULL and purge
     * all remaining branchers inside the space. This is interesting
     * for the case SS_SOLVED, where the call to choice can serve as
     * garbage collection.
     *
     * Throws an exception of type SpaceNotStable when applied to a not
     * yet stable space.
     *
     * \ingroup TaskSearch
     */
    GECODE_KERNEL_EXPORT
    const Choice* choice(void);

    /**
     * \brief Create new choice from \a e
     *
     * The archived representation \a e must have been created from
     * a Choice that is compatible with this space (i.e., created from
     * the same model).
     *
     * \ingroup TaskSearch
     */
    GECODE_KERNEL_EXPORT
    const Choice* choice(Archive& e) const;
  
    /**
     * \brief Clone space
     *
     * Assumes that the space is stable and not failed. If the space is
     * failed, an exception of type SpaceFailed is thrown. If the space
     * is not stable, an exception of SpaceNotStable is thrown.
     *
     * Otherwise, a clone of the space is returned and the statistics
     * information \a stat is updated. If \a share is true,
     * sharable datastructures are shared among the clone and the original
     * space. If \a share is false, independent copies of the shared
     * datastructures must be created. This means that a clone with no
     * sharing can be used in a different thread without any interaction
     * with the original space.
     *
     * Throws an exception of type SpaceNotCloned when the copy constructor
     * of the Space class is not invoked during cloning.
     *
     * \ingroup TaskSearch
     */
    Space* clone(bool share=true, CloneStatistics& stat=unused_clone) const;

    /**
     * \brief Commit choice \a c for alternative \a a
     *
     * The current brancher in the space performs a commit from
     * the information provided by the choice \a c
     * and the alternative \a a. The statistics information \a stat is
     * updated.
     *
     * Note that no propagation is perfomed (to support path
     * recomputation), in order to perform propagation the member
     * function status must be used.
     *
     * Committing with choices must be carried
     * out in the same order as the choices have been
     * obtained by the member function Space::choice().
     *
     * It is perfectly okay to add constraints interleaved with
     * choices (provided they are in the right order).
     * However, if propagation is performed by calling the member
     * function status and then new choices are
     * computed, these choices are different.
     *
     * Only choices can be used that are up-to-date in the following
     * sense: if a new choice is created (via the choice member
     * function), no older choices can be used.
     *
     * Committing throws the following exceptions:
     *  - SpaceNoBrancher, if the space has no current brancher (it is
     *    already solved).
     *  - SpaceIllegalAlternative, if \a a is not smaller than the number
     *    of alternatives supported by the choice \a c.
     *
     * \ingroup TaskSearch
     */
    void commit(const Choice& c, unsigned int a,
                CommitStatistics& stat=unused_commit);
    /**
     * \brief If possible, commit choice \a c for alternative \a a
     *
     * The current brancher in the space performs a commit from
     * the information provided by the choice \a c
     * and the alternative \a a. The statistics information \a stat is
     * updated.
     *
     * Note that no propagation is perfomed (to support path
     * recomputation), in order to perform propagation the member
     * function status must be used.
     *
     * Committing with choices must be carried
     * out in the same order as the choices have been
     * obtained by the member function Space::choice().
     *
     * It is perfectly okay to add constraints interleaved with
     * choices (provided they are in the right order).
     * However, if propagation is performed by calling the member
     * function status and then new choices are
     * computed, these choices are different.
     *
     * Only choices can be used that are up-to-date in the following
     * sense: if a new choice is created (via the choice member
     * function), no older choices can be used.
     *
     * Committing throws the following exceptions:
     *  - SpaceIllegalAlternative, if \a a is not smaller than the number
     *    of alternatives supported by the choice \a c.
     *
     * \ingroup TaskSearch
     */
    void trycommit(const Choice& c, unsigned int a,
                   CommitStatistics& stat=unused_commit);
    /**
     * \brief Create no-good literal for choice \a c and alternative \a a
     *
     * The current brancher in the space \a home creates a no-good literal
     * from the information provided by the choice \a c and the alternative
     * \a a. The brancher has the following options:
     *  - it returns NULL for all alternatives \a a. This means that the
     *    brancher does not support no-good literals.
     *  - it returns NULL for the last alternative \a a. This means that
     *    this alternative is equivalent to the negation of the disjunction
     *    of all other alternatives.
     *
     * It throws the following exceptions:
     *  - SpaceIllegalAlternative, if \a a is not smaller than the number
     *    of alternatives supported by the choice \a c.
     *
     * \ingroup TaskSearch
     */
    GECODE_KERNEL_EXPORT
    NGL* ngl(const Choice& c, unsigned int a);

    /**
     * \brief Print branch for choice \a c and alternative \a a
     *
     * Prints an explanation of the alternative \a a of choice \a c
     * on the stream \a o.
     *
     * Print throws the following exceptions:
     *  - SpaceNoBrancher, if the space has no current brancher (it is
     *    already solved).
     *  - SpaceIllegalAlternative, if \a a is not smaller than the number
     *    of alternatives supported by the choice \a c.
     *
     * \ingroup TaskSearch
     */
    GECODE_KERNEL_EXPORT
    void print(const Choice& c, unsigned int a, std::ostream& o) const;

    /**
     * \brief Notice actor property
     *
     * Make the space notice that the actor \a a has the property \a p.
     * Note that the same property can only be noticed once for an actor 
     * unless \a duplicate is true.
     * \ingroup TaskActor
     */
    GECODE_KERNEL_EXPORT
    void notice(Actor& a, ActorProperty p, bool duplicate=false);
    /**
     * \brief Ignore actor property
     *
     * Make the space ignore that the actor \a a has the property \a p.
     * Note that a property must be ignored before an actor is disposed.
     * \ingroup TaskActor
     */
    GECODE_KERNEL_EXPORT
    void ignore(Actor& a, ActorProperty p, bool duplicate=false);


    /**
     * \Brief %Propagator \a p is subsumed
     *
     * First disposes the propagator and then returns subsumption.
     *
     * \warning Has a side-effect on the propagator. Overwrites
     *          the modification event delta of a propagator.
     *          Use only directly with returning from propagation.
     * \ingroup TaskActorStatus
     */
    ExecStatus ES_SUBSUMED(Propagator& p);
    /**
     * \brief %Propagator \a p is subsumed
     *
     * The size of the propagator is \a s.
     *
     * Note that the propagator must be subsumed and also disposed. So
     * in general, there should be code such as
     * \code return ES_SUBSUMED_DISPOSE(home,*this,dispose(home)) \endcode.
     *
     * \warning Has a side-effect on the propagator. Overwrites
     *          the modification event delta of a propagator.
     *          Use only directly with returning from propagation.
     * \ingroup TaskActorStatus
     */
    ExecStatus ES_SUBSUMED_DISPOSED(Propagator& p, size_t s);
    /**
     * \brief %Propagator \a p has computed partial fixpoint
     *
     * %Set modification event delta to \a med and schedule propagator
     * accordingly.
     *
     * \warning Has a side-effect on the propagator.
     *          Use only directly with returning from propagation.
     * \ingroup TaskActorStatus
     */
    ExecStatus ES_FIX_PARTIAL(Propagator& p, const ModEventDelta& med);
    /**
     * \brief %Propagator \a p has not computed partial fixpoint
     *
     * Combine current modification event delta with \a and schedule
     * propagator accordingly.
     *
     * \warning Has a side-effect on the propagator.
     *          Use only directly with returning from propagation.
     * \ingroup TaskActorStatus
     */
    ExecStatus ES_NOFIX_PARTIAL(Propagator& p, const ModEventDelta& med);
    
    /**
     * \brief %Advisor \a a must be disposed
     *
     * Disposes the advisor and returns that the propagator of \a a 
     * need not be run.
     *
     * \warning Has a side-effect on the advisor. Use only directly when
     *          returning from advise.
     * \ingroup TaskActorStatus
     */
    template<class A>
    ExecStatus ES_FIX_DISPOSE(Council<A>& c, A& a);
    /**
     * \brief %Advisor \a a must be disposed and its propagator must be run
     *
     * Disposes the advisor and returns that the propagator of \a a 
     * must be run.
     *
     * \warning Has a side-effect on the advisor. Use only directly when
     *          returning from advise.
     * \ingroup TaskActorStatus
     */
    template<class A>
    ExecStatus ES_NOFIX_DISPOSE(Council<A>& c, A& a);
    /**
     * \brief %Advisor \a a must be disposed and its propagator must be forcefully rescheduled
     *
     * Disposes the advisor and returns that the propagator of \a a 
     * must be run and must be forcefully rescheduled (including 
     * recomputation of cost).
     *
     * \warning Has a side-effect on the advisor. Use only directly when
     *          returning from advise.
     * \ingroup TaskActorStatus
     */
    template<class A>
    ExecStatus ES_NOFIX_DISPOSE_FORCE(Council<A>& c, A& a);
    
    /**
     * \brief Fail space
     *
     * This is useful for failing outside of actors. Never use inside
     * a propagate or commit member function. The system will crash!
     * \ingroup TaskActor
     */
    void fail(void);
    /**
     * \brief Check whether space is failed
     *
     * Note that this does not perform propagation. This is useful
     * for posting actors: only if a space is not yet failed, new
     * actors are allowed to be created.
     * \ingroup TaskActor
     */
    bool failed(void) const;
    /**
     * \brief Return if space is stable (at fixpoint or failed)
     * \ingroup TaskActor
     */
    bool stable(void) const;
    /**
     * \brief Return number of propagators
     *
     * Note that this function takes linear time in the number of
     * propagators.
     */
    GECODE_KERNEL_EXPORT unsigned int propagators(void) const;
    /**
     * \brief Return number of branchers
     *
     * Note that this function takes linear time in the number of
     * branchers.
     */
    GECODE_KERNEL_EXPORT unsigned int branchers(void) const;

    /// \name Conversion from Space to Home
    //@{
    /// Return a home for this space with the information that \a p is being rewritten
    Home operator ()(Propagator& p);
    //@}

    /**
     * \defgroup FuncMemSpace Space-memory management
     * \ingroup FuncMem
     */
    //@{
    /**
     * \brief Allocate block of \a n objects of type \a T from space heap
     *
     * Note that this function implements C++ semantics: the default
     * constructor of \a T is run for all \a n objects.
     */
    template<class T>
    T* alloc(long unsigned int n);
    /**
     * \brief Allocate block of \a n objects of type \a T from space heap
     *
     * Note that this function implements C++ semantics: the default
     * constructor of \a T is run for all \a n objects.
     */
    template<class T>
    T* alloc(long int n);
    /**
     * \brief Allocate block of \a n objects of type \a T from space heap
     *
     * Note that this function implements C++ semantics: the default
     * constructor of \a T is run for all \a n objects.
     */
    template<class T>
    T* alloc(unsigned int n);
    /**
     * \brief Allocate block of \a n objects of type \a T from space heap
     *
     * Note that this function implements C++ semantics: the default
     * constructor of \a T is run for all \a n objects.
     */
    template<class T>
    T* alloc(int n);
    /**
     * \brief Delete \a n objects allocated from space heap starting at \a b
     *
     * Note that this function implements C++ semantics: the destructor
     * of \a T is run for all \a n objects.
     *
     * Note that the memory is not freed, it is just scheduled for later
     * reusal.
     */
    template<class T>
    void free(T* b, long unsigned int n);
    /**
     * \brief Delete \a n objects allocated from space heap starting at \a b
     *
     * Note that this function implements C++ semantics: the destructor
     * of \a T is run for all \a n objects.
     *
     * Note that the memory is not freed, it is just scheduled for later
     * reusal.
     */
    template<class T>
    void free(T* b, long int n);
    /**
     * \brief Delete \a n objects allocated from space heap starting at \a b
     *
     * Note that this function implements C++ semantics: the destructor
     * of \a T is run for all \a n objects.
     *
     * Note that the memory is not freed, it is just scheduled for later
     * reusal.
     */
    template<class T>
    void free(T* b, unsigned int n);
    /**
     * \brief Delete \a n objects allocated from space heap starting at \a b
     *
     * Note that this function implements C++ semantics: the destructor
     * of \a T is run for all \a n objects.
     *
     * Note that the memory is not freed, it is just scheduled for later
     * reusal.
     */
    template<class T>
    void free(T* b, int n);
    /**
     * \brief Reallocate block of \a n objects starting at \a b to \a m objects of type \a T from the space heap
     *
     * Note that this function implements C++ semantics: the copy constructor
     * of \a T is run for all \f$\min(n,m)\f$ objects, the default
     * constructor of \a T is run for all remaining
     * \f$\max(n,m)-\min(n,m)\f$ objects, and the destrucor of \a T is
     * run for all \a n objects in \a b.
     *
     * Returns the address of the new block.
     */
    template<class T>
    T* realloc(T* b, long unsigned int n, long unsigned int m);
    /**
     * \brief Reallocate block of \a n objects starting at \a b to \a m objects of type \a T from the space heap
     *
     * Note that this function implements C++ semantics: the copy constructor
     * of \a T is run for all \f$\min(n,m)\f$ objects, the default
     * constructor of \a T is run for all remaining
     * \f$\max(n,m)-\min(n,m)\f$ objects, and the destrucor of \a T is
     * run for all \a n objects in \a b.
     *
     * Returns the address of the new block.
     */
    template<class T>
    T* realloc(T* b, long int n, long int m);
    /**
     * \brief Reallocate block of \a n objects starting at \a b to \a m objects of type \a T from the space heap
     *
     * Note that this function implements C++ semantics: the copy constructor
     * of \a T is run for all \f$\min(n,m)\f$ objects, the default
     * constructor of \a T is run for all remaining
     * \f$\max(n,m)-\min(n,m)\f$ objects, and the destrucor of \a T is
     * run for all \a n objects in \a b.
     *
     * Returns the address of the new block.
     */
    template<class T>
    T* realloc(T* b, unsigned int n, unsigned int m);
    /**
     * \brief Reallocate block of \a n objects starting at \a b to \a m objects of type \a T from the space heap
     *
     * Note that this function implements C++ semantics: the copy constructor
     * of \a T is run for all \f$\min(n,m)\f$ objects, the default
     * constructor of \a T is run for all remaining
     * \f$\max(n,m)-\min(n,m)\f$ objects, and the destrucor of \a T is
     * run for all \a n objects in \a b.
     *
     * Returns the address of the new block.
     */
    template<class T>
    T* realloc(T* b, int n, int m);
    /**
     * \brief Reallocate block of \a n pointers starting at \a b to \a m objects of type \a T* from the space heap
     *
     * Returns the address of the new block.
     *
     * This is a specialization for performance.
     */
    template<class T>
    T** realloc(T** b, long unsigned int n, long unsigned int m);
    /**
     * \brief Reallocate block of \a n pointers starting at \a b to \a m objects of type \a T* from the space heap
     *
     * Returns the address of the new block.
     *
     * This is a specialization for performance.
     */
    template<class T>
    T** realloc(T** b, long int n, long int m);
    /**
     * \brief Reallocate block of \a n pointers starting at \a b to \a m objects of type \a T* from the space heap
     *
     * Returns the address of the new block.
     *
     * This is a specialization for performance.
     */
    template<class T>
    T** realloc(T** b, unsigned int n, unsigned int m);
    /**
     * \brief Reallocate block of \a n pointers starting at \a b to \a m objects of type \a T* from the space heap
     *
     * Returns the address of the new block.
     *
     * This is a specialization for performance.
     */
    template<class T>
    T** realloc(T** b, int n, int m);
    /// Allocate memory on space heap
    void* ralloc(size_t s);
    /// Free memory previously allocated with alloc (might be reused later)
    void rfree(void* p, size_t s);
    /// Reallocate memory block starting at \a b from size \a n to size \a s
    void* rrealloc(void* b, size_t n, size_t m);
    /// Allocate from freelist-managed memory
    template<size_t> void* fl_alloc(void);
    /**
     * \brief Return freelist-managed memory to freelist
     *
     * The first list element to be retuned is \a f, the last is \a l.
     */
    template<size_t> void  fl_dispose(FreeList* f, FreeList* l);
    /**
     * \brief Flush cached memory blocks
     *
     * All spaces that are obtained as non-shared clones from some same space
     * try to cache memory blocks from failed spaces. To minimize memory
     * consumption, these blocks can be flushed.
     *
     */
    GECODE_KERNEL_EXPORT void flush(void);
    //@}
    /// Construction routines
    //@{
    /**
     * \brief Constructs a single object of type \a T from space heap using the default constructor.
     */
    template<class T> 
    T& construct(void);
    /**
     * \brief Constructs a single object of type \a T from space heap using a unary constructor.
     *
     * The parameter is passed as a const reference.
     */
    template<class T, typename A1> 
    T& construct(A1 const& a1);
    /**
     * \brief Constructs a single object of type \a T from space heap using a binary constructor.
     *
     * The parameters are passed as const references.
     */
    template<class T, typename A1, typename A2> 
    T& construct(A1 const& a1, A2 const& a2);
    /**
     * \brief Constructs a single object of type \a T from space heap using a ternary constructor.
     *
     * The parameters are passed as const references.
     */
    template<class T, typename A1, typename A2, typename A3>
    T& construct(A1 const& a1, A2 const& a2, A3 const& a3);
    /**
     * \brief Constructs a single object of type \a T from space heap using a quaternary constructor.
     *
     * The parameters are passed as const references.
     */
    template<class T, typename A1, typename A2, typename A3, typename A4>
    T& construct(A1 const& a1, A2 const& a2, A3 const& a3, A4 const& a4);
    /**
     * \brief Constructs a single object of type \a T from space heap using a quinary constructor.
     *
     * The parameters are passed as const references.
     */
    template<class T, typename A1, typename A2, typename A3, typename A4, typename A5>
    T& construct(A1 const& a1, A2 const& a2, A3 const& a3, A4 const& a4, A5 const& a5);
    //@}

    /**
     * \brief Class to iterate over propagators of a space
     * 
     * Note that the iterator cannot be used during cloning.
     */
    class Propagators {
    private:
      /// current space
      const Space& home;
      /// current queue
      const ActorLink* q;
      /// current propagator
      const ActorLink* c;
      /// end mark
      const ActorLink* e;
    public:
      /// Initialize
      Propagators(const Space& home);
      /// Test whether there are propagators left
      bool operator ()(void) const;
      /// Move iterator to next propagator
      void operator ++(void);
      /// Return propagator
      const Propagator& propagator(void) const;
    };

    /**
     * \brief Class to iterate over branchers of a space
     * 
     * Note that the iterator cannot be used during cloning.
     */
    class Branchers {
    private:
      /// current brancher
      const ActorLink* c;
      /// end mark
      const ActorLink* e;
    public:
      /// Initialize
      Branchers(const Space& home);
      /// Test whether there are branchers left
      bool operator ()(void) const;
      /// Move iterator to next brancher
      void operator ++(void);
      /// Return propagator
      const Brancher& brancher(void) const;
    };    

    /// \name Low-level support for AFC
    //@{
    /// %Set AFC decay factor to \a d
    GECODE_KERNEL_EXPORT
    void afc_decay(double d);
    /// Return AFC decay factor
    double afc_decay(void) const;
    /// Reset AFC to \a a
    GECODE_KERNEL_EXPORT
    void afc_set(double a);
    //@}
  };




  /*
   * Memory management
   *
   */

  // Heap allocated
  forceinline void*
  SharedHandle::Object::operator new(size_t s) {
    return heap.ralloc(s);
  }
  forceinline void
  SharedHandle::Object::operator delete(void* p) {
    heap.rfree(p);
  }

  forceinline void*
  Space::operator new(size_t s) {
    return heap.ralloc(s);
  }
  forceinline void
  Space::operator delete(void* p) {
    heap.rfree(p);
  }

  forceinline void
  Choice::operator delete(void* p) {
    heap.rfree(p);
  }
  forceinline void*
  Choice::operator new(size_t s) {
    return heap.ralloc(s);
  }


  // Space allocation: general space heaps and free lists
  forceinline void*
  Space::ralloc(size_t s) {
    return mm.alloc(sm,s);
  }
  forceinline void
  Space::rfree(void* p, size_t s) {
    return mm.reuse(p,s);
  }
  forceinline void*
  Space::rrealloc(void* _b, size_t n, size_t m) {
    char* b = static_cast<char*>(_b);
    if (n < m) {
      char* p = static_cast<char*>(ralloc(m));
      memcpy(p,b,n);
      rfree(b,n);
      return p;
    } else {
      rfree(b+m,m-n);
      return b;
    }
  }

  template<size_t s>
  forceinline void*
  Space::fl_alloc(void) {
    return mm.template fl_alloc<s>(sm);
  }
  template<size_t s>
  forceinline void
  Space::fl_dispose(FreeList* f, FreeList* l) {
    mm.template fl_dispose<s>(f,l);
  }

  /*
   * Typed allocation routines
   *
   */
  template<class T>
  forceinline T*
  Space::alloc(long unsigned int n) {
    T* p = static_cast<T*>(ralloc(sizeof(T)*n));
    for (long unsigned int i=n; i--; )
      (void) new (p+i) T();
    return p;
  }
  template<class T>
  forceinline T*
  Space::alloc(long int n) {
    assert(n >= 0);
    return alloc<T>(static_cast<long unsigned int>(n));
  }
  template<class T>
  forceinline T*
  Space::alloc(unsigned int n) {
    return alloc<T>(static_cast<long unsigned int>(n));
  }
  template<class T>
  forceinline T*
  Space::alloc(int n) {
    assert(n >= 0);
    return alloc<T>(static_cast<long unsigned int>(n));
  }

  template<class T>
  forceinline void
  Space::free(T* b, long unsigned int n) {
    for (long unsigned int i=n; i--; )
      b[i].~T();
    rfree(b,n*sizeof(T));
  }
  template<class T>
  forceinline void
  Space::free(T* b, long int n) {
    assert(n >= 0);
    free<T>(b,static_cast<long unsigned int>(n));
  }
  template<class T>
  forceinline void
  Space::free(T* b, unsigned int n) {
    free<T>(b,static_cast<long unsigned int>(n));
  }
  template<class T>
  forceinline void
  Space::free(T* b, int n) {
    assert(n >= 0);
    free<T>(b,static_cast<long unsigned int>(n));
  }

  template<class T>
  forceinline T*
  Space::realloc(T* b, long unsigned int n, long unsigned int m) {
    if (n < m) {
      T* p = static_cast<T*>(ralloc(sizeof(T)*m));
      for (long unsigned int i=n; i--; )
        (void) new (p+i) T(b[i]);
      for (long unsigned int i=n; i<m; i++)
        (void) new (p+i) T();
      free<T>(b,n);
      return p;
    } else {
      free<T>(b+m,m-n);
      return b;
    }
  }
  template<class T>
  forceinline T*
  Space::realloc(T* b, long int n, long int m) {
    assert((n >= 0) && (m >= 0));
    return realloc<T>(b,static_cast<long unsigned int>(n),
                      static_cast<long unsigned int>(m));
  }
  template<class T>
  forceinline T*
  Space::realloc(T* b, unsigned int n, unsigned int m) {
    return realloc<T>(b,static_cast<long unsigned int>(n),
                      static_cast<long unsigned int>(m));
  }
  template<class T>
  forceinline T*
  Space::realloc(T* b, int n, int m) {
    assert((n >= 0) && (m >= 0));
    return realloc<T>(b,static_cast<long unsigned int>(n),
                      static_cast<long unsigned int>(m));
  }

#define GECODE_KERNEL_REALLOC(T)                                        \
  template<>                                                            \
  forceinline T*                                                        \
  Space::realloc<T>(T* b, long unsigned int n, long unsigned int m) {   \
    return static_cast<T*>(rrealloc(b,n*sizeof(T),m*sizeof(T)));        \
  }                                                                     \
  template<>                                                            \
  forceinline T*                                                        \
  Space::realloc<T>(T* b, long int n, long int m) {                     \
    assert((n >= 0) && (m >= 0));                                       \
    return realloc<T>(b,static_cast<long unsigned int>(n),              \
                      static_cast<long unsigned int>(m));               \
  }                                                                     \
  template<>                                                            \
  forceinline T*                                                        \
  Space::realloc<T>(T* b, unsigned int n, unsigned int m) {             \
    return realloc<T>(b,static_cast<long unsigned int>(n),              \
                      static_cast<long unsigned int>(m));               \
  }                                                                     \
  template<>                                                            \
  forceinline T*                                                        \
  Space::realloc<T>(T* b, int n, int m) {                               \
    assert((n >= 0) && (m >= 0));                                       \
    return realloc<T>(b,static_cast<long unsigned int>(n),              \
                      static_cast<long unsigned int>(m));               \
  }

  GECODE_KERNEL_REALLOC(bool)
  GECODE_KERNEL_REALLOC(signed char)
  GECODE_KERNEL_REALLOC(unsigned char)
  GECODE_KERNEL_REALLOC(signed short int)
  GECODE_KERNEL_REALLOC(unsigned short int)
  GECODE_KERNEL_REALLOC(signed int)
  GECODE_KERNEL_REALLOC(unsigned int)
  GECODE_KERNEL_REALLOC(signed long int)
  GECODE_KERNEL_REALLOC(unsigned long int)
  GECODE_KERNEL_REALLOC(float)
  GECODE_KERNEL_REALLOC(double)

#undef GECODE_KERNEL_REALLOC

  template<class T>
  forceinline T**
  Space::realloc(T** b, long unsigned int n, long unsigned int m) {
    return static_cast<T**>(rrealloc(b,n*sizeof(T),m*sizeof(T*)));
  }
  template<class T>
  forceinline T**
  Space::realloc(T** b, long int n, long int m) {
    assert((n >= 0) && (m >= 0));
    return realloc<T*>(b,static_cast<long unsigned int>(n),
                       static_cast<long unsigned int>(m));
  }
  template<class T>
  forceinline T**
  Space::realloc(T** b, unsigned int n, unsigned int m) {
    return realloc<T*>(b,static_cast<long unsigned int>(n),
                       static_cast<long unsigned int>(m));
  }
  template<class T>
  forceinline T**
  Space::realloc(T** b, int n, int m) {
    assert((n >= 0) && (m >= 0));
    return realloc<T*>(b,static_cast<long unsigned int>(n),
                       static_cast<long unsigned int>(m));
  }


#ifdef GECODE_HAS_VAR_DISPOSE
  template<class VIC>
  forceinline VarImpBase*
  Space::vars_d(void) const {
    return _vars_d[VIC::idx_d];
  }
  template<class VIC>
  forceinline void
  Space::vars_d(VarImpBase* x) {
    _vars_d[VIC::idx_d] = x;
  }
#endif

  // Space allocated entities: Actors, variable implementations, and advisors
  forceinline void
  Actor::operator delete(void*) {}
  forceinline void
  Actor::operator delete(void*, Space&) {}
  forceinline void*
  Actor::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::operator delete(void*) {}
  template<class VIC>
  forceinline void
  VarImp<VIC>::operator delete(void*, Space&) {}
  template<class VIC>
  forceinline void*
  VarImp<VIC>::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

#ifndef __GNUC__
  forceinline void
  Advisor::operator delete(void*) {}
#endif
  forceinline void
  Advisor::operator delete(void*, Space&) {}
  forceinline void*
  Advisor::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

  forceinline void
  NGL::operator delete(void*) {}
  forceinline void
  NGL::operator delete(void*, Space&) {}
  forceinline void*
  NGL::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

  /*
   * Shared objects and handles
   *
   */
  forceinline
  SharedHandle::Object::Object(void)
    : next(NULL), fwd(NULL), use_cnt(0) {}
  forceinline
  SharedHandle::Object::~Object(void) {
    assert(use_cnt == 0);
  }

  forceinline SharedHandle::Object*
  SharedHandle::object(void) const {
    return o;
  }
  forceinline void
  SharedHandle::subscribe(void) {
    if (o != NULL) o->use_cnt++;
  }
  forceinline void
  SharedHandle::cancel(void) {
    if ((o != NULL) && (--o->use_cnt == 0))
      delete o;
    o=NULL;
  }
  forceinline void
  SharedHandle::object(SharedHandle::Object* n) {
    if (n != o) {
      cancel(); o=n; subscribe();
    }
  }
  forceinline
  SharedHandle::SharedHandle(void) : o(NULL) {}
  forceinline
  SharedHandle::SharedHandle(SharedHandle::Object* so) : o(so) {
    subscribe();
  }
  forceinline
  SharedHandle::SharedHandle(const SharedHandle& sh) : o(sh.o) {
    subscribe();
  }
  forceinline SharedHandle&
  SharedHandle::operator =(const SharedHandle& sh) {
    if (&sh != this) {
      cancel(); o=sh.o; subscribe();
    }
    return *this;
  }
  forceinline void
  SharedHandle::update(Space& home, bool share, SharedHandle& sh) {
    if (sh.o == NULL) {
      o=NULL; return;
    } else if (share) {
      o=sh.o;
    } else if (sh.o->fwd != NULL) {
      o=sh.o->fwd;
    } else {
      o = sh.o->copy();
      sh.o->fwd = o;
      sh.o->next = home.pc.c.shared;
      home.pc.c.shared = sh.o;
    }
    subscribe();
  }
  forceinline
  SharedHandle::~SharedHandle(void) {
    cancel();
  }



  /*
   * No-goods
   *
   */
  forceinline
  NoGoods::NoGoods(void) 
    : n(0) {}
  forceinline unsigned long int
  NoGoods::ng(void) const {
    return n;
  }
  forceinline void
  NoGoods::ng(unsigned long int n0) {
    n=n0;
  }
  forceinline
  NoGoods::~NoGoods(void) {}


  /*
   * ActorLink
   *
   */
  forceinline ActorLink*
  ActorLink::prev(void) const {
    return _prev;
  }

  forceinline ActorLink*
  ActorLink::next(void) const {
    return _next;
  }

  forceinline ActorLink**
  ActorLink::next_ref(void) {
    return &_next;
  }

  forceinline void
  ActorLink::prev(ActorLink* al) {
    _prev = al;
  }

  forceinline void
  ActorLink::next(ActorLink* al) {
    _next = al;
  }

  forceinline void
  ActorLink::unlink(void) {
    ActorLink* p = _prev; ActorLink* n = _next;
    p->_next = n; n->_prev = p;
  }

  forceinline void
  ActorLink::init(void) {
    _next = this; _prev =this;
  }

  forceinline void
  ActorLink::head(ActorLink* a) {
    // Inserts al at head of link-chain (that is, after this)
    ActorLink* n = _next;
    this->_next = a; a->_prev = this;
    a->_next = n; n->_prev = a;
  }

  forceinline void
  ActorLink::tail(ActorLink* a) {
    // Inserts al at tail of link-chain (that is, before this)
    ActorLink* p = _prev;
    a->_next = this; this->_prev = a;
    p->_next = a; a->_prev = p;
  }

  forceinline bool
  ActorLink::empty(void) const {
    return _next == this;
  }

  template<class T>
  forceinline ActorLink*
  ActorLink::cast(T* a) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(a);
    ActorLink& t = *a;
    return static_cast<ActorLink*>(&t);
  }

  template<class T>
  forceinline const ActorLink*
  ActorLink::cast(const T* a) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(a);
    const ActorLink& t = *a;
    return static_cast<const ActorLink*>(&t);
  }


  /*
   * Actor
   *
   */
  forceinline Actor*
  Actor::cast(ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    ActorLink& t = *al;
    return static_cast<Actor*>(&t);
  }

  forceinline const Actor*
  Actor::cast(const ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    const ActorLink& t = *al;
    return static_cast<const Actor*>(&t);
  }

  forceinline void
  Space::afc_enable(void) {
    _wmp_afc |= 1U;
  }
  forceinline bool
  Space::afc_enabled(void) const {
    return (_wmp_afc & 1U) != 0U;
  }
  forceinline void
  Space::wmp(unsigned int n) {
    _wmp_afc = (_wmp_afc & 1U) | (n << 1);
  }
  forceinline unsigned int
  Space::wmp(void) const {
    return _wmp_afc >> 1U;
  }

  forceinline void
  Home::notice(Actor& a, ActorProperty p, bool duplicate) {
    s.notice(a,p,duplicate);
  }

  forceinline Space*
  Space::clone(bool share, CloneStatistics&) const {
    // Clone is only const for search engines. During cloning, several data
    // structures are updated (e.g. forwarding pointers), so we have to
    // cast away the constness.
    return const_cast<Space*>(this)->_clone(share);
  }

  forceinline void
  Space::commit(const Choice& c, unsigned int a, CommitStatistics&) {
    _commit(c,a);
  }

  forceinline void
  Space::trycommit(const Choice& c, unsigned int a, CommitStatistics&) {
    _trycommit(c,a);
  }

  forceinline double
  Space::afc_decay(void) const {
    return gafc.decay();
  }

  forceinline size_t
  Actor::dispose(Space&) {
    return sizeof(*this);
  }


  /*
   * Home for posting actors
   *
   */
  forceinline
  Home::Home(Space& s0, Propagator* p0) : s(s0), p(p0) {}
  forceinline Home&
  Home::operator =(const Home& h) {
    s=h.s; p=h.p;
    return *this;
  }
  forceinline
  Home::operator Space&(void) { 
    return s; 
  }
  forceinline Home
  Home::operator ()(Propagator& p) {
    return Home(s,&p);
  }
  forceinline Home
  Space::operator ()(Propagator& p) {
    return Home(*this,&p);
  }
  forceinline Propagator*
  Home::propagator(void) const {
    return p;
  }

  /*
   * Propagator
   *
   */
  forceinline Propagator*
  Propagator::cast(ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    ActorLink& t = *al;
    return static_cast<Propagator*>(&t);
  }

  forceinline const Propagator*
  Propagator::cast(const ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    const ActorLink& t = *al;
    return static_cast<const Propagator*>(&t);
  }

  forceinline Propagator*
  Propagator::fwd(void) const {
    return static_cast<Propagator*>(prev());
  }

  forceinline
  Propagator::Propagator(Home home) 
    : gafc((home.propagator() != NULL) ?
           // Inherit time counter information
           home.propagator()->gafc :
           // New propagator information
           static_cast<Space&>(home).gafc.allocate()) {
    u.advisors = NULL;
    assert((u.med == 0) && (u.size == 0));
    static_cast<Space&>(home).pl.head(this);
  }

  forceinline
  Propagator::Propagator(Space&, bool, Propagator& p) 
    : gafc(p.gafc) {
    u.advisors = NULL;
    assert((u.med == 0) && (u.size == 0));
    // Set forwarding pointer
    p.prev(this);
  }

  forceinline ModEventDelta
  Propagator::modeventdelta(void) const {
    return u.med;
  }

  forceinline double
  Propagator::afc(const Space& home) const {
    return const_cast<Space&>(home).gafc.afc(gafc);
  }

  forceinline ExecStatus
  Space::ES_SUBSUMED_DISPOSED(Propagator& p, size_t s) {
    p.u.size = s;
    return __ES_SUBSUMED;
  }

  forceinline ExecStatus
  Space::ES_SUBSUMED(Propagator& p) {
    p.u.size = p.dispose(*this);
    return __ES_SUBSUMED;
  }

  forceinline ExecStatus
  Space::ES_FIX_PARTIAL(Propagator& p, const ModEventDelta& med) {
    p.u.med = med;
    assert(p.u.med != 0);
    return __ES_PARTIAL;
  }

  forceinline ExecStatus
  Space::ES_NOFIX_PARTIAL(Propagator& p, const ModEventDelta& med) {
    p.u.med = AllVarConf::med_combine(p.u.med,med);
    assert(p.u.med != 0);
    return __ES_PARTIAL;
  }



  /*
   * Brancher
   *
   */
  forceinline Brancher*
  Brancher::cast(ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    ActorLink& t = *al;
    return static_cast<Brancher*>(&t);
  }

  forceinline const Brancher*
  Brancher::cast(const ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    const ActorLink& t = *al;
    return static_cast<const Brancher*>(&t);
  }

  forceinline
  Brancher::Brancher(Home home) :
    _id(static_cast<Space&>(home).pc.p.branch_id++) {
    if (static_cast<Space&>(home).pc.p.branch_id == 0U)
      throw TooManyBranchers("Brancher::Brancher");
    // If no brancher available, make it the first one
    if (static_cast<Space&>(home).b_status == &static_cast<Space&>(home).bl) {
      static_cast<Space&>(home).b_status = this;
      if (static_cast<Space&>(home).b_commit == &static_cast<Space&>(home).bl)
        static_cast<Space&>(home).b_commit = this;
    }
    static_cast<Space&>(home).bl.tail(this);
  }

  forceinline
  Brancher::Brancher(Space&, bool, Brancher& b)
    : _id(b._id) {
    // Set forwarding pointer
    b.prev(this);
  }

  forceinline unsigned int
  Brancher::id(void) const {
    return _id;
  }

  forceinline Brancher*
  Space::brancher(unsigned int id) {
    /*
     * Due to weakly monotonic propagators the following scenario might
     * occur: a brancher has been committed with all its available
     * choices. Then, propagation determines less information
     * than before and the brancher now will create new choices.
     * Later, during recomputation, all of these choices
     * can be used together, possibly interleaved with 
     * choices for other branchers. That means all branchers
     * must be scanned to find the matching brancher for the choice.
     *
     * b_commit tries to optimize scanning as it is most likely that
     * recomputation does not generate new choices during recomputation
     * and hence b_commit is moved from newer to older branchers.
     */
    Brancher* b_old = b_commit;
    // Try whether we are lucky
    while (b_commit != Brancher::cast(&bl))
      if (id != b_commit->id())
        b_commit = Brancher::cast(b_commit->next());
      else
        return b_commit;
    if (b_commit == Brancher::cast(&bl)) {
      // We did not find the brancher, start at the beginning
      b_commit = Brancher::cast(bl.next());
      while (b_commit != b_old)
        if (id != b_commit->id())
          b_commit = Brancher::cast(b_commit->next());
        else
          return b_commit;
    }
    return NULL;
  }
  
  /*
   * Brancher handle
   *
   */
  forceinline
  BrancherHandle::BrancherHandle(void)
    : _id(Space::reserved_branch_id) {}
  forceinline
  BrancherHandle::BrancherHandle(const Brancher& b)
    : _id(b.id()) {}
  forceinline void
  BrancherHandle::update(Space&, bool, BrancherHandle& bh) {
    _id = bh._id;
  }
  forceinline unsigned int
  BrancherHandle::id(void) const {
    return _id;
  }
  forceinline bool
  BrancherHandle::operator ()(const Space& home) const {
    return const_cast<Space&>(home).brancher(_id) != NULL;
  }
  forceinline void
  BrancherHandle::kill(Space& home) {
    home.kill_brancher(_id);
  }

  
  /*
   * Local objects
   *
   */

  forceinline LocalObject*
  LocalObject::cast(ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    ActorLink& t = *al;
    return static_cast<LocalObject*>(&t);
  }

  forceinline const LocalObject*
  LocalObject::cast(const ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    const ActorLink& t = *al;
    return static_cast<const LocalObject*>(&t);
  }

  forceinline
  LocalObject::LocalObject(Home) {
    ActorLink::cast(this)->prev(NULL);
  }

  forceinline
  LocalObject::LocalObject(Space&,bool,LocalObject&) {
    ActorLink::cast(this)->prev(NULL);
  }

  forceinline LocalObject*
  LocalObject::fwd(Space& home, bool share) {
    if (prev() == NULL)
      fwdcopy(home,share);
    return LocalObject::cast(prev());
  }

  forceinline
  LocalHandle::LocalHandle(void) : o(NULL) {}
  forceinline
  LocalHandle::LocalHandle(LocalObject* lo) : o(lo) {}
  forceinline
  LocalHandle::LocalHandle(const LocalHandle& lh) : o(lh.o) {}
  forceinline LocalHandle&
  LocalHandle::operator =(const LocalHandle& lh) {
    o = lh.o;
    return *this;
  }
  forceinline
  LocalHandle::~LocalHandle(void) {}
  forceinline LocalObject*
  LocalHandle::object(void) const { return o; }
  forceinline void
  LocalHandle::object(LocalObject* n) { o = n; }
  forceinline void
  LocalHandle::update(Space& home, bool share, LocalHandle& lh) {
    object(lh.object()->fwd(home,share));
  }


  /*
   * Choices
   *
   */
  forceinline
  Choice::Choice(const Brancher& b, const unsigned int a)
    : _id(b.id()), _alt(a) {}

  forceinline unsigned int
  Choice::alternatives(void) const {
    return _alt;
  }

  forceinline unsigned int
  Choice::id(void) const {
    return _id;
  }

  forceinline
  Choice::~Choice(void) {}



  /*
   * No-good literal
   *
   */
  forceinline bool
  NGL::leaf(void) const {
    return Support::marked(nl);
  }
  forceinline NGL*
  NGL::next(void) const {
    return static_cast<NGL*>(Support::funmark(nl));
  }
  forceinline void
  NGL::leaf(bool l) {
    nl = l ? Support::fmark(nl) : Support::funmark(nl);
  }
  forceinline void
  NGL::next(NGL* n) {
    nl = Support::marked(nl) ? Support::mark(n) : n;
  }
  forceinline NGL*
  NGL::add(NGL* n, bool l) {
    nl = Support::marked(nl) ? Support::mark(n) : n;
    n->leaf(l);
    return n;
  }

  forceinline
  NGL::NGL(void) 
    : nl(NULL) {}
  forceinline
  NGL::NGL(Space&) 
    : nl(NULL) {}
  forceinline
  NGL::NGL(Space&, bool, NGL&) 
    : nl(NULL) {}
  forceinline size_t
  NGL::dispose(Space&) {
    return sizeof(*this);
  }

  /*
   * Advisor
   *
   */
  template<class A>
  forceinline
  Advisor::Advisor(Space&, Propagator& p, Council<A>& c) {
    // Store propagator and forwarding in prev()
    ActorLink::prev(&p);
    // Link to next advisor in next()
    ActorLink::next(c.advisors); c.advisors = static_cast<A*>(this);
  }

  forceinline
  Advisor::Advisor(Space&, bool, Advisor&) {}

  forceinline bool
  Advisor::disposed(void) const {
    return prev() == NULL;
  }

  forceinline Advisor*
  Advisor::cast(ActorLink* al) {
    return static_cast<Advisor*>(al);
  }

  forceinline const Advisor*
  Advisor::cast(const ActorLink* al) {
    return static_cast<const Advisor*>(al);
  }

  forceinline Propagator&
  Advisor::propagator(void) const {
    assert(!disposed());
    return *Propagator::cast(ActorLink::prev());
  }

  template<class A>
  forceinline void
  Advisor::dispose(Space&,Council<A>&) {
    assert(!disposed());
    ActorLink::prev(NULL);
    // Shorten chains of disposed advisors by one, if possible
    Advisor* n = Advisor::cast(next());
    if ((n != NULL) && n->disposed())
      next(n->next());
  }

  template<class A>
  forceinline ExecStatus
  Space::ES_FIX_DISPOSE(Council<A>& c, A& a) {
    a.dispose(*this,c);
    return ES_FIX;
  }

  template<class A>
  forceinline ExecStatus
  Space::ES_NOFIX_DISPOSE(Council<A>& c, A& a) {
    a.dispose(*this,c);
    return ES_NOFIX;
  }

  template<class A>
  forceinline ExecStatus
  Space::ES_NOFIX_DISPOSE_FORCE(Council<A>& c, A& a) {
    a.dispose(*this,c);
    return ES_NOFIX_FORCE;
  }



  /*
   * Advisor council
   *
   */
  template<class A>
  forceinline
  Council<A>::Council(void) {}

  template<class A>
  forceinline
  Council<A>::Council(Space&)
    : advisors(NULL) {}

  template<class A>
  forceinline bool
  Council<A>::empty(void) const {
    ActorLink* a = advisors;
    while ((a != NULL) && static_cast<A*>(a)->disposed())
      a = a->next();
    advisors = a;
    return a == NULL;
  }

  template<class A>
  forceinline void
  Council<A>::update(Space& home, bool share, Council<A>& c) {
    // Skip all disposed advisors
    {
      ActorLink* a = c.advisors;
      while ((a != NULL) && static_cast<A*>(a)->disposed())
        a = a->next();
      c.advisors = a;
    }
    // Are there any advisors to be cloned?
    if (c.advisors != NULL) {
      // The propagator in from-space
      Propagator* p_f = &static_cast<A*>(c.advisors)->propagator();
      // The propagator in to-space
      Propagator* p_t = Propagator::cast(p_f->prev());
      // Advisors in from-space
      ActorLink** a_f = &c.advisors;
      // Advisors in to-space
      A* a_t = NULL;
      while (*a_f != NULL) {
        if (static_cast<A*>(*a_f)->disposed()) {
          *a_f = (*a_f)->next();
        } else {
          // Run specific copying part
          A* a = new (home) A(home,share,*static_cast<A*>(*a_f));
          // Set propagator pointer
          a->prev(p_t);
          // Set forwarding pointer
          (*a_f)->prev(a);
          // Link
          a->next(a_t);
          a_t = a;
          a_f = (*a_f)->next_ref();
        }
      }
      advisors = a_t;
      // Enter advisor link for reset
      assert(p_f->u.advisors == NULL);
      p_f->u.advisors = c.advisors;
    } else {
      advisors = NULL;
    }
  }

  template<class A>
  forceinline void
  Council<A>::dispose(Space& home) {
    ActorLink* a = advisors;
    while (a != NULL) {
      if (!static_cast<A*>(a)->disposed())
        static_cast<A*>(a)->dispose(home,*this);
      a = a->next();
    }
  }



  /*
   * Advisor iterator
   *
   */
  template<class A>
  forceinline
  Advisors<A>::Advisors(const Council<A>& c)
    : a(c.advisors) {
    while ((a != NULL) && static_cast<A*>(a)->disposed())
      a = a->next();
  }

  template<class A>
  forceinline bool
  Advisors<A>::operator ()(void) const {
    return a != NULL;
  }

  template<class A>
  forceinline void
  Advisors<A>::operator ++(void) {
    do {
      a = a->next();
    } while ((a != NULL) && static_cast<A*>(a)->disposed());
  }

  template<class A>
  forceinline A&
  Advisors<A>::advisor(void) const {
    return *static_cast<A*>(a);
  }



  /*
   * Space
   *
   */
  forceinline void
  Space::enqueue(Propagator* p) {
    ActorLink::cast(p)->unlink();
    ActorLink* c = &pc.p.queue[p->cost(*this,p->u.med).ac];
    c->tail(ActorLink::cast(p));
    if (c > pc.p.active)
      pc.p.active = c;
  }

  forceinline void
  Space::fail(void) {
    /*
     * Now active points beyond the last queue. This is essential as
     * enqueuing a propagator in a failed space keeps the space
     * failed.
     */
    pc.p.active = &pc.p.queue[PropCost::AC_MAX+1]+1;
  }
  forceinline void
  Home::fail(void) {
    s.fail();
  }

  forceinline bool
  Space::failed(void) const {
    return pc.p.active > &pc.p.queue[PropCost::AC_MAX+1];
  }
  forceinline bool
  Home::failed(void) const {
    return s.failed();
  }

  forceinline bool
  Space::stable(void) const {
    return ((pc.p.active < &pc.p.queue[0]) ||
            (pc.p.active > &pc.p.queue[PropCost::AC_MAX+1]));
  }



  /*
   * Variable implementation
   *
   */
  template<class VIC>
  forceinline ActorLink**
  VarImp<VIC>::actor(PropCond pc) {
    assert((pc >= 0)  && (pc < pc_max+2));
    return (pc == 0) ? b.base : b.base+u.idx[pc-1];
  }

  template<class VIC>
  forceinline ActorLink**
  VarImp<VIC>::actorNonZero(PropCond pc) {
    assert((pc > 0)  && (pc < pc_max+2));
    return b.base+u.idx[pc-1];
  }

  template<class VIC>
  forceinline unsigned int&
  VarImp<VIC>::idx(PropCond pc) {
    assert((pc > 0)  && (pc < pc_max+2));
    return u.idx[pc-1];
  }

  template<class VIC>
  forceinline unsigned int
  VarImp<VIC>::idx(PropCond pc) const {
    assert((pc > 0)  && (pc < pc_max+2));
    return u.idx[pc-1];
  }

  template<class VIC>
  forceinline
  VarImp<VIC>::VarImp(Space&) {
    b.base = NULL; entries = 0;
    for (PropCond pc=1; pc<pc_max+2; pc++)
      idx(pc) = 0;
    free_and_bits = 0;
  }

  template<class VIC>
  forceinline
  VarImp<VIC>::VarImp(void) {
    b.base = NULL; entries = 0;
    for (PropCond pc=1; pc<pc_max+2; pc++)
      idx(pc) = 0;
    free_and_bits = 0;
  }

  template<class VIC>
  forceinline unsigned int
  VarImp<VIC>::degree(void) const {
    assert(!copied());
    return entries;
  }

  template<class VIC>
  forceinline double
  VarImp<VIC>::afc(const Space& home) const {
    double d = 0.0;
    // Count the afc of each propagator
    {
      ActorLink** a = const_cast<VarImp<VIC>*>(this)->actor(0);
      ActorLink** e = const_cast<VarImp<VIC>*>(this)->actorNonZero(pc_max+1);
      while (a < e) {
        d += Propagator::cast(*a)->afc(home); a++;
      }
    }
    // Count the afc of each advisor's propagator
    {
      ActorLink** a = const_cast<VarImp<VIC>*>(this)->actorNonZero(pc_max+1);
      ActorLink** e = const_cast<VarImp<VIC>*>(this)->b.base+entries;
      while (a < e) {
        d += Advisor::cast(*a)->propagator().afc(home); a++;
      }
    }
    return d;
  }

  template<class VIC>
  forceinline ModEvent
  VarImp<VIC>::modevent(const Delta& d) {
    return d.me;
  }

  template<class VIC>
  forceinline unsigned int
  VarImp<VIC>::bits(void) const {
    return free_and_bits;
  }

  template<class VIC>
  forceinline unsigned int&
  VarImp<VIC>::bits(void) {
    return free_and_bits;
  }

#ifdef GECODE_HAS_VAR_DISPOSE
  template<class VIC>
  forceinline VarImp<VIC>*
  VarImp<VIC>::vars_d(Space& home) {
    return static_cast<VarImp<VIC>*>(home.vars_d<VIC>());
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::vars_d(Space& home, VarImp<VIC>* x) {
    home.vars_d<VIC>(x);
  }
#endif

  template<class VIC>
  forceinline bool
  VarImp<VIC>::copied(void) const {
    return Support::marked(b.fwd);
  }

  template<class VIC>
  forceinline VarImp<VIC>*
  VarImp<VIC>::forward(void) const {
    assert(copied());
    return static_cast<VarImp<VIC>*>(Support::unmark(b.fwd));
  }

  template<class VIC>
  forceinline VarImp<VIC>*
  VarImp<VIC>::next(void) const {
    assert(copied());
    return u.next;
  }

  template<class VIC>
  forceinline
  VarImp<VIC>::VarImp(Space& home, bool, VarImp<VIC>& x) {
    VarImpBase** reg;
    free_and_bits = x.free_and_bits & ((1 << free_bits) - 1);
    if (x.b.base == NULL) {
      // Variable implementation needs no index structure
      reg = &home.pc.c.vars_noidx;
      assert(x.degree() == 0);
    } else {
      reg = &home.pc.c.vars_u[idx_c];
    }
    // Save subscriptions in copy
    b.base = x.b.base;
    entries = x.entries;
    for (PropCond pc=1; pc<pc_max+2; pc++)
      idx(pc) = x.idx(pc);

    // Set forwarding pointer
    x.b.fwd = static_cast<VarImp<VIC>*>(Support::mark(this));
    // Register original
    x.u.next = static_cast<VarImp<VIC>*>(*reg); *reg = &x;
  }

  template<class VIC>
  forceinline ModEvent
  VarImp<VIC>::me(const ModEventDelta& med) {
    return static_cast<ModEvent>((med & VIC::med_mask) >> VIC::med_fst);
  }

  template<class VIC>
  forceinline ModEventDelta
  VarImp<VIC>::med(ModEvent me) {
    return static_cast<ModEventDelta>(me << VIC::med_fst);
  }

  template<class VIC>
  forceinline ModEvent
  VarImp<VIC>::me_combine(ModEvent me1, ModEvent me2) {
    return VIC::me_combine(me1,me2);
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::schedule(Space& home, Propagator& p, ModEvent me,
                        bool force) {
    if (VIC::med_update(p.u.med,me) || force)
      home.enqueue(&p);
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::schedule(Space& home, PropCond pc1, PropCond pc2, ModEvent me) {
    ActorLink** b = actor(pc1);
    ActorLink** p = actorNonZero(pc2+1);
    while (p-- > b)
      schedule(home,*Propagator::cast(*p),me);
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::enter(Space& home, Propagator* p, PropCond pc) {
    assert(pc <= pc_max);
    // Count one new subscription
    home.pc.p.n_sub += 1;
    if ((free_and_bits >> free_bits) == 0)
      resize(home);
    free_and_bits -= 1 << free_bits;

    // Enter subscription
    b.base[entries] = *actorNonZero(pc_max+1);
    entries++;
    for (PropCond j = pc_max; j > pc; j--) {
      *actorNonZero(j+1) = *actorNonZero(j);
      idx(j+1)++;
    }
    *actorNonZero(pc+1) = *actor(pc);
    idx(pc+1)++;
    *actor(pc) = ActorLink::cast(p);

#ifdef GECODE_AUDIT
    ActorLink** f = actor(pc);
    while (f < (pc == pc_max+1 ? b.base+entries : actorNonZero(pc+1)))
      if (*f == p)
        goto found;
      else
        f++;
    GECODE_NEVER;
    found: ;
#endif
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::enter(Space& home, Advisor* a) {
    // Count one new subscription
    home.pc.p.n_sub += 1;
    if ((free_and_bits >> free_bits) == 0)
      resize(home);
    free_and_bits -= 1 << free_bits;

    // Enter subscription
    b.base[entries++] = *actorNonZero(pc_max+1);
    *actorNonZero(pc_max+1) = a;
  }

  template<class VIC>
  void
  VarImp<VIC>::resize(Space& home) {
    if (b.base == NULL) {
      assert((free_and_bits >> free_bits) == 0);
      // Create fresh dependency array with four entries
      free_and_bits += 4 << free_bits;
      b.base = home.alloc<ActorLink*>(4);
      for (int i=0; i<pc_max+1; i++)
        u.idx[i] = 0;
    } else {
      // Resize dependency array
      unsigned int n = degree();
      // Find out whether the area is most likely in the special area
      // reserved for subscriptions. If yes, just resize mildly otherwise
      // more agressively
      ActorLink** s = static_cast<ActorLink**>(home.mm.subscriptions());
      unsigned int m =
        ((s <= b.base) && (b.base < s+home.pc.p.n_sub)) ?
        (n+4) : ((n+1)*3>>1);
      ActorLink** prop = home.alloc<ActorLink*>(m);
      free_and_bits += (m-n) << free_bits;
      // Copy entries
      Heap::copy<ActorLink*>(prop, b.base, n);
      home.free<ActorLink*>(b.base,n);
      b.base = prop;
    }
  }

  template<class VIC>
  void
  VarImp<VIC>::subscribe(Space& home, Propagator& p, PropCond pc,
                         bool assigned, ModEvent me, bool schedule) {
    if (assigned) {
      // Do not subscribe, just schedule the propagator
      if (schedule)
        VarImp<VIC>::schedule(home,p,ME_GEN_ASSIGNED);
    } else {
      enter(home,&p,pc);
      // Schedule propagator
      if (schedule && (pc != PC_GEN_ASSIGNED))
        VarImp<VIC>::schedule(home,p,me);
    }
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::subscribe(Space& home, Advisor& a, bool assigned) {
    if (!assigned)
      enter(home,&a);
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::remove(Space& home, Propagator* p, PropCond pc) {
    assert(pc <= pc_max);
    ActorLink* a = ActorLink::cast(p);
    // Find actor in dependency array
    ActorLink** f = actor(pc);
#ifdef GECODE_AUDIT
    while (f < actorNonZero(pc+1))
      if (*f == a)
        goto found;
      else
        f++;
    GECODE_NEVER;
  found: ;
#else
    while (*f != a) f++;
#endif
    // Remove actor
    *f = *(actorNonZero(pc+1)-1);
    for (PropCond j = pc+1; j< pc_max+1; j++) {
      *(actorNonZero(j)-1) = *(actorNonZero(j+1)-1);
      idx(j)--;
    }
    *(actorNonZero(pc_max+1)-1) = b.base[entries-1];
    idx(pc_max+1)--;
    entries--;
    free_and_bits += 1 << free_bits;
    home.pc.p.n_sub -= 1;
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::remove(Space& home, Advisor* a) {
    // Find actor in dependency array
    ActorLink** f = actorNonZero(pc_max+1);
#ifdef GECODE_AUDIT
    while (f < b.base+entries)
      if (*f == a)
        goto found;
      else
        f++;
    GECODE_NEVER;
  found: ;
#else
    while (*f != a) f++;
#endif
    // Remove actor
    *f = b.base[--entries];
    free_and_bits += 1 << free_bits;
    home.pc.p.n_sub -= 1;
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::cancel(Space& home, Propagator& p, PropCond pc, bool assigned) {
    if (!assigned)
      remove(home,&p,pc);
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::cancel(Space& home, Advisor& a, bool assigned) {
    if (!assigned)
      remove(home,&a);
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::cancel(Space& home) {
    unsigned int n_sub = degree();
    home.pc.p.n_sub -= n_sub;
    unsigned int n = (free_and_bits >> VIC::free_bits) + n_sub;
    home.free<ActorLink*>(b.base,n);
    // Must be NULL such that cloning works
    b.base = NULL;
    // Must be 0 such that degree works
    entries = 0;
  }

  template<class VIC>
  forceinline bool
  VarImp<VIC>::advise(Space& home, ModEvent me, Delta& d) {
    /*
     * An advisor that is executed might remove itself due to subsumption.
     * As entries are removed from front to back, the advisors must
     * be iterated in forward direction.
     */
    ActorLink** la = actorNonZero(pc_max+1);
    ActorLink** le = b.base+entries;
    if (la == le)
      return true;
    d.me = me;
    // An advisor that is run, might be removed during execution.
    // As removal is done from the back the advisors have to be executed
    // in inverse order.
    do {
      Advisor* a = Advisor::cast(*la);
      assert(!a->disposed());
      Propagator& p = a->propagator();
      switch (p.advise(home,*a,d)) {
      case ES_FIX:
        break;
      case ES_FAILED:
        if (home.afc_enabled())
          home.gafc.fail(p.gafc);
        return false;
      case ES_NOFIX:
        schedule(home,p,me);
        break;
      case ES_NOFIX_FORCE:
        schedule(home,p,me,true);
        break;
      default:
        GECODE_NEVER;
      }
    } while (++la < le);
    return true;
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::update(VarImp<VIC>* x, ActorLink**& sub) {
    // this refers to the variable to be updated (clone)
    // x refers to the original
    // Recover from copy
    x->b.base = b.base;
    x->u.idx[0] = u.idx[0];
    if (pc_max > 0 && sizeof(ActorLink**) > sizeof(unsigned int))
      x->u.idx[1] = u.idx[1];

    ActorLink** f = x->b.base;
    unsigned int n = x->degree();
    ActorLink** t = sub;
    sub += n;
    b.base = t;
    // Set subscriptions using actor forwarding pointers
    while (n >= 4) {
      n -= 4;
      t[0]=f[0]->prev(); t[1]=f[1]->prev();
      t[2]=f[2]->prev(); t[3]=f[3]->prev();
      t += 4; f += 4;
    }
    if (n >= 2) {
      n -= 2;
      t[0]=f[0]->prev(); t[1]=f[1]->prev();
      t += 2; f += 2;
    }
    if (n > 0) {
      t[0]=f[0]->prev();
    }
  }

  template<class VIC>
  forceinline void
  VarImp<VIC>::update(Space& home, ActorLink**& sub) {
    VarImp<VIC>* x = static_cast<VarImp<VIC>*>(home.pc.c.vars_u[idx_c]);
    while (x != NULL) {
      VarImp<VIC>* n = x->next(); x->forward()->update(x,sub); x = n;
    }
  }



  /*
   * Variable disposer
   *
   */
  template<class VarImp>
  VarImpDisposer<VarImp>::VarImpDisposer(void) {
#ifdef GECODE_HAS_VAR_DISPOSE
    Space::vd[VarImp::idx_d] = this;
#endif
  }

  template<class VarImp>
  void
  VarImpDisposer<VarImp>::dispose(Space& home, VarImpBase* _x) {
    VarImp* x = static_cast<VarImp*>(_x);
    do {
      x->dispose(home); x = static_cast<VarImp*>(x->next_d());
    } while (x != NULL);
  }

  /*
   * Statistics
   */

  forceinline void
  StatusStatistics::reset(void) {
    propagate = 0;
    wmp = false;
  }
  forceinline
  StatusStatistics::StatusStatistics(void) {
    reset();
  }
  forceinline StatusStatistics&
  StatusStatistics::operator +=(const StatusStatistics& s) { 
    propagate += s.propagate;
    wmp |= s.wmp;
    return *this;
  }
  forceinline StatusStatistics
  StatusStatistics::operator +(const StatusStatistics& s) {
    StatusStatistics t(s);
    return t += *this;
  }

  forceinline void
  CloneStatistics::reset(void) {}

  forceinline
  CloneStatistics::CloneStatistics(void) {
    reset();
  }
  forceinline CloneStatistics
  CloneStatistics::operator +(const CloneStatistics&) {
    CloneStatistics s;
    return s;
  }
  forceinline CloneStatistics&
  CloneStatistics::operator +=(const CloneStatistics&) { 
    return *this;
  }

  forceinline void
  CommitStatistics::reset(void) {}

  forceinline
  CommitStatistics::CommitStatistics(void) {
    reset();
  }
  forceinline CommitStatistics
  CommitStatistics::operator +(const CommitStatistics&) {
    CommitStatistics s;
    return s;
  }
  forceinline CommitStatistics&
  CommitStatistics::operator +=(const CommitStatistics&) { 
    return *this;
  }

  /*
   * Cost computation
   *
   */

  forceinline
  PropCost::PropCost(PropCost::ActualCost ac0) : ac(ac0) {}

  forceinline PropCost
  PropCost::cost(PropCost::Mod m,
                 PropCost::ActualCost lo, PropCost::ActualCost hi,
                 unsigned int n) {
    if (n < 2)
      return (m == LO) ? AC_UNARY_LO : AC_UNARY_HI;
    else if (n == 2)
      return (m == LO) ? AC_BINARY_LO : AC_BINARY_HI;
    else if (n == 3)
      return (m == LO) ? AC_TERNARY_LO : AC_TERNARY_HI;
    else
      return (m == LO) ? lo : hi;
  }

  forceinline PropCost
  PropCost::crazy(PropCost::Mod m, unsigned int n) {
    return cost(m,AC_CRAZY_LO,AC_CRAZY_HI,n);
  }
  forceinline PropCost
  PropCost::crazy(PropCost::Mod m, int n) {
    assert(n >= 0);
    return crazy(m,static_cast<unsigned int>(n));
  }
  forceinline PropCost
  PropCost::cubic(PropCost::Mod m, unsigned int n) {
    return cost(m,AC_CUBIC_LO,AC_CUBIC_HI,n);
  }
  forceinline PropCost
  PropCost::cubic(PropCost::Mod m, int n) {
    assert(n >= 0);
    return cubic(m,static_cast<unsigned int>(n));
  }
  forceinline PropCost
  PropCost::quadratic(PropCost::Mod m, unsigned int n) {
    return cost(m,AC_QUADRATIC_LO,AC_QUADRATIC_HI,n);
  }
  forceinline PropCost
  PropCost::quadratic(PropCost::Mod m, int n) {
    assert(n >= 0);
    return quadratic(m,static_cast<unsigned int>(n));
  }
  forceinline PropCost
  PropCost::linear(PropCost::Mod m, unsigned int n) {
    return cost(m,AC_LINEAR_LO,AC_LINEAR_HI,n);
  }
  forceinline PropCost
  PropCost::linear(PropCost::Mod m, int n) {
    assert(n >= 0);
    return linear(m,static_cast<unsigned int>(n));
  }
  forceinline PropCost
  PropCost::ternary(PropCost::Mod m) {
    return (m == LO) ? AC_TERNARY_LO : AC_TERNARY_HI;
  }
  forceinline PropCost
  PropCost::binary(PropCost::Mod m) {
    return (m == LO) ? AC_BINARY_LO : AC_BINARY_HI;
  }
  forceinline PropCost
  PropCost::unary(PropCost::Mod m) {
    return (m == LO) ? AC_UNARY_LO : AC_UNARY_HI;
  }

  /*
   * Iterators for propagators and branchers of a space
   *
   */
  forceinline
  Space::Propagators::Propagators(const Space& home0) 
    : home(home0), q(home.pc.p.active) {
    while (q >= &home.pc.p.queue[0]) {
      if (q->next() != q) {
        c = q->next(); e = q; q--;
        return;
      }
      q--;
    }
    q = NULL;
    if (!home.pl.empty()) {
      c = Propagator::cast(home.pl.next());
      e = Propagator::cast(&home.pl);
    } else {
      c = e = NULL;
    }
  }
  forceinline bool 
  Space::Propagators::operator ()(void) const {
    return c != NULL;
  }
  forceinline void 
  Space::Propagators::operator ++(void) {
    c = c->next();
    if (c == e) {
      if (q == NULL) {
        c = NULL;
      } else {
        while (q >= &home.pc.p.queue[0]) {
          if (q->next() != q) {
            c = q->next(); e = q; q--;
            return;
          }
          q--;
        }
        q = NULL;
        if (!home.pl.empty()) {
          c = Propagator::cast(home.pl.next());
          e = Propagator::cast(&home.pl);
        } else {
          c = NULL;
        }
      }
    }
  }
  forceinline const Propagator& 
  Space::Propagators::propagator(void) const {
    return *Propagator::cast(c);
  }

  forceinline
  Space::Branchers::Branchers(const Space& home) 
    : c(Brancher::cast(home.bl.next())), e(&home.bl) {}
  forceinline bool 
  Space::Branchers::operator ()(void) const {
    return c != e;
  }
  forceinline void 
  Space::Branchers::operator ++(void) {
    c = c->next();
  }
  forceinline const Brancher& 
  Space::Branchers::brancher(void) const {
    return *Brancher::cast(c);
  }

  /*
   * Space construction support
   *
   */
  template<class T> 
  forceinline T& 
  Space::construct(void) {
    return alloc<T>(1);
  }
  template<class T, typename A1> 
  forceinline T& 
  Space::construct(A1 const& a1) {
    T& t = *static_cast<T*>(ralloc(sizeof(T)));
    new (&t) T(a1);
    return t;
  }
  template<class T, typename A1, typename A2> 
  forceinline T& 
  Space::construct(A1 const& a1, A2 const& a2) {
    T& t = *static_cast<T*>(ralloc(sizeof(T)));
    new (&t) T(a1,a2);
    return t;
  }
  template<class T, typename A1, typename A2, typename A3>
  forceinline T& 
  Space::construct(A1 const& a1, A2 const& a2, A3 const& a3) {
    T& t = *static_cast<T*>(ralloc(sizeof(T)));
    new (&t) T(a1,a2,a3);
    return t;
  }
  template<class T, typename A1, typename A2, typename A3, typename A4>
  forceinline T& 
  Space::construct(A1 const& a1, A2 const& a2, A3 const& a3, A4 const& a4) {
    T& t = *static_cast<T*>(ralloc(sizeof(T)));
    new (&t) T(a1,a2,a3,a4);
    return t;
  }
  template<class T, typename A1, typename A2, typename A3, typename A4, typename A5>
  forceinline T& 
  Space::construct(A1 const& a1, A2 const& a2, A3 const& a3, A4 const& a4, A5 const& a5) {
    T& t = *static_cast<T*>(ralloc(sizeof(T)));
    new (&t) T(a1,a2,a3,a4,a5);
    return t;
  }

}

// STATISTICS: kernel-core
