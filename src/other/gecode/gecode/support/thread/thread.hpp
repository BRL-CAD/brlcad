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

namespace Gecode { namespace Support {

  /*
   * Runnable objects
   */
  forceinline void
  Runnable::operator delete(void* p) {
    heap.rfree(p);
  }
  forceinline void*
  Runnable::operator new(size_t s) {
    return heap.ralloc(s);
  }


  /*
   * Mutexes
   *
   */
  forceinline void*
  Mutex::operator new(size_t s) {
    return Gecode::heap.ralloc(s);
  }

  forceinline void
  Mutex::operator delete(void* p) {
    Gecode::heap.rfree(p);
  }

#if defined(GECODE_THREADS_OSX) || defined(GECODE_THREADS_PTHREADS_SPINLOCK)

  /*
   * Fast mutexes
   *
   */
  forceinline void*
  FastMutex::operator new(size_t s) {
    return Gecode::heap.ralloc(s);
  }

  forceinline void
  FastMutex::operator delete(void* p) {
    Gecode::heap.rfree(p);
  }

#endif

  /*
   * Locks
   */
  forceinline
  Lock::Lock(Mutex& m0) : m(m0) {
    m.acquire();
  }
  forceinline
  Lock::~Lock(void) {
    m.release();
  }


  /*
   * Threads
   */
  inline void
  Thread::Run::run(Runnable* r0) {
    m.acquire();
    r = r0;
    m.release();
    e.signal();
  }
  inline void
  Thread::run(Runnable* r) {
    m()->acquire();
    if (idle != NULL) {
      Run* i = idle;
      idle = idle->n;
      m()->release();
      i->run(r);
    } else {
      m()->release();
      (void) new Run(r);
    }
  }
  forceinline void
  Thread::Run::operator delete(void* p) {
    heap.rfree(p);
  }
  forceinline void*
  Thread::Run::operator new(size_t s) {
    return heap.ralloc(s);
  }

}}

// STATISTICS: support-any
