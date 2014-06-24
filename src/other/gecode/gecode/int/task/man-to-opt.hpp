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

namespace Gecode { namespace Int {

  template<class ManTask>
  forceinline
  ManToOptTask<ManTask>::ManToOptTask(void) {}

  template<class ManTask>
  forceinline bool
  ManToOptTask<ManTask>::mandatory(void) const {
    return _m.one();
  }
  template<class ManTask>
  forceinline bool
  ManToOptTask<ManTask>::excluded(void) const {
    return _m.zero();
  }
  template<class ManTask>
  forceinline bool
  ManToOptTask<ManTask>::optional(void) const {
    return _m.none();
  }

  template<class ManTask>
  forceinline bool
  ManToOptTask<ManTask>::assigned(void) const {
    return ManTask::assigned() && _m.assigned();
  }

  template<class ManTask>
  forceinline ModEvent 
  ManToOptTask<ManTask>::mandatory(Space& home) {
    return _m.one(home);
  }
  template<class ManTask>
  forceinline ModEvent 
  ManToOptTask<ManTask>::excluded(Space& home) {
    return _m.zero(home);
  }

  template<class ManTask>
  forceinline void
  ManToOptTask<ManTask>::update(Space& home, bool share, 
                                ManToOptTask<ManTask>& t) {
    ManTask::update(home, share, t);
    _m.update(home,share,t._m);
  }

  template<class ManTask>
  forceinline void
  ManToOptTask<ManTask>::subscribe(Space& home, Propagator& p, PropCond pc) {
    ManTask::subscribe(home, p, pc);
    _m.subscribe(home, p, Int::PC_BOOL_VAL);
  }
  template<class ManTask>
  forceinline void
  ManToOptTask<ManTask>::cancel(Space& home, Propagator& p, PropCond pc) {
    _m.cancel(home, p, Int::PC_BOOL_VAL);
    ManTask::cancel(home, p, pc);
  }

}}

// STATISTICS: int-var
