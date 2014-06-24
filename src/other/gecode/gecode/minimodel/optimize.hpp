/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main author:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
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

namespace Gecode { namespace MiniModel {

  template<IntRelType irt>
  forceinline
  IntOptimizeSpace<irt>::IntOptimizeSpace(void) {}

  template<IntRelType irt>
  forceinline
  IntOptimizeSpace<irt>::IntOptimizeSpace(bool share, IntOptimizeSpace& s)
    : Space(share,s) {}

  template<IntRelType irt>
  void
  IntOptimizeSpace<irt>::constrain(const Space& _best) {
    const IntOptimizeSpace<irt>* best =
      dynamic_cast<const IntOptimizeSpace<irt>*>(&_best);
    if (best == NULL)
      throw DynamicCastFailed("IntOptimizeSpace::constrain");
    rel(*this, cost(), irt, best->cost().val());
  }

#ifdef GECODE_HAS_FLOAT_VARS 

  template<FloatRelType frt>
  forceinline
  FloatOptimizeSpace<frt>::FloatOptimizeSpace(void) {}

  template<FloatRelType frt>
  forceinline
  FloatOptimizeSpace<frt>::FloatOptimizeSpace(bool share, 
                                              FloatOptimizeSpace& s)
    : Space(share,s) {}

  template<FloatRelType frt>
  void
  FloatOptimizeSpace<frt>::constrain(const Space& _best) {
    const FloatOptimizeSpace<frt>* best =
      dynamic_cast<const FloatOptimizeSpace<frt>*>(&_best);
    if (best == NULL)
      throw DynamicCastFailed("FloatOptimizeSpace::constrain");
    rel(*this, cost(), frt, best->cost().val());
  }

#endif

}}

// STATISTICS: minimodel-search

