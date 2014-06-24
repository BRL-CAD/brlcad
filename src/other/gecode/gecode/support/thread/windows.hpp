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
  Mutex::Mutex(void) {
    InitializeCriticalSection(&w_cs);
  }
  forceinline void
  Mutex::acquire(void) {
    EnterCriticalSection(&w_cs);
  }
  forceinline bool
  Mutex::tryacquire(void) {
    return TryEnterCriticalSection(&w_cs) != 0;
  }
  forceinline void
  Mutex::release(void) {
    LeaveCriticalSection(&w_cs);
  }
  forceinline
  Mutex::~Mutex(void) {
    DeleteCriticalSection(&w_cs);
  }


  /*
   * Event
   */
  forceinline
  Event::Event(void)
    : w_h(CreateEvent(NULL, FALSE, FALSE, NULL)) {
    if (w_h == NULL)
      throw OperatingSystemError("Event::Event[Windows::CreateEvent]");
  }
  forceinline void
  Event::signal(void) {
    if (SetEvent(w_h) == 0)
      throw OperatingSystemError("Event::signal[Windows::SetEvent]");
  }
  forceinline void
  Event::wait(void) {
    if (WaitForSingleObject(w_h,INFINITE) != 0)
      throw OperatingSystemError("Event::wait[Windows::WaitForSingleObject]");
  }
  forceinline
  Event::~Event(void) {
    if (CloseHandle(w_h) == 0)
      throw OperatingSystemError("Event::~Event[Windows::CloseHandle]");
  }


  /*
   * Thread
   */
  forceinline void
  Thread::sleep(unsigned int ms) {
    Sleep(static_cast<DWORD>(ms));
  }

  forceinline unsigned int
  Thread::npu(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return static_cast<unsigned int>(si.dwNumberOfProcessors);
  }

}}

// STATISTICS: support-any
