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

namespace Gecode { namespace Int { namespace Linear {

  /*
   * Array of scale Boolean views
   *
   */
  forceinline
  ScaleBoolArray::ScaleBoolArray(void) {}
  forceinline
  ScaleBoolArray::ScaleBoolArray(Space& home, int n) {
    if (n > 0) {
      _fst = home.alloc<ScaleBool>(n);
      _lst = _fst+n;
    } else {
      _fst = _lst = NULL;
    }
  }
  forceinline void
  ScaleBoolArray::subscribe(Space& home, Propagator& p) {
    for (ScaleBool* f = _fst; f < _lst; f++)
      f->x.subscribe(home,p,PC_BOOL_VAL);
  }
  forceinline void
  ScaleBoolArray::cancel(Space& home, Propagator& p) {
    for (ScaleBool* f = _fst; f < _lst; f++)
      f->x.cancel(home,p,PC_BOOL_VAL);
  }
  forceinline void
  ScaleBoolArray::update(Space& home, bool share, ScaleBoolArray& sba) {
    int n = static_cast<int>(sba._lst - sba._fst);
    if (n > 0) {
      _fst = home.alloc<ScaleBool>(n);
      _lst = _fst+n;
      for (int i=n; i--; ) {
        _fst[i].a = sba._fst[i].a;
        _fst[i].x.update(home,share,sba._fst[i].x);
      }
    } else {
      _fst = _lst = NULL;
    }
  }
  forceinline ScaleBool*
  ScaleBoolArray::fst(void) const {
    return _fst;
  }
  forceinline ScaleBool*
  ScaleBoolArray::lst(void) const {
    return _lst;
  }
  forceinline void
  ScaleBoolArray::fst(ScaleBool* f) {
    _fst = f;
  }
  forceinline void
  ScaleBoolArray::lst(ScaleBool* l) {
    _lst = l;
  }
  forceinline bool
  ScaleBoolArray::empty(void) const {
    return _fst == _lst;
  }
  forceinline int
  ScaleBoolArray::size(void) const {
    return static_cast<int>(_lst - _fst);
  }
  forceinline bool
  ScaleBoolArray::ScaleDec::operator ()(const ScaleBool& x,
                                       const ScaleBool& y) {
    return x.a > y.a;
  }

  inline void
  ScaleBoolArray::sort(void) {
    ScaleDec scale_dec;
    Support::quicksort<ScaleBool,ScaleDec>(fst(), size(), scale_dec);
  }


  /*
   * Empty array of scale Boolean views
   *
   */
  forceinline
  EmptyScaleBoolArray::EmptyScaleBoolArray(void) {}
  forceinline
  EmptyScaleBoolArray::EmptyScaleBoolArray(Space&, int) {}
  forceinline void
  EmptyScaleBoolArray::subscribe(Space&, Propagator&) {}
  forceinline void
  EmptyScaleBoolArray::cancel(Space&, Propagator&) {}
  forceinline void
  EmptyScaleBoolArray::update(Space&, bool, EmptyScaleBoolArray&) {}
  forceinline ScaleBool*
  EmptyScaleBoolArray::fst(void) const { return NULL; }
  forceinline ScaleBool*
  EmptyScaleBoolArray::lst(void) const { return NULL; }
  forceinline void
  EmptyScaleBoolArray::fst(ScaleBool*) {}
  forceinline void
  EmptyScaleBoolArray::lst(ScaleBool*) {}
  forceinline bool
  EmptyScaleBoolArray::empty(void) const { return true; }
  forceinline int
  EmptyScaleBoolArray::size(void) const { return 0; }
  forceinline void
  EmptyScaleBoolArray::sort(void) {}


  /*
   * Base-class for Boolean constraints with coefficients
   *
   */

  template<class SBAP, class SBAN, class VX, PropCond pcx>
  forceinline
  LinBoolScale<SBAP,SBAN,VX,pcx>::LinBoolScale(Home home,
                                               SBAP& p0, SBAN& n0,
                                               VX x0, int c0)
    : Propagator(home), p(p0), n(n0), x(x0), c(c0) {
    x.subscribe(home,*this,pcx);
    p.subscribe(home,*this);
    n.subscribe(home,*this);
  }

  template<class SBAP, class SBAN, class VX, PropCond pcx>
  PropCost
  LinBoolScale<SBAP,SBAN,VX,pcx>::cost(const Space&,
                                       const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO, p.size() + n.size());
  }

