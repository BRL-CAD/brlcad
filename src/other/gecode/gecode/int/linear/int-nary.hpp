/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

#include <gecode/int/linear/int-noview.hpp>

namespace Gecode { namespace Int { namespace Linear {

  /**
   * \brief Test if only unit-coefficient arrays used
   *
   */
  template<class P, class N>
  forceinline bool
  isunit(ViewArray<P>&, ViewArray<N>&) { return false; }
  template<>
  forceinline bool
  isunit(ViewArray<IntView>&, ViewArray<IntView>&) { return true; }
  template<>
  forceinline bool
  isunit(ViewArray<IntView>&, ViewArray<NoView>&) { return true; }
  template<>
  forceinline bool
  isunit(ViewArray<NoView>&, ViewArray<IntView>&) { return true; }

  /*
   * Linear propagators
   *
   */
  template<class Val, class P, class N, PropCond pc>
  forceinline
  Lin<Val,P,N,pc>::Lin(Home home, ViewArray<P>& x0, ViewArray<N>& y0, Val c0)
    : Propagator(home), x(x0), y(y0), c(c0) {
    x.subscribe(home,*this,pc);
    y.subscribe(home,*this,pc);
  }

  template<class Val, class P, class N, PropCond pc>
  forceinline
  Lin<Val,P,N,pc>::Lin(Space& home, bool share, Lin<Val,P,N,pc>& p)
    : Propagator(home,share,p), c(p.c) {
    x.update(home,share,p.x);
    y.update(home,share,p.y);
  }

  template<class Val, class P, class N, PropCond pc>
  PropCost
  Lin<Val,P,N,pc>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO, x.size()+y.size());
  }

