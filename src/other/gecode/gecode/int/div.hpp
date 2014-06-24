/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2013
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

  template<class IntType>
  forceinline IntType
  ceil_div_pp(IntType x, IntType y) {
    assert((x >= 0) && (y >= 0));
    /*
     * This is smarter than it looks: for many cpus, % and / are
     * implemented by the same instruction and an optimizing compiler
     * will generate the instruction only once.
     */
    return ((x % y) == 0) ? x/y : (x/y + 1);
  }
  template<class IntType>
  forceinline IntType
  floor_div_pp(IntType x, IntType y) {
    assert((x >= 0) && (y >= 0));
    return x / y;
  }

  template<class IntType>
  forceinline IntType 
  ceil_div_px(IntType x, IntType y) {
    assert(x >= 0);
    return (y >= 0) ? ceil_div_pp(x,y) : -floor_div_pp(x,-y);
  }
  template<class IntType>
  forceinline IntType 
  floor_div_px(IntType x, IntType y) {
    assert(x >= 0);
    return (y >= 0) ? floor_div_pp(x,y) : -ceil_div_pp(x,-y);
  }

  template<class IntType>
  forceinline IntType 
  ceil_div_xp(IntType x, IntType y) {
    assert(y >= 0);
    return (x >= 0) ? ceil_div_pp(x,y) : -floor_div_pp(-x,y);
  }
  template<class IntType>
  forceinline IntType 
  floor_div_xp(IntType x, IntType y) {
    assert(y >= 0);
    return (x >= 0) ? floor_div_pp(x,y) : -ceil_div_pp(-x,y);
  }

  template<class IntType>
  forceinline IntType
  ceil_div_xx(IntType x, IntType y) {
    return (x >= 0) ? ceil_div_px(x,y) : -floor_div_px(-x,y);
  }
  template<class IntType>
  forceinline IntType
  floor_div_xx(IntType x, IntType y) {
    return (x >= 0) ? floor_div_px(x,y) : -ceil_div_px(-x,y);
  }

}}

// STATISTICS: int-prop