  template<class SBAP, class SBAN, class VX, PropCond pcx>
  forceinline size_t
  LinBoolScale<SBAP,SBAN,VX,pcx>::dispose(Space& home) {
    x.cancel(home,*this,pcx);
    p.cancel(home,*this);
    n.cancel(home,*this);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class SBAP, class SBAN, class VX, PropCond pcx>
  forceinline
  LinBoolScale<SBAP,SBAN,VX,pcx>::LinBoolScale(Space& home, bool share,
                                               Propagator& pr,
                                               SBAP& p0, SBAN& n0,
                                               VX x0, int c0)
    : Propagator(home,share,pr), c(c0) {
    x.update(home,share,x0);
    p.update(home,share,p0);
    n.update(home,share,n0);
  }

  /*
   * Boolean equality with coefficients
   *
   */

  template<class SBAP, class SBAN, class VX>
  forceinline
  EqBoolScale<SBAP,SBAN,VX>::EqBoolScale(Home home,
                                         SBAP& p, SBAN& n,
                                         VX x, int c)
    : LinBoolScale<SBAP,SBAN,VX,PC_INT_BND>(home,p,n,x,c) {}

  template<class SBAP, class SBAN, class VX>
  forceinline
  EqBoolScale<SBAP,SBAN,VX>::EqBoolScale(Space& home, bool share,
                                         Propagator& pr,
                                         SBAP& p, SBAN& n,
                                         VX x, int c)
    : LinBoolScale<SBAP,SBAN,VX,PC_INT_BND>(home,share,pr,p,n,x,c) {}

  template<class SBAP, class SBAN, class VX>
  Actor*
  EqBoolScale<SBAP,SBAN,VX>::copy(Space& home, bool share) {
    if (p.empty()) {
      EmptyScaleBoolArray ep;
      if (x.assigned()) {
        ZeroIntView z;
        return new (home) EqBoolScale<EmptyScaleBoolArray,SBAN,ZeroIntView>
          (home,share,*this,ep,n,z,c+x.val());
      } else {
        return new (home) EqBoolScale<EmptyScaleBoolArray,SBAN,VX>
          (home,share,*this,ep,n,x,c);
      }
    } else if (n.empty()) {
      EmptyScaleBoolArray en;
      if (x.assigned()) {
        ZeroIntView z;
        return new (home) EqBoolScale<SBAP,EmptyScaleBoolArray,ZeroIntView>
          (home,share,*this,p,en,z,c+x.val());
      } else {
        return new (home) EqBoolScale<SBAP,EmptyScaleBoolArray,VX>
          (home,share,*this,p,en,x,c);
      }
    } else {
      return new (home) EqBoolScale<SBAP,SBAN,VX>(home,share,*this,p,n,x,c);
    }
  }

  template<class SBAP, class SBAN, class VX>
  ExecStatus
  EqBoolScale<SBAP,SBAN,VX>::propagate(Space& home, const ModEventDelta& med) {
    int sl_p = 0; // Lower bound, computed positive
    int su_n = 0; // Upper bound, computed negative
    if (BoolView::me(med) == ME_BOOL_VAL) {
      // Eliminate assigned positive views while keeping order
      {
        // Skip not assigned views
        ScaleBool* f = p.fst();
        ScaleBool* l = p.lst();
        while ((f < l) && f->x.none()) {
          su_n += f->a; f++;
        }
        // Copy unassigned views to t
        ScaleBool* t = f;
        while (f < l) {
          if (f->x.one()) {
            c -= f->a;
          } else if (f->x.none()) {
            su_n += f->a; *t = *f; t++;
          }
          f++;
        }
        p.lst(t);
      }
      // Eliminate assigned negative views while keeping order
      {
        // Skip not assigned views
        ScaleBool* f = n.fst();
        ScaleBool* l = n.lst();
        while ((f < l) && f->x.none()) {
          sl_p += f->a; f++;
        }
        // Copy unassigned views to t
        ScaleBool* t = f;
        while (f < l) {
          if (f->x.one()) {
            c += f->a;
          } else if (f->x.none()) {
            sl_p += f->a; *t = *f; t++;
          }
          f++;
        }
        n.lst(t);
      }
    } else {
      for (ScaleBool* f=p.fst(); f<p.lst(); f++)
        su_n += f->a;
      for (ScaleBool* f=n.fst(); f<n.lst(); f++)
        sl_p += f->a;
    }

    if (p.empty() && n.empty()) {
      GECODE_ME_CHECK(x.eq(home,-c));
      return home.ES_SUBSUMED(*this);
    }

    sl_p += x.max() + c;
    su_n -= x.min() + c;

    const int MOD_SL = 1 << 0;
    const int MOD_SU = 1 << 1;

    int mod = MOD_SL | MOD_SU;

    do {
      if ((mod & MOD_SL) != 0) {
        mod -= MOD_SL;
        // Propagate lower bound for positive Boolean views
        {
          ScaleBool* f=p.fst();
          for (ScaleBool* l=p.lst(); (f < l) && (f->a > sl_p); f++) {
            GECODE_ME_CHECK(f->x.zero_none(home));
            su_n -= f->a;
          }
          if (f > p.fst()) {
            p.fst(f); mod |= MOD_SU;
          }
        }
        // Propagate lower bound for negative Boolean views
        {
          ScaleBool* f=n.fst();
          for (ScaleBool* l=n.lst(); (f < l) && (f->a > sl_p); f++) {
            GECODE_ME_CHECK(f->x.one_none(home)); c += f->a;
            su_n -= f->a;
          }
          if (f > n.fst()) {
            n.fst(f); mod |= MOD_SU;
          }
        }
        // Propagate lower bound for integer view
        {
          const int x_min = x.min();
          ModEvent me = x.gq(home,x.max() - sl_p);
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            su_n -= x.min() - x_min;
            mod |= MOD_SU;
          }
        }
      }
      if ((mod & MOD_SU) != 0) {
        mod -= MOD_SU;
        // Propagate upper bound for positive Boolean views
        {
          ScaleBool* f=p.fst();
          for (ScaleBool* l=p.lst(); (f < l) && (f->a > su_n); f++) {
            GECODE_ME_CHECK(f->x.one_none(home)); c -= f->a;
            sl_p -= f->a;
          }
          if (f > p.fst()) {
            p.fst(f); mod |= MOD_SL;;
          }
        }
        // Propagate upper bound for negative Boolean views
        {
          ScaleBool* f=n.fst();
          for (ScaleBool* l=n.lst(); (f < l) && (f->a > su_n); f++) {
            GECODE_ME_CHECK(f->x.zero_none(home));
            sl_p -= f->a;
          }
          if (f > n.fst()) {
            n.fst(f); mod |= MOD_SL;;
          }
        }
        // Propagate upper bound for integer view
        {
          const int x_max = x.max();
          ModEvent me = x.lq(home,x.min() + su_n);
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            sl_p += x.max() - x_max;
            mod |= MOD_SL;;
          }
        }
      }
    } while (mod != 0);

