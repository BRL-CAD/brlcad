/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Guido Tack, 2004
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

#ifndef __GECODE_SEARCH_HH__
#define __GECODE_SEARCH_HH__

#include <gecode/kernel.hh>

/*
 * Configure linking
 *
 */
#if !defined(GECODE_STATIC_LIBS) && \
    (defined(__CYGWIN__) || defined(__MINGW32__) || defined(_MSC_VER))

#ifdef GECODE_BUILD_SEARCH
#define GECODE_SEARCH_EXPORT __declspec( dllexport )
#else
#define GECODE_SEARCH_EXPORT __declspec( dllimport )
#endif

#else

#ifdef GECODE_GCC_HAS_CLASS_VISIBILITY
#define GECODE_SEARCH_EXPORT __attribute__ ((visibility("default")))
#else
#define GECODE_SEARCH_EXPORT
#endif

#endif

// Configure auto-linking
#ifndef GECODE_BUILD_SEARCH
#define GECODE_LIBRARY_NAME "Search"
#include <gecode/support/auto-link.hpp>
#endif


namespace Gecode { namespace Search {

  /// %Sequential search engine implementations
  namespace Sequential {}
  
  /// %Parallel search engine implementations
  namespace Parallel {}

  /// %Meta search engine implementations
  namespace Meta {}

  /**
   * \brief %Search configuration
   *
   * \ingroup TaskModelSearch
   */
  namespace Config {
    /// Whether engines create a clone when being initialized
    const bool clone = true;
    /// Number of threads to use
    const double threads = 1.0;
    /// Create a clone after every \a c_d commits (commit distance)
    const unsigned int c_d = 8;
    /// Create a clone during recomputation if distance is greater than \a a_d (adaptive distance)
    const unsigned int a_d = 2;
    
    /// Minimal number of open nodes for stealing
    const unsigned int steal_limit = 3;
    /// Initial delay in milliseconds for all but first worker thread
    const unsigned int initial_delay = 5;

    /// Depth limit for no-good generation during search
    const unsigned int nogoods_limit = 128;
  }

}}

namespace Gecode { namespace Search {

  /**
   * \defgroup FuncThrowSearch %Search exceptions
   * \ingroup FuncThrow
   */
  //@{
  /// %Exception: Uninitialized cutoff for restart-based search
  class GECODE_VTABLE_EXPORT UninitializedCutoff : public Exception {
  public:
    /// Initialize with location \a l
    UninitializedCutoff(const char* l);
  };
  //@}
}}

#include <gecode/search/exception.hpp>

namespace Gecode { namespace Search {

  /**
   * \brief %Search engine statistics
   * \ingroup TaskModelSearch
   */
  class Statistics : public StatusStatistics {
  public:
    /// Number of failed nodes in search tree
    unsigned long int fail;
    /// Number of nodes expanded
    unsigned long int node;
    /// Maximum depth of search stack
    unsigned long int depth;
    /// Number of restarts
    unsigned long int restart;
    /// Number of no-goods posted
    unsigned long int nogood;
    /// Initialize
    Statistics(void);
    /// Reset
    void reset(void);
    /// Return sum with \a s
    Statistics operator +(const Statistics& s);
    /// Increment by statistics \a s
    Statistics& operator +=(const Statistics& s);
  };

}}

#include <gecode/search/statistics.hpp>

namespace Gecode { namespace Search {

    class Stop;
    class Cutoff;

