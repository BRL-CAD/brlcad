/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

namespace Gecode {

  /*
   * The reification class
   *
   */

  forceinline
  Reify::Reify(void)
    : rm(RM_EQV) {}
  forceinline
  Reify::Reify(BoolVar x0, ReifyMode rm0)
    : x(x0), rm(rm0) {}
  forceinline BoolVar
  Reify::var(void) const {
    return x;
  }
  forceinline void
  Reify::var(BoolVar x0) {
    x = x0;
  }
  forceinline ReifyMode
  Reify::mode(void) const {
    return rm;
  }
  forceinline void
  Reify::mode(ReifyMode rm0) {
    rm = rm0;
  }

  /*
   * Reification convenience functions
   *
   */
  forceinline Reify
  eqv(BoolVar x) {
    return Reify(x,RM_EQV);
  }
  forceinline Reify
  imp(BoolVar x) {
    return Reify(x,RM_IMP);
  }
  forceinline Reify
  pmi(BoolVar x) {
    return Reify(x,RM_PMI);
  }

}

// STATISTICS: int-post

