/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
 *
 *  Bugfixes provided by:
 *     David Rijsman <david.rijsman@quintiq.com>
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

#include <cstddef>

#ifdef GECODE_THREADS_WINDOWS

#ifndef NOMINMAX
#  define NOMINMAX
#endif

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x400
#endif

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#endif

#ifdef GECODE_THREADS_PTHREADS

#include <pthread.h>

#ifdef GECODE_THREADS_OSX

#include <libkern/OSAtomic.h>

#endif

#endif

/**
 * \defgroup FuncSupportThread Simple thread and synchronization support
 *
 * This is very simplistic, just enough for parallel search engines. Do
 * not mistake it for a full-fledged thread package.
 *
 * If the platform supports threads, the macro GECODE_HAS_THREADS is 
 * defined. If threads are not supported, all classes are
 * still available, but are noops with the exception of trying to
 * create a new thread which will throw an exception.
 *
 *
 * \ingroup FuncSupport
 */  

namespace Gecode { namespace Support {

  /**
   * \brief A mutex for mutual exclausion among several threads
   * 
   * It is not specified whether the mutex is recursive or not.
   * Likewise, there is no guarantee of fairness among the
   * blocking threads.
   *
   * \ingroup FuncSupportThread
   */
  class Mutex {
  private:
#ifdef GECODE_THREADS_WINDOWS
    /// Use a simple but more efficient critical section on Windows
    CRITICAL_SECTION w_cs;
#endif
#ifdef GECODE_THREADS_PTHREADS
    /// The Pthread mutex
    pthread_mutex_t p_m;
#endif
  public:
    /// Initialize mutex
    Mutex(void);
    /// Acquire the mutex and possibly block
    void acquire(void);
    /// Try to acquire the mutex, return true if succesful
    bool tryacquire(void);
    /// Release the mutex
    void release(void);
    /// Delete mutex
    ~Mutex(void);
    /// Allocate memory from heap
    static void* operator new(size_t s);
    /// Free memory allocated from heap
    static void  operator delete(void* p);
  private:
    /// A mutex cannot be copied
    Mutex(const Mutex&) {}
    /// A mutex cannot be assigned
    void operator=(const Mutex&) {}
  };

#if defined(GECODE_THREADS_WINDOWS) || !defined(GECODE_THREADS_PTHREADS)

  typedef Mutex FastMutex;

#endif

#ifdef GECODE_THREADS_PTHREADS

#if defined(GECODE_THREADS_OSX) || defined(GECODE_THREADS_PTHREADS_SPINLOCK)

  /**
   * \brief A fast mutex for mutual exclausion among several threads
   * 
   * This mutex is implemeneted using spin locks on some platforms
   * and is not guaranteed to be compatible with events. It should be used
   * for low-contention locks that are only acquired for short periods of 
   * time.
   *
   * It is not specified whether the mutex is recursive or not.
   * Likewise, there is no guarantee of fairness among the
   * blocking threads.
   *
   * \ingroup FuncSupportThread
   */
  class FastMutex {
  private:
#ifdef GECODE_THREADS_OSX
    /// The OSX spin lock
    OSSpinLock lck;
#else
    /// The Pthread spinlock
    pthread_spinlock_t p_s;
#endif
  public:
    /// Initialize mutex
    FastMutex(void);
    /// Acquire the mutex and possibly block
    void acquire(void);
    /// Try to acquire the mutex, return true if succesful
    bool tryacquire(void);
    /// Release the mutex
    void release(void);
    /// Delete mutex
    ~FastMutex(void);
    /// Allocate memory from heap
    static void* operator new(size_t s);
    /// Free memory allocated from heap
    static void  operator delete(void* p);
  private:
    /// A mutex cannot be copied
    FastMutex(const FastMutex&) {}
    /// A mutex cannot be assigned
    void operator=(const FastMutex&) {}
  };

#else

  typedef Mutex FastMutex;

#endif

#endif

  /**
   * \brief A lock as a scoped frontend for a mutex
   *
   * \ingroup FuncSupportThread
   */
  class Lock {
  private:
    /// The mutex used for the lock
    Mutex& m;
  public:
    /// Enter lock
    Lock(Mutex& m0);
    /// Leave lock
    ~Lock(void);
  private:
    /// A lock cannot be copied
    Lock(const Lock& l) : m(l.m) {}
    /// A lock cannot be assigned
    void operator=(const Lock&) {}
  };

  /**
   * \brief An event for synchronization
   * 
   * An event can be waited on by a single thread until the event is
   * signalled.
   *
   * \ingroup FuncSupportThread
   */
  class Event {
  private:
#ifdef GECODE_THREADS_WINDOWS
    /// The Windows specific handle to an event
    HANDLE w_h;    
#endif
#ifdef GECODE_THREADS_PTHREADS
    /// The Pthread mutex
    pthread_mutex_t p_m;
    /// The Pthread condition variable
    pthread_cond_t p_c;
    /// Whether the event is signalled
    bool p_s;
#endif
  public:
    /// Initialize event
    Event(void);
    /// Signal the event
    void signal(void);
    /// Wait until the event becomes signalled
    void wait(void);
    /// Delete event
    ~Event(void);
  private:
    /// An event cannot be copied
    Event(const Event&) {}
    /// An event cannot be assigned
    void operator=(const Event&) {}
  };

  /**
   * \brief An interface for objects that can be run by a thread
   *
   * \ingroup FuncSupportThread
   */
  class Runnable {
  public:
    /// The function that is executed when the thread starts
    virtual void run(void) = 0;
    /// Destructor
    virtual ~Runnable(void) {}
    /// Allocate memory from heap
    static void* operator new(size_t s);
    /// Free memory allocated from heap
    static void  operator delete(void* p);
  };

  /**
   * \brief Simple threads
   *
   * Threads cannot be created, only runnable objects can be submitted
   * for execution by a thread. Threads are pooled to avoid
   * creation/destruction of threads as much as possible.
   *
   * \ingroup FuncSupportThread
   */
  class Thread {
  public:
    /// A real thread
    class Run {
    public:
      /// Next idle thread
      Run* n;
      /// Runnable object to execute
      Runnable* r;
      /// Event to wait for next runnable object to execute
      Event e;
      /// Mutex for synchronization
      Mutex m;
      /// Create a new thread
      GECODE_SUPPORT_EXPORT Run(Runnable* r);
      /// Infinite loop for execution
      GECODE_SUPPORT_EXPORT void exec(void);
      /// Run a runnable object
      void run(Runnable* r);
      /// Allocate memory from heap
      static void* operator new(size_t s);
      /// Free memory allocated from heap
      static void  operator delete(void* p);
    };
    /// Mutex for synchronization
    GECODE_SUPPORT_EXPORT static Mutex* m(void);
    /// Idle runners
    GECODE_SUPPORT_EXPORT static Run* idle;
  public:
    /**
     * \brief Construct a new thread and run \a r
     *
     * After \a r terminates, \a r is deleted. After that, the thread
     * terminates.
     *
     * If the operatins system does not support any threads, throws an
     * exception of type Support::OperatingSystemError.
     */
    static void run(Runnable* r);
    /// Put current thread to sleep for \a ms milliseconds
    static void sleep(unsigned int ms);
    /// Return number of processing units (1 if information not available)
    static unsigned int npu(void);
  private:
    /// A thread cannot be copied
    Thread(const Thread&) {}
    /// A thread cannot be assigned
    void operator=(const Thread&) {}
  };

}}

// STATISTICS: support-any