    /**
     * \brief %Search engine options
     *
     * Defines options for search engines. Not all search engines might
     * honor all option values.
     *
     *  - \a c_d as minimal recomputation distance: this guarantees that
     *    a path between two nodes in the search tree for which copies are
     *    stored has at least length \a c_d. That is, in order to recompute
     *    a node in the search tree, \a c_d recomputation steps are needed.
     *    The minimal recomputation distance yields a guarantee on saving
     *    memory compared to full copying: it stores \a c_d times less nodes
     *    than full copying.
     *  - \a a_d as adaptive recomputation distance: when a node needs to be
     *    recomputed and the path is longer than \a a_d, an intermediate copy
     *    is created (approximately in the middle of the path) to speed up
     *    future recomputation. Note that small values of \a a_d can increase
     *    the memory consumption considerably.
     *
     * Full copying corresponds to a maximal recomputation distance
     * \a c_d of 1.
     *
     * All recomputation performed is based on batch recomputation: batch
     * recomputation performs propagation only once for an entire path
     * used in recomputation.
     *
     * The number of threads to be used is controlled by a double \f$n\f$
     * (assume that \f$m\f$ is the number of processing units available). If
     * \f$1 \leq n\f$, \f$n\f$ threads are chosen (of course with rounding).
     * If \f$n \leq -1\f$, then \f$m + n\f$ threads are 
     * chosen (all but \f$-n\f$ processing units get a thread). If \f$n\f$
     * is zero, \f$m\f$ threads are chosen. If \f$0<n<1\f$,
     * \f$n \times m\f$ threads are chosen. If \f$-1 <n<0\f$, 
     * \f$(1+n)\times m\f$ threads are chosen.
     * 
     * \ingroup TaskModelSearch
     */
    class Options {
    public:
      /// Whether engines create a clone when being initialized
      bool clone;
      /// Number of threads to use
      double threads;
      /// Create a clone after every \a c_d commits (commit distance)
      unsigned int c_d;
      /// Create a clone during recomputation if distance is greater than \a a_d (adaptive distance)
      unsigned int a_d;
      /// Depth limit for extraction of no-goods
      unsigned int nogoods_limit;
      /// Stop object for stopping search
      Stop* stop;
      /// Cutoff for restart-based search
      Cutoff* cutoff;
      /// Default options
      GECODE_SEARCH_EXPORT static const Options def;
      /// Initialize with default values
      Options(void);
      /// Expand with real number of threads
      GECODE_SEARCH_EXPORT Options
      expand(void) const;
    };

}}

#include <gecode/search/options.hpp>

namespace Gecode {

  template<template<class> class E, class T>
  class RBS;

}

namespace Gecode { namespace Search { namespace Meta {

  class RBS;

}}}

namespace Gecode { namespace Search {

  /**
   * \defgroup TaskModelSearchStop Stop-objects for stopping search
   * \ingroup TaskModelSearch
   *
   * Allows to specify various criteria when a search engine should
   * stop exploration. Only exploration but neither recomputation
   * nor propagation will be interrupted.
   *
   */

  /**
   * \brief Base-class for %Stop-object
   * \ingroup TaskModelSearchStop
   */
  class GECODE_SEARCH_EXPORT Stop {
  public:
    /// Default constructor
    Stop(void);
    /// Stop search, if returns true
    virtual bool stop(const Statistics& s, const Options& o) = 0;
    /// Destructor
    virtual ~Stop(void);
    /// Allocate memory from heap
    static void* operator new(size_t s);
    /// Free memory allocated from heap
    static void  operator delete(void* p);
  };
  
  /**
   * \brief %Stop-object based on number of nodes
   *
   * The number of nodes reported (by the statistics) is the
   * number since the engine started exploration. It is not the
   * number since the last stop!
   * \ingroup TaskModelSearchStop
   */
  class GECODE_SEARCH_EXPORT NodeStop : public Stop {
  protected:
    /// Node limit
    unsigned long int l;
  public:
    /// Stop if node limit \a l is exceeded
    NodeStop(unsigned long int l);
    /// Return current limit
    unsigned long int limit(void) const;
    /// Set current limit to \a l nodes
    void limit(unsigned long int l);
    /// Return true if node limit is exceeded
    virtual bool stop(const Statistics& s, const Options& o);
  };

  /**
   * \brief %Stop-object based on number of failures
   *
   * The number of failures reported (by the statistics) is the
   * number since the engine started exploration. It is not the
   * number since the last stop!
   * \ingroup TaskModelSearchStop
   */
  class GECODE_SEARCH_EXPORT FailStop : public Stop {
  protected:
    /// Failure limit
    unsigned long int l;
  public:
    /// Stop if failure limit \a l is exceeded
    FailStop(unsigned long int l);
    /// Return current limit
    unsigned long int limit(void) const;
    /// Set current limit to \a l failures
    void limit(unsigned long int l);
    /// Return true if failure limit is exceeded
    virtual bool stop(const Statistics& s, const Options& o);
  };
  
