/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2012
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

#include <gecode/int/rel.hh>

#include <climits>
#include <algorithm>

namespace Gecode { namespace Int { namespace Arithmetic {

  /*
   * Positive bounds consistent power
   *
   */
  template<class VA, class VB, class Ops>
  inline ExecStatus
  prop_pow_plus_bnd(Space& home, VA x0, VB x1, const Ops& ops) {
    bool mod;
    do {
      mod = false;
      {
        ModEvent me = x0.lq(home,ops.fnroot(x1.max()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x0.gq(home,ops.cnroot(x1.min()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x1.lq(home,ops.pow(x0.max()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
      {
        ModEvent me = x1.gq(home,ops.pow(x0.min()));
        if (me_failed(me)) return ES_FAILED;
        mod |= me_modified(me);
      }
    } while (mod);
    return ES_OK;
  }

  template<class VA, class VB, class Ops>
  forceinline
  PowPlusBnd<VA,VB,Ops>::PowPlusBnd(Home home, VA x0, VB x1, const Ops& o)
    : MixBinaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND>(home,x0,x1), 
      ops(o) {}

  template<class VA, class VB, class Ops>
  forceinline ExecStatus
  PowPlusBnd<VA,VB,Ops>::post(Home home, VA x0, VB x1, Ops ops) {
    GECODE_ME_CHECK(x0.gq(home,0));
    GECODE_ME_CHECK(x1.gq(home,0));
    GECODE_ES_CHECK(prop_pow_plus_bnd(home,x0,x1,ops));
    if (!x0.assigned()) {
      assert(!x1.assigned());
      (void) new (home) PowPlusBnd<VA,VB,Ops>(home,x0,x1,ops);
    }
    return ES_OK;
  }

  template<class VA, class VB, class Ops>
  forceinline
  PowPlusBnd<VA,VB,Ops>::PowPlusBnd(Space& home, bool share, 
                                    PowPlusBnd<VA,VB,Ops>& p)
    : MixBinaryPropagator<VA,PC_INT_BND,VB,PC_INT_BND>(home,share,p),
      ops(p.ops) {}

  template<class VA, class VB, class Ops>
  Actor*
  PowPlusBnd<VA,VB,Ops>::copy(Space& home, bool share) {
    return new (home) PowPlusBnd<VA,VB,Ops>(home,share,*this);
  }

  template<class VA, class VB, class Ops>
  ExecStatus
  PowPlusBnd<VA,VB,Ops>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ES_CHECK(prop_pow_plus_bnd(home,x0,x1,ops));
    return x0.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }



  /*
   * Bounds consistent power
   *
   */

  template<class Ops>
  inline ExecStatus
  prop_pow_bnd(Space& home, IntView x0, IntView x1, const Ops& ops) {
    assert((x0.min() < 0) && (0 < x0.max()));
    if (ops.even()) {
      assert(x1.min() >= 0);
      int u = ops.fnroot(x1.max());
      GECODE_ME_CHECK(x0.lq(home,u));
      GECODE_ME_CHECK(x0.gq(home,-u));
      GECODE_ME_CHECK(x1.lq(home,std::max(ops.pow(x0.max()),
                                          ops.pow(-x0.min()))));
    } else {
      assert((x1.min() < 0) && (0 < x1.max()));
      GECODE_ME_CHECK(x0.lq(home,ops.fnroot(x1.max())));
      GECODE_ME_CHECK(x0.gq(home,-ops.fnroot(-x1.min())));
      GECODE_ME_CHECK(x1.lq(home,ops.pow(x0.max())));
      GECODE_ME_CHECK(x1.gq(home,ops.pow(x0.min())));
    }       
    return ES_OK;
  }

  template<class Ops>
  forceinline
  PowBnd<Ops>::PowBnd(Home home, IntView x0, IntView x1, const Ops& o)
    : BinaryPropagator<IntView,PC_INT_BND>(home,x0,x1), 
      ops(o) {}

  template<class Ops>
  inline ExecStatus
  PowBnd<Ops>::post(Home home, IntView x0, IntView x1, Ops ops) {
    if (static_cast<unsigned int>(ops.exp()) >= sizeof(int) * CHAR_BIT) {
      // The integer limits allow only -1, 0, 1 for x0
      GECODE_ME_CHECK(x0.lq(home,1));
      GECODE_ME_CHECK(x0.gq(home,-1));
      // Just rewrite to values that can be handeled without overflow
      ops.exp(ops.even() ? 2 : 1);
    }

    if (ops.exp() == 0) {
      GECODE_ME_CHECK(x1.eq(home,1));
      return ES_OK;
    } else if (ops.exp() == 1) {
      return Rel::EqBnd<IntView,IntView>::post(home,x0,x1);
    }

    if (same(x0,x1)) {
      assert(ops.exp() != 0);
      GECODE_ME_CHECK(x0.lq(home,1));
      GECODE_ME_CHECK(x0.gq(home,ops.even() ? 0 : -1));
      return ES_OK;
    }

    // Limits values such that no overflow can occur
    assert(Limits::max == -Limits::min);
    {
      int l = ops.fnroot(Limits::max);
      GECODE_ME_CHECK(x0.lq(home,l));
      GECODE_ME_CHECK(x0.gq(home,-l));
    }

    if ((x0.min() >= 0) || ((x1.min() >= 0) && !ops.even()))
      return PowPlusBnd<IntView,IntView,Ops>::post(home,x0,x1,ops);

    if (ops.even() && (x0.max() <= 0))
      return PowPlusBnd<MinusView,IntView,Ops>
        ::post(home,MinusView(x0),x1,ops);

    if (!ops.even() && ((x0.max() <= 0) || (x1.max() <= 0)))
      return PowPlusBnd<MinusView,MinusView,Ops>
        ::post(home,MinusView(x0),MinusView(x1),ops);
    
    if (ops.even())
      GECODE_ME_CHECK(x1.gq(home,0));
    
    assert((x0.min() < 0) && (x0.max() > 0));

    if (ops.even()) {
      GECODE_ME_CHECK(x1.lq(home,std::max(ops.pow(x0.min()),
                                          ops.pow(x0.max()))));
    } else {
      GECODE_ME_CHECK(x1.lq(home,ops.pow(x0.max())));
      GECODE_ME_CHECK(x1.gq(home,ops.pow(x0.min())));
    }

    (void) new (home) PowBnd<Ops>(home,x0,x1,ops);
    return ES_OK;
  }

  template<class Ops>
  forceinline
  PowBnd<Ops>::PowBnd(Space& home, bool share, PowBnd<Ops>& p)
    : BinaryPropagator<IntView,PC_INT_BND>(home,share,p),
      ops(p.ops) {}

  template<class Ops>
  Actor*
  PowBnd<Ops>::copy(Space& home, bool share) {
    return new (home) PowBnd<Ops>(home,share,*this);
  }

  template<class Ops>
  ExecStatus
  PowBnd<Ops>::propagate(Space& home, const ModEventDelta&) {
    if ((x0.min() >= 0) || ((x1.min() >= 0) && !ops.even()))
      GECODE_REWRITE(*this,(PowPlusBnd<IntView,IntView,Ops>
                            ::post(home(*this),x0,x1,ops)));
    
    if (ops.even() && (x0.max() <= 0))
      GECODE_REWRITE(*this,(PowPlusBnd<MinusView,IntView,Ops>
                            ::post(home(*this),MinusView(x0),x1,ops)));

    if (!ops.even() && ((x0.max() <= 0) || (x1.max() <= 0)))
      GECODE_REWRITE(*this,(PowPlusBnd<MinusView,MinusView,Ops>
                            ::post(home(*this),MinusView(x0),
                                   MinusView(x1),ops)));

    GECODE_ES_CHECK(prop_pow_bnd<Ops>(home,x0,x1,ops));
    
    if (x0.assigned() && x1.assigned())
      return (ops.pow(x0.val()) == x1.val()) ?
        home.ES_SUBSUMED(*this) : ES_FAILED;

    return ES_NOFIX;
  }


  /*
   * Value mappings for power and nroot
   *
   */

  /// Mapping integer to power
  template<class Ops>
  class ValuesMapPow {
  protected:
    /// Operations
    Ops ops;
  public:
    /// Initialize with operations \a o
    forceinline ValuesMapPow(const Ops& o) : ops(o) {}
    /// Perform mapping
    forceinline int val(int x) const {
      return ops.pow(x);
    }
  };

  /// Mapping integer (must be an n-th power) to n-th root
  template<class Ops>
  class ValuesMapNroot {
  protected:
    /// Operations
    Ops ops;
  public:
    /// Initialize with operations \a o
    forceinline ValuesMapNroot(const Ops& o) : ops(o) {}
    /// Perform mapping
    forceinline int val(int x) const {
      return ops.fnroot(x);
    }
  };

  /// Mapping integer (must be an n-th power) to n-th root (signed)
  template<class Ops>
  class ValuesMapNrootSigned {
  protected:
    /// Operations
    Ops ops;
  public:
    /// Initialize with operations \a o
    forceinline ValuesMapNrootSigned(const Ops& o) : ops(o) {}
    /// Perform mapping
    forceinline int val(int x) const {
      if (x < 0)
        return -ops.fnroot(-x);
      else
        return ops.fnroot(x);
    }
  };


  /*
   * Positive domain consistent power
   *
   */
  template<class VA, class VB, class Ops>
  forceinline
  PowPlusDom<VA,VB,Ops>::PowPlusDom(Home home, VA x0, VB x1, const Ops& o)
    : MixBinaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM>(home,x0,x1), 
      ops(o) {}

  template<class VA, class VB, class Ops>
  forceinline ExecStatus
  PowPlusDom<VA,VB,Ops>::post(Home home, VA x0, VB x1, Ops ops) {
    GECODE_ME_CHECK(x0.gq(home,0));
    GECODE_ME_CHECK(x1.gq(home,0));
    GECODE_ES_CHECK(prop_pow_plus_bnd(home,x0,x1,ops));
    if (!x0.assigned()) {
      assert(!x1.assigned());
      (void) new (home) PowPlusDom<VA,VB,Ops>(home,x0,x1,ops);
    }
    return ES_OK;
  }

  template<class VA, class VB, class Ops>
  forceinline
  PowPlusDom<VA,VB,Ops>::PowPlusDom(Space& home, bool share, 
                                    PowPlusDom<VA,VB,Ops>& p)
    : MixBinaryPropagator<VA,PC_INT_DOM,VB,PC_INT_DOM>(home,share,p),
      ops(p.ops) {}

  template<class VA, class VB, class Ops>
  Actor*
  PowPlusDom<VA,VB,Ops>::copy(Space& home, bool share) {
    return new (home) PowPlusDom<VA,VB,Ops>(home,share,*this);
  }

  template<class VA, class VB, class Ops>
  PropCost
  PowPlusDom<VA,VB,Ops>::cost(const Space&, const ModEventDelta& med) const {
    if (VA::me(med) == ME_INT_VAL)
      return PropCost::unary(PropCost::LO);
    else if (VA::me(med) == ME_INT_DOM)
      return PropCost::binary(PropCost::HI);
    else
      return PropCost::binary(PropCost::LO);
  }

  template<class VA, class VB, class Ops>
  ExecStatus
  PowPlusDom<VA,VB,Ops>::propagate(Space& home, const ModEventDelta& med) {
    if (VA::me(med) != ME_INT_DOM) {
      GECODE_ES_CHECK(prop_pow_plus_bnd(home,x0,x1,ops));
      return x0.assigned() ?
        home.ES_SUBSUMED(*this)
        : home.ES_NOFIX_PARTIAL(*this,VA::med(ME_INT_DOM));
    }

    {
      ViewValues<VA> v0(x0);
      ValuesMapPow<Ops> vmp(ops);
      Iter::Values::Map<ViewValues<VA>,ValuesMapPow<Ops> > s0(v0,vmp);
      GECODE_ME_CHECK(x1.inter_v(home,s0,false));
    }

    {
      ViewValues<VB> v1(x1);
      ValuesMapNroot<Ops> vmn(ops);
      Iter::Values::Map<ViewValues<VB>,ValuesMapNroot<Ops> > s1(v1,vmn);
      GECODE_ME_CHECK(x0.inter_v(home,s1,false));
    }

    return x0.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }


  /*
   * Domain consistent power
   *
   */

  template<class Ops>
  forceinline
  PowDom<Ops>::PowDom(Home home, IntView x0, IntView x1, const Ops& o)
    : BinaryPropagator<IntView,PC_INT_DOM>(home,x0,x1), ops(o) {}

  template<class Ops>
  inline ExecStatus
  PowDom<Ops>::post(Home home, IntView x0, IntView x1, Ops ops) {
    if (static_cast<unsigned int>(ops.exp()) >= sizeof(int) * CHAR_BIT) {
      // The integer limits allow only -1, 0, 1 for x0
      GECODE_ME_CHECK(x0.lq(home,1));
      GECODE_ME_CHECK(x0.gq(home,-1));
      // Just rewrite to values that can be handeled without overflow
      ops.exp(ops.even() ? 2 : 1);
    }

    if (ops.exp() == 0) {
      GECODE_ME_CHECK(x1.eq(home,1));
      return ES_OK;
    } else if (ops.exp() == 1) {
      return Rel::EqDom<IntView,IntView>::post(home,x0,x1);
    }

    if (same(x0,x1)) {
      assert(ops.exp() != 0);
      GECODE_ME_CHECK(x0.lq(home,1));
      GECODE_ME_CHECK(x0.gq(home,ops.even() ? 0 : -1));
      return ES_OK;
    }

    // Limits values such that no overflow can occur
    assert(Limits::max == -Limits::min);
    {
      int l = ops.fnroot(Limits::max);
      GECODE_ME_CHECK(x0.lq(home,l));
      GECODE_ME_CHECK(x0.gq(home,-l));
    }

    if ((x0.min() >= 0) || ((x1.min() >= 0) && !ops.even()))
      return PowPlusDom<IntView,IntView,Ops>::post(home,x0,x1,ops);

    if (ops.even() && (x0.max() <= 0))
      return PowPlusDom<MinusView,IntView,Ops>
        ::post(home,MinusView(x0),x1,ops);

    if (!ops.even() && ((x0.max() <= 0) || (x1.max() <= 0)))
      return PowPlusDom<MinusView,MinusView,Ops>
        ::post(home,MinusView(x0),MinusView(x1),ops);
    
    if (ops.even())
      GECODE_ME_CHECK(x1.gq(home,0));
    
    assert((x0.min() < 0) && (x0.max() > 0));

    if (ops.even()) {
      GECODE_ME_CHECK(x1.lq(home,std::max(ops.pow(x0.min()),
                                          ops.pow(x0.max()))));
    } else {
      GECODE_ME_CHECK(x1.lq(home,ops.pow(x0.max())));
      GECODE_ME_CHECK(x1.gq(home,ops.pow(x0.min())));
    }

    (void) new (home) PowDom<Ops>(home,x0,x1,ops);
    return ES_OK;
  }

  template<class Ops>
  forceinline
  PowDom<Ops>::PowDom(Space& home, bool share, PowDom<Ops>& p)
    : BinaryPropagator<IntView,PC_INT_DOM>(home,share,p), 
      ops(p.ops) {}

  template<class Ops>
  Actor*
  PowDom<Ops>::copy(Space& home, bool share) {
    return new (home) PowDom<Ops>(home,share,*this);
  }

  template<class Ops>
  PropCost
  PowDom<Ops>::cost(const Space&, const ModEventDelta& med) const {
    if (IntView::me(med) == ME_INT_VAL)
      return PropCost::unary(PropCost::LO);
    else if (IntView::me(med) == ME_INT_DOM)
      return PropCost::binary(PropCost::HI);
    else
      return PropCost::binary(PropCost::LO);
  }

  template<class Ops>
  ExecStatus
  PowDom<Ops>::propagate(Space& home, const ModEventDelta& med) {
    if ((x0.min() >= 0) || ((x1.min() >= 0) && !ops.even()))
      GECODE_REWRITE(*this,(PowPlusDom<IntView,IntView,Ops>
                            ::post(home(*this),x0,x1,ops)));
    
    if (ops.even() && (x0.max() <= 0))
      GECODE_REWRITE(*this,(PowPlusDom<MinusView,IntView,Ops>
                            ::post(home(*this),MinusView(x0),x1,ops)));
    
    if (!ops.even() && ((x0.max() <= 0) || (x1.max() <= 0)))
      GECODE_REWRITE(*this,(PowPlusDom<MinusView,MinusView,Ops>
                            ::post(home(*this),MinusView(x0),
                                   MinusView(x1),ops)));

    if (IntView::me(med) != ME_INT_DOM) {
      GECODE_ES_CHECK(prop_pow_bnd<Ops>(home,x0,x1,ops));
      if (x0.assigned() && x1.assigned())
        return (ops.pow(x0.val()) == x1.val()) ?
          home.ES_SUBSUMED(*this) : ES_FAILED;

      return home.ES_NOFIX_PARTIAL(*this,IntView::med(ME_INT_DOM));
    }

    Region r(home);
    if (ops.even()) {
      ViewValues<IntView> i(x0), j(x0);
      using namespace Iter::Values;
      Positive<ViewValues<IntView> > pos(i);
      Negative<ViewValues<IntView> > neg(j);
      Minus m(r,neg);

      ValuesMapPow<Ops> vmp(ops);
      Map<Positive<ViewValues<IntView> >,ValuesMapPow<Ops>,true> sp(pos,vmp);
      Map<Minus,ValuesMapPow<Ops>,true> sm(m,vmp);
      Union<Map<Positive<ViewValues<IntView> >,ValuesMapPow<Ops>,true>,
        Map<Minus,ValuesMapPow<Ops>,true> > u(sp,sm);
      GECODE_ME_CHECK(x1.inter_v(home,u,false));
    } else {
      ViewValues<IntView> v0(x0);
      ValuesMapPow<Ops> vmp(ops);
      Iter::Values::Map<ViewValues<IntView>,ValuesMapPow<Ops> > s0(v0,vmp);
      GECODE_ME_CHECK(x1.inter_v(home,s0,false));
    }

    if (ops.even()) {
      ViewValues<IntView> i(x1), j(x1);
      using namespace Iter::Values;
      ValuesMapNroot<Ops> vmn(ops);
      Map<ViewValues<IntView>,ValuesMapNroot<Ops>,true> si(i,vmn), sj(j,vmn);
      Minus mi(r,si);
      Union<Minus,
        Map<ViewValues<IntView>,ValuesMapNroot<Ops>,true> > u(mi,sj);
      GECODE_ME_CHECK(x0.inter_v(home,u,false));
    } else {
      ViewValues<IntView> v1(x1);
      ValuesMapNrootSigned<Ops> vmn(ops);
      Iter::Values::Map<ViewValues<IntView>,ValuesMapNrootSigned<Ops> > 
        s1(v1,vmn);
      GECODE_ME_CHECK(x0.inter_v(home,s1,false));
    }

    return x0.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

}}}

// STATISTICS: int-prop

