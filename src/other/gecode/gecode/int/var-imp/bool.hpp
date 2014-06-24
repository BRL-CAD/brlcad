/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
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

  /*
   * Creation of new variable implementations
   *
   */
  forceinline
  BoolVarImp::BoolVarImp(int n) {
    assert(bits() == 0);
    bits() |= (n << 1) | n;
  }
  forceinline
  BoolVarImp::BoolVarImp(Space& home, int min, int max)
    : BoolVarImpBase(home) {
    assert(bits() == 0);
    bits() |= (max << 1) | min;
  }


  /*
   * Operations on Boolean variable implementations
   *
   */
  forceinline BoolStatus
  BoolVarImp::status(void) const {
    return bits() & 3;
  }
  forceinline int
  BoolVarImp::min(void) const {
    return static_cast<int>(bits() & 1);
  }
  forceinline int
  BoolVarImp::max(void) const {
    return static_cast<int>((bits() & 2) >> 1);
  }
  forceinline int
  BoolVarImp::med(void) const {
    return min();
  }

  forceinline int
  BoolVarImp::val(void) const {
    assert(status() != NONE);
    return min();
  }

  forceinline bool
  BoolVarImp::range(void) const {
    return true;
  }
  forceinline bool
  BoolVarImp::assigned(void) const {
    return status() != NONE;
  }


  forceinline unsigned int
  BoolVarImp::width(void) const {
    return assigned() ? 1U : 2U;
  }

  forceinline unsigned int
  BoolVarImp::size(void) const {
    return assigned() ? 1U : 2U;
  }

  forceinline unsigned int
  BoolVarImp::regret_min(void) const {
    return assigned() ? 1U : 0U;
  }
  forceinline unsigned int
  BoolVarImp::regret_max(void) const {
    return assigned() ? 1U : 0U;
  }



  /*
   * Tests
   *
   */

  forceinline bool
  BoolVarImp::in(int n) const {
    return (n >= min()) && (n <= max());
  }
  forceinline bool
  BoolVarImp::in(long long int n) const {
    return (n >= min()) && (n <= max());
  }


  /*
   * Boolean domain tests
   *
   */
  forceinline bool
  BoolVarImp::zero(void) const {
    return status() < NONE;
  }
  forceinline bool
  BoolVarImp::one(void) const {
    return status() > NONE;
  }
  forceinline bool
  BoolVarImp::none(void) const {
    return status() == NONE;
  }


  /*
   * Support for delta information
   *
   */
  forceinline ModEvent
  BoolVarImp::modevent(const Delta&) {
    return ME_BOOL_VAL;
  }
  forceinline int
  BoolVarImp::min(const Delta& d) {
    return static_cast<const IntDelta&>(d).min();
  }
  forceinline int
  BoolVarImp::max(const Delta& d) {
    return static_cast<const IntDelta&>(d).min();
  }
  forceinline bool
  BoolVarImp::any(const Delta&) {
    return false;
  }
  forceinline bool
  BoolVarImp::zero(const Delta& d) {
    return static_cast<const IntDelta&>(d).min() != 0;
  }
  forceinline bool
  BoolVarImp::one(const Delta& d) {
    return static_cast<const IntDelta&>(d).min() == 0;
  }


  /*
   * Boolean tell operations
   *
   */
  forceinline ModEvent
  BoolVarImp::zero(Space& home) {
    if (one())  return ME_BOOL_FAILED;
    if (zero()) return ME_BOOL_NONE;
    return zero_none(home);
  }
  forceinline ModEvent
  BoolVarImp::one(Space& home) {
    if (one())  return ME_BOOL_NONE;
    if (zero()) return ME_BOOL_FAILED;
    return one_none(home);
  }


  /*
   * Tell operations
   *
   */
  forceinline ModEvent
  BoolVarImp::gq(Space& home, int n) {
    if (n <= 0) return ME_INT_NONE;
    if (n > 1)  return ME_INT_FAILED;
    return one(home);
  }
  forceinline ModEvent
  BoolVarImp::gq(Space& home, long long int n) {
    if (n <= 0) return ME_INT_NONE;
    if (n > 1)  return ME_INT_FAILED;
    return one(home);
  }

  forceinline ModEvent
  BoolVarImp::lq(Space& home, int n) {
    if (n < 0)  return ME_INT_FAILED;
    if (n >= 1) return ME_INT_NONE;
    return zero(home);
  }
  forceinline ModEvent
  BoolVarImp::lq(Space& home, long long int n) {
    if (n < 0)  return ME_INT_FAILED;
    if (n >= 1) return ME_INT_NONE;
    return zero(home);
  }

  forceinline ModEvent
  BoolVarImp::eq(Space& home, int n) {
    if ((n < 0) || (n > 1)) return ME_INT_FAILED;
    return (n == 0) ? zero(home): one(home);
  }
  forceinline ModEvent
  BoolVarImp::eq(Space& home, long long int n) {
    if ((n < 0) || (n > 1)) return ME_INT_FAILED;
    return (n == 0) ? zero(home): one(home);
  }

  forceinline ModEvent
  BoolVarImp::nq(Space& home, int n) {
    if ((n < 0) || (n > 1)) return ME_INT_NONE;
    return (n == 0) ? one(home): zero(home);
  }
  forceinline ModEvent
  BoolVarImp::nq(Space& home, long long int n) {
    if ((n < 0) || (n > 1)) return ME_INT_NONE;
    return (n == 0) ? one(home): zero(home);
  }


  /*
   * Copying a variable
   *
   */

  forceinline
  BoolVarImp::BoolVarImp(Space& home, bool share, BoolVarImp& x)
    : BoolVarImpBase(home,share,x) {}
  forceinline BoolVarImp*
  BoolVarImp::copy(Space& home, bool share) {
    if (copied())
      return static_cast<BoolVarImp*>(forward());
    else if (zero())
      return &s_zero;
    else if (one())
      return &s_one;
    else
      return new (home) BoolVarImp(home,share,*this);
  }


  /*
   * Iterator-based domain operations
   *
   */
  template<class I>
  forceinline ModEvent
  BoolVarImp::narrow_r(Space& home, I& i, bool) {
    // Is new domain empty?
    if (!i())
      return ME_INT_FAILED;
    assert((i.min() == 0) || (i.min() == 1));
    assert((i.max() == 0) || (i.max() == 1));
    if (i.max() == 0) {
      assert(!one());
      // Assign domain to be zero (domain cannot be one)
      return zero(home);
    }
    if (i.min() == 1) {
      // Assign domain to be one (domain cannot be zero)
      assert(!zero());
      return one(home);
    }
    assert(none());
    return ME_INT_NONE;
  }
  template<class I>
  forceinline ModEvent
  BoolVarImp::inter_r(Space& home, I& i, bool) {
    // Skip all ranges that are too small
    while (i() && (i.max() < 0))
      ++i;
    // Is new domain empty?
    if (!i() || (i.min() > 1))
      return ME_INT_FAILED;
    assert(i.min() <= 1);
    if (i.min() == 1)
      return one(home);
    if (i.max() == 0)
      return zero(home);
    assert((i.min() <= 0) && (i.max() >= 1));
    return ME_INT_NONE;
  }
  template<class I>
  forceinline ModEvent
  BoolVarImp::minus_r(Space& home, I& i, bool) {
    // Skip all ranges that are too small
    while (i() && (i.max() < 0))
      ++i;
    // Is new domain empty?
    if (!i() || (i.min() > 1))
      return ME_INT_NONE;
    assert(i.min() <= 1);
    if (i.min() == 1)
      return zero(home);
    if (i.max() == 0)
      return one(home);
    assert((i.min() <= 0) && (i.max() >= 1));
    return ME_INT_FAILED;
  }

  template<class I>
  forceinline ModEvent
  BoolVarImp::narrow_v(Space& home, I& i, bool) {
    if (!i())
      return ME_INT_FAILED;
    if (!none())
      return ME_INT_NONE;
    if (i.val() == 0) {
      do {
        ++i;
      } while (i() && (i.val() == 0));
      if (!i())
        return zero_none(home);
      return ME_INT_NONE;
    } else {
      assert(i.val() == 1);
      return one_none(home);
    }
  }
  template<class I>
  forceinline ModEvent
  BoolVarImp::inter_v(Space& home, I& i, bool) {
    while (i() && (i.val() < 0))
      ++i;
    if (!i() || (i.val() > 1))
      return ME_INT_FAILED;
    if (i.val() == 0) {
      do {
        ++i;
      } while (i() && (i.val() == 0));
      if (!i() || (i.val() > 1))
        return zero(home);
      return ME_INT_NONE;
    } else {
      assert(i.val() == 1);
      return one(home);
    }
  }
  template<class I>
  forceinline ModEvent
  BoolVarImp::minus_v(Space& home, I& i, bool) {
    while (i() && (i.val() < 0))
      ++i;
    if (!i() || (i.val() > 1))
      return ME_INT_NONE;
    if (i.val() == 0) {
      do {
        ++i;
      } while (i() && (i.val() == 0));
      if (!i() || (i.val() > 1))
        return one(home);
      return ME_INT_FAILED;
    } else {
      assert(i.val() == 1);
      return zero(home);
    }
  }



  /*
   * Dependencies
   *
   */
  forceinline void
  BoolVarImp::subscribe(Space& home, Propagator& p, PropCond,
                        bool schedule) {
    // Subscription can be used with integer propagation conditions,
    // which must be remapped to the single Boolean propagation condition.
    BoolVarImpBase::subscribe(home,p,PC_BOOL_VAL,assigned(),schedule);
  }
  forceinline void
  BoolVarImp::cancel(Space& home, Propagator& p, PropCond) {
    BoolVarImpBase::cancel(home,p,PC_BOOL_VAL,assigned());
  }

  forceinline void
  BoolVarImp::subscribe(Space& home, Advisor& a) {
    BoolVarImpBase::subscribe(home,a,assigned());
  }
  forceinline void
  BoolVarImp::cancel(Space& home, Advisor& a) {
    BoolVarImpBase::cancel(home,a,assigned());
  }

  forceinline void
  BoolVarImp::schedule(Space& home, Propagator& p, ModEvent me) {
    if (me == ME_GEN_ASSIGNED)
      BoolVarImpBase::schedule(home,p,me);
  }

  forceinline ModEventDelta
  BoolVarImp::med(ModEvent me) {
    return BoolVarImpBase::med(me);
  }

}}

// STATISTICS: int-var