    return (sl_p == -su_n) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }



  template<class SBAP, class SBAN, class VX>
  ExecStatus
  EqBoolScale<SBAP,SBAN,VX>::post(Home home,
                                  SBAP& p, SBAN& n, VX x, int c) {
    p.sort(); n.sort();
    if (p.empty()) {
      EmptyScaleBoolArray ep;
      (void) new (home) EqBoolScale<EmptyScaleBoolArray,SBAN,VX>
        (home,ep,n,x,c);
    } else if (n.empty()) {
      EmptyScaleBoolArray en;
      (void) new (home) EqBoolScale<SBAP,EmptyScaleBoolArray,VX>
        (home,p,en,x,c);
    } else {
      (void) new (home) EqBoolScale<SBAP,SBAN,VX>
        (home,p,n,x,c);
    }
    return ES_OK;
  }


  /*
   * Boolean inequality with coefficients
   *
   */

  template<class SBAP, class SBAN, class VX>
  forceinline
  LqBoolScale<SBAP,SBAN,VX>::LqBoolScale(Home home,
                                         SBAP& p, SBAN& n,
                                         VX x, int c)
    : LinBoolScale<SBAP,SBAN,VX,PC_INT_BND>(home,p,n,x,c) {}

  template<class SBAP, class SBAN, class VX>
  forceinline
  LqBoolScale<SBAP,SBAN,VX>::LqBoolScale(Space& home, bool share,
                                         Propagator& pr,
                                         SBAP& p, SBAN& n,
                                         VX x, int c)
    : LinBoolScale<SBAP,SBAN,VX,PC_INT_BND>(home,share,pr,p,n,x,c) {}