  /**
   * \brief %Stop-object based on time
   * \ingroup TaskModelSearchStop
   */
  class GECODE_SEARCH_EXPORT TimeStop : public Stop {
  protected:
    /// Time when execution should stop
    Support::Timer t;
    /// Current limit in milliseconds
    unsigned long int l;
  public:
    /// Stop if search exceeds \a l milliseconds (from creation of this object)
    TimeStop(unsigned long int l);
    /// Return current limit in milliseconds
    unsigned long int limit(void) const;
    /// Set current limit to \a l milliseconds
    void limit(unsigned long int l);
    /// Reset time to zero
    void reset(void);
    /// Return true if time limit is exceeded
    virtual bool stop(const Statistics& s, const Options& o);
  };

  /**
   * \brief %Stop-object for meta engine
   * \ingroup TaskModelSearchStop
   */
  class GECODE_SEARCH_EXPORT MetaStop : public Stop {
    template<template<class>class,class> friend class ::Gecode::RBS;
    friend class ::Gecode::Search::Meta::RBS;
  private:
    /// The failure stop object for the engine
    FailStop* e_stop;
    /// The stop object for the meta engine
    Stop* m_stop;
    /// Whether the engine was stopped
    bool e_stopped;
    /// Accumulated statistics for the meta engine
    Statistics m_stat;
  public:
    /// Stop the meta engine if indicated by the stop object \a s
    MetaStop(Stop* s);
    /// Return true if meta engine must be stopped
    virtual bool stop(const Statistics& s, const Options& o);
    /// Set current limit for the engine to \a l fails
    void limit(const Search::Statistics& s, unsigned long int l);
    /// Update statistics
    void update(const Search::Statistics& s);
    /// Return the stop object to control the engine
    Stop* enginestop(void) const;
    /// Return whether the engine has been stopped
    bool enginestopped(void) const;
    /// Return statistics for the meta engine
    Statistics metastatistics(void) const;
    /// Delete object
    ~MetaStop(void);
  };

}}

#include <gecode/search/stop.hpp>

namespace Gecode { namespace Search {

  /**
   * \brief Base class for cutoff generators for restart-based meta engine
   */
  class GECODE_SEARCH_EXPORT Cutoff {
  public:
    /// Default constructor
    Cutoff(void);
    /// Return next cutoff value
    virtual unsigned long int operator ()(void) = 0;
      /// Destructor
    virtual ~Cutoff(void);
    /// Create generator for constant sequence with constant \a s
    static Cutoff*
    constant(unsigned long int scale=1U);
    /// Create generator for linear sequence scaled by \a scale
    static Cutoff*
    linear(unsigned long int scale=1U);
    /** Create generator for geometric sequence scaled by
     *  \a scale using base \a base
     */
    static Cutoff*
    geometric(unsigned long int scale=1U, double base=1.5);
    /// Create generator for luby sequence with scale-factor \a scale
    static Cutoff*
    luby(unsigned long int scale=1U);
    /** Create generator for random sequence with seed \a seed that
     *  generates values between \a min and \a max with \a n steps
     *  between the extreme values (use 0 for \a n to get step size 1).
     */
    static Cutoff*
    rnd(unsigned int seed, 
        unsigned long int min, unsigned long int max, 
        unsigned long int n);
    /// Append cutoff values from \a c2 after \a n values from \a c1
    static Cutoff*
    append(Cutoff* c1, unsigned long int n, Cutoff* c2);
    /// Create generator that repeats \a n times each cutoff value from \a c
    static Cutoff*
    repeat(Cutoff* c, unsigned long int n);
    /// Allocate memory from heap
    static void* operator new(size_t s);
    /// Free memory allocated from heap
    static void  operator delete(void* p);
  };
    
}}

#include <gecode/search/cutoff.hpp>

namespace Gecode { namespace Search {

  /**
   * \brief %Search engine implementation interface
   */
  class Engine {
  public:
    /// Return next solution (NULL, if none exists or search has been stopped)
    virtual Space* next(void) = 0;
    /// Return statistics
    virtual Statistics statistics(void) const = 0;
    /// Check whether engine has been stopped
    virtual bool stopped(void) const = 0;
    /// Reset engine to restart at space \a s
    virtual void reset(Space* s) = 0;
    /// Return no-goods
    virtual NoGoods& nogoods(void) = 0;
    /// Destructor
    virtual ~Engine(void) {}
  };

}}

namespace Gecode {

  /**
   *  \brief Base-class for search engines
   */
  class EngineBase {
    template<template<class>class,class> friend class ::Gecode::RBS;
  protected:
    /// The actual search engine
    Search::Engine* e;
    /// Destructor
    ~EngineBase(void);
    /// Constructor
    EngineBase(Search::Engine* e = NULL);
  };

}

