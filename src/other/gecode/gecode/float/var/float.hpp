/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Vincent Barichard, 2012
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
  FloatVar::_init(Space& home, FloatNum min, FloatNum max) {
    x = new (home) Float::FloatVarImp(home,FloatVal(min,max));
  }

  forceinline
  FloatVar::FloatVar(void) {}
  forceinline
  FloatVar::FloatVar(const FloatVar& y)
    : VarImpVar<Float::FloatVarImp>(y.varimp()) {}
  forceinline
  FloatVar::FloatVar(const Float::FloatView& y)
    : VarImpVar<Float::FloatVarImp>(y.varimp()) {}

  forceinline FloatVal
  FloatVar::val(void) const {
    if (!x->assigned())
      throw Float::ValOfUnassignedVar("FloatVar::val");
    return x->val();
  }
  forceinline FloatNum
  FloatVar::min(void) const {
    return x->min();
  }
  forceinline FloatNum
  FloatVar::med(void) const {
    return x->med();
  }
  forceinline FloatNum
  FloatVar::max(void) const {
    return x->max();
  }

  forceinline FloatVal
  FloatVar::domain(void) const {
    return x->domain();
  }

  forceinline FloatNum
  FloatVar::size(void) const {
    return x->size();
  }

  forceinline bool
  FloatVar::in(const FloatVal& n) const {
    return x->in(n);
  }

}

// STATISTICS: float-var