  template<class SBAP, class SBAN, class VX>
  Actor*
  LqBoolScale<SBAP,SBAN,VX>::copy(Space& home, bool share) {
    if (p.empty()) {
      EmptyScaleBoolArray ep;
      if (x.assigned()) {
        ZeroIntView z;
        return new (home) LqBoolScale<EmptyScaleBoolArray,SBAN,ZeroIntView>
          (home,share,*this,ep,n,z,c+x.val());
      } else {
        return new (home) LqBoolScale<EmptyScaleBoolArray,SBAN,VX>
          (home,share,*this,ep,n,x,c);
      }
    } else if (n.empty()) {
      EmptyScaleBoolArray en;
      if (x.assigned()) {
        ZeroIntView z;
        return new (home) LqBoolScale<SBAP,EmptyScaleBoolArray,ZeroIntView>
          (home,share,*this,p,en,z,c+x.val());
      } else {
        return new (home) LqBoolScale<SBAP,EmptyScaleBoolArray,VX>
          (home,share,*this,p,en,x,c);
      }
    } else {
      return new (home) LqBoolScale<SBAP,SBAN,VX>(home,share,*this,p,n,x,c);
    }
  }

  template<class SBAP, class SBAN, class VX>
  ExecStatus
  LqBoolScale<SBAP,SBAN,VX>::propagate(Space& home, const ModEventDelta& med) {
    int sl = 0;
    if (BoolView::me(med) == ME_BOOL_VAL) {
      // Eliminate assigned positive views while keeping order
      {
        // Skip not assigned views
        ScaleBool* f = p.fst();
        ScaleBool* l = p.lst();
        while ((f < l) && f->x.none())
          f++;
        // Copy unassigned views to t
        ScaleBool* t = f;
        while (f < l) {
          if (f->x.one()) {
            c -= f->a;
          } else if (f->x.none()) {
            *t = *f; t++;
          }
          f++;
        }
        p.lst(t);
      }
      // Eliminate assigned negative views while keeping order
      {
        // Skip not assigned views
        ScaleBool* f = n.fst();
        ScaleBool* l = n.lst();
        while ((f < l) && f->x.none()) {
          sl += f->a; f++;
        }
        // Copy unassigned views to t
        ScaleBool* t = f;
        while (f < l) {
          if (f->x.one()) {
            c += f->a;
          } else if (f->x.none()) {
            sl += f->a; *t = *f; t++;
          }
          f++;
        }
        n.lst(t);
      }
    } else {
      for (ScaleBool* f=n.fst(); f<n.lst(); f++)
        sl += f->a;
    }

    sl += x.max() + c;

    // Propagate upper bound for positive Boolean views
    {
      ScaleBool* f=p.fst();
      for (ScaleBool* l=p.lst(); (f < l) && (f->a > sl); f++)
        GECODE_ME_CHECK(f->x.zero_none(home));
      p.fst(f);
    }
    // Propagate lower bound for negative Boolean views
    {
      ScaleBool* f=n.fst();
      for (ScaleBool* l=n.lst(); (f < l) && (f->a > sl); f++) {
        c += f->a;
        GECODE_ME_CHECK(f->x.one_none(home));
      }
      n.fst(f);
    }
    ExecStatus es = ES_FIX;
    // Propagate lower bound for integer view
    {
      const int slx = x.max() - sl;
      ModEvent me = x.gq(home,slx);
      if (me_failed(me))
        return ES_FAILED;
      if (me_modified(me) && (slx != x.min()))
        es = ES_NOFIX;
    }

    if (p.empty() && n.empty())
      return home.ES_SUBSUMED(*this);

    return es;
  }



  template<class SBAP, class SBAN, class VX>
  ExecStatus
  LqBoolScale<SBAP,SBAN,VX>::post(Home home,
                                  SBAP& p, SBAN& n, VX x, int c) {
    p.sort(); n.sort();
    if (p.empty()) {
      EmptyScaleBoolArray ep;
      (void) new (home) LqBoolScale<EmptyScaleBoolArray,SBAN,VX>
        (home,ep,n,x,c);
    } else if (n.empty()) {
      EmptyScaleBoolArray en;
      (void) new (home) LqBoolScale<SBAP,EmptyScaleBoolArray,VX>
        (home,p,en,x,c);
    } else {
      (void) new (home) LqBoolScale<SBAP,SBAN,VX>
        (home,p,n,x,c);
    }
    return ES_OK;
  }

  /*
   * Boolean disequality with coefficients
   *
   */

