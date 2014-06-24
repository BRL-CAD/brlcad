/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

#include <climits>

namespace Gecode { namespace Int { namespace Distinct {

  /*
   * The propagation controller
   *
   */

  template<class View>
  forceinline
  DomCtrl<View>::DomCtrl(void) {}

  template<class View>
  forceinline bool
  DomCtrl<View>::available(void) {
    return g.initialized();
  }

  template<class View>
  ExecStatus
  DomCtrl<View>::init(Space& home, ViewArray<View>& x) {
    return g.init(home,x);
  }

  template<class View>
  ExecStatus
  DomCtrl<View>::sync(Space& home) {
    g.purge();
    return g.sync(home) ? ES_OK : ES_FAILED;
  }

  template<class View>
  ExecStatus
  DomCtrl<View>::propagate(Space& home, bool& assigned) {
    if (!g.mark(home))
      return ES_OK;
    return g.prune(home,assigned);
  }

}}}

// STATISTICS: int-prop