  template<class Val, class P, class N, PropCond pc>
  forceinline size_t
  Lin<Val,P,N,pc>::dispose(Space& home) {
    x.cancel(home,*this,pc);
    y.cancel(home,*this,pc);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  /*
   * Reified linear propagators
   *
   */
  template<class Val, class P, class N, PropCond pc, class Ctrl>
  forceinline
  ReLin<Val,P,N,pc,Ctrl>::ReLin
  (Home home, ViewArray<P>& x, ViewArray<N>& y, Val c, Ctrl b0)
    : Lin<Val,P,N,pc>(home,x,y,c), b(b0) {
    b.subscribe(home,*this,PC_INT_VAL);
  }

  template<class Val, class P, class N, PropCond pc, class Ctrl>
  forceinline
  ReLin<Val,P,N,pc,Ctrl>::ReLin
  (Space& home, bool share, ReLin<Val,P,N,pc,Ctrl>& p)
    : Lin<Val,P,N,pc>(home,share,p) {
    b.update(home,share,p.b);
  }

  template<class Val, class P, class N, PropCond pc, class Ctrl>
  forceinline size_t
  ReLin<Val,P,N,pc,Ctrl>::dispose(Space& home) {
    b.cancel(home,*this,PC_BOOL_VAL);
    (void) Lin<Val,P,N,pc>::dispose(home);
    return sizeof(*this);
  }

  /*
   * Computing bounds
   *
   */

  template<class Val, class View>
  void
  bounds_p(ModEventDelta med, ViewArray<View>& x, Val& c, Val& sl, Val& su) {
    int n = x.size();
    if (IntView::me(med) == ME_INT_VAL) {
      for (int i = n; i--; ) {
        Val m = x[i].min();
        if (x[i].assigned()) {
          c -= m; x[i] = x[--n];
        } else {
          sl -= m; su -= x[i].max();
        }
      }
      x.size(n);
    } else {
      for (int i = n; i--; ) {
        sl -= x[i].min(); su -= x[i].max();
      }
    }
  }

  template<class Val, class View>
  void
  bounds_n(ModEventDelta med, ViewArray<View>& y, Val& c, Val& sl, Val& su) {
    int n = y.size();
    if (IntView::me(med) == ME_INT_VAL) {
      for (int i = n; i--; ) {
        Val m = y[i].max();
        if (y[i].assigned()) {
          c += m; y[i] = y[--n];
        } else {
          sl += m; su += y[i].min();
        }
      }
      y.size(n);
    } else {
      for (int i = n; i--; ) {
        sl += y[i].max(); su += y[i].min();
      }
    }
  }


  template<class Val, class P, class N>
  ExecStatus
  prop_bnd(Space& home, ModEventDelta med, Propagator& p,
           ViewArray<P>& x, ViewArray<N>& y, Val& c) {
    // Eliminate singletons
    Val sl = 0;
    Val su = 0;

    bounds_p<Val,P>(med, x, c, sl, su);
    bounds_n<Val,N>(med, y, c, sl, su);

    if ((IntView::me(med) == ME_INT_VAL) && ((x.size() + y.size()) <= 1)) {
      if (x.size() == 1) {
        GECODE_ME_CHECK(x[0].eq(home,c));
        return home.ES_SUBSUMED(p);
      }
      if (y.size() == 1) {
        GECODE_ME_CHECK(y[0].eq(home,-c));
        return home.ES_SUBSUMED(p);
      }
      return (c == static_cast<Val>(0)) ?
        home.ES_SUBSUMED(p) : ES_FAILED;
    }

    sl += c; su += c;

    const int mod_sl = 1;
    const int mod_su = 2;

    int mod = mod_sl | mod_su;

    do {
      if (mod & mod_sl) {
        mod -= mod_sl;
        // Propagate upper bound for positive variables
        for (int i = x.size(); i--; ) {
          const Val xi_max = x[i].max();
          ModEvent me = x[i].lq(home,sl + x[i].min());
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            su += xi_max - x[i].max();
            mod |= mod_su;
          }
        }
        // Propagate lower bound for negative variables
        for (int i = y.size(); i--; ) {
          const Val yi_min = y[i].min();
          ModEvent me = y[i].gq(home,y[i].max() - sl);
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            su += y[i].min() - yi_min;
            mod |= mod_su;
          }
        }
      }
      if (mod & mod_su) {
        mod -= mod_su;
        // Propagate lower bound for positive variables
        for (int i = x.size(); i--; ) {
          const Val xi_min = x[i].min();
          ModEvent me = x[i].gq(home,su + x[i].max());
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            sl += xi_min - x[i].min();
            mod |= mod_sl;
          }
        }
        // Propagate upper bound for negative variables
        for (int i = y.size(); i--; ) {
          const Val yi_max = y[i].max();
          ModEvent me = y[i].lq(home,y[i].min() - su);
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            sl += y[i].max() - yi_max;
            mod |= mod_sl;
          }
        }
      }
    } while (mod);

    return (sl == su) ? home.ES_SUBSUMED(p) : ES_FIX;
  }

  /*
   * Bound consistent linear equation
   *
   */

  template<class Val, class P, class N>
  forceinline
  Eq<Val,P,N>::Eq(Home home, ViewArray<P>& x, ViewArray<N>& y, Val c)
    : Lin<Val,P,N,PC_INT_BND>(home,x,y,c) {}

  template<class Val, class P, class N>
  ExecStatus
  Eq<Val,P,N>::post(Home home, ViewArray<P>& x, ViewArray<N>& y, Val c) {
    ViewArray<NoView> nva;
    if (y.size() == 0) {
      (void) new (home) Eq<Val,P,NoView>(home,x,nva,c);
    } else if (x.size() == 0) {
      (void) new (home) Eq<Val,N,NoView>(home,y,nva,-c);
    } else {
      (void) new (home) Eq<Val,P,N>(home,x,y,c);
    }
    return ES_OK;
  }


  template<class Val, class P, class N>
  forceinline
  Eq<Val,P,N>::Eq(Space& home, bool share, Eq<Val,P,N>& p)
    : Lin<Val,P,N,PC_INT_BND>(home,share,p) {}

  /**
   * \brief Rewriting of equality to binary propagators
   *
   */
  template<class Val, class P, class N>
  forceinline Actor*
  eqtobin(Space&, bool, Propagator&, ViewArray<P>&, ViewArray<N>&, Val) {
    return NULL;
  }
  template<class Val>
  forceinline Actor*
  eqtobin(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<NoView>&, Val c) {
    assert(x.size() == 2);
    return new (home) EqBin<Val,IntView,IntView>
      (home,share,p,x[0],x[1],c);
  }
  template<class Val>
  forceinline Actor*
  eqtobin(Space& home, bool share, Propagator& p,
          ViewArray<NoView>&, ViewArray<IntView>& y, Val c) {
    assert(y.size() == 2);
    return new (home) EqBin<Val,IntView,IntView>
      (home,share,p,y[0],y[1],-c);
  }
  template<class Val>
  forceinline Actor*
  eqtobin(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<IntView>& y, Val c) {
    if (x.size() == 2)
      return new (home) EqBin<Val,IntView,IntView>
        (home,share,p,x[0],x[1],c);
    if (x.size() == 1)
      return new (home) EqBin<Val,IntView,MinusView>
        (home,share,p,x[0],MinusView(y[0]),c);
    return new (home) EqBin<Val,IntView,IntView>
      (home,share,p,y[0],y[1],-c);
  }

  /**
   * \brief Rewriting of equality to ternary propagators
   *
   */
  template<class Val, class P, class N>
  forceinline Actor*
  eqtoter(Space&, bool, Propagator&, ViewArray<P>&, ViewArray<N>&, Val) {
    return NULL;
  }
  template<class Val>
  forceinline Actor*
  eqtoter(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<NoView>&, Val c) {
    assert(x.size() == 3);
    return new (home) EqTer<Val,IntView,IntView,IntView>
      (home,share,p,x[0],x[1],x[2],c);
  }
  template<class Val>
  forceinline Actor*
  eqtoter(Space& home, bool share, Propagator& p,
          ViewArray<NoView>&, ViewArray<IntView>& y, Val c) {
    assert(y.size() == 3);
    return new (home) EqTer<Val,IntView,IntView,IntView>
      (home,share,p,y[0],y[1],y[2],-c);
  }
  template<class Val>
  forceinline Actor*
  eqtoter(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<IntView>& y, Val c) {
    if (x.size() == 3)
      return new (home) EqTer<Val,IntView,IntView,IntView>
        (home,share,p,x[0],x[1],x[2],c);
    if (x.size() == 2)
      return new (home) EqTer<Val,IntView,IntView,MinusView>
        (home,share,p,x[0],x[1],MinusView(y[0]),c);
    if (x.size() == 1)
      return new (home) EqTer<Val,IntView,IntView,MinusView>
        (home,share,p,y[0],y[1],MinusView(x[0]),-c);
    return new (home) EqTer<Val,IntView,IntView,IntView>
      (home,share,p,y[0],y[1],y[2],-c);
  }

  template<class Val, class P, class N>
  Actor*
  Eq<Val,P,N>::copy(Space& home, bool share) {
    if (isunit(x,y)) {
      // Check whether rewriting is possible
      if (x.size() + y.size() == 2)
        return eqtobin(home,share,*this,x,y,c);
      if (x.size() + y.size() == 3)
        return eqtoter(home,share,*this,x,y,c);
    }
    return new (home) Eq<Val,P,N>(home,share,*this);
  }

  template<class Val, class P, class N>
  ExecStatus
  Eq<Val,P,N>::propagate(Space& home, const ModEventDelta& med) {
    return prop_bnd<Val,P,N>(home,med,*this,x,y,c);
  }

  /*
   * Reified bound consistent linear equation
   *
   */

  template<class Val, class P, class N, class Ctrl, ReifyMode rm>
  forceinline
  ReEq<Val,P,N,Ctrl,rm>::ReEq(Home home,
                              ViewArray<P>& x, ViewArray<N>& y, Val c, Ctrl b)
    : ReLin<Val,P,N,PC_INT_BND,Ctrl>(home,x,y,c,b) {}

  template<class Val, class P, class N, class Ctrl, ReifyMode rm>
  ExecStatus
  ReEq<Val,P,N,Ctrl,rm>::post(Home home,
                              ViewArray<P>& x, ViewArray<N>& y, Val c, Ctrl b) {
    ViewArray<NoView> nva;
    if (y.size() == 0) {
      (void) new (home) ReEq<Val,P,NoView,Ctrl,rm>(home,x,nva,c,b);
    } else if (x.size() == 0) {
      (void) new (home) ReEq<Val,N,NoView,Ctrl,rm>(home,y,nva,-c,b);
    } else {
      (void) new (home) ReEq<Val,P,N,Ctrl,rm>(home,x,y,c,b);
    }
    return ES_OK;
  }


  template<class Val, class P, class N, class Ctrl, ReifyMode rm>
  forceinline
  ReEq<Val,P,N,Ctrl,rm>::ReEq(Space& home, bool share, 
                              ReEq<Val,P,N,Ctrl,rm>& p)
    : ReLin<Val,P,N,PC_INT_BND,Ctrl>(home,share,p) {}

  template<class Val, class P, class N, class Ctrl, ReifyMode rm>
  Actor*
  ReEq<Val,P,N,Ctrl,rm>::copy(Space& home, bool share) {
    return new (home) ReEq<Val,P,N,Ctrl,rm>(home,share,*this);
  }

  template<class Val, class P, class N, class Ctrl, ReifyMode rm>
  ExecStatus
  ReEq<Val,P,N,Ctrl,rm>::propagate(Space& home, const ModEventDelta& med) {
    if (b.zero()) {
      if (rm == RM_IMP)
        return home.ES_SUBSUMED(*this);        
      GECODE_REWRITE(*this,(Nq<Val,P,N>::post(home(*this),x,y,c)));
    }
    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);        
      GECODE_REWRITE(*this,(Eq<Val,P,N>::post(home(*this),x,y,c)));
    }

    Val sl = 0;
    Val su = 0;

    bounds_p<Val,P>(med, x, c, sl, su);
    bounds_n<Val,N>(med, y, c, sl, su);

    if ((-sl == c) && (-su == c)) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(b.one_none(home));
      return home.ES_SUBSUMED(*this);
    }
    if ((-sl > c) || (-su < c)) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(b.zero_none(home));
      return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }


  /*
   * Domain consistent linear disequation
   *
   */

  template<class Val, class P, class N>
  forceinline
  Nq<Val,P,N>::Nq(Home home, ViewArray<P>& x, ViewArray<N>& y, Val c)
    : Lin<Val,P,N,PC_INT_VAL>(home,x,y,c) {}

  template<class Val, class P, class N>
  ExecStatus
  Nq<Val,P,N>::post(Home home, ViewArray<P>& x, ViewArray<N>& y, Val c) {
    ViewArray<NoView> nva;
    if (y.size() == 0) {
      (void) new (home) Nq<Val,P,NoView>(home,x,nva,c);
    } else if (x.size() == 0) {
      (void) new (home) Nq<Val,N,NoView>(home,y,nva,-c);
    } else {
      (void) new (home) Nq<Val,P,N>(home,x,y,c);
    }
    return ES_OK;
  }


  template<class Val, class P, class N>
  forceinline
  Nq<Val,P,N>::Nq(Space& home, bool share, Nq<Val,P,N>& p)
    : Lin<Val,P,N,PC_INT_VAL>(home,share,p) {}

  /**
   * \brief Rewriting of disequality to binary propagators
   *
   */
  template<class Val, class P, class N>
  forceinline Actor*
  nqtobin(Space&, bool, Propagator&, ViewArray<P>&, ViewArray<N>&, Val) {
    return NULL;
  }
  template<class Val>
  forceinline Actor*
  nqtobin(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<NoView>&, Val c) {
    assert(x.size() == 2);
    return new (home) NqBin<Val,IntView,IntView>
      (home,share,p,x[0],x[1],c);
  }
  template<class Val>
  forceinline Actor*
  nqtobin(Space& home, bool share, Propagator& p,
          ViewArray<NoView>&, ViewArray<IntView>& y, Val c) {
    assert(y.size() == 2);
    return new (home) NqBin<Val,IntView,IntView>
      (home,share,p,y[0],y[1],-c);
  }
  template<class Val>
  forceinline Actor*
  nqtobin(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<IntView>& y, Val c) {
    if (x.size() == 2)
      return new (home) NqBin<Val,IntView,IntView>
        (home,share,p,x[0],x[1],c);
    if (x.size() == 1)
      return new (home) NqBin<Val,IntView,MinusView>
        (home,share,p,x[0],MinusView(y[0]),c);
    return new (home) NqBin<Val,IntView,IntView>
      (home,share,p,y[0],y[1],-c);
  }

  /**
   * \brief Rewriting of disequality to ternary propagators
   *
   */
  template<class Val, class P, class N>
  forceinline Actor*
  nqtoter(Space&, bool, Propagator&,ViewArray<P>&, ViewArray<N>&, Val) {
    return NULL;
  }
  template<class Val>
  forceinline Actor*
  nqtoter(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<NoView>&, Val c) {
    assert(x.size() == 3);
    return new (home) NqTer<Val,IntView,IntView,IntView>
      (home,share,p,x[0],x[1],x[2],c);
  }
  template<class Val>
  forceinline Actor*
  nqtoter(Space& home, bool share, Propagator& p,
          ViewArray<NoView>&, ViewArray<IntView>& y, Val c) {
    assert(y.size() == 3);
    return new (home) NqTer<Val,IntView,IntView,IntView>
      (home,share,p,y[0],y[1],y[2],-c);
  }
  template<class Val>
  forceinline Actor*
  nqtoter(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<IntView>& y, Val c) {
    if (x.size() == 3)
      return new (home) NqTer<Val,IntView,IntView,IntView>
        (home,share,p,x[0],x[1],x[2],c);
    if (x.size() == 2)
      return new (home) NqTer<Val,IntView,IntView,MinusView>
        (home,share,p,x[0],x[1],MinusView(y[0]),c);
    if (x.size() == 1)
      return new (home) NqTer<Val,IntView,IntView,MinusView>
        (home,share,p,y[0],y[1],MinusView(x[0]),-c);
    return new (home) NqTer<Val,IntView,IntView,IntView>
      (home,share,p,y[0],y[1],y[2],-c);
  }

  template<class Val, class P, class N>
  Actor*
  Nq<Val,P,N>::copy(Space& home, bool share) {
    if (isunit(x,y)) {
      // Check whether rewriting is possible
      if (x.size() + y.size() == 2)
        return nqtobin(home,share,*this,x,y,c);
      if (x.size() + y.size() == 3)
        return nqtoter(home,share,*this,x,y,c);
    }
    return new (home) Nq<Val,P,N>(home,share,*this);
  }

  template<class Val, class P, class N>
  ExecStatus
  Nq<Val,P,N>::propagate(Space& home, const ModEventDelta&) {
    for (int i = x.size(); i--; )
      if (x[i].assigned()) {
        c -= x[i].val();  x.move_lst(i);
      }
    for (int i = y.size(); i--; )
      if (y[i].assigned()) {
        c += y[i].val();  y.move_lst(i);
      }
    if (x.size() + y.size() <= 1) {
      if (x.size() == 1) {
        GECODE_ME_CHECK(x[0].nq(home,c)); return home.ES_SUBSUMED(*this);
      }
      if (y.size() == 1) {
        GECODE_ME_CHECK(y[0].nq(home,-c)); return home.ES_SUBSUMED(*this);
      }
      return (c == static_cast<Val>(0)) ?
        ES_FAILED : home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }


  /*
   * Bound consistent linear inequation
   *
   */

  template<class Val, class P, class N>
  forceinline
  Lq<Val,P,N>::Lq(Home home, ViewArray<P>& x, ViewArray<N>& y, Val c)
    : Lin<Val,P,N,PC_INT_BND>(home,x,y,c) {}

  template<class Val, class P, class N>
  ExecStatus
  Lq<Val,P,N>::post(Home home, ViewArray<P>& x, ViewArray<N>& y, Val c) {
    ViewArray<NoView> nva;
    if (y.size() == 0) {
      (void) new (home) Lq<Val,P,NoView>(home,x,nva,c);
    } else if (x.size() == 0) {
      (void) new (home) Lq<Val,NoView,N>(home,nva,y,c);
    } else {
      (void) new (home) Lq<Val,P,N>(home,x,y,c);
    }
    return ES_OK;
  }


  template<class Val, class P, class N>
  forceinline
  Lq<Val,P,N>::Lq(Space& home, bool share, Lq<Val,P,N>& p)
    : Lin<Val,P,N,PC_INT_BND>(home,share,p) {}

  /**
   * \brief Rewriting of inequality to binary propagators
   *
   */
  template<class Val, class P, class N>
  forceinline Actor*
  lqtobin(Space&, bool, Propagator&,ViewArray<P>&, ViewArray<N>&, Val) {
    return NULL;
  }
  template<class Val>
  forceinline Actor*
  lqtobin(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<NoView>&, Val c) {
    assert(x.size() == 2);
    return new (home) LqBin<Val,IntView,IntView>
      (home,share,p,x[0],x[1],c);
  }
  template<class Val>
  forceinline Actor*
  lqtobin(Space& home, bool share, Propagator& p,
          ViewArray<NoView>&, ViewArray<IntView>& y, Val c) {
    assert(y.size() == 2);
    return new (home) LqBin<Val,MinusView,MinusView>
      (home,share,p,MinusView(y[0]),MinusView(y[1]),c);
  }
  template<class Val>
  forceinline Actor*
  lqtobin(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<IntView>& y, Val c) {
    if (x.size() == 2)
      return new (home) LqBin<Val,IntView,IntView>
        (home,share,p,x[0],x[1],c);
    if (x.size() == 1)
      return new (home) LqBin<Val,IntView,MinusView>
        (home,share,p,x[0],MinusView(y[0]),c);
    return new (home) LqBin<Val,MinusView,MinusView>
      (home,share,p,MinusView(y[0]),MinusView(y[1]),c);
  }

  /**
   * \brief Rewriting of inequality to ternary propagators
   *
   */
  template<class Val, class P, class N>
  forceinline Actor*
  lqtoter(Space&, bool, Propagator&, ViewArray<P>&, ViewArray<N>&, Val) {
    return NULL;
  }
  template<class Val>
  forceinline Actor*
  lqtoter(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<NoView>&, Val c) {
    assert(x.size() == 3);
    return new (home) LqTer<Val,IntView,IntView,IntView>
      (home,share,p,x[0],x[1],x[2],c);
  }
  template<class Val>
  forceinline Actor*
  lqtoter(Space& home, bool share, Propagator& p,
          ViewArray<NoView>&, ViewArray<IntView>& y, Val c) {
    assert(y.size() == 3);
    return new (home) LqTer<Val,MinusView,MinusView,MinusView>
      (home,share,p,MinusView(y[0]),MinusView(y[1]),MinusView(y[2]),c);
  }
  template<class Val>
  forceinline Actor*
  lqtoter(Space& home, bool share, Propagator& p,
          ViewArray<IntView>& x, ViewArray<IntView>& y, Val c) {
    if (x.size() == 3)
      return new (home) LqTer<Val,IntView,IntView,IntView>
        (home,share,p,x[0],x[1],x[2],c);
    if (x.size() == 2)
      return new (home) LqTer<Val,IntView,IntView,MinusView>
        (home,share,p,x[0],x[1],MinusView(y[0]),c);
    if (x.size() == 1)
      return new (home) LqTer<Val,IntView,MinusView,MinusView>
        (home,share,p,x[0],MinusView(y[0]),MinusView(y[1]),c);
    return new (home) LqTer<Val,MinusView,MinusView,MinusView>
      (home,share,p,MinusView(y[0]),MinusView(y[1]),MinusView(y[2]),c);
  }

  template<class Val, class P, class N>
  Actor*
  Lq<Val,P,N>::copy(Space& home, bool share) {
    if (isunit(x,y)) {
      // Check whether rewriting is possible
      if (x.size() + y.size() == 2)
        return lqtobin(home,share,*this,x,y,c);
      if (x.size() + y.size() == 3)
        return lqtoter(home,share,*this,x,y,c);
    }
    return new (home) Lq<Val,P,N>(home,share,*this);
  }

  template<class Val, class P, class N>
  ExecStatus
  Lq<Val,P,N>::propagate(Space& home, const ModEventDelta& med) {
    // Eliminate singletons
    Val sl = 0;

    if (IntView::me(med) == ME_INT_VAL) {
      for (int i = x.size(); i--; ) {
        Val m = x[i].min();
        if (x[i].assigned()) {
          c  -= m;  x.move_lst(i);
        } else {
          sl -= m;
        }
      }
      for (int i = y.size(); i--; ) {
        Val m = y[i].max();
        if (y[i].assigned()) {
          c  += m;  y.move_lst(i);
        } else {
          sl += m;
        }
      }
      if ((x.size() + y.size()) <= 1) {
        if (x.size() == 1) {
          GECODE_ME_CHECK(x[0].lq(home,c));
          return home.ES_SUBSUMED(*this);
        }
        if (y.size() == 1) {
          GECODE_ME_CHECK(y[0].gq(home,-c));
          return home.ES_SUBSUMED(*this);
        }
        return (c >= static_cast<Val>(0)) ?
          home.ES_SUBSUMED(*this) : ES_FAILED;
      }
    } else {
      for (int i = x.size(); i--; )
        sl -= x[i].min();
      for (int i = y.size(); i--; )
        sl += y[i].max();
    }

    sl += c;

    ExecStatus es = ES_FIX;
    bool assigned = true;
    for (int i = x.size(); i--; ) {
      assert(!x[i].assigned());
      Val slx = sl + x[i].min();
      ModEvent me = x[i].lq(home,slx);
      if (me == ME_INT_FAILED)
        return ES_FAILED;
      if (me != ME_INT_VAL)
        assigned = false;
      if (me_modified(me) && (slx != x[i].max()))
        es = ES_NOFIX;
    }

    for (int i = y.size(); i--; ) {
      assert(!y[i].assigned());
      Val sly = y[i].max() - sl;
      ModEvent me = y[i].gq(home,sly);
      if (me == ME_INT_FAILED)
        return ES_FAILED;
      if (me != ME_INT_VAL)
        assigned = false;
      if (me_modified(me) && (sly != y[i].min()))
        es = ES_NOFIX;
    }
    return assigned ? home.ES_SUBSUMED(*this) : es;
  }

  /*
   * Reified bound consistent linear inequation
   *
   */

  template<class Val, class P, class N, ReifyMode rm>
  forceinline
  ReLq<Val,P,N,rm>::ReLq(Home home, 
                      ViewArray<P>& x, ViewArray<N>& y, Val c, BoolView b)
    : ReLin<Val,P,N,PC_INT_BND,BoolView>(home,x,y,c,b) {}

  template<class Val, class P, class N, ReifyMode rm>
  ExecStatus
  ReLq<Val,P,N,rm>::post(Home home, 
                         ViewArray<P>& x, ViewArray<N>& y, Val c, BoolView b) {
    ViewArray<NoView> nva;
    if (y.size() == 0) {
      (void) new (home) ReLq<Val,P,NoView,rm>(home,x,nva,c,b);
    } else if (x.size() == 0) {
      (void) new (home) ReLq<Val,NoView,N,rm>(home,nva,y,c,b);
    } else {
      (void) new (home) ReLq<Val,P,N,rm>(home,x,y,c,b);
    }
    return ES_OK;
  }


  template<class Val, class P, class N, ReifyMode rm>
  forceinline
  ReLq<Val,P,N,rm>::ReLq(Space& home, bool share, ReLq<Val,P,N,rm>& p)
    : ReLin<Val,P,N,PC_INT_BND,BoolView>(home,share,p) {}

  template<class Val, class P, class N, ReifyMode rm>
  Actor*
  ReLq<Val,P,N,rm>::copy(Space& home, bool share) {
    return new (home) ReLq<Val,P,N,rm>(home,share,*this);
  }

  template<class Val, class P, class N, ReifyMode rm>
  ExecStatus
  ReLq<Val,P,N,rm>::propagate(Space& home, const ModEventDelta& med) {
    if (b.zero()) {
      if (rm == RM_IMP)
        return home.ES_SUBSUMED(*this);              
      GECODE_REWRITE(*this,(Lq<Val,N,P>::post(home(*this),y,x,-c-1)));
    }
    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);        
      GECODE_REWRITE(*this,(Lq<Val,P,N>::post(home(*this),x,y,c)));
    }

    // Eliminate singletons
    Val sl = 0;
    Val su = 0;

    bounds_p<Val,P>(med,x,c,sl,su);
    bounds_n<Val,N>(med,y,c,sl,su);

    if (-sl > c) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(b.zero_none(home));
      return home.ES_SUBSUMED(*this);
    }
    if (-su <= c) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(b.one_none(home));
      return home.ES_SUBSUMED(*this);
    }

    return ES_FIX;
  }

}}}

// STATISTICS: int-prop