  template<class SBAP, class SBAN, class VX>
  forceinline
  NqBoolScale<SBAP,SBAN,VX>::NqBoolScale(Home home,
                                         SBAP& p, SBAN& n,
                                         VX x, int c)
    : LinBoolScale<SBAP,SBAN,VX,PC_INT_VAL>(home,p,n,x,c) {}

  template<class SBAP, class SBAN, class VX>
  forceinline
  NqBoolScale<SBAP,SBAN,VX>::NqBoolScale(Space& home, bool share,
                                         Propagator& pr,
                                         SBAP& p, SBAN& n,
                                         VX x, int c)
    : LinBoolScale<SBAP,SBAN,VX,PC_INT_VAL>(home,share,pr,p,n,x,c) {}

  template<class SBAP, class SBAN, class VX>
  Actor*
  NqBoolScale<SBAP,SBAN,VX>::copy(Space& home, bool share) {
    if (p.empty()) {
      EmptyScaleBoolArray ep;
      if (x.assigned()) {
        ZeroIntView z;
        return new (home) NqBoolScale<EmptyScaleBoolArray,SBAN,ZeroIntView>
          (home,share,*this,ep,n,z,c+x.val());
      } else {
        return new (home) NqBoolScale<EmptyScaleBoolArray,SBAN,VX>
          (home,share,*this,ep,n,x,c);
      }
    } else if (n.empty()) {
      EmptyScaleBoolArray en;
      if (x.assigned()) {
        ZeroIntView z;
        return new (home) NqBoolScale<SBAP,EmptyScaleBoolArray,ZeroIntView>
          (home,share,*this,p,en,z,c+x.val());
      } else {
        return new (home) NqBoolScale<SBAP,EmptyScaleBoolArray,VX>
          (home,share,*this,p,en,x,c);
      }
    } else {
      return new (home) NqBoolScale<SBAP,SBAN,VX>(home,share,*this,p,n,x,c);
    }
  }

  template<class SBAP, class SBAN, class VX>
  ExecStatus
  NqBoolScale<SBAP,SBAN,VX>::propagate(Space& home, const ModEventDelta& med) {
    if (BoolView::me(med) == ME_BOOL_VAL) {
      // Eliminate assigned positive views
      {
        ScaleBool* f = p.fst();
        ScaleBool* t = f;
        ScaleBool* l = p.lst();
        while (f < l) {
          if (f->x.one()) {
            c -= f->a; *f = *(t++);
          } else if (f->x.zero()) {
            *f = *(t++);
          }
          f++;
        }
        p.fst(t);
      }
      // Eliminate assigned negative views
      {
        ScaleBool* f = n.fst();
        ScaleBool* t = f;
        ScaleBool* l = n.lst();
        while (f < l) {
          if (f->x.one()) {
            c += f->a; *f = *(t++);
          } else if (f->x.zero()) {
            *f = *(t++);
          }
          f++;
        }
        n.fst(t);
      }
    }

    if (p.empty() && n.empty()) {
      GECODE_ME_CHECK(x.nq(home,-c));
      return home.ES_SUBSUMED(*this);
    }

    if (x.assigned()) {
      int r = x.val()+c;
      if (p.empty() && (n.size() == 1)) {
        if (r == -n.fst()->a) {
          GECODE_ME_CHECK(n.fst()->x.zero_none(home));
        } else if (r == 0) {
          GECODE_ME_CHECK(n.fst()->x.one_none(home));
        }
        return home.ES_SUBSUMED(*this);
      }
      if ((p.size() == 1) && n.empty()) {
        if (r == p.fst()->a) {
          GECODE_ME_CHECK(p.fst()->x.zero_none(home));
        } else if (r == 0) {
          GECODE_ME_CHECK(p.fst()->x.one_none(home));
        }
        return home.ES_SUBSUMED(*this);
      }
    }
    return ES_FIX;
  }



  template<class SBAP, class SBAN, class VX>
  ExecStatus
  NqBoolScale<SBAP,SBAN,VX>::post(Home home,
                                  SBAP& p, SBAN& n, VX x, int c) {
    if (p.empty()) {
      EmptyScaleBoolArray ep;
      (void) new (home) NqBoolScale<EmptyScaleBoolArray,SBAN,VX>
        (home,ep,n,x,c);
    } else if (n.empty()) {
      EmptyScaleBoolArray en;
      (void) new (home) NqBoolScale<SBAP,EmptyScaleBoolArray,VX>
        (home,p,en,x,c);
    } else {
      (void) new (home) NqBoolScale<SBAP,SBAN,VX>
        (home,p,n,x,c);
    }
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

