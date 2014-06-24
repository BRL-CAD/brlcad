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

#include <gecode/support.hh>

#ifdef GECODE_HAS_THREADS

#include <gecode/search/parallel/engine.hh>

namespace Gecode { namespace Search { namespace Parallel {

  /*
   * Engine: search control
   */
  Space* 
  Engine::next(void) {
    // Invariant: the worker holds the wait mutex
    m_search.acquire();
    if (!solutions.empty()) {
      // No search needs to be done, take leftover solution
      Space* s = solutions.pop();
      m_search.release();
      return s;
    }
    // We ignore stopped (it will be reported again if needed)
    has_stopped = false;
    // No more solutions?
    if (n_busy == 0) {
      m_search.release();
      return NULL;
    }
    m_search.release();
    // Okay, now search has to continue, make the guys work
    release(C_WORK);

    /*
     * Wait until a search related event has happened. It might be that
     * the event has already been signalled in the last run, but the
     * solution has been removed. So we have to try until there has
     * something new happened.
     */
    while (true) {
      e_search.wait();
      m_search.acquire();
      if (!solutions.empty()) {
        // Report solution
        Space* s = solutions.pop();
        m_search.release();
        // Make workers wait again
        block();
        return s;
      }
      // No more solutions or stopped?
      if ((n_busy == 0) || has_stopped) {
        m_search.release();
        // Make workers wait again
        block();
        return NULL;
      }
      m_search.release();
    }
    GECODE_NEVER;
    return NULL;
  }

  bool 
  Engine::stopped(void) const {
    return has_stopped;
  }

  /*
   * Termination and deletion
   */
  Engine::Worker::~Worker(void) {
    delete cur;
    path.reset(0);
  }

}}}

#endif

// STATISTICS: search-parallel
