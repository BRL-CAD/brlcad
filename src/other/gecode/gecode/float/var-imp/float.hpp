/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Filip Konvicka <filip.konvicka@logis.cz>
 *     Lubomir Moric <lubomir.moric@logis.cz>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     LOGIS, s.r.o., 2008
 *     Christian Schulte, 2010
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

namespace Gecode { namespace Float {

  /*
   * Creation of new variable implementations
   *
   */

  forceinline
  FloatVarImp::FloatVarImp(Space& home, const FloatVal& d)
    : FloatVarImpBase(home), dom(d) {}

  forceinline
  FloatVarImp::FloatVarImp(Space& home, bool share, FloatVarImp& x)
    : FloatVarImpBase(home, share, x), dom(x.dom) {}


  /*
   * Operations on float variable implementations
   *
   */

  forceinline FloatVal
  FloatVarImp::domain(void) const {
    return dom;
  }
  forceinline FloatNum
  FloatVarImp::min(void) const {
    return dom.min();
  }
  forceinline FloatNum
  FloatVarImp::max(void) const {
    return dom.max();
  }
  forceinline FloatVal
  FloatVarImp::val(void) const {
    return dom;
  }
  forceinline FloatNum
  FloatVarImp::med(void) const {
    return dom.med();
  }

  forceinline bool
  FloatVarImp::assigned(void) const {
    return dom.tight();
  }

  forceinline FloatNum
  FloatVarImp::size(void) const {
    return dom.size();
  }


  /*
   * Tests
   *
   */

  forceinline bool
  FloatVarImp::zero_in(void) const {
    return dom.zero_in();
  }
  forceinline bool
  FloatVarImp::in(FloatNum n) const {
    return dom.in(n);
  }
  forceinline bool
  FloatVarImp::in(const FloatVal& n) const {
    return subset(n,dom);
  }


  /*
   * Support for delta information
   *
   */
  forceinline FloatNum
  FloatVarImp::min(const Delta& d) {
    return static_cast<const FloatDelta&>(d).min();
  }
  forceinline FloatNum
  FloatVarImp::max(const Delta& d) {
    return static_cast<const FloatDelta&>(d).max();
  }


  /*
   * Tell operations (to be inlined: performing bounds checks first)
   *
   */

  forceinline ModEvent
  FloatVarImp::gq(Space& home, FloatNum n) {
    if (n > dom.max())  return ME_FLOAT_FAILED;
    if ((n <= dom.min()) || assigned()) return ME_FLOAT_NONE;
    FloatDelta d(dom.min(),n);
    ModEvent me = ME_FLOAT_BND;
    dom = intersect(dom,FloatVal(n,dom.max()));
    if (assigned()) me = ME_FLOAT_VAL;
    GECODE_ASSUME((me == ME_FLOAT_VAL) |
                  (me == ME_FLOAT_BND));
    return notify(home,me,d);
  }
  forceinline ModEvent
  FloatVarImp::gq(Space& home, const FloatVal& n) {
    if (n.min() > dom.max())  return ME_FLOAT_FAILED;
    if ((n.min() <= dom.min()) || assigned()) return ME_FLOAT_NONE;
    FloatDelta d(dom.min(),n.min());
    ModEvent me = ME_FLOAT_BND;
    dom = intersect(dom,FloatVal(n.min(),dom.max()));
    if (assigned()) me = ME_FLOAT_VAL;
    GECODE_ASSUME((me == ME_FLOAT_VAL) |
                  (me == ME_FLOAT_BND));
    return notify(home,me,d);
  }


  forceinline ModEvent
  FloatVarImp::lq(Space& home, FloatNum n) {
    if (n < dom.min())  return ME_FLOAT_FAILED;
    if ((n >= dom.max()) || assigned()) return ME_FLOAT_NONE;
    FloatDelta d(n,dom.max());
    ModEvent me = ME_FLOAT_BND;
    dom = intersect(dom,FloatVal(dom.min(),n));
    if (assigned()) me = ME_FLOAT_VAL;
    GECODE_ASSUME((me == ME_FLOAT_VAL) |
                  (me == ME_FLOAT_BND));
    return notify(home,me,d);
  }
  forceinline ModEvent
  FloatVarImp::lq(Space& home, const FloatVal& n) {
    if (n.max() < dom.min())  return ME_FLOAT_FAILED;
    if ((n.max() >= dom.max()) || assigned()) return ME_FLOAT_NONE;
    FloatDelta d(n.max(),dom.max());
    ModEvent me = ME_FLOAT_BND;
    dom = intersect(dom,FloatVal(dom.min(),n.max()));
    if (assigned()) me = ME_FLOAT_VAL;
    GECODE_ASSUME((me == ME_FLOAT_VAL) |
                  (me == ME_FLOAT_BND));
    return notify(home,me,d);
  }


  forceinline ModEvent
  FloatVarImp::eq(Space& home, FloatNum n) {
    if (!dom.in(n))
      return ME_FLOAT_FAILED;
    if (assigned())
      return ME_FLOAT_NONE;
    FloatDelta d;
    dom = n;
    return notify(home,ME_FLOAT_VAL,d);
  }
  forceinline ModEvent
  FloatVarImp::eq(Space& home, const FloatVal& n) {
    if (!overlap(dom,n))
      return ME_FLOAT_FAILED;
    if (assigned() || subset(dom,n))
      return ME_FLOAT_NONE;
    FloatDelta d;
    ModEvent me = ME_FLOAT_BND;
    dom = intersect(dom,n);
    if (assigned()) me = ME_FLOAT_VAL;
    GECODE_ASSUME((me == ME_FLOAT_VAL) |
                  (me == ME_FLOAT_BND));
    return notify(home,me,d);
  }


  /*
   * Copying a variable
   *
   */

  forceinline FloatVarImp*
  FloatVarImp::copy(Space& home, bool share) {
    return copied() ? static_cast<FloatVarImp*>(forward())
      : perform_copy(home,share);
  }

  /// Return copy of not-yet copied variable
  forceinline FloatVarImp*
  FloatVarImp::perform_copy(Space& home, bool share) {
    return new (home) FloatVarImp(home, share, *this);
  }

  /*
   * Dependencies
   *
   */
  forceinline void
  FloatVarImp::subscribe(Space& home, Propagator& p, PropCond pc, bool schedule) {
    FloatVarImpBase::subscribe(home,p,pc,assigned(),schedule);
  }
  forceinline void
  FloatVarImp::cancel(Space& home, Propagator& p, PropCond pc) {
    FloatVarImpBase::cancel(home,p,pc,assigned());
  }

  forceinline void
  FloatVarImp::subscribe(Space& home, Advisor& a) {
    FloatVarImpBase::subscribe(home,a,assigned());
  }
  forceinline void
  FloatVarImp::cancel(Space& home, Advisor& a) {
    FloatVarImpBase::cancel(home,a,assigned());
  }

  forceinline ModEventDelta
  FloatVarImp::med(ModEvent me) {
    return FloatVarImpBase::med(me);
  }

}}

// STATISTICS: float-var
