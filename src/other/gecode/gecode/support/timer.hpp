/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
 *     Mikael Lagerkvist, 2009
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

#ifdef GECODE_USE_GETTIMEOFDAY
#include <sys/time.h>
#endif

#ifdef GECODE_USE_CLOCK
#include <ctime>
#endif

namespace Gecode { namespace Support {

  /** \brief %Timer
   * 
   * This class represents a best-effort at measuring wall-clock time
   * in milliseconds.
   *
   * \ingroup FuncSupport
   */
  class GECODE_SUPPORT_EXPORT Timer {
  private:
#if   defined(GECODE_USE_GETTIMEOFDAY)
    timeval t0; ///< Start time
#elif defined(GECODE_USE_CLOCK)
    clock_t t0; ///< Start time
#endif
  public:
    /// Start timer
    void start(void);
    /// Get time since start of timer
    double stop(void);
  };

  inline void
  Timer::start(void) {
#if   defined(GECODE_USE_GETTIMEOFDAY)
    if (gettimeofday(&t0, NULL))
      throw OperatingSystemError("Timer::start[gettimeofday]");
#elif defined(GECODE_USE_CLOCK)
    t0 = clock();
#endif
  }

  inline double
  Timer::stop(void) {
#if   defined(GECODE_USE_GETTIMEOFDAY)
    timeval t1, t;
    if (gettimeofday(&t1, NULL))
      throw OperatingSystemError("Timer::stop[gettimeofday]");
    
    // t = t1 - t2
    t.tv_sec = t1.tv_sec - t0.tv_sec;
    t.tv_usec = t1.tv_usec - t0.tv_usec;
    if (t.tv_usec < 0) {
      t.tv_sec--;
      t.tv_usec += 1000000;
    }
    
    return (static_cast<double>(t.tv_sec) * 1000.0) + 
      (static_cast<double>(t.tv_usec)/1000.0);
#elif defined(GECODE_USE_CLOCK)
    return (static_cast<double>(clock()-t0) / CLOCKS_PER_SEC) * 1000.0;
#endif
  }

}}

// STATISTICS: support-any
