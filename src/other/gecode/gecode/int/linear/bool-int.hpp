/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Tias Guns <tias.guns@cs.kuleuven.be>
 *
 *  Copyright:
 *     Christian Schulte, 2006
 *     Tias Guns, 2009
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

#include <algorithm>

#include <gecode/int/bool.hh>

namespace Gecode { namespace Int { namespace Linear {

  /*
   * Baseclass for integer Boolean sum using dependencies
   *
   */
  template<class VX>
  forceinline
  LinBoolInt<VX>::LinBoolInt(Home home, ViewArray<VX>& x0,
                             int n_s, int c0)
    : Propagator(home), co(home), x(x0), n_as(n_s), n_hs(n_s), c(c0) {
    Advisor* a = new (home) Advisor(home,*this,co);
    for (int i=n_as; i--; )
      x[i].subscribe(home,*a);
  }

  template<class VX>
  forceinline void
  LinBoolInt<VX>::normalize(void) {
    if (n_as != n_hs) {
      // Remove views for which no more subscriptions exist
      int n_x = x.size();
      for (int i=n_hs; i--; )
        if (!x[i].none()) {
          x[i]=x[--n_hs]; x[n_hs]=x[--n_x];
        }
      x.size(n_x);
    }
    assert(n_as == n_hs);
    // Remove assigned yet unsubscribed views
    {
      int n_x = x.size();
      for (int i=n_x-1; i>=n_hs; i--)
        if (x[i].one()) {
          c--; x[i]=x[--n_x];
        } else if (x[i].zero()) {
          x[i]=x[--n_x];
        }
      x.size(n_x);
    }
  }

  template<class VX>
  forceinline
  LinBoolInt<VX>::LinBoolInt(Space& home, bool share, LinBoolInt<VX>& p)
    : Propagator(home,share,p), n_as(p.n_as), n_hs(n_as) {
    p.normalize();
    c=p.c;
    co.update(home,share,p.co);
    x.update(home,share,p.x);
  }

  template<class VX>
  PropCost
  LinBoolInt<VX>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::HI);
  }

