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

namespace Gecode { namespace Int { namespace Bool {

  /*
   * Less or equal propagator
   *
   */

  template<class BV>
  forceinline
  Lq<BV>::Lq(Home home, BV b0, BV b1)
    : BoolBinary<BV,BV>(home,b0,b1) {}

  template<class BV>
  forceinline
  Lq<BV>::Lq(Space& home, bool share, Lq<BV>& p)
    : BoolBinary<BV,BV>(home,share,p) {}

  template<class BV>
  Actor*
  Lq<BV>::copy(Space& home, bool share) {
    return new (home) Lq<BV>(home,share,*this);
  }

  template<class BV>
  inline ExecStatus
  Lq<BV>::post(Home home, BV b0, BV b1) {
    if (b0.zero()) {
      return ES_OK;
    } else if (b0.one()) {
      GECODE_ME_CHECK(b1.one(home));
    } else if (b1.zero()) {
      GECODE_ME_CHECK(b0.zero(home));
    } else if (b1.one()) {
      return ES_OK;
    } else {
      (void) new (home) Lq<BV>(home,b0,b1);
    }
    return ES_OK;
  }

  template<class BV>
  ExecStatus
  Lq<BV>::propagate(Space& home, const ModEventDelta&) {
#define GECODE_INT_STATUS(S0,S1) \
  ((BV::S0<<(1*BV::BITS))|(BV::S1<<(0*BV::BITS)))
    switch ((x0.status()<<(1*BV::BITS)) | (x1.status()<<(0*BV::BITS))) {
    case GECODE_INT_STATUS(NONE,NONE):
      GECODE_NEVER;
    case GECODE_INT_STATUS(NONE,ZERO):
      GECODE_ME_CHECK(x0.zero_none(home)); break;
    case GECODE_INT_STATUS(NONE,ONE):
    case GECODE_INT_STATUS(ZERO,NONE):
    case GECODE_INT_STATUS(ZERO,ZERO):
    case GECODE_INT_STATUS(ZERO,ONE):
      break;
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


  /*
   * N-ary Boolean less or equal propagator
   *
   */

  template<class VX>
  forceinline
  NaryLq<VX>::NaryLq(Home home, ViewArray<VX>& x)
    : NaryPropagator<VX,PC_BOOL_NONE>(home,x),
      run(false), n_zero(0), n_one(0), c(home) {
    x.subscribe(home,*new (home) Advisor(home,*this,c));
  }

  template<class VX>
  forceinline
  NaryLq<VX>::NaryLq(Space& home, bool share, NaryLq<VX>& p)
    : NaryPropagator<VX,PC_BOOL_NONE>(home,share,p),
      run(false), n_zero(0), n_one(0) {
    c.update(home,share,p.c);
  }

  template<class VX>
  Actor*
  NaryLq<VX>::copy(Space& home, bool share) {
    return new (home) NaryLq<VX>(home,share,*this);
  }

  template<class VX>
  inline ExecStatus
  NaryLq<VX>::post(Home home, ViewArray<VX>& x) {
    int i = 0;
    while (i < x.size())
      if (x[i].zero()) {
        // All x[j] left of i must be zero as well
        for (int j=i; j--; )
          GECODE_ME_CHECK(x[j].zero_none(home));
        x.drop_fst(i+1); i=0;
      } else if (x[i].one()) {
        // All x[j] right of i must be one as well
        for (int j=i+1; j<x.size(); j++)
          GECODE_ME_CHECK(x[j].one(home));
        x.drop_lst(i-1); break;
      } else {
        i++;
      }

    if (x.size() == 2)
      return Lq<VX>::post(home,x[0],x[1]);
    if (x.size() > 2)
      (void) new (home) NaryLq(home,x);
    return ES_OK;
  }

  template<class VX>
  PropCost
  NaryLq<VX>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::binary(PropCost::LO);
  }

  template<class VX>
  ExecStatus
  NaryLq<VX>::advise(Space&, Advisor&, const Delta& d) {
    if (VX::zero(d))
      n_zero++;
    else
      n_one++;
    return run ? ES_FIX : ES_NOFIX;
  }

  template<class VX>
  forceinline size_t
  NaryLq<VX>::dispose(Space& home) {
    Advisors<Advisor> as(c);
    x.cancel(home,as.advisor());
    c.dispose(home);
    (void) NaryPropagator<VX,PC_BOOL_NONE>::dispose(home);
    return sizeof(*this);
  }

  template<class VX>
  ExecStatus
  NaryLq<VX>::propagate(Space& home, const ModEventDelta&) {
    run = true;
    while (n_zero > 0) {
      int i = 0;
      while (x[i].none())
        i++;
      if (x[i].one())
        return ES_FAILED;
      // As the x[j] might be shared, only zero() but not zero_none()
      for (int j=i; j--; )
        GECODE_ME_CHECK(x[j].zero(home));
      n_zero -= i + 1;
      assert(n_zero >= 0);
      x.drop_fst(i+1);
    }

    while (n_one > 0) {
      int i = x.size() - 1;
      while (x[i].none())
        i--;
      assert(x[i].one());
      // As the x[j] might be shared, only one() but not one_none()
      for (int j=i+1; j<x.size(); j++)
        GECODE_ME_CHECK(x[j].one(home));
      n_one -= x.size() - i;
      assert(n_one >= 0);
      x.drop_lst(i-1);
    }

    if (x.size() < 2)
      return home.ES_SUBSUMED(*this);

    run = false;
    return ES_FIX;
  }


  /*
   * Less posting
   *
   */

  template<class BV>
  forceinline ExecStatus
  Le<BV>::post(Home home, BV b0, BV b1) {
    GECODE_ME_CHECK(b0.zero(home));
    GECODE_ME_CHECK(b1.one(home));
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

