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

#ifndef __GECODE_SEARCH_PARALLEL_ENGINE_HH__
#define __GECODE_SEARCH_PARALLEL_ENGINE_HH__

#include <gecode/search.hh>
#include <gecode/search/support.hh>
#include <gecode/search/worker.hh>
#include <gecode/search/parallel/path.hh>

namespace Gecode { namespace Search { namespace Parallel {

  /// %Parallel depth-first search engine
  class Engine : public Search::Engine {
  protected:
    /// %Parallel depth-first search worker
    class Worker : public Search::Worker, public Support::Runnable {
    protected:
      /// Reference to engine
      Engine& _engine;
      /// Mutex for access to worker
      Support::Mutex m;
      /// Current path ins search tree
      Path path;
      /// Current space being explored
      Space* cur;
      /// Distance until next clone
      unsigned int d;
      /// Whether the worker is idle
      bool idle;
    public:
      /// Initialize for space \a s with engine \a e
      Worker(Space* s, Engine& e);
      /// Hand over some work (NULL if no work available)
      Space* steal(unsigned long int& d);
      /// Return statistics
      Statistics statistics(void);
      /// Provide access to engine
      Engine& engine(void) const;
      /// Return no-goods
      NoGoods& nogoods(void);
      /// Destructor
      virtual ~Worker(void);
    };
    /// Search options
    const Options _opt;
  public:
    /// Provide access to search options
    const Options& opt(void) const;
    /// Return number of workers
    unsigned int workers(void) const;
    
    /// \name Commands from engine to workers and wait management
    //@{
    /// Commands from engine to workers
    enum Cmd {
      C_WORK,     ///< Perform work
      C_WAIT,     ///< Run into wait lock
      C_RESET,    ///< Perform reset operation
      C_TERMINATE ///< Terminate
    };
  protected:
    /// The current command
    volatile Cmd _cmd;
    /// Mutex for forcing workers to wait
    Support::Mutex _m_wait;
  public:
    /// Return current command
    Cmd cmd(void) const;
    /// Block all workers
    void block(void);
    /// Release all workers
    void release(Cmd c);
    /// Ensure that worker waits
    void wait(void);
    //@}

    /// \name Termination control
    //@{
  protected:
    /// Mutex for access to termination information
    Support::Mutex _m_term;
    /// Number of workers that have not yet acknowledged termination
    volatile unsigned int _n_term_not_ack;
    /// Event for termination acknowledgment
    Support::Event _e_term_ack;
    /// Mutex for waiting for termination
    Support::Mutex _m_wait_terminate;
    /// Number of not yet terminated workers
    volatile unsigned int _n_not_terminated;
    /// Event for termination (all threads have terminated)
    Support::Event _e_terminate;
  public:
    /// For worker to acknowledge termination command
    void ack_terminate(void);
    /// For worker to register termination
    void terminated(void);
    /// For worker to wait until termination is legal
    void wait_terminate(void);
    /// For engine to peform thread termination
    void terminate(void);
    //@}

    /// \name Reset control
    //@{
  protected:
    /// Mutex for access to reset information
    Support::Mutex _m_reset;
    /// Number of workers that have not yet acknowledged reset
    volatile unsigned int _n_reset_not_ack;
    /// Event for reset acknowledgment started
    Support::Event e_reset_ack_start;
    /// Event for reset acknowledgment stopped
    Support::Event e_reset_ack_stop;
    /// Mutex for waiting for reset
    Support::Mutex m_wait_reset;
  public:
    /// For worker to acknowledge start of reset cycle
    void ack_reset_start(void);
    /// For worker to acknowledge stop of reset cycle
    void ack_reset_stop(void);
    /// For worker to wait for all workers to reset
    void wait_reset(void);
    //@}

    /// \name Search control
    //@{
  protected:
    /// Mutex for search
    Support::Mutex m_search;
    /// Event for search (solution found, no more solutions, search stopped)
    Support::Event e_search;
    /// Queue of solutions
    Support::DynamicQueue<Space*,Heap> solutions;
    /// Number of busy workers
    volatile unsigned int n_busy;
    /// Whether a worker had been stopped
    volatile bool has_stopped;
    /// Whether search state changed such that signal is needed
    bool signal(void) const;
  public:
    /// Report that worker is idle
    void idle(void);
    /// Report that worker is busy
    void busy(void);
    /// Report that worker has been stopped
    void stop(void);
    //@}

    /// \name Engine interface
    //@{
    /// Initialize with options \a o
    Engine(const Options& o);
    /// Return next solution (NULL, if none exists or search has been stopped)
    virtual Space* next(void);
    /// Check whether engine has been stopped
    virtual bool stopped(void) const;
    //@}
  };


  /*
   * Basic access routines
   */
  forceinline Engine&
  Engine::Worker::engine(void) const {
    return _engine;
  }
  forceinline const Options&
  Engine::opt(void) const {
    return _opt;
  }
  forceinline unsigned int
  Engine::workers(void) const {
    return static_cast<unsigned int>(opt().threads);
  }


