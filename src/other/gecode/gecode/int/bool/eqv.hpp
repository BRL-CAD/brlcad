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

  template<class BVA, class BVB, class BVC>
  forceinline
  Eqv<BVA,BVB,BVC>::Eqv(Home home, BVA b0, BVB b1, BVC b2)
    : BoolTernary<BVA,BVB,BVC>(home,b0,b1,b2) {}

  template<class BVA, class BVB, class BVC>
  forceinline
  Eqv<BVA,BVB,BVC>::Eqv(Space& home, bool share, Eqv<BVA,BVB,BVC>& p)
    : BoolTernary<BVA,BVB,BVC>(home,share,p) {}

  template<class BVA, class BVB, class BVC>
  inline ExecStatus
  Eqv<BVA,BVB,BVC>::post(Home home, BVA b0, BVB b1, BVC b2){
    switch (bool_test(b0,b1)) {
    case BT_SAME:
      GECODE_ME_CHECK(b2.one(home)); break;
    case BT_COMP:
      GECODE_ME_CHECK(b2.zero(home)); break;
    case BT_NONE:
      if (b2.one())
        return Eq<BVA,BVB>::post(home,b0,b1);
      if (b0.one()) {
        if (b1.one()) {
          GECODE_ME_CHECK(b2.one(home)); return ES_OK;
        } else if (b1.zero()) {
          GECODE_ME_CHECK(b2.zero(home)); return ES_OK;
        }
      }
      if (b0.zero()) {
        if (b1.one()) {
          GECODE_ME_CHECK(b2.zero(home)); return ES_OK;
        } else if (b1.zero()) {
          GECODE_ME_CHECK(b2.one(home)); return ES_OK;
        }
      }
      (void) new (home) Eqv(home,b0,b1,b2);
      break;
    default:
      GECODE_NEVER;
    }
    return ES_OK;
  }

  template<class BVA, class BVB, class BVC>
  Actor*
  Eqv<BVA,BVB,BVC>::copy(Space& home, bool share) {
    return new (home) Eqv<BVA,BVB,BVC>(home,share,*this);
  }

  template<class BVA, class BVB, class BVC>
  ExecStatus
  Eqv<BVA,BVB,BVC>::propagate(Space& home, const ModEventDelta&) {
#define GECODE_INT_STATUS(S0,S1,S2) \
  ((BVA::S0<<(2*BVA::BITS))|(BVB::S1<<(1*BVB::BITS))|(BVC::S2<<(0*BVC::BITS)))
    switch ((x0.status() << (2*BVA::BITS)) | (x1.status() << (1*BVB::BITS)) |
            (x2.status() << (0*BVC::BITS))) {
    case GECODE_INT_STATUS(NONE,NONE,NONE):
      GECODE_NEVER;
    case GECODE_INT_STATUS(NONE,NONE,ZERO):
    case GECODE_INT_STATUS(NONE,NONE,ONE):
    case GECODE_INT_STATUS(NONE,ZERO,NONE):
      return ES_FIX;
    case GECODE_INT_STATUS(NONE,ZERO,ZERO):
      GECODE_ME_CHECK(x0.one_none(home)); break;
    case GECODE_INT_STATUS(NONE,ZERO,ONE):
      GECODE_ME_CHECK(x0.zero_none(home)); break;
    case GECODE_INT_STATUS(NONE,ONE,NONE):
      return ES_FIX;
    case GECODE_INT_STATUS(NONE,ONE,ZERO):
      GECODE_ME_CHECK(x0.zero_none(home)); break;
    case GECODE_INT_STATUS(NONE,ONE,ONE):
      GECODE_ME_CHECK(x0.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,NONE,NONE):
      return ES_FIX;
    case GECODE_INT_STATUS(ZERO,NONE,ZERO):
      GECODE_ME_CHECK(x1.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,NONE,ONE):
      GECODE_ME_CHECK(x1.zero_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ZERO,NONE):
      GECODE_ME_CHECK(x2.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ZERO,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(ZERO,ZERO,ONE):
      break;
    case GECODE_INT_STATUS(ZERO,ONE,NONE):
      GECODE_ME_CHECK(x2.zero_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ONE,ZERO):
      break;
    case GECODE_INT_STATUS(ZERO,ONE,ONE):
      return ES_FAILED;
    case GECODE_INT_STATUS(ONE,NONE,NONE):
      return ES_FIX;
    case GECODE_INT_STATUS(ONE,NONE,ZERO):
      GECODE_ME_CHECK(x1.zero_none(home)); break;
    case GECODE_INT_STATUS(ONE,NONE,ONE):
      GECODE_ME_CHECK(x1.one_none(home)); break;
    case GECODE_INT_STATUS(ONE,ZERO,NONE):
      GECODE_ME_CHECK(x2.zero_none(home)); break;
    case GECODE_INT_STATUS(ONE,ZERO,ZERO):
      break;
    case GECODE_INT_STATUS(ONE,ZERO,ONE):
      return ES_FAILED;
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
   * N-ary Boolean equivalence propagator
   *
   */

  forceinline
  NaryEqv::NaryEqv(Home home, ViewArray<BoolView>& x0, int pm20)
    : BinaryPropagator<BoolView,PC_BOOL_VAL>(home,x0[0],x0[1]), 
      x(x0), pm2(pm20) {
    assert(x.size() >= 2);
    x.drop_fst(2);
  }

  forceinline
  NaryEqv::NaryEqv(Space& home, bool share, NaryEqv& p)
    : BinaryPropagator<BoolView,PC_BOOL_VAL>(home,share,p), pm2(p.pm2) {
    x.update(home,share,p.x);
  }

  forceinline size_t
  NaryEqv::dispose(Space& home) {
    (void) BinaryPropagator<BoolView,PC_BOOL_VAL>::dispose(home);
    return sizeof(*this);
  }

  forceinline void
  NaryEqv::resubscribe(Space& home, BoolView& x0) {
    if (x0.assigned()) {
      pm2 ^= x0.val();
      int n = x.size();
      for (int i=n; i--; )
        if (x[i].assigned()) {
          pm2 ^= x[i].val();
          x[i] = x[--n];
        } else {
          // Move to x0 and subscribe
          x0=x[i]; x[i]=x[--n];
          x0.subscribe(home,*this,PC_BOOL_VAL,false);
          break;
        }
      x.size(n);
    }
  }

}}}

// STATISTICS: int-prop
