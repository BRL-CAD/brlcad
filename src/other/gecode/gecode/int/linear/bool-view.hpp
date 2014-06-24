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

namespace Gecode { namespace Int { namespace Linear {

  /*
   * Base-class
   *
   */
  template<class XV, class YV>
  LinBoolView<XV,YV>::LinBoolView(Home home,
                                  ViewArray<XV>& x0, YV y0, int c0)
    :  Propagator(home), x(x0), y(y0), c(c0) {
    x.subscribe(home,*this,PC_INT_VAL);
    y.subscribe(home,*this,PC_INT_BND);
  }

  template<class XV, class YV>
  forceinline size_t
  LinBoolView<XV,YV>::dispose(Space& home) {
    x.cancel(home,*this,PC_INT_VAL);
    y.cancel(home,*this,PC_INT_BND);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class XV, class YV>
  forceinline
  LinBoolView<XV,YV>::LinBoolView(Space& home, bool share, LinBoolView& p)
    : Propagator(home,share,p), c(p.c) {
    x.update(home,share,p.x);
    y.update(home,share,p.y);
  }

  template<class XV, class YV>
  PropCost
  LinBoolView<XV,YV>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO, x.size());
  }


  /*
   * Equality propagator
   *
   */
  template<class XV, class YV>
  forceinline
  EqBoolView<XV,YV>::EqBoolView(Home home, ViewArray<XV>& x, YV y, int c)
    : LinBoolView<XV,YV>(home,x,y,c) {}

  template<class XV, class YV>
  ExecStatus
  EqBoolView<XV,YV>::post(Home home, ViewArray<XV>& x, YV y, int c) {
    if (y.assigned())
      return EqBoolInt<XV>::post(home,x,y.val()+c);
    int n = x.size();
    for (int i = n; i--; )
      if (x[i].one()) {
        x[i]=x[--n]; c--;
      } else if (x[i].zero()) {
        x[i]=x[--n];
      }
    x.size(n);
    GECODE_ME_CHECK(y.lq(home,n-c));
    GECODE_ME_CHECK(y.gq(home,-c));
    if (n == 0)
      return ES_OK;
    if (y.min()+c == n) {
      assert(y.assigned());
      for (int i = n; i--; )
        GECODE_ME_CHECK(x[i].one_none(home));
      return ES_OK;
    }
    if (y.max()+c == 0) {
      assert(y.assigned());
      for (int i = n; i--; )
        GECODE_ME_CHECK(x[i].zero_none(home));
      return ES_OK;
    }
    (void) new (home) EqBoolView<XV,YV>(home,x,y,c);
    return ES_OK;
  }

  template<class XV, class YV>
  forceinline
  EqBoolView<XV,YV>::EqBoolView(Space& home, bool share, EqBoolView<XV,YV>& p)
    : LinBoolView<XV,YV>(home,share,p) {}

  template<class XV, class YV>
  Actor*
  EqBoolView<XV,YV>::copy(Space& home, bool share) {
    return new (home) EqBoolView<XV,YV>(home,share,*this);
  }

  template<class XV, class YV>
  ExecStatus
  EqBoolView<XV,YV>::propagate(Space& home, const ModEventDelta&) {
    int n = x.size();
    for (int i = n; i--; )
      if (x[i].one()) {
        x[i]=x[--n]; c--;
      } else if (x[i].zero()) {
        x[i]=x[--n];
      }
    x.size(n);
    GECODE_ME_CHECK(y.lq(home,n-c));
    GECODE_ME_CHECK(y.gq(home,-c));
    if (n == 0)
      return home.ES_SUBSUMED(*this);
    if (y.min()+c == n) {
      assert(y.assigned());
      for (int i = n; i--; )
        GECODE_ME_CHECK(x[i].one_none(home));
      return home.ES_SUBSUMED(*this);
    }
    if (y.max()+c == 0) {
      assert(y.assigned());
      for (int i = n; i--; )
        GECODE_ME_CHECK(x[i].zero_none(home));
      return home.ES_SUBSUMED(*this);
    }
    if (y.assigned())
      GECODE_REWRITE(*this,EqBoolInt<XV>::post(home(*this),x,y.val()+c));
    return ES_FIX;
  }


  /*
   * Disequality propagator
   *
   */
  template<class XV, class YV>
  forceinline
  NqBoolView<XV,YV>::NqBoolView(Home home, ViewArray<XV>& x, YV y, int c)
    : LinBoolView<XV,YV>(home,x,y,c) {}

