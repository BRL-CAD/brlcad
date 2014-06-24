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

  /// Binary Boolean disjunction propagator (subsumed)
  template<class BV>
  class OrTrueSubsumed : public BoolBinary<BV,BV> {
  protected:
    using BoolBinary<BV,BV>::x0;
    using BoolBinary<BV,BV>::x1;
    /// Constructor for cloning \a p
    OrTrueSubsumed(Space& home, bool share, OrTrueSubsumed& p);
    /// Post propagator
    static ExecStatus post(Home home, BV b0, BV b1);
  public:
    /// Constructor
    OrTrueSubsumed(Home home, BV b0, BV b1);
    /// Constructor for rewriting \a p during cloning
    OrTrueSubsumed(Space& home, bool share, Propagator& p,
                   BV b0, BV b1);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
  };

  template<class BV>
  forceinline
  OrTrueSubsumed<BV>::OrTrueSubsumed
  (Home home, BV b0, BV b1)
    : BoolBinary<BV,BV>(home,b0,b1) {}

  template<class BV>
  forceinline ExecStatus
  OrTrueSubsumed<BV>::post(Home home, BV b0, BV b1) {
    (void) new (home) OrTrueSubsumed(home,b0,b1);
    return ES_OK;
  }

  template<class BV>
  forceinline
  OrTrueSubsumed<BV>::OrTrueSubsumed
  (Space& home, bool share, OrTrueSubsumed<BV>& p)
    : BoolBinary<BV,BV>(home,share,p) {}

  template<class BV>
  forceinline
  OrTrueSubsumed<BV>::OrTrueSubsumed(Space& home, bool share, Propagator& p,
                                     BV b0, BV b1)
    : BoolBinary<BV,BV>(home,share,p,b0,b1) {}

  template<class BV>
  Actor*
  OrTrueSubsumed<BV>::copy(Space& home, bool share) {
    return new (home) OrTrueSubsumed<BV>(home,share,*this);
  }

  template<class BV>
  ExecStatus
  OrTrueSubsumed<BV>::propagate(Space& home, const ModEventDelta&) {
    return home.ES_SUBSUMED(*this);
  }


  /*
   * Binary Boolean disjunction propagator (true)
   *
   */

  template<class BVA, class BVB>
  forceinline
  BinOrTrue<BVA,BVB>::BinOrTrue(Home home, BVA b0, BVB b1)
    : BoolBinary<BVA,BVB>(home,b0,b1) {}

  template<class BVA, class BVB>
  forceinline
  BinOrTrue<BVA,BVB>::BinOrTrue(Space& home, bool share, BinOrTrue<BVA,BVB>& p)
    : BoolBinary<BVA,BVB>(home,share,p) {}

  template<class BVA, class BVB>
  forceinline
  BinOrTrue<BVA,BVB>::BinOrTrue(Space& home, bool share, Propagator& p,
                              BVA b0, BVB b1)
    : BoolBinary<BVA,BVB>(home,share,p,b0,b1) {}

  template<class BVA, class BVB>
  Actor*
  BinOrTrue<BVA,BVB>::copy(Space& home, bool share) {
    return new (home) BinOrTrue<BVA,BVB>(home,share,*this);
  }

  template<class BVA, class BVB>
  inline ExecStatus
  BinOrTrue<BVA,BVB>::post(Home home, BVA b0, BVB b1) {
    switch (bool_test(b0,b1)) {
    case BT_SAME:
      GECODE_ME_CHECK(b0.one(home));
      break;
    case BT_COMP:
      break;
    case BT_NONE:
      if (b0.zero()) {
        GECODE_ME_CHECK(b1.one(home));
      } else if (b1.zero()) {
        GECODE_ME_CHECK(b0.one(home));
      } else if (!b0.one() && !b1.one()) {
        (void) new (home) BinOrTrue<BVA,BVB>(home,b0,b1);
      }
      break;
    default: GECODE_NEVER;
    }
    return ES_OK;
  }

  template<class BVA, class BVB>
  ExecStatus
  BinOrTrue<BVA,BVB>::propagate(Space& home, const ModEventDelta&) {
#define GECODE_INT_STATUS(S0,S1) \
  ((BVA::S0<<(1*BVA::BITS))|(BVB::S1<<(0*BVB::BITS)))
    switch ((x0.status() << (1*BVA::BITS)) | (x1.status() << (0*BVB::BITS))) {
    case GECODE_INT_STATUS(NONE,NONE):
      GECODE_NEVER;
    case GECODE_INT_STATUS(NONE,ZERO):
      GECODE_ME_CHECK(x0.one_none(home)); break;
    case GECODE_INT_STATUS(NONE,ONE):
      break;
    case GECODE_INT_STATUS(ZERO,NONE):
      GECODE_ME_CHECK(x1.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(ZERO,ONE):
    case GECODE_INT_STATUS(ONE,NONE):
    case GECODE_INT_STATUS(ONE,ZERO):
    case GECODE_INT_STATUS(ONE,ONE):
      break;
    default:
        GECODE_NEVER;
    }
    return home.ES_SUBSUMED(*this);
#undef GECODE_INT_STATUS
  }

  /*
   * Boolean disjunction propagator (true)
   *
   */

  template<class BV>
  forceinline
  TerOrTrue<BV>::TerOrTrue(Home home, BV b0, BV b1, BV b2)
    : BoolBinary<BV,BV>(home,b0,b1), x2(b2) {}

  template<class BV>
  forceinline size_t
  TerOrTrue<BV>::dispose(Space& home) {
    (void) BoolBinary<BV,BV>::dispose(home);
    return sizeof(*this);
  }

  template<class BV>
  forceinline
  TerOrTrue<BV>::TerOrTrue(Space& home, bool share, TerOrTrue<BV>& p)
    : BoolBinary<BV,BV>(home,share,p) {
    x2.update(home,share,p.x2);
  }

  template<class BV>
  forceinline
  TerOrTrue<BV>::TerOrTrue(Space& home, bool share, Propagator& p,
                           BV b0, BV b1, BV b2)
    : BoolBinary<BV,BV>(home,share,p,b0,b1) {
    x2.update(home,share,b2);
  }

  template<class BV>
  Actor*
  TerOrTrue<BV>::copy(Space& home, bool share) {
    assert(x0.none() && x1.none());
    if (x2.one())
      return new (home) OrTrueSubsumed<BV>(home,share,*this,x0,x1);
    else if (x2.zero())
      return new (home) BinOrTrue<BV,BV>(home,share,*this,x0,x1);
    else
      return new (home) TerOrTrue<BV>(home,share,*this);
  }

  template<class BV>
  forceinline ExecStatus
  TerOrTrue<BV>::post(Home home, BV b0, BV b1, BV b2) {
    (void) new (home) TerOrTrue<BV>(home,b0,b1,b2);
    return ES_OK;
  }

  template<class BV>
  ExecStatus
  TerOrTrue<BV>::propagate(Space& home, const ModEventDelta&) {
#define GECODE_INT_STATUS(S0,S1,S2) \
    ((BV::S0<<(2*BV::BITS))|(BV::S1<<(1*BV::BITS))|(BV::S2<<(0*BV::BITS)))
    switch ((x0.status() << (2*BV::BITS)) | (x1.status() << (1*BV::BITS)) |
            (x2.status() << (0*BV::BITS))) {
    case GECODE_INT_STATUS(NONE,NONE,NONE):
    case GECODE_INT_STATUS(NONE,NONE,ZERO):
    case GECODE_INT_STATUS(NONE,NONE,ONE):
      GECODE_NEVER;
    case GECODE_INT_STATUS(NONE,ZERO,NONE):
      std::swap(x1,x2); x1.subscribe(home,*this,PC_BOOL_VAL);
      return ES_FIX;
    case GECODE_INT_STATUS(NONE,ZERO,ZERO):
      GECODE_ME_CHECK(x0.one_none(home)); break;
    case GECODE_INT_STATUS(NONE,ZERO,ONE):
    case GECODE_INT_STATUS(NONE,ONE,NONE):
    case GECODE_INT_STATUS(NONE,ONE,ZERO):
    case GECODE_INT_STATUS(NONE,ONE,ONE):
      break;
    case GECODE_INT_STATUS(ZERO,NONE,NONE):
      std::swap(x0,x2); x0.subscribe(home,*this,PC_BOOL_VAL);
      return ES_FIX;
    case GECODE_INT_STATUS(ZERO,NONE,ZERO):
      GECODE_ME_CHECK(x1.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,NONE,ONE):
      break;
    case GECODE_INT_STATUS(ZERO,ZERO,NONE):
      GECODE_ME_CHECK(x2.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ZERO,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(ZERO,ZERO,ONE):
    case GECODE_INT_STATUS(ZERO,ONE,NONE):
    case GECODE_INT_STATUS(ZERO,ONE,ZERO):
    case GECODE_INT_STATUS(ZERO,ONE,ONE):
    case GECODE_INT_STATUS(ONE,NONE,NONE):
    case GECODE_INT_STATUS(ONE,NONE,ZERO):
    case GECODE_INT_STATUS(ONE,NONE,ONE):
    case GECODE_INT_STATUS(ONE,ZERO,NONE):
    case GECODE_INT_STATUS(ONE,ZERO,ZERO):
    case GECODE_INT_STATUS(ONE,ZERO,ONE):
    case GECODE_INT_STATUS(ONE,ONE,NONE):
    case GECODE_INT_STATUS(ONE,ONE,ZERO):
    case GECODE_INT_STATUS(ONE,ONE,ONE):
      break;
    default:
      GECODE_NEVER;
    }
    return home.ES_SUBSUMED(*this);
#undef GECODE_INT_STATUS
  }

  /*
   * Boolean disjunction propagator (true)
   *
   */

  template<class BV>
  forceinline
  QuadOrTrue<BV>::QuadOrTrue(Home home, BV b0, BV b1, BV b2, BV b3)
    : BoolBinary<BV,BV>(home,b0,b1), x2(b2), x3(b3) {}

  template<class BV>
  forceinline size_t
  QuadOrTrue<BV>::dispose(Space& home) {
    (void) BoolBinary<BV,BV>::dispose(home);
    return sizeof(*this);
  }

  template<class BV>
  forceinline
  QuadOrTrue<BV>::QuadOrTrue(Space& home, bool share, QuadOrTrue<BV>& p)
    : BoolBinary<BV,BV>(home,share,p) {
    x2.update(home,share,p.x2);
    x3.update(home,share,p.x3);
  }

  template<class BV>
  forceinline
  QuadOrTrue<BV>::QuadOrTrue(Space& home, bool share, Propagator& p,
                             BV b0, BV b1, BV b2, BV b3)
    : BoolBinary<BV,BV>(home,share,p,b0,b1) {
    x2.update(home,share,b2);
    x3.update(home,share,b3);
  }

  template<class BV>
  Actor*
  QuadOrTrue<BV>::copy(Space& home, bool share) {
    assert(x0.none() && x1.none());
    if (x2.one() || x3.one())
      return new (home) OrTrueSubsumed<BV>(home,share,*this,x0,x1);
    else if (x2.zero() && x3.zero())
      return new (home) BinOrTrue<BV,BV>(home,share,*this,x0,x1);
    else if (x2.zero())
      return new (home) TerOrTrue<BV>(home,share,*this,x0,x1,x3);
    else if (x3.zero())
      return new (home) TerOrTrue<BV>(home,share,*this,x0,x1,x2);
    else
      return new (home) QuadOrTrue<BV>(home,share,*this);
  }

  template<class BV>
  forceinline ExecStatus
  QuadOrTrue<BV>::post(Home home, BV b0, BV b1, BV b2, BV b3) {
    (void) new (home) QuadOrTrue<BV>(home,b0,b1,b2,b3);
    return ES_OK;
  }

  template<class BV>
  ExecStatus
  QuadOrTrue<BV>::propagate(Space& home, const ModEventDelta&) {
#define GECODE_INT_STATUS(S0,S1,S2,S3)                        \
    ((BV::S0 << (3*BV::BITS)) | (BV::S1 << (2*BV::BITS)) |    \
     (BV::S2 << (1*BV::BITS)) | (BV::S3 << (0*BV::BITS)))
    switch ((x0.status() << (3*BV::BITS)) | (x1.status() << (2*BV::BITS)) |
            (x2.status() << (1*BV::BITS)) | (x3.status() << (0*BV::BITS))) {
    case GECODE_INT_STATUS(NONE,NONE,NONE,NONE):
    case GECODE_INT_STATUS(NONE,NONE,NONE,ZERO):
    case GECODE_INT_STATUS(NONE,NONE,NONE,ONE):
    case GECODE_INT_STATUS(NONE,NONE,ZERO,NONE):
    case GECODE_INT_STATUS(NONE,NONE,ZERO,ZERO):
    case GECODE_INT_STATUS(NONE,NONE,ZERO,ONE):
    case GECODE_INT_STATUS(NONE,NONE,ONE,NONE):
    case GECODE_INT_STATUS(NONE,NONE,ONE,ZERO):
    case GECODE_INT_STATUS(NONE,NONE,ONE,ONE):
      GECODE_NEVER;
    case GECODE_INT_STATUS(NONE,ZERO,NONE,NONE):
    case GECODE_INT_STATUS(NONE,ZERO,NONE,ZERO):
      std::swap(x1,x2); x1.subscribe(home,*this,PC_BOOL_VAL,false);
      return ES_FIX;
    case GECODE_INT_STATUS(NONE,ZERO,NONE,ONE):
      break;
    case GECODE_INT_STATUS(NONE,ZERO,ZERO,NONE):
      std::swap(x1,x3); x1.subscribe(home,*this,PC_BOOL_VAL,false);
      return ES_FIX;
    case GECODE_INT_STATUS(NONE,ZERO,ZERO,ZERO):
      GECODE_ME_CHECK(x0.one_none(home)); break;
    case GECODE_INT_STATUS(NONE,ZERO,ZERO,ONE):
    case GECODE_INT_STATUS(NONE,ZERO,ONE,NONE):
    case GECODE_INT_STATUS(NONE,ZERO,ONE,ZERO):
    case GECODE_INT_STATUS(NONE,ZERO,ONE,ONE):
    case GECODE_INT_STATUS(NONE,ONE,NONE,NONE):
    case GECODE_INT_STATUS(NONE,ONE,NONE,ZERO):
    case GECODE_INT_STATUS(NONE,ONE,NONE,ONE):
    case GECODE_INT_STATUS(NONE,ONE,ZERO,NONE):
    case GECODE_INT_STATUS(NONE,ONE,ZERO,ZERO):
    case GECODE_INT_STATUS(NONE,ONE,ZERO,ONE):
    case GECODE_INT_STATUS(NONE,ONE,ONE,NONE):
    case GECODE_INT_STATUS(NONE,ONE,ONE,ZERO):
    case GECODE_INT_STATUS(NONE,ONE,ONE,ONE):
      break;
    case GECODE_INT_STATUS(ZERO,NONE,NONE,NONE):
    case GECODE_INT_STATUS(ZERO,NONE,NONE,ZERO):
      std::swap(x0,x2); x0.subscribe(home,*this,PC_BOOL_VAL,false);
      return ES_FIX;
    case GECODE_INT_STATUS(ZERO,NONE,NONE,ONE):
      break;
    case GECODE_INT_STATUS(ZERO,NONE,ZERO,NONE):
      std::swap(x0,x3); x0.subscribe(home,*this,PC_BOOL_VAL,false);
      return ES_FIX;
    case GECODE_INT_STATUS(ZERO,NONE,ZERO,ZERO):
      GECODE_ME_CHECK(x1.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,NONE,ZERO,ONE):
    case GECODE_INT_STATUS(ZERO,NONE,ONE,NONE):
    case GECODE_INT_STATUS(ZERO,NONE,ONE,ZERO):
    case GECODE_INT_STATUS(ZERO,NONE,ONE,ONE):
      break;
    case GECODE_INT_STATUS(ZERO,ZERO,NONE,NONE):
      std::swap(x0,x2); x0.subscribe(home,*this,PC_BOOL_VAL,false);
      std::swap(x1,x3); x1.subscribe(home,*this,PC_BOOL_VAL,false);
      return ES_FIX;
    case GECODE_INT_STATUS(ZERO,ZERO,NONE,ZERO):
      GECODE_ME_CHECK(x2.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ZERO,NONE,ONE):
      break;
    case GECODE_INT_STATUS(ZERO,ZERO,ZERO,NONE):
      GECODE_ME_CHECK(x3.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ZERO,ZERO,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(ZERO,ZERO,ZERO,ONE):
    case GECODE_INT_STATUS(ZERO,ZERO,ONE,NONE):
    case GECODE_INT_STATUS(ZERO,ZERO,ONE,ZERO):
    case GECODE_INT_STATUS(ZERO,ZERO,ONE,ONE):
    case GECODE_INT_STATUS(ZERO,ONE,NONE,NONE):
    case GECODE_INT_STATUS(ZERO,ONE,NONE,ZERO):
    case GECODE_INT_STATUS(ZERO,ONE,NONE,ONE):
    case GECODE_INT_STATUS(ZERO,ONE,ZERO,NONE):
    case GECODE_INT_STATUS(ZERO,ONE,ZERO,ZERO):
    case GECODE_INT_STATUS(ZERO,ONE,ZERO,ONE):
    case GECODE_INT_STATUS(ZERO,ONE,ONE,NONE):
    case GECODE_INT_STATUS(ZERO,ONE,ONE,ZERO):
    case GECODE_INT_STATUS(ZERO,ONE,ONE,ONE):
    case GECODE_INT_STATUS(ONE,NONE,NONE,NONE):
    case GECODE_INT_STATUS(ONE,NONE,NONE,ZERO):
    case GECODE_INT_STATUS(ONE,NONE,NONE,ONE):
    case GECODE_INT_STATUS(ONE,NONE,ZERO,NONE):
    case GECODE_INT_STATUS(ONE,NONE,ZERO,ZERO):
    case GECODE_INT_STATUS(ONE,NONE,ZERO,ONE):
    case GECODE_INT_STATUS(ONE,NONE,ONE,NONE):
    case GECODE_INT_STATUS(ONE,NONE,ONE,ZERO):
    case GECODE_INT_STATUS(ONE,NONE,ONE,ONE):
    case GECODE_INT_STATUS(ONE,ZERO,NONE,NONE):
    case GECODE_INT_STATUS(ONE,ZERO,NONE,ZERO):
    case GECODE_INT_STATUS(ONE,ZERO,NONE,ONE):
    case GECODE_INT_STATUS(ONE,ZERO,ZERO,NONE):
    case GECODE_INT_STATUS(ONE,ZERO,ZERO,ZERO):
    case GECODE_INT_STATUS(ONE,ZERO,ZERO,ONE):
    case GECODE_INT_STATUS(ONE,ZERO,ONE,NONE):
    case GECODE_INT_STATUS(ONE,ZERO,ONE,ZERO):
    case GECODE_INT_STATUS(ONE,ZERO,ONE,ONE):
    case GECODE_INT_STATUS(ONE,ONE,NONE,NONE):
    case GECODE_INT_STATUS(ONE,ONE,NONE,ZERO):
    case GECODE_INT_STATUS(ONE,ONE,NONE,ONE):
    case GECODE_INT_STATUS(ONE,ONE,ZERO,NONE):
    case GECODE_INT_STATUS(ONE,ONE,ZERO,ZERO):
    case GECODE_INT_STATUS(ONE,ONE,ZERO,ONE):
    case GECODE_INT_STATUS(ONE,ONE,ONE,NONE):
    case GECODE_INT_STATUS(ONE,ONE,ONE,ZERO):
    case GECODE_INT_STATUS(ONE,ONE,ONE,ONE):
      break;
    default:
      GECODE_NEVER;
    }
    return home.ES_SUBSUMED(*this);
#undef GECODE_INT_STATUS
  }

  /*
   * Boolean disjunction propagator
   *
   */

  template<class BVA, class BVB, class BVC>
  forceinline
  Or<BVA,BVB,BVC>::Or(Home home, BVA b0, BVB b1, BVC b2)
    : BoolTernary<BVA,BVB,BVC>(home,b0,b1,b2) {}

  template<class BVA, class BVB, class BVC>
  forceinline
  Or<BVA,BVB,BVC>::Or(Space& home, bool share, Or<BVA,BVB,BVC>& p)
    : BoolTernary<BVA,BVB,BVC>(home,share,p) {}

  template<class BVA, class BVB, class BVC>
  forceinline
  Or<BVA,BVB,BVC>::Or(Space& home, bool share, Propagator& p,
                        BVA b0, BVB b1, BVC b2)
    : BoolTernary<BVA,BVB,BVC>(home,share,p,b0,b1,b2) {}

  template<class BVA, class BVB, class BVC>
  Actor*
  Or<BVA,BVB,BVC>::copy(Space& home, bool share) {
    if (x2.one()) {
      assert(x0.none() && x1.none());
      return new (home) BinOrTrue<BVA,BVB>(home,share,*this,x0,x1);
    } else if (x0.zero()) {
      assert(x1.none() && x2.none());
      return new (home) Eq<BVB,BVC>(home,share,*this,x1,x2);
    } else if (x1.zero()) {
      assert(x0.none() && x2.none());
      return new (home) Eq<BVA,BVC>(home,share,*this,x0,x2);
    } else {
      return new (home) Or<BVA,BVB,BVC>(home,share,*this);
    }
  }

  template<class BVA, class BVB, class BVC>
  inline ExecStatus
  Or<BVA,BVB,BVC>::post(Home home, BVA b0, BVB b1, BVC b2) {
    if (b2.zero()) {
      GECODE_ME_CHECK(b0.zero(home));
      GECODE_ME_CHECK(b1.zero(home));
    } else if (b2.one()) {
      return BinOrTrue<BVA,BVB>::post(home,b0,b1);
    } else {
      switch (bool_test(b0,b1)) {
      case BT_SAME:
        return Eq<BVA,BVC>::post(home,b0,b2);
      case BT_COMP:
        GECODE_ME_CHECK(b2.one(home));
        break;
      case BT_NONE:
        if (b0.one() || b1.one()) {
          GECODE_ME_CHECK(b2.one(home));
        } else if (b0.zero()) {
          return Eq<BVB,BVC>::post(home,b1,b2);
        } else if (b1.zero()) {
          return Eq<BVA,BVC>::post(home,b0,b2);
        } else {
          (void) new (home) Or<BVA,BVB,BVC>(home,b0,b1,b2);
        }
        break;
      default: GECODE_NEVER;
      }
    }
    return ES_OK;
  }

  template<class BVA, class BVB, class BVC>
  ExecStatus
  Or<BVA,BVB,BVC>::propagate(Space& home, const ModEventDelta&) {
#define GECODE_INT_STATUS(S0,S1,S2) \
  ((BVA::S0<<(2*BVA::BITS))|(BVB::S1<<(1*BVB::BITS))|(BVC::S2<<(0*BVC::BITS)))
    switch ((x0.status() << (2*BVA::BITS)) | (x1.status() << (1*BVB::BITS)) |
            (x2.status() << (0*BVC::BITS))) {
    case GECODE_INT_STATUS(NONE,NONE,NONE):
      GECODE_NEVER;
    case GECODE_INT_STATUS(NONE,NONE,ZERO):
      GECODE_ME_CHECK(x0.zero_none(home));
      GECODE_ME_CHECK(x1.zero_none(home));
      break;
    case GECODE_INT_STATUS(NONE,NONE,ONE):
      return ES_FIX;
    case GECODE_INT_STATUS(NONE,ZERO,NONE):
      switch (bool_test(x0,x2)) {
      case BT_SAME: return home.ES_SUBSUMED(*this);
      case BT_COMP: return ES_FAILED;
      case BT_NONE: return ES_FIX;
      default: GECODE_NEVER;
      }
      GECODE_NEVER;
    case GECODE_INT_STATUS(NONE,ZERO,ZERO):
      GECODE_ME_CHECK(x0.zero_none(home)); break;
    case GECODE_INT_STATUS(NONE,ZERO,ONE):
      GECODE_ME_CHECK(x0.one_none(home)); break;
    case GECODE_INT_STATUS(NONE,ONE,NONE):
      GECODE_ME_CHECK(x2.one_none(home)); break;
    case GECODE_INT_STATUS(NONE,ONE,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(NONE,ONE,ONE):
      break;
    case GECODE_INT_STATUS(ZERO,NONE,NONE):
      switch (bool_test(x1,x2)) {
      case BT_SAME: return home.ES_SUBSUMED(*this);
      case BT_COMP: return ES_FAILED;
      case BT_NONE: return ES_FIX;
      default: GECODE_NEVER;
      }
      GECODE_NEVER;
    case GECODE_INT_STATUS(ZERO,NONE,ZERO):
      GECODE_ME_CHECK(x1.zero_none(home)); break;
    case GECODE_INT_STATUS(ZERO,NONE,ONE):
      GECODE_ME_CHECK(x1.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ZERO,NONE):
      GECODE_ME_CHECK(x2.zero_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ZERO,ZERO):
      break;
    case GECODE_INT_STATUS(ZERO,ZERO,ONE):
      return ES_FAILED;
    case GECODE_INT_STATUS(ZERO,ONE,NONE):
      GECODE_ME_CHECK(x2.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ONE,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(ZERO,ONE,ONE):
      break;
    case GECODE_INT_STATUS(ONE,NONE,NONE):
      GECODE_ME_CHECK(x2.one_none(home)); break;
    case GECODE_INT_STATUS(ONE,NONE,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(ONE,NONE,ONE):
      break;
    case GECODE_INT_STATUS(ONE,ZERO,NONE):
      GECODE_ME_CHECK(x2.one_none(home)); break;
    case GECODE_INT_STATUS(ONE,ZERO,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(ONE,ZERO,ONE):
      break;
    case GECODE_INT_STATUS(ONE,ONE,NONE):
      GECODE_ME_CHECK(x2.one_none(home)); break;
    case GECODE_INT_STATUS(ONE,ONE,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(ONE,ONE,ONE):
      break;
    default:
      GECODE_NEVER;
    }
    return home.ES_SUBSUMED(*this);
#undef GECODE_INT_STATUS
  }

  /*
   * N-ary Boolean disjunction propagator (true)
   *
   */

  template<class BV>
  forceinline
  NaryOrTrue<BV>::NaryOrTrue(Home home, ViewArray<BV>& b)
    : BinaryPropagator<BV,PC_BOOL_VAL>(home,b[0],b[1]), x(b) {
    assert(x.size() > 2);
    x.drop_fst(2);
  }

  template<class BV>
  PropCost
  NaryOrTrue<BV>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::binary(PropCost::LO);
  }

  template<class BV>
  forceinline
  NaryOrTrue<BV>::NaryOrTrue(Space& home, bool share, NaryOrTrue<BV>& p)
    : BinaryPropagator<BV,PC_BOOL_VAL>(home,share,p) {
    x.update(home,share,p.x);
  }

  template<class BV>
  Actor*
  NaryOrTrue<BV>::copy(Space& home, bool share) {
    int n = x.size();
    if (n > 0) {
      // Eliminate all zeros and find a one
      for (int i=n; i--; )
        if (x[i].one()) {
          // Only keep the one
          x[0]=x[i]; x.size(1);
          return new (home) OrTrueSubsumed<BV>(home,share,*this,x0,x1);
        } else if (x[i].zero()) {
          // Eliminate the zero
          x[i]=x[--n];
        }
      x.size(n);
    }
    switch (n) {
    case 0:
      return new (home) BinOrTrue<BV,BV>(home,share,*this,x0,x1);
    case 1:
      return new (home) TerOrTrue<BV>(home,share,*this,x0,x1,x[0]);
    case 2:
      return new (home) QuadOrTrue<BV>(home,share,*this,x0,x1,x[0],x[1]);
    default:
      return new (home) NaryOrTrue<BV>(home,share,*this);
    }
  }

  template<class BV>
  inline ExecStatus
  NaryOrTrue<BV>::post(Home home, ViewArray<BV>& b) {
    for (int i=b.size(); i--; )
      if (b[i].one())
        return ES_OK;
      else if (b[i].zero())
        b.move_lst(i);
    if (b.size() == 0)
      return ES_FAILED;
    if (b.size() == 1) {
      GECODE_ME_CHECK(b[0].one(home));
    } else if (b.size() == 2) {
       return BinOrTrue<BV,BV>::post(home,b[0],b[1]);
    } else if (b.size() == 3) {
       return TerOrTrue<BV>::post(home,b[0],b[1],b[2]);
    } else if (b.size() == 4) {
       return QuadOrTrue<BV>::post(home,b[0],b[1],b[2],b[3]);
    } else {
      (void) new (home) NaryOrTrue(home,b);
    }
    return ES_OK;
  }

  template<class BV>
  forceinline size_t
  NaryOrTrue<BV>::dispose(Space& home) {
    (void) BinaryPropagator<BV,PC_BOOL_VAL>::dispose(home);
    return sizeof(*this);
  }

  template<class BV>
  forceinline ExecStatus
  NaryOrTrue<BV>::resubscribe(Space& home, BV& x0, BV x1) {
    if (x0.zero()) {
      int n = x.size();
      for (int i=n; i--; )
        if (x[i].one()) {
          return home.ES_SUBSUMED(*this);
        } else if (x[i].zero()) {
          x[i] = x[--n];
        } else {
          // Move to x0 and subscribe
          x0=x[i]; x[i]=x[--n];
          x.size(n);
          x0.subscribe(home,*this,PC_BOOL_VAL,false);
          return ES_FIX;
        }
      // All views have been assigned!
      GECODE_ME_CHECK(x1.one(home));
      return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }

  template<class BV>
  ExecStatus
  NaryOrTrue<BV>::propagate(Space& home, const ModEventDelta&) {
    if (x0.one())
      return home.ES_SUBSUMED(*this);
    if (x1.one())
      return home.ES_SUBSUMED(*this);
    GECODE_ES_CHECK(resubscribe(home,x0,x1));
    GECODE_ES_CHECK(resubscribe(home,x1,x0));
    return ES_FIX;
  }


  /*
   * N-ary Boolean disjunction propagator
   *
   */

  template<class VX, class VY>
  forceinline
  NaryOr<VX,VY>::NaryOr(Home home, ViewArray<VX>& x, VY y)
    : MixNaryOnePropagator<VX,PC_BOOL_NONE,VY,PC_BOOL_VAL>(home,x,y),
      n_zero(0), c(home) {
    x.subscribe(home,*new (home) Advisor(home,*this,c));
  }

  template<class VX, class VY>
  forceinline
  NaryOr<VX,VY>::NaryOr(Space& home, bool share, NaryOr<VX,VY>& p)
    : MixNaryOnePropagator<VX,PC_BOOL_NONE,VY,PC_BOOL_VAL>(home,share,p),
      n_zero(p.n_zero) {
    c.update(home,share,p.c);
  }

  template<class VX, class VY>
  Actor*
  NaryOr<VX,VY>::copy(Space& home, bool share) {
    assert(n_zero < x.size());
    if (n_zero > 0) {
      int n=x.size();
      // Eliminate all zeros
      for (int i=n; i--; )
        if (x[i].zero())
          x[i]=x[--n];
      x.size(n);
      n_zero = 0;
    }
    assert(n_zero < x.size());
    return new (home) NaryOr<VX,VY>(home,share,*this);
  }

  template<class VX, class VY>
  inline ExecStatus
  NaryOr<VX,VY>::post(Home home, ViewArray<VX>& x, VY y) {
    assert(!x.shared(home));
    if (y.one())
      return NaryOrTrue<VX>::post(home,x);
    if (y.zero()) {
      for (int i=x.size(); i--; )
        GECODE_ME_CHECK(x[i].zero(home));
      return ES_OK;
    }
    for (int i=x.size(); i--; )
      if (x[i].one()) {
        GECODE_ME_CHECK(y.one_none(home));
        return ES_OK;
      } else if (x[i].zero()) {
        x.move_lst(i);
      }
    if (x.size() == 0) {
      GECODE_ME_CHECK(y.zero_none(home));
    } else if (x.size() == 1) {
      return Eq<VX,VY>::post(home,x[0],y);
    } else if (x.size() == 2) {
      return Or<VX,VX,VY>::post(home,x[0],x[1],y);
    } else {
      (void) new (home) NaryOr(home,x,y);
    }
    return ES_OK;
  }

  template<class VX, class VY>
  PropCost
  NaryOr<VX,VY>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::LO);
  }

  template<class VX, class VY>
  ExecStatus
  NaryOr<VX,VY>::advise(Space&, Advisor&, const Delta& d) {
    // Decides whether the propagator must be run
    if (VX::zero(d) && (++n_zero < x.size()))
      return ES_FIX;
    else
      return ES_NOFIX;
  }

  template<class VX, class VY>
  forceinline size_t
  NaryOr<VX,VY>::dispose(Space& home) {
    Advisors<Advisor> as(c);
    x.cancel(home,as.advisor());
    c.dispose(home);
    (void) MixNaryOnePropagator<VX,PC_BOOL_NONE,VY,PC_BOOL_VAL>
      ::dispose(home);
    return sizeof(*this);
  }

  template<class VX, class VY>
  ExecStatus
  NaryOr<VX,VY>::propagate(Space& home, const ModEventDelta&) {
    if (y.one())
      GECODE_REWRITE(*this,NaryOrTrue<VX>::post(home(*this),x));
    if (y.zero()) {
      // Note that this might trigger the advisor of this propagator!
      for (int i = x.size(); i--; )
        GECODE_ME_CHECK(x[i].zero(home));
    } else if (n_zero == x.size()) {
      // All views are zero
      GECODE_ME_CHECK(y.zero_none(home));
    } else {
      // There is exactly one view which is one
      GECODE_ME_CHECK(y.one_none(home));
    }
    return home.ES_SUBSUMED(*this);
  }

}}}

// STATISTICS: int-prop