#include <gecode/search/engine-base.hpp>

namespace Gecode {


  /**
   * \brief Depth-first search engine
   *
   * This class supports depth-first search for subclasses \a T of
   * Space.
   * \ingroup TaskModelSearch
   */
  template<class T>
  class DFS : public EngineBase {
  public:
    /// Initialize search engine for space \a s with options \a o
    DFS(T* s, const Search::Options& o=Search::Options::def);
    /// Return next solution (NULL, if none exists or search has been stopped)
    T* next(void);
    /// Return statistics
    Search::Statistics statistics(void) const;
    /// Check whether engine has been stopped
    bool stopped(void) const;
    /// Return no-goods
    NoGoods& nogoods(void);
  };

  /// Invoke depth-first search engine for subclass \a T of space \a s with options \a o
  template<class T>
  T* dfs(T* s, const Search::Options& o=Search::Options::def);

}

#include <gecode/search/dfs.hpp>

namespace Gecode {

  /**
   * \brief Depth-first branch-and-bound search engine
   *
   * Additionally, \a s must implement a member function
   * \code virtual void constrain(const T& t) \endcode
   * Whenever exploration requires to add a constraint
   * to the space \a c currently being explored, the engine
   * executes \c c.constrain(t) where \a t is the so-far
   * best solution.
   * \ingroup TaskModelSearch
   */
  template<class T>
  class BAB : public EngineBase {
  public:
    /// Initialize engine for space \a s and options \a o
    BAB(T* s, const Search::Options& o=Search::Options::def);
    /// Return next better solution (NULL, if none exists or search has been stopped)
    T* next(void);
    /// Return statistics
    Search::Statistics statistics(void) const;
    /// Check whether engine has been stopped
    bool stopped(void) const;
    /// Return no-goods
    NoGoods& nogoods(void);
  };

  /**
   * \brief Perform depth-first branch-and-bound search for subclass \a T of space \a s and options \a o
   *
   * Additionally, \a s must implement a member function
   * \code virtual void constrain(const T& t) \endcode
   * Whenever exploration requires to add a constraint
   * to the space \a c currently being explored, the engine
   * executes \c c.constrain(t) where \a t is the so-far
   * best solution.
   *
   * \ingroup TaskModelSearch
   */
  template<class T>
  T* bab(T* s, const Search::Options& o=Search::Options::def);

}

#include <gecode/search/bab.hpp>

namespace Gecode {

  /**
   * \brief Meta-engine performing restart-based search
   *
   * The engine uses the Cutoff sequence supplied in the options \a o to
   * periodically restart the search of engine \a E.
   *
   * The class \a T can implement member functions
   * \code virtual void master(unsigned long int i, const Space* s) \endcode
   * and
   * \code virtual void slave(unsigned long int i, const Space* s) \endcode
   *
   * Whenever exploration restarts or a solution is found, the
   * engine executes the functions on the master and slave
   * space. For more details, consult "Modeling and Programming
   * with Gecode".
   *
   * \ingroup TaskModelSearch
   */
  template<template<class> class E, class T>
  class RBS : public EngineBase {
  public:
    /// Initialize engine for space \a s and options \a o
    RBS(T* s, const Search::Options& o);
    /// Return next solution (NULL, if non exists or search has been stopped)
    T* next(void);
    /// Return statistics
    Search::Statistics statistics(void) const;
    /// Check whether engine has been stopped
    bool stopped(void) const;
  };

  /**
   * \brief Perform restart-based search
   *
   * The engine uses the Cutoff sequence supplied in the options \a o to
   * periodically restart the search of an engine of type \a E.
   *
   * The class \a T can implement member functions
   * \code virtual void master(unsigned long int i, const Space* s) \endcode
   * and
   * \code virtual void slave(unsigned long int i, const Space* s) \endcode
   *
   * Whenever exploration restarts or a solution is found, the
   * engine executes the functions on the master and slave
   * space. For more details, consult "Modeling and Programming
   * with Gecode".
   *
   * \ingroup TaskModelSearch
   */
  template<template<class> class E, class T>
  T* rbs(T* s, const Search::Options& o);

}

#include <gecode/search/rbs.hpp>

#endif

// STATISTICS: search-other