  template<class XV, class YV>
  ExecStatus
  NqBoolView<XV,YV>::post(Home home, ViewArray<XV>& x, YV y, int c) {
    if (y.assigned())
      return NqBoolInt<XV>::post(home,x,y.val()+c);
    int n = x.size();
    for (int i = n; i--; )
      if (x[i].one()) {
        x[i]=x[--n]; c--;
      } else if (x[i].zero()) {
        x[i]=x[--n];
      }
    x.size(n);
    if ((n-c < y.min() ) || (-c > y.max()))
      return ES_OK;
    if (n == 0) {
      GECODE_ME_CHECK(y.nq(home,-c));
      return ES_OK;
    }
    if ((n == 1) && y.assigned()) {
      if (y.val()+c == 1) {
        GECODE_ME_CHECK(x[0].zero_none(home));
      } else {
        assert(y.val()+c == 0);
        GECODE_ME_CHECK(x[0].one_none(home));
      }
      return ES_OK;
    }
    (void) new (home) NqBoolView<XV,YV>(home,x,y,c);
    return ES_OK;
  }


  template<class XV, class YV>
  forceinline
  NqBoolView<XV,YV>::NqBoolView(Space& home, bool share, NqBoolView<XV,YV>& p)
    : LinBoolView<XV,YV>(home,share,p) {}

  template<class XV, class YV>
  Actor*
  NqBoolView<XV,YV>::copy(Space& home, bool share) {
    return new (home) NqBoolView<XV,YV>(home,share,*this);
  }

  template<class XV, class YV>
  ExecStatus
  NqBoolView<XV,YV>::propagate(Space& home, const ModEventDelta&) {
    int n = x.size();
    for (int i = n; i--; )
      if (x[i].one()) {
        x[i]=x[--n]; c--;
      } else if (x[i].zero()) {
        x[i]=x[--n];
      }
    x.size(n);
    if ((n-c < y.min() ) || (-c > y.max()))
      return home.ES_SUBSUMED(*this);
    if (n == 0) {
      GECODE_ME_CHECK(y.nq(home,-c));
      return home.ES_SUBSUMED(*this);
    }
    if ((n == 1) && y.assigned()) {
      if (y.val()+c == 1) {
        GECODE_ME_CHECK(x[0].zero_none(home));
      } else {
        assert(y.val()+c == 0);
        GECODE_ME_CHECK(x[0].one_none(home));
      }
      return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }


  /*
   * Greater or equal propagator
   *
   */
  template<class XV, class YV>
  forceinline
  GqBoolView<XV,YV>::GqBoolView(Home home, ViewArray<XV>& x, YV y, int c)
    : LinBoolView<XV,YV>(home,x,y,c) {}

  template<class XV, class YV>
  ExecStatus
  GqBoolView<XV,YV>::post(Home home, ViewArray<XV>& x, YV y, int c) {
    if (y.assigned())
      return GqBoolInt<XV>::post(home,x,y.val()+c);
    // Eliminate assigned views
    int n = x.size();
    for (int i = n; i--; )
      if (x[i].one()) {
        x[i]=x[--n]; c--;
      } else if (x[i].zero()) {
        x[i]=x[--n];
      }
    x.size(n);
    GECODE_ME_CHECK(y.lq(home,n-c));
    if (-c >= y.max())
      return ES_OK;
    if (y.min()+c == n) {
      for (int i = n; i--; )
        GECODE_ME_CHECK(x[i].one_none(home));
      return ES_OK;
    }
    (void) new (home) GqBoolView<XV,YV>(home,x,y,c);
    return ES_OK;
  }


  template<class XV, class YV>
  forceinline
  GqBoolView<XV,YV>::GqBoolView(Space& home, bool share, GqBoolView<XV,YV>& p)
    : LinBoolView<XV,YV>(home,share,p) {}

  template<class XV, class YV>
  Actor*
  GqBoolView<XV,YV>::copy(Space& home, bool share) {
    return new (home) GqBoolView<XV,YV>(home,share,*this);
  }

  template<class XV, class YV>
  ExecStatus
  GqBoolView<XV,YV>::propagate(Space& home, const ModEventDelta&) {
    int n = x.size();
    for (int i = n; i--; )
      if (x[i].one()) {
        x[i]=x[--n]; c--;
      } else if (x[i].zero()) {
        x[i]=x[--n];
      }
    x.size(n);
    GECODE_ME_CHECK(y.lq(home,n-c));
    if (-c >= y.max())
      return home.ES_SUBSUMED(*this);
    if (y.min()+c == n) {
      for (int i = n; i--; )
        GECODE_ME_CHECK(x[i].one_none(home));
      return home.ES_SUBSUMED(*this);
    }
    if (y.assigned())
      GECODE_REWRITE(*this,GqBoolInt<XV>::post(home(*this),x,y.val()+c));
    return ES_FIX;
  }

}}}

// STATISTICS: int-prop