  template<class VX>
  forceinline size_t
  LinBoolInt<VX>::dispose(Space& home) {
    Advisors<Advisor> as(co);
    for (int i=n_hs; i--; )
      x[i].cancel(home,as.advisor());
    co.dispose(home);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  /*
   * Greater or equal propagator (integer rhs)
   *
   */
  template<class VX>
  forceinline
  GqBoolInt<VX>::GqBoolInt(Home home, ViewArray<VX>& x, int c)
    : LinBoolInt<VX>(home,x,c+1,c) {}

  template<class VX>
  forceinline
  GqBoolInt<VX>::GqBoolInt(Space& home, bool share, GqBoolInt<VX>& p)
    : LinBoolInt<VX>(home,share,p) {}

  template<class VX>
  Actor*
  GqBoolInt<VX>::copy(Space& home, bool share) {
    return new (home) GqBoolInt<VX>(home,share,*this);
  }

  template<class VX>
  ExecStatus
  GqBoolInt<VX>::advise(Space& home, Advisor& a, const Delta& d) {
    // Check whether propagator is running
    if (n_as == 0)
      return ES_FIX;

    if (VX::one(d)) {
      c--; goto check;
    }
    if (c+1 < n_as)
      goto check;
    // Find a new subscription
    for (int i = x.size()-1; i>=n_hs; i--)
      if (x[i].none()) {
        std::swap(x[i],x[n_hs]);
        x[n_hs++].subscribe(home,a);
        x.size(i+1);
        return ES_FIX;
      } else if (x[i].one()) {
        c--;
        if (c+1 < n_as) {
          x.size(i);
          assert(n_hs <= x.size());
          goto check;
        }
      }
    // No view left for subscription
    x.size(n_hs);
  check:
    // Do not update subscription
    n_as--;
    int n = x.size()-n_hs+n_as;
    if (n < c)
      return ES_FAILED;
    if ((c <= 0) || (c == n))
      return ES_NOFIX;
    else
      return ES_FIX;
  }

  template<class VX>
  ExecStatus
  GqBoolInt<VX>::propagate(Space& home, const ModEventDelta&) {
    if (c > 0) {
      assert((n_as == c) && (x.size() == n_hs));
      // Signal that propagator is running
      n_as = 0;
      // All views must be one to satisfy inequality
      for (int i=n_hs; i--; )
        if (x[i].none())
          GECODE_ME_CHECK(x[i].one_none(home));
    }
    return home.ES_SUBSUMED(*this);
  }

  template<class VX>
  ExecStatus
  GqBoolInt<VX>::post(Home home, ViewArray<VX>& x, int c) {
    // Eliminate assigned views
    int n_x = x.size();
    for (int i=n_x; i--; )
      if (x[i].zero()) {
        x[i] = x[--n_x];
      } else if (x[i].one()) {
        x[i] = x[--n_x]; c--;
      }
    x.size(n_x);
    // RHS too large
    if (n_x < c)
      return ES_FAILED;
    // Whatever the x[i] take for values, the inequality is subsumed
    if (c <= 0)
      return ES_OK;
    // Use Boolean disjunction for this special case
    if (c == 1)
      return Bool::NaryOrTrue<VX>::post(home,x);
    // All views must be one to satisfy inequality
    if (c == n_x) {
      for (int i=n_x; i--; )
        GECODE_ME_CHECK(x[i].one_none(home));
      return ES_OK;
    }
    // This is the needed invariant as c+1 subscriptions must be created
    assert(n_x > c);
    (void) new (home) GqBoolInt<VX>(home,x,c);
    return ES_OK;
  }




  /*
   * Equal propagator (integer rhs)
   *
   */
  template<class VX>
  forceinline
  EqBoolInt<VX>::EqBoolInt(Home home, ViewArray<VX>& x, int c)
    : LinBoolInt<VX>(home,x,std::max(c,x.size()-c)+1,c) {}

  template<class VX>
  forceinline
  EqBoolInt<VX>::EqBoolInt(Space& home, bool share, EqBoolInt<VX>& p)
    : LinBoolInt<VX>(home,share,p) {}

  template<class VX>
  Actor*
  EqBoolInt<VX>::copy(Space& home, bool share) {
    return new (home) EqBoolInt<VX>(home,share,*this);
  }

  template<class VX>
  ExecStatus
  EqBoolInt<VX>::advise(Space& home, Advisor& a, const Delta& d) {
    // Check whether propagator is running
    if (n_as == 0)
      return ES_FIX;

    if (VX::one(d))
      c--;
    if ((c+1 < n_as) && (x.size()-n_hs < c))
      goto check;
    // Find a new subscription
    for (int i = x.size()-1; i>=n_hs; i--)
      if (x[i].none()) {
        std::swap(x[i],x[n_hs]);
        x[n_hs++].subscribe(home,a);
        x.size(i+1);
        return ES_FIX;
      } else if (x[i].one()) {
        c--;
      }
    // No view left for subscription
    x.size(n_hs);
  check:
    // Do not update subscription
    n_as--;
    int n = x.size()-n_hs+n_as;
    if ((c < 0) || (c > n))
      return ES_FAILED;
    if ((c == 0) || (c == n))
      return ES_NOFIX;
    else
      return ES_FIX;
  }

  template<class VX>
  ExecStatus
  EqBoolInt<VX>::propagate(Space& home, const ModEventDelta&) {
    assert(x.size() == n_hs);
    // Signal that propagator is running
    n_as = 0;
    if (c == 0) {
      // All views must be zero to satisfy equality
      for (int i=n_hs; i--; )
        if (x[i].none())
          GECODE_ME_CHECK(x[i].zero_none(home));
    } else {
      // All views must be one to satisfy equality
      for (int i=n_hs; i--; )
        if (x[i].none())
          GECODE_ME_CHECK(x[i].one_none(home));
    }
    return home.ES_SUBSUMED(*this);
  }

  template<class VX>
  ExecStatus
  EqBoolInt<VX>::post(Home home, ViewArray<VX>& x, int c) {
    // Eliminate assigned views
    int n_x = x.size();
    for (int i=n_x; i--; )
      if (x[i].zero()) {
        x[i] = x[--n_x];
      } else if (x[i].one()) {
        x[i] = x[--n_x]; c--;
      }
    x.size(n_x);
    // RHS too small or too large
    if ((c < 0) || (c > n_x))
      return ES_FAILED;
    // All views must be zero to satisfy equality
    if (c == 0) {
      for (int i=n_x; i--; )
        GECODE_ME_CHECK(x[i].zero_none(home));
      return ES_OK;
    }
    // All views must be one to satisfy equality
    if (c == n_x) {
      for (int i=n_x; i--; )
        GECODE_ME_CHECK(x[i].one_none(home));
      return ES_OK;
    }
    (void) new (home) EqBoolInt<VX>(home,x,c);
    return ES_OK;
  }


  /*
   * Integer disequal to Boolean sum
   *
   */

  template<class VX>
  forceinline
  NqBoolInt<VX>::NqBoolInt(Home home, ViewArray<VX>& b, int c0)
    : BinaryPropagator<VX,PC_INT_VAL>(home,
                                      b[b.size()-2],
                                      b[b.size()-1]), x(b), c(c0) {
    assert(x.size() >= 2);
    x.size(x.size()-2);
  }

  template<class VX>
  forceinline size_t
  NqBoolInt<VX>::dispose(Space& home) {
    (void) BinaryPropagator<VX,PC_INT_VAL>::dispose(home);
    return sizeof(*this);
  }

  template<class VX>
  forceinline
  NqBoolInt<VX>::NqBoolInt(Space& home, bool share, NqBoolInt<VX>& p)
    : BinaryPropagator<VX,PC_INT_VAL>(home,share,p), x(home,p.x.size()) {
    // Eliminate all zeros and ones in original and update
    int n = p.x.size();
    int p_c = p.c;
    for (int i=n; i--; )
      if (p.x[i].zero()) {
        n--; p.x[i]=p.x[n]; x[i]=x[n];
      } else if (p.x[i].one()) {
        n--; p_c--; p.x[i]=p.x[n]; x[i]=x[n];
      } else {
        x[i].update(home,share,p.x[i]);
      }
    c = p_c; p.c = p_c;
    x.size(n); p.x.size(n);
  }

  template<class VX>
  forceinline ExecStatus
  NqBoolInt<VX>::post(Home home, ViewArray<VX>& x, int c) {
    int n = x.size();
    for (int i=n; i--; )
      if (x[i].one()) {
        x[i]=x[--n]; c--;
      } else if (x[i].zero()) {
        x[i]=x[--n];
      }
    x.size(n);
    if ((n < c) || (c < 0))
      return ES_OK;
    if (n == 0)
      return (c == 0) ? ES_FAILED : ES_OK;
    if (n == 1) {
      if (c == 1) {
        GECODE_ME_CHECK(x[0].zero_none(home));
      } else {
        GECODE_ME_CHECK(x[0].one_none(home));
      }
      return ES_OK;
    }
    (void) new (home) NqBoolInt(home,x,c);
    return ES_OK;
  }

  template<class VX>
  Actor*
  NqBoolInt<VX>::copy(Space& home, bool share) {
    return new (home) NqBoolInt<VX>(home,share,*this);
  }

  template<class VX>
  PropCost
  NqBoolInt<VX>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO, x.size());
  }

  template<class VX>
  forceinline bool
  NqBoolInt<VX>::resubscribe(Space& home, VX& y) {
    if (y.one())
      c--;
    int n = x.size();
    for (int i=n; i--; )
      if (x[i].one()) {
        c--; x[i]=x[--n];
      } else if (x[i].zero()) {
        x[i] = x[--n];
      } else {
        // New unassigned view found
        assert(!x[i].zero() && !x[i].one());
        // Move to y and subscribe
        y=x[i]; x[i]=x[--n];
        x.size(n);
        y.subscribe(home,*this,PC_INT_VAL,false);
        return true;
      }
    // All views have been assigned!
    x.size(0);
    return false;
  }

  template<class VX>
  ExecStatus
  NqBoolInt<VX>::propagate(Space& home, const ModEventDelta&) {
    bool s0 = true;
    if (x0.zero() || x0.one())
      s0 = resubscribe(home,x0);
    bool s1 = true;
    if (x1.zero() || x1.one())
      s1 = resubscribe(home,x1);
    int n = x.size() + s0 + s1;
    if ((n < c) || (c < 0))
      return home.ES_SUBSUMED(*this);
    if (n == 0)
      return (c == 0) ? ES_FAILED : home.ES_SUBSUMED(*this);
    if (n == 1) {
      if (s0) {
        if (c == 1) {
          GECODE_ME_CHECK(x0.zero_none(home));
        } else {
          GECODE_ME_CHECK(x0.one_none(home));
        }
      } else {
        assert(s1);
        if (c == 1) {
          GECODE_ME_CHECK(x1.zero_none(home));
        } else {
          GECODE_ME_CHECK(x1.one_none(home));
        }
      }
      return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }

  /*
   * Baseclass for reified integer Boolean sum
   *
   */
  template<class VX, class VB>
  forceinline
  ReLinBoolInt<VX,VB>::ReLinBoolInt(Home home, ViewArray<VX>& x0,
                                    int c0, VB b0)
    : Propagator(home), co(home), x(x0), n_s(x.size()), c(c0), b(b0) {
    x.subscribe(home,*new (home) Advisor(home,*this,co));
    b.subscribe(home,*this,PC_BOOL_VAL);
  }

  template<class VX, class VB>
  forceinline void
  ReLinBoolInt<VX,VB>::normalize(void) {
    if (n_s != x.size()) {
      int n_x = x.size();
      for (int i=n_x; i--; )
        if (!x[i].none())
          x[i] = x[--n_x];
      x.size(n_x);
      assert(x.size() == n_s);
    }
  }

  template<class VX, class VB>
  forceinline
  ReLinBoolInt<VX,VB>::ReLinBoolInt(Space& home, bool share, 
                                    ReLinBoolInt<VX,VB>& p)
    : Propagator(home,share,p), n_s(p.n_s), c(p.c) {
    p.normalize();
    co.update(home,share,p.co);
    x.update(home,share,p.x);
    b.update(home,share,p.b);
  }

  template<class VX, class VB>
  forceinline size_t
  ReLinBoolInt<VX,VB>::dispose(Space& home) {
    Advisors<Advisor> as(co);
    x.cancel(home,as.advisor());
    co.dispose(home);
    b.cancel(home,*this,PC_BOOL_VAL);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class VX, class VB>
  PropCost
  ReLinBoolInt<VX,VB>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::HI);
  }


  template<>
  /// Traits for Boolean negation view
  class BoolNegTraits<BoolView> {
  public:
    /// The negated view
    typedef NegBoolView NegView;
    /// Return negated View
    static NegBoolView neg(BoolView x) {
      NegBoolView y(x); return y;
    }
  };

  template<>
  /// Traits for Boolean negation view
  class BoolNegTraits<NegBoolView> {
  public:
    /// The negated view
    typedef BoolView NegView;
    /// Return negated View
    static BoolView neg(NegBoolView x) {
      return x.base();
    }
  };


  /*
   * Reified greater or equal propagator (integer rhs)
   * 
   */
  template<class VX, class VB, ReifyMode rm>
  forceinline
  ReGqBoolInt<VX,VB,rm>::ReGqBoolInt(Home home, ViewArray<VX>& x, int c, VB b)
    : ReLinBoolInt<VX,VB>(home,x,c,b) {}

  template<class VX, class VB, ReifyMode rm>
  forceinline
  ReGqBoolInt<VX,VB,rm>::ReGqBoolInt(Space& home, bool share, 
                                     ReGqBoolInt<VX,VB,rm>& p)
    : ReLinBoolInt<VX,VB>(home,share,p) {}

  template<class VX, class VB, ReifyMode rm>
  Actor*
  ReGqBoolInt<VX,VB,rm>::copy(Space& home, bool share) {
    return new (home) ReGqBoolInt<VX,VB,rm>(home,share,*this);
  }

  template<class VX, class VB, ReifyMode rm>
  ExecStatus
  ReGqBoolInt<VX,VB,rm>::advise(Space&, Advisor&, const Delta& d) {
    if (VX::one(d))
      c--;
    n_s--;
    if ((n_s < c) || (c <= 0))
      return ES_NOFIX;
    else
      return ES_FIX;
  }

  template<class VX, class VB, ReifyMode rm>
  ExecStatus
  ReGqBoolInt<VX,VB,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.none()) {
      if (c <= 0) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
      } else {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
      }
    } else {
      normalize();
      if (b.one()) {
        if (rm != RM_PMI)
          GECODE_REWRITE(*this,(GqBoolInt<VX>::post(home(*this),x,c)));
      } else {
        if (rm != RM_IMP) {
          ViewArray<typename BoolNegTraits<VX>::NegView> nx(home,x.size());
          for (int i=x.size(); i--; )
            nx[i]=BoolNegTraits<VX>::neg(x[i]);
          GECODE_REWRITE(*this,GqBoolInt<typename BoolNegTraits<VX>::NegView>
                         ::post(home(*this),nx,x.size()-c+1));
        }
      }
    }
    return home.ES_SUBSUMED(*this);
  }

  template<class VX, class VB, ReifyMode rm>
  ExecStatus
  ReGqBoolInt<VX,VB,rm>::post(Home home, ViewArray<VX>& x, int c, VB b) {
    assert(!b.assigned()); // checked before posting

    // Eliminate assigned views
    int n_x = x.size();
    for (int i=n_x; i--; )
      if (x[i].zero()) {
        x[i] = x[--n_x];
      } else if (x[i].one()) {
        x[i] = x[--n_x]; c--;
      }
    x.size(n_x);
    if (n_x < c) {
      // RHS too large
      if (rm != RM_PMI)
        GECODE_ME_CHECK(b.zero_none(home));
    } else if (c <= 0) {
      // Whatever the x[i] take for values, the inequality is subsumed
      if (rm != RM_IMP)
        GECODE_ME_CHECK(b.one_none(home));
    } else if ((c == 1) && (rm == RM_EQV)) {
      // Equivalent to Boolean disjunction
      return Bool::NaryOr<VX,VB>::post(home,x,b);
    } else if ((c == n_x) && (rm == RM_EQV)) {
      // Equivalent to Boolean conjunction, transform to Boolean disjunction
      ViewArray<typename BoolNegTraits<VX>::NegView> nx(home,n_x);
      for (int i=n_x; i--; )
        nx[i]=BoolNegTraits<VX>::neg(x[i]);
      return Bool::NaryOr
        <typename BoolNegTraits<VX>::NegView,
         typename BoolNegTraits<VB>::NegView>
        ::post(home,nx,BoolNegTraits<VB>::neg(b));
    } else {
      (void) new (home) ReGqBoolInt<VX,VB,rm>(home,x,c,b);
    }
    return ES_OK;
  }

  /*
   * Reified equal propagator (integer rhs)
   * 
   */
  template<class VX, class VB, ReifyMode rm>
  forceinline
  ReEqBoolInt<VX,VB,rm>::ReEqBoolInt(Home home, ViewArray<VX>& x, int c, VB b)
    : ReLinBoolInt<VX,VB>(home,x,c,b) {}

  template<class VX, class VB, ReifyMode rm>
  forceinline
  ReEqBoolInt<VX,VB,rm>::ReEqBoolInt(Space& home, bool share, 
                                     ReEqBoolInt<VX,VB,rm>& p)
    : ReLinBoolInt<VX,VB>(home,share,p) {}

  template<class VX, class VB, ReifyMode rm>
  Actor*
  ReEqBoolInt<VX,VB,rm>::copy(Space& home, bool share) {
    return new (home) ReEqBoolInt<VX,VB,rm>(home,share,*this);
  }

  template<class VX, class VB, ReifyMode rm>
  ExecStatus
  ReEqBoolInt<VX,VB,rm>::advise(Space&, Advisor&, const Delta& d) {
    if (VX::one(d))
      c--;
    n_s--;

    if ((c < 0) || (c > n_s) || (n_s == 0))
      return ES_NOFIX;
    else
      return ES_FIX;
  }

  template<class VX, class VB, ReifyMode rm>
  ExecStatus
  ReEqBoolInt<VX,VB,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.none()) {
      if ((c == 0) && (n_s == 0)) {
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
      } else {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
      }
    } else {
      normalize();
      if (b.one()) {
        if (rm != RM_PMI)
          GECODE_REWRITE(*this,(EqBoolInt<VX>::post(home(*this),x,c)));
      } else {
        if (rm != RM_IMP)
          GECODE_REWRITE(*this,(NqBoolInt<VX>::post(home(*this),x,c)));
      }
    }
    return home.ES_SUBSUMED(*this);
  }

  template<class VX, class VB, ReifyMode rm>
  ExecStatus
  ReEqBoolInt<VX,VB,rm>::post(Home home, ViewArray<VX>& x, int c, VB b) {
    assert(!b.assigned()); // checked before posting

    // Eliminate assigned views
    int n_x = x.size();
    for (int i=n_x; i--; )
      if (x[i].zero()) {
        x[i] = x[--n_x];
      } else if (x[i].one()) {
        x[i] = x[--n_x]; c--;
      }
    x.size(n_x);
    if ((n_x < c) || (c < 0)) {
      // RHS too large
      if (rm != RM_PMI)
        GECODE_ME_CHECK(b.zero_none(home));
    } else if ((c == 0) && (n_x == 0)) {
      // all variables set, and c == 0: equality
      if (rm != RM_IMP)
        GECODE_ME_CHECK(b.one_none(home));
    } else if ((c == 0) && (rm == RM_EQV)) {
      // Equivalent to Boolean disjunction
      return Bool::NaryOr<VX,typename BoolNegTraits<VB>::NegView>
        ::post(home,x,BoolNegTraits<VB>::neg(b));
    } else if ((c == n_x) && (rm == RM_EQV)) {
      // Equivalent to Boolean conjunction, transform to Boolean disjunction
      ViewArray<typename BoolNegTraits<VX>::NegView> nx(home,n_x);
      for (int i=n_x; i--; )
        nx[i]=BoolNegTraits<VX>::neg(x[i]);
      return Bool::NaryOr
        <typename BoolNegTraits<VX>::NegView,
         typename BoolNegTraits<VB>::NegView>
        ::post(home,nx,BoolNegTraits<VB>::neg(b));
    } else {
      (void) new (home) ReEqBoolInt<VX,VB,rm>(home,x,c,b);
    }
    return ES_OK;
  }


}}}

// STATISTICS: int-prop

