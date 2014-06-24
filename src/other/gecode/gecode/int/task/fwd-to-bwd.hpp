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

  /*
   * Task mapper: map forward task to view to backward task View
   */
  template<class TaskView>
  forceinline int 
  FwdToBwd<TaskView>::est(void) const {
    return -TaskView::lct();
  }
  template<class TaskView>
  forceinline int
  FwdToBwd<TaskView>::ect(void) const {
    return -TaskView::lst();
  }
  template<class TaskView>
  forceinline int
  FwdToBwd<TaskView>::lst(void) const {
    return -TaskView::ect();
  }
  template<class TaskView>
  forceinline int
  FwdToBwd<TaskView>::lct(void) const {
    return -TaskView::est();
  }
  template<class TaskView>
  forceinline int
  FwdToBwd<TaskView>::pmin(void) const {
    return TaskView::pmin();
  }
  template<class TaskView>
  forceinline int
  FwdToBwd<TaskView>::pmax(void) const {
    return TaskView::pmax();
  }

  template<class TaskView>
  forceinline ModEvent 
  FwdToBwd<TaskView>::est(Space& home, int n) {
    return TaskView::lct(home,-n);
  }
  template<class TaskView>
  forceinline ModEvent
  FwdToBwd<TaskView>::ect(Space& home, int n) {
    return TaskView::lst(home,-n);
  }
  template<class TaskView>
  forceinline ModEvent
  FwdToBwd<TaskView>::lst(Space& home, int n) {
    return TaskView::ect(home,-n);
  }
  template<class TaskView>
  forceinline ModEvent
  FwdToBwd<TaskView>::lct(Space& home, int n) {
    return TaskView::est(home,-n);
  }
  template<class TaskView>
  forceinline ModEvent
  FwdToBwd<TaskView>::norun(Space& home, int e, int l) {
    return TaskView::norun(home,-l,-e);
  }

}}

// STATISTICS: int-var
