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
  BoolVar::_init(Space& home, int min, int max) {
    assert((min >= 0) && (max <= 1) && (min <= max));
    if (min > 0)
      x = &Int::BoolVarImp::s_one;
    else if (max == 0)
      x = &Int::BoolVarImp::s_zero;
    else
      x = new (home) Int::BoolVarImp(home,0,1);
  }

  forceinline
  BoolVar::BoolVar(void) {}
  forceinline
  BoolVar::BoolVar(const BoolVar& y)
    : VarImpVar<Int::BoolVarImp>(y.varimp()) {}
  forceinline
  BoolVar::BoolVar(const Int::BoolView& y)
    : VarImpVar<Int::BoolVarImp>(y.varimp()) {}

  forceinline int
  BoolVar::val(void) const {
    if (!x->assigned())
      throw Int::ValOfUnassignedVar("BoolVar::val");
    return x->val();
  }
  forceinline int
  BoolVar::min(void) const {
    return x->min();
  }
  forceinline int
  BoolVar::med(void) const {
    return x->med();
  }
  forceinline int
  BoolVar::max(void) const {
    return x->max();
  }


  forceinline unsigned int
  BoolVar::width(void) const {
    return x->width();
  }
  forceinline unsigned int
  BoolVar::size(void) const {
    return x->size();
  }
  forceinline unsigned int
  BoolVar::regret_min(void) const {
    return x->regret_min();
  }
  forceinline unsigned int
  BoolVar::regret_max(void) const {
    return x->regret_max();
  }

  forceinline bool
  BoolVar::range(void) const {
    return x->range();
  }
  forceinline bool
  BoolVar::in(int n) const {
    return x->in(n);
  }

  forceinline bool
  BoolVar::zero(void) const {
    return x->zero();
  }
  forceinline bool
  BoolVar::one(void) const {
    return x->one();
  }
  forceinline bool
  BoolVar::none(void) const {
    return x->none();
  }

}

// STATISTICS: int-var
