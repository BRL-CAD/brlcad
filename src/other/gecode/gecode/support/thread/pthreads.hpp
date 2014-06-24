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

#ifdef GECODE_HAS_UNISTD_H
#include <unistd.h>
#endif

namespace Gecode { namespace Support {

  /*
   * Mutex
   */
  forceinline
  Mutex::Mutex(void) {
    if (pthread_mutex_init(&p_m,NULL) != 0)
      throw OperatingSystemError("Mutex::Mutex[pthread_mutex_init]");
  }
  forceinline void
  Mutex::acquire(void) {
    if (pthread_mutex_lock(&p_m) != 0)
      throw OperatingSystemError("Mutex::acquire[pthread_mutex_lock]");
  }
  forceinline bool
  Mutex::tryacquire(void) {
    return pthread_mutex_trylock(&p_m) == 0;
  }
  forceinline void
  Mutex::release(void) {
    if (pthread_mutex_unlock(&p_m) != 0)
      throw OperatingSystemError("Mutex::release[pthread_mutex_unlock]");
  }
  forceinline
  Mutex::~Mutex(void) {
    if (pthread_mutex_destroy(&p_m) != 0)
      throw OperatingSystemError("Mutex::~Mutex[pthread_mutex_destroy]");
  }

#ifdef GECODE_THREADS_OSX

  /*
   * FastMutex
   */
  forceinline
  FastMutex::FastMutex(void) : lck(OS_SPINLOCK_INIT) {}
  forceinline void
  FastMutex::acquire(void) {
    OSSpinLockLock(&lck);
  }
  forceinline bool
  FastMutex::tryacquire(void) {
    return OSSpinLockTry(&lck);
  }
  forceinline void
  FastMutex::release(void) {
    OSSpinLockUnlock(&lck);
  }
  forceinline
  FastMutex::~FastMutex(void) {}

#endif

#ifdef GECODE_THREADS_PTHREADS_SPINLOCK

  /*
   * FastMutex
   */
  forceinline
  FastMutex::FastMutex(void) {
    if (pthread_spin_init(&p_s,PTHREAD_PROCESS_PRIVATE) != 0)
      throw OperatingSystemError("FastMutex::FastMutex[pthread_spin_init]");
  }
  forceinline void
  FastMutex::acquire(void) {
    if (pthread_spin_lock(&p_s) != 0)
      throw OperatingSystemError("FastMutex::acquire[pthread_spin_lock]");
  }
  forceinline bool
  FastMutex::tryacquire(void) {
    return pthread_spin_trylock(&p_s) == 0;
  }
  forceinline void
  FastMutex::release(void) {
    if (pthread_spin_unlock(&p_s) != 0)
      throw OperatingSystemError("FastMutex::release[pthread_spin_unlock]");
  }
  forceinline
  FastMutex::~FastMutex(void) {
    if (pthread_spin_destroy(&p_s) != 0)
      throw OperatingSystemError(
        "FastMutex::~FastMutex[pthread_spin_destroy]");
  }

#endif

  /*
   * Event
   */
  forceinline
  Event::Event(void) : p_s(false) {
    if (pthread_mutex_init(&p_m,NULL) != 0)
      throw OperatingSystemError("Event::Event[pthread_mutex_init]");
    if (pthread_cond_init(&p_c,NULL) != 0)
      throw OperatingSystemError("Event::Event[pthread_cond_init]");
  }
  forceinline void
  Event::signal(void) {
    if (pthread_mutex_lock(&p_m) != 0)
      throw OperatingSystemError("Event::signal[pthread_mutex_lock]");
    if (!p_s) {
      p_s = true;
      if (pthread_cond_signal(&p_c) != 0)
        throw OperatingSystemError("Event::signal[pthread_cond_signal]");
    }
    if (pthread_mutex_unlock(&p_m) != 0)
      throw OperatingSystemError("Event::signal[pthread_mutex_unlock]");
  }
  forceinline void
  Event::wait(void) {
    if (pthread_mutex_lock(&p_m) != 0)
      throw OperatingSystemError("Event::wait[pthread_mutex_lock]");
    while (!p_s)
      if (pthread_cond_wait(&p_c,&p_m) != 0)
        throw OperatingSystemError("Event::wait[pthread_cond_wait]");
    p_s = false;
    if (pthread_mutex_unlock(&p_m) != 0)
      throw OperatingSystemError("Event::wait[pthread_mutex_unlock]");
  }
  forceinline
  Event::~Event(void) {
    if (pthread_cond_destroy(&p_c) != 0)
      throw OperatingSystemError("Event::~Event[pthread_cond_destroy]");
    if (pthread_mutex_destroy(&p_m) != 0)
      throw OperatingSystemError("Event::~Event[pthread_mutex_destroy]");
  }


  /*
   * Thread
   */
  forceinline void
  Thread::sleep(unsigned int ms) {
#ifdef GECODE_HAS_UNISTD_H
    unsigned int s = ms / 1000;
    ms -= 1000 * s;
    if (s > 0) {
      // More than one million microseconds, use sleep
      ::sleep(s);
    }
    usleep(ms * 1000);
#endif
  }
  forceinline unsigned int
  Thread::npu(void) {
#ifdef GECODE_HAS_UNISTD_H
    int n=static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    return (n>1) ? n : 1;
#else
    return 1;
#endif
  }

}}

// STATISTICS: support-any
