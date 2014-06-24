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

  template<class BVA, class BVB>
  forceinline
  Eq<BVA,BVB>::Eq(Home home, BVA b0, BVB b1)
    : BoolBinary<BVA,BVB>(home,b0,b1) {}

  template<class BVA, class BVB>
  forceinline
  Eq<BVA,BVB>::Eq(Space& home, bool share, Eq<BVA,BVB>& p)
    : BoolBinary<BVA,BVB>(home,share,p) {}

  template<class BVA, class BVB>
  forceinline
  Eq<BVA,BVB>::Eq(Space& home, bool share, Propagator& p,
                  BVA b0, BVB b1)
    : BoolBinary<BVA,BVB>(home,share,p,b0,b1) {}

  template<class BVA, class BVB>
  Actor*
  Eq<BVA,BVB>::copy(Space& home, bool share) {
    return new (home) Eq<BVA,BVB>(home,share,*this);
  }

  template<class BVA, class BVB>
  inline ExecStatus
  Eq<BVA,BVB>::post(Home home, BVA b0, BVB b1) {
    switch (bool_test(b0,b1)) {
    case BT_SAME: return ES_OK;
    case BT_COMP: return ES_FAILED;
    case BT_NONE:
      if (b0.zero()) {
        GECODE_ME_CHECK(b1.zero(home));
      } else if (b0.one()) {
        GECODE_ME_CHECK(b1.one(home));
      } else if (b1.zero()) {
        GECODE_ME_CHECK(b0.zero(home));
      } else if (b1.one()) {
        GECODE_ME_CHECK(b0.one(home));
      } else {
        (void) new (home) Eq<BVA,BVB>(home,b0,b1);
      }
      break;
    default: GECODE_NEVER;
    }
    return ES_OK;
  }

  template<class BVA, class BVB>
  ExecStatus
  Eq<BVA,BVB>::propagate(Space& home, const ModEventDelta&) {
#define GECODE_INT_STATUS(S0,S1) \
  ((BVA::S0<<(1*BVA::BITS))|(BVB::S1<<(0*BVB::BITS)))
    switch ((x0.status() << (1*BVA::BITS)) | (x1.status() << (0*BVB::BITS))) {
    case GECODE_INT_STATUS(NONE,NONE):
      GECODE_NEVER;
    case GECODE_INT_STATUS(NONE,ZERO):
      GECODE_ME_CHECK(x0.zero_none(home)); break;
    case GECODE_INT_STATUS(NONE,ONE):
      GECODE_ME_CHECK(x0.one_none(home)); break;
    case GECODE_INT_STATUS(ZERO,NONE):
      GECODE_ME_CHECK(x1.zero_none(home)); break;
    case GECODE_INT_STATUS(ZERO,ZERO):
      break;
    case GECODE_INT_STATUS(ZERO,ONE):
      return ES_FAILED;
    case GECODE_INT_STATUS(ONE,NONE):
      GECODE_ME_CHECK(x1.one_none(home)); break;
    case GECODE_INT_STATUS(ONE,ZERO):
      return ES_FAILED;
    case GECODE_INT_STATUS(ONE,ONE):
      break;
    default:
      GECODE_NEVER;
    }
    return home.ES_SUBSUMED(*this);
#undef GECODE_INT_STATUS
  }

  template<class BV>
  forceinline
  NaryEq<BV>::NaryEq(Home home, ViewArray<BV>& x)
    : NaryPropagator<BV,PC_BOOL_VAL>(home,x) {}

  template<class BV>
  forceinline
  NaryEq<BV>::NaryEq(Space& home, bool share, NaryEq<BV>& p)
    : NaryPropagator<BV,PC_BOOL_VAL>(home,share,p) {}

  template<class BV>
  Actor*
  NaryEq<BV>::copy(Space& home, bool share) {
    return new (home) NaryEq<BV>(home,share,*this);
  }

  template<class BV>
  inline ExecStatus
  NaryEq<BV>::post(Home home, ViewArray<BV>& x) {
    x.unique(home);
    int n = x.size();
    if (n < 2)
      return ES_OK;
    if (n == 2)
      return Eq<BV,BV>::post(home,x[0],x[1]);
    for (int i=n; i--; )
      if (x[i].assigned()) {
        if (x[i].one()) {
          for (int j=i; j--; )
            GECODE_ME_CHECK(x[j].one(home));
          for (int j=i+1; j<n; j++)
            GECODE_ME_CHECK(x[j].one_none(home));
        } else {
          for (int j=i; j--; )
            GECODE_ME_CHECK(x[j].zero(home));
          for (int j=i+1; j<n; j++)
            GECODE_ME_CHECK(x[j].zero_none(home));
        }
        return ES_OK;
      }
    (void) new (home) NaryEq<BV>(home,x);
    return ES_OK;
  }

  template<class BV>
  PropCost
  NaryEq<BV>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::LO);
  }

  template<class BV>
  ExecStatus
  NaryEq<BV>::propagate(Space& home, const ModEventDelta&) {
    int n=x.size();
    int i=0;
    while (true) {
      if (x[i].assigned()) {
        if (x[i].one()) {
          for (int j=0; j<i; j++)
            GECODE_ME_CHECK(x[j].one_none(home));
          for (int j=i+1; j<n; j++)
            GECODE_ME_CHECK(x[j].one(home));
        } else {
          for (int j=0; j<i; j++)
            GECODE_ME_CHECK(x[j].zero_none(home));
          for (int j=i+1; j<n; j++)
            GECODE_ME_CHECK(x[j].zero(home));
        }
        return home.ES_SUBSUMED(*this);
      }
      i++;
    }
    GECODE_NEVER;
    return ES_FIX;
  }

}}}

// STATISTICS: int-prop

