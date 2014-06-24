/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
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

namespace Gecode { namespace Int { namespace Bool {

  /*
   * Binary Boolean propagators
   *
   */
  template<class BVA, class BVB>
  forceinline
  BoolBinary<BVA,BVB>::BoolBinary(Home home, BVA b0, BVB b1)
    : Propagator(home), x0(b0), x1(b1) {
    x0.subscribe(home,*this,PC_BOOL_VAL);
    x1.subscribe(home,*this,PC_BOOL_VAL);
  }

  template<class BVA, class BVB>
  forceinline
  BoolBinary<BVA,BVB>::BoolBinary(Space& home, bool share,
                                  BoolBinary<BVA,BVB>& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
  }

  template<class BVA, class BVB>
  forceinline
  BoolBinary<BVA,BVB>::BoolBinary(Space& home, bool share, Propagator& p,
                                  BVA b0, BVB b1)
    : Propagator(home,share,p) {
    x0.update(home,share,b0);
    x1.update(home,share,b1);
  }

  template<class BVA, class BVB>
  PropCost
  BoolBinary<BVA,BVB>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::LO);
  }

  template<class BVA, class BVB>
  forceinline size_t
  BoolBinary<BVA,BVB>::dispose(Space& home) {
    x0.cancel(home,*this,PC_BOOL_VAL);
    x1.cancel(home,*this,PC_BOOL_VAL);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  /*
   * Ternary Boolean propagators
   *
   */
  template<class BVA, class BVB, class BVC>
  forceinline
  BoolTernary<BVA,BVB,BVC>::BoolTernary
  (Home home, BVA b0, BVB b1, BVC b2)
    : Propagator(home), x0(b0), x1(b1), x2(b2) {
    x0.subscribe(home,*this,PC_BOOL_VAL);
    x1.subscribe(home,*this,PC_BOOL_VAL);
    x2.subscribe(home,*this,PC_BOOL_VAL);
  }

  template<class BVA, class BVB, class BVC>
  forceinline
  BoolTernary<BVA,BVB,BVC>::BoolTernary(Space& home, bool share,
                                        BoolTernary<BVA,BVB,BVC>& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    x2.update(home,share,p.x2);
  }

  template<class BVA, class BVB, class BVC>
  forceinline
  BoolTernary<BVA,BVB,BVC>::BoolTernary(Space& home, bool share, Propagator& p,
                                        BVA b0, BVB b1, BVC b2)
    : Propagator(home,share,p) {
    x0.update(home,share,b0);
    x1.update(home,share,b1);
    x2.update(home,share,b2);
  }

  template<class BVA, class BVB, class BVC>
  PropCost
  BoolTernary<BVA,BVB,BVC>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::binary(PropCost::LO);
  }

  template<class BVA, class BVB, class BVC>
  forceinline size_t
  BoolTernary<BVA,BVB,BVC>::dispose(Space& home) {
    x0.cancel(home,*this,PC_BOOL_VAL);
    x1.cancel(home,*this,PC_BOOL_VAL);
    x2.cancel(home,*this,PC_BOOL_VAL);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

}}}

// STATISTICS: int-prop
