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

namespace Gecode { namespace Int { namespace Unary {
  
  template<class ManTask>
  forceinline
  ManProp<ManTask>::ManProp(Home home, TaskArray<ManTask>& t)
    : TaskProp<ManTask,Int::PC_INT_BND>(home,t) {}

  template<class ManTask>
  forceinline
  ManProp<ManTask>::ManProp(Space& home, bool shared, 
                                ManProp<ManTask>& p) 
    : TaskProp<ManTask,Int::PC_INT_BND>(home,shared,p) {}

  template<class ManTask>
  forceinline ExecStatus 
  ManProp<ManTask>::post(Home home, TaskArray<ManTask>& t) {
    if (t.size() > 1)
      (void) new (home) ManProp<ManTask>(home,t);
    return ES_OK;
  }

  template<class ManTask>
  Actor* 
  ManProp<ManTask>::copy(Space& home, bool share) {
    return new (home) ManProp<ManTask>(home,share,*this);
  }

  template<class ManTask>
  ExecStatus 
  ManProp<ManTask>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ES_CHECK(overload(home,t));
    GECODE_ES_CHECK(detectable(home,t));
    GECODE_ES_CHECK(notfirstnotlast(home,t));
    GECODE_ES_CHECK(edgefinding(home,t));
    GECODE_ES_CHECK(subsumed(home,*this,t));
    return ES_NOFIX;
  }

}}}

// STATISTICS: int-prop
