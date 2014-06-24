/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
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
   * Boolean clause propagator (disjunctive, true)
   *
   */

  template<class VX, class VY>
  forceinline
  ClauseTrue<VX,VY>::ClauseTrue(Home home,
                                ViewArray<VX>& x0, ViewArray<VY>& y0)
    : MixBinaryPropagator<VX,PC_BOOL_VAL,VY,PC_BOOL_VAL>
  (home,x0[x0.size()-1],y0[y0.size()-1]), x(x0), y(y0) {
    assert((x.size() > 0) && (y.size() > 0));
    x.size(x.size()-1); y.size(y.size()-1);
  }

  template<class VX, class VY>
  PropCost
  ClauseTrue<VX,VY>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::binary(PropCost::LO);
  }

  template<class VX, class VY>
  forceinline
  ClauseTrue<VX,VY>::ClauseTrue(Space& home, bool share, ClauseTrue<VX,VY>& p)
    : MixBinaryPropagator<VX,PC_BOOL_VAL,VY,PC_BOOL_VAL>(home,share,p) {
    x.update(home,share,p.x);
    y.update(home,share,p.y);
  }

  template<class VX, class VY>
  Actor*
  ClauseTrue<VX,VY>::copy(Space& home, bool share) {
    {
      int n = x.size();
      if (n > 0) {
        // Eliminate all zeros and find a one
        for (int i=n; i--; )
          if (x[i].one()) {
            // Only keep the one
            x[0]=x[i]; n=1; break;
          } else if (x[i].zero()) {
            // Eliminate the zero
            x[i]=x[--n];
          }
        x.size(n);
      }
    }
    {
      int n = y.size();
      if (n > 0) {
        // Eliminate all zeros and find a one
        for (int i=n; i--; )
          if (y[i].one()) {
            // Only keep the one
            y[0]=y[i]; n=1; break;
          } else if (y[i].zero()) {
            // Eliminate the zero
            y[i]=y[--n];
          }
        y.size(n);
      }
    }
    if ((x.size() == 0) && (y.size() == 0))
      return new (home) BinOrTrue<VX,VY>(home,share,*this,x0,x1);
    else
      return new (home) ClauseTrue<VX,VY>(home,share,*this);
  }

  template<class VX, class VY>
  inline ExecStatus
  ClauseTrue<VX,VY>::post(Home home, ViewArray<VX>& x, ViewArray<VY>& y) {
    for (int i=x.size(); i--; )
      if (x[i].one())
        return ES_OK;
      else if (x[i].zero())
        x.move_lst(i);
    if (x.size() == 0)
      return NaryOrTrue<VY>::post(home,y);
    for (int i=y.size(); i--; )
      if (y[i].one())
        return ES_OK;
      else if (y[i].zero())
        y.move_lst(i);
    if (y.size() == 0)
      return NaryOrTrue<VX>::post(home,x);
    if ((x.size() == 1) && (y.size() == 1)) {
      return BinOrTrue<VX,VY>::post(home,x[0],y[0]);
    } else if (!x.shared(home,y)) {
      (void) new (home) ClauseTrue(home,x,y);
    }
    return ES_OK;
  }

  template<class VX, class VY>
  forceinline size_t
  ClauseTrue<VX,VY>::dispose(Space& home) {
    (void) MixBinaryPropagator<VX,PC_BOOL_VAL,VY,PC_BOOL_VAL>::dispose(home);
    return sizeof(*this);
  }

  template<class VX, class VY>
  forceinline ExecStatus
  resubscribe(Space& home, Propagator& p,
              VX& x0, ViewArray<VX>& x,
              VY& x1, ViewArray<VY>& y) {
    if (x0.zero()) {
      int n = x.size();
      for (int i=n; i--; )
        if (x[i].one()) {
          x.size(n);
          return home.ES_SUBSUMED(p);
        } else if (x[i].zero()) {
          x[i] = x[--n];
        } else {
          // Rewrite if there is just one view left
          if ((i == 0) && (y.size() == 0)) {
            VX z = x[0]; x.size(0);
            GECODE_REWRITE(p,(BinOrTrue<VX,VY>::post(home(p),z,x1)));
          }
          // Move to x0 and subscribe
          x0=x[i]; x[i]=x[--n];
          x.size(n);
          x0.subscribe(home,p,PC_BOOL_VAL,false);
          return ES_FIX;
        }
      // All x-views have been assigned!
      ViewArray<VY> z(home,y.size()+1);
      for (int i=y.size(); i--; )
        z[i]=y[i];
      z[y.size()] = x1;
      GECODE_REWRITE(p,(NaryOrTrue<VY>::post(home(p),z)));
    }
    return ES_FIX;
  }

  template<class VX, class VY>
  ExecStatus
  ClauseTrue<VX,VY>::propagate(Space& home, const ModEventDelta&) {
    if (x0.one() || x1.one())
      return home.ES_SUBSUMED(*this);
    GECODE_ES_CHECK(resubscribe(home,*this,x0,x,x1,y));
    GECODE_ES_CHECK(resubscribe(home,*this,x1,y,x0,x));
    return ES_FIX;
  }


  /*
   * Boolean clause propagator (disjunctive)
   *
   */

  /*
   * Index advisors
   *
   */
  template<class VX, class VY>
  forceinline
  Clause<VX,VY>::Tagged::Tagged(Space& home, Propagator& p,
                                Council<Tagged>& c, bool x0)
    : Advisor(home,p,c), x(x0) {}

  template<class VX, class VY>
  forceinline
  Clause<VX,VY>::Tagged::Tagged(Space& home, bool share, Tagged& a)
    : Advisor(home,share,a), x(a.x) {}

  template<class VX, class VY>
  forceinline
  Clause<VX,VY>::Clause(Home home, ViewArray<VX>& x0, ViewArray<VY>& y0,
                        VX z0)
    : Propagator(home), x(x0), y(y0), z(z0), n_zero(0), c(home) {
    x.subscribe(home,*new (home) Tagged(home,*this,c,true));
    y.subscribe(home,*new (home) Tagged(home,*this,c,false));
    z.subscribe(home,*this,PC_BOOL_VAL);
  }

  template<class VX, class VY>
  forceinline
  Clause<VX,VY>::Clause(Space& home, bool share, Clause<VX,VY>& p)
    : Propagator(home,share,p), n_zero(p.n_zero) {
    x.update(home,share,p.x);
    y.update(home,share,p.y);
    z.update(home,share,p.z);
    c.update(home,share,p.c);
  }

  template<class VX>
  forceinline void
  eliminate_zero(ViewArray<VX>& x, int& n_zero) {
    if (n_zero > 0) {
      int n=x.size();
      // Eliminate all zeros
      for (int i=n; i--; )
        if (x[i].zero()) {
          x[i]=x[--n]; n_zero--;
        }
      x.size(n);
    }
  }

  template<class VX, class VY>
  Actor*
  Clause<VX,VY>::copy(Space& home, bool share) {
    eliminate_zero(x,n_zero);
    eliminate_zero(y,n_zero);
    return new (home) Clause<VX,VY>(home,share,*this);
  }

  template<class VX, class VY>
  inline ExecStatus
  Clause<VX,VY>::post(Home home, ViewArray<VX>& x, ViewArray<VY>& y, VX z) {
    assert(!x.shared(home) && !y.shared(home));
    if (z.one())
      return ClauseTrue<VX,VY>::post(home,x,y);
    if (z.zero()) {
      for (int i=x.size(); i--; )
        GECODE_ME_CHECK(x[i].zero(home));
      for (int i=y.size(); i--; )
        GECODE_ME_CHECK(y[i].zero(home));
      return ES_OK;
    }
    for (int i=x.size(); i--; )
      if (x[i].one()) {
        GECODE_ME_CHECK(z.one_none(home));
        return ES_OK;
      } else if (x[i].zero()) {
        x.move_lst(i);
      }
    if (x.size() == 0)
      return NaryOr<VY,VX>::post(home,y,z);
    for (int i=y.size(); i--; )
      if (y[i].one()) {
        GECODE_ME_CHECK(z.one_none(home));
        return ES_OK;
      } else if (y[i].zero()) {
        y.move_lst(i);
      }
    if (y.size() == 0)
      return NaryOr<VX,VX>::post(home,x,z);
    if ((x.size() == 1) && (y.size() == 1)) {
      return Or<VX,VY,VX>::post(home,x[0],y[0],z);
    } else if (x.shared(home,y)) {
      GECODE_ME_CHECK(z.one_none(home));
    } else {
      (void) new (home) Clause<VX,VY>(home,x,y,z);
    }
    return ES_OK;
  }

  template<class VX, class VY>
  PropCost
  Clause<VX,VY>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::unary(PropCost::LO);
  }

  template<class VX, class VY>
  forceinline void
  Clause<VX,VY>::cancel(Space& home) {
    for (Advisors<Tagged> as(c); as(); ++as) {
      if (as.advisor().x)
        x.cancel(home,as.advisor());
      else
        y.cancel(home,as.advisor());
      as.advisor().dispose(home,c);
    }
    c.dispose(home);
    z.cancel(home,*this,PC_BOOL_VAL);
  }

  template<class VX, class VY>
  forceinline size_t
  Clause<VX,VY>::dispose(Space& home) {
    cancel(home);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }


  template<class VX, class VY>
  ExecStatus
  Clause<VX,VY>::advise(Space&, Advisor& _a, const Delta& d) {
    Tagged& a = static_cast<Tagged&>(_a);
    // Decides whether the propagator must be run
    if ((a.x && VX::zero(d)) || (!a.x && VY::zero(d)))
      if (++n_zero < x.size() + y.size())
        return ES_FIX;
    return ES_NOFIX;
  }

  template<class VX, class VY>
  ExecStatus
  Clause<VX,VY>::propagate(Space& home, const ModEventDelta&) {
    if (z.one())
      GECODE_REWRITE(*this,(ClauseTrue<VX,VY>::post(home(*this),x,y)));
    if (z.zero()) {
      for (int i = x.size(); i--; )
        GECODE_ME_CHECK(x[i].zero(home));
      for (int i = y.size(); i--; )
        GECODE_ME_CHECK(y[i].zero(home));
      c.dispose(home);
    } else if (n_zero == x.size() + y.size()) {
      GECODE_ME_CHECK(z.zero_none(home));
      c.dispose(home);
    } else {
      // There is exactly one view which is one
      GECODE_ME_CHECK(z.one_none(home));
    }
    return home.ES_SUBSUMED(*this);
  }

}}}

// STATISTICS: int-prop

