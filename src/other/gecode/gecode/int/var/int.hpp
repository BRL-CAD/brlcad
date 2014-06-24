/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

  forceinline void
  IntVar::_init(Space& home, int min, int max) {
    x = new (home) Int::IntVarImp(home,min,max);
  }

  forceinline void
  IntVar::_init(Space& home, const IntSet& ds) {
    x = new (home) Int::IntVarImp(home,ds);
  }

  forceinline
  IntVar::IntVar(void) {}
  forceinline
  IntVar::IntVar(const IntVar& y)
    : VarImpVar<Int::IntVarImp>(y.varimp()) {}
  forceinline
  IntVar::IntVar(const Int::IntView& y)
    : VarImpVar<Int::IntVarImp>(y.varimp()) {}

  forceinline int
  IntVar::val(void) const {
    if (!x->assigned())
      throw Int::ValOfUnassignedVar("IntVar::val");
    return x->val();
  }
  forceinline int
  IntVar::min(void) const {
    return x->min();
  }
  forceinline int
  IntVar::med(void) const {
    return x->med();
  }
  forceinline int
  IntVar::max(void) const {
    return x->max();
  }


  forceinline unsigned int
  IntVar::width(void) const {
    return x->width();
  }
  forceinline unsigned int
  IntVar::size(void) const {
    return x->size();
  }
  forceinline unsigned int
  IntVar::regret_min(void) const {
    return x->regret_min();
  }
  forceinline unsigned int
  IntVar::regret_max(void) const {
    return x->regret_max();
  }

  forceinline bool
  IntVar::range(void) const {
    return x->range();
  }
  forceinline bool
  IntVar::in(int n) const {
    return x->in(n);
  }

  /*
   * Range iterator
   *
   */
  forceinline
  IntVarRanges::IntVarRanges(void) {}

  forceinline
  IntVarRanges::IntVarRanges(const IntVar& x)
    : Int::IntVarImpFwd(x.varimp()) {}

  forceinline void
  IntVarRanges::init(const IntVar& x) {
    Int::IntVarImpFwd::init(x.varimp());
  }


  /*
   * Value iterator
   *
   */

  forceinline
  IntVarValues::IntVarValues(void) {}

  forceinline
  IntVarValues::IntVarValues(const IntVar& x) {
    IntVarRanges r(x);
    Iter::Ranges::ToValues<IntVarRanges>::init(r);
  }

  forceinline void
  IntVarValues::init(const IntVar& x) {
    IntVarRanges r(x);
    Iter::Ranges::ToValues<IntVarRanges>::init(r);
  }

}

// STATISTICS: int-var