  /*
   * Engine: command and wait handling
   */
  forceinline Engine::Cmd
  Engine::cmd(void) const {
    return _cmd;
  }
  forceinline void 
  Engine::block(void) {
    _cmd = C_WAIT;
    _m_wait.acquire();
  }
  forceinline void 
  Engine::release(Cmd c) {
    _cmd = c;
    _m_wait.release();
  }
  forceinline void 
  Engine::wait(void) {
    _m_wait.acquire(); _m_wait.release();
  }


  /*
   * Engine: initialization
   */
  forceinline
  Engine::Worker::Worker(Space* s, Engine& e)
    : _engine(e), 
      path(s == NULL ? 0 : static_cast<int>(e.opt().nogoods_limit)), d(0), 
      idle(false) {
    if (s != NULL) {
      if (s->status(*this) == SS_FAILED) {
        fail++;
        cur = NULL;
        if (!engine().opt().clone)
          delete s;
      } else {
        cur = snapshot(s,engine().opt(),false);
      }
    } else {
      cur = NULL;
    }
  }

  forceinline
  Engine::Engine(const Options& o)
    : _opt(o), solutions(heap) {
    // Initialize termination information
    _n_term_not_ack = workers();
    _n_not_terminated = workers();
    // Initialize search information
    n_busy = workers();
    has_stopped = false;
    // Initialize reset information
    _n_reset_not_ack = workers();
  }


  /*
   * Statistics
   */
  forceinline Statistics
  Engine::Worker::statistics(void) {
    m.acquire();
    Statistics s = *this;
    m.release();
    return s;
  }


  /*
   * Engine: search control
   */
  forceinline bool
  Engine::signal(void) const {
    return solutions.empty() && (n_busy > 0) && !has_stopped;
  }
  forceinline void 
  Engine::idle(void) {
    m_search.acquire();
    bool bs = signal();
    n_busy--;
    if (bs && (n_busy == 0))
      e_search.signal();
    m_search.release();
  }
  forceinline void 
  Engine::busy(void) {
    m_search.acquire();
    assert(n_busy > 0);
    n_busy++;
    m_search.release();
  }
  forceinline void 
  Engine::stop(void) {
    m_search.acquire();
    bool bs = signal();
    has_stopped = true;
    if (bs)
      e_search.signal();
    m_search.release();
  }
  

  /*
   * Engine: termination control
   */
  forceinline void 
  Engine::terminated(void) {
    unsigned int n;
    _m_term.acquire();
    n = --_n_not_terminated;
    _m_term.release();
    // The signal must be outside of the look, otherwise a thread might be 
    // terminated that still holds a mutex.
    if (n == 0)
      _e_terminate.signal();
  }

  forceinline void 
  Engine::ack_terminate(void) {
    _m_term.acquire();
    if (--_n_term_not_ack == 0)
      _e_term_ack.signal();
    _m_term.release();
  }

  forceinline void 
  Engine::wait_terminate(void) {
    _m_wait_terminate.acquire();
    _m_wait_terminate.release();
  }

  forceinline void
  Engine::terminate(void) {
    // Grab the wait mutex for termination
    _m_wait_terminate.acquire();
    // Release all threads
    release(C_TERMINATE);
    // Wait until all threads have acknowledged termination request
    _e_term_ack.wait();
    // Release waiting threads
    _m_wait_terminate.release();    
    // Wait until all threads have in fact terminated
    _e_terminate.wait();
    // Now all threads are terminated!
  }



  /*
   * Engine: reset control
   */
  forceinline void 
  Engine::ack_reset_start(void) {
    _m_reset.acquire();
    if (--_n_reset_not_ack == 0)
      e_reset_ack_start.signal();
    _m_reset.release();
  }

  forceinline void 
  Engine::ack_reset_stop(void) {
    _m_reset.acquire();
    if (++_n_reset_not_ack == workers())
      e_reset_ack_stop.signal();
    _m_reset.release();
  }

  forceinline void 
  Engine::wait_reset(void) {
    m_wait_reset.acquire();
    m_wait_reset.release();
  }



  /*
   * Worker: finding and stealing working
   */
  forceinline Space*
  Engine::Worker::steal(unsigned long int& d) {
    /*
     * Make a quick check whether the worker might have work
     *
     * If that is not true any longer, the worker will be asked
     * again eventually.
     */
    if (!path.steal())
      return NULL;
    m.acquire();
    Space* s = path.steal(*this,d);
    m.release();
    // Tell that there will be one more busy worker
    if (s != NULL) 
      engine().busy();
    return s;
  }

  /*
   * Return No-Goods
   */
  forceinline NoGoods&
  Engine::Worker::nogoods(void) {
    return path;
  }

}}}

#endif

// STATISTICS: search-parallel
