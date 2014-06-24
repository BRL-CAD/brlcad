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
   * Mutex
   */
  forceinline
  Mutex::Mutex(void) {}
  forceinline void
  Mutex::acquire(void) {}
  forceinline bool
  Mutex::tryacquire(void) {
    return true;
  }
  forceinline void
  Mutex::release(void) {}
  forceinline
  Mutex::~Mutex(void) {}


  /*
   * Event
   */
  forceinline
  Event::Event(void) {}
  forceinline void
  Event::signal(void) {}
  forceinline void
  Event::wait(void) {}
  forceinline
  Event::~Event(void) {}


  /*
   * Thread
   */
  inline
  Thread::Run::Run(Runnable*) {
    throw OperatingSystemError("Thread::run[Threads not supported]");
  }
  forceinline void
  Thread::sleep(unsigned int) {}
  forceinline unsigned int
  Thread::npu(void) {
    return 1;
  }


}}

// STATISTICS: support-any
