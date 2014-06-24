/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
 *     Gabor Szokoli, 2003
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

namespace Gecode { namespace Int { namespace Rel {

  /*
   * Less or equal propagator
   *
   */

  template<class View>
  forceinline
  Lq<View>::Lq(Home home, View x0, View x1)
    : BinaryPropagator<View,PC_INT_BND>(home,x0,x1) {}

  template<class View>
  ExecStatus
  Lq<View>::post(Home home, View x0, View x1) {
    GECODE_ME_CHECK(x0.lq(home,x1.max()));
    GECODE_ME_CHECK(x1.gq(home,x0.min()));
    if (!same(x0,x1) && (x0.max() > x1.min()))
      (void) new (home) Lq<View>(home,x0,x1);
    return ES_OK;
  }

  template<class View>
  forceinline
  Lq<View>::Lq(Space& home, bool share, Lq<View>& p)
    : BinaryPropagator<View,PC_INT_BND>(home,share,p) {}

  template<class View>
  Actor*
  Lq<View>::copy(Space& home, bool share) {
    return new (home) Lq<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  Lq<View>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x0.lq(home,x1.max()));
    GECODE_ME_CHECK(x1.gq(home,x0.min()));
    return (x0.max() <= x1.min()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }




  /*
   * Less propagator
   *
   */
  template<class View>
  forceinline
  Le<View>::Le(Home home, View x0, View x1)
    : BinaryPropagator<View,PC_INT_BND>(home,x0,x1) {}

  template<class View>
  ExecStatus
  Le<View>::post(Home home, View x0, View x1) {
    if (same(x0,x1))
      return ES_FAILED;
    GECODE_ME_CHECK(x0.le(home,x1.max()));
    GECODE_ME_CHECK(x1.gr(home,x0.min()));
    if (x0.max() >= x1.min())
      (void) new (home) Le<View>(home,x0,x1);
    return ES_OK;
  }

  template<class View>
  forceinline
  Le<View>::Le(Space& home, bool share, Le<View>& p)
    : BinaryPropagator<View,PC_INT_BND>(home,share,p) {}

  template<class View>
  Actor*
  Le<View>::copy(Space& home, bool share) {
    return new (home) Le<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  Le<View>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ME_CHECK(x0.le(home,x1.max()));
    GECODE_ME_CHECK(x1.gr(home,x0.min()));
    return (x0.max() < x1.min()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }



  /*
   * Nary less and less or equal propagator
   *
   */

  template<class View, int o>
  forceinline
  NaryLqLe<View,o>::Index::Index(Space& home, Propagator& p,
                                 Council<Index>& c, int i0)
    : Advisor(home,p,c), i(i0) {}

  template<class View, int o>
  forceinline
  NaryLqLe<View,o>::Index::Index(Space& home, bool share, Index& a)
    : Advisor(home,share,a), i(a.i) {}



  template<class View, int o>
  forceinline
  NaryLqLe<View,o>::Pos::Pos(int p0, Pos* n)
    : FreeList(n), p(p0) {}

  template<class View, int o>
  forceinline typename NaryLqLe<View,o>::Pos*
  NaryLqLe<View,o>::Pos::next(void) const {
    return static_cast<Pos*>(FreeList::next());
  }

  template<class View, int o>
  forceinline void
  NaryLqLe<View,o>::Pos::operator delete(void*) {}

  template<class View, int o>
  forceinline void
  NaryLqLe<View,o>::Pos::operator delete(void*, Space&) {
    GECODE_NEVER;
  }

  template<class View, int o>
  forceinline void*
  NaryLqLe<View,o>::Pos::operator new(size_t, Space& home) {
    return home.fl_alloc<sizeof(Pos)>();
  }

  template<class View, int o>
  forceinline void
  NaryLqLe<View,o>::Pos::dispose(Space& home) {
    home.fl_dispose<sizeof(Pos)>(this,this);
  }


  template<class View, int o>
  forceinline bool
  NaryLqLe<View,o>::empty(void) const {
    return pos == NULL;
  }
  template<class View, int o>
  forceinline void
  NaryLqLe<View,o>::push(Space& home, int p) {
    // Try to avoid entering same position twice
    if ((pos != NULL) && (pos->p == p))
      return;
    pos = new (home) Pos(p,pos);
  }
  template<class View, int o>
  forceinline int
  NaryLqLe<View,o>::pop(Space& home) {
    Pos* t = pos;
    int p = t->p;
    pos = pos->next();
    t->dispose(home);
    return p;
  }

  template<class View, int o>
  forceinline
  NaryLqLe<View,o>::NaryLqLe(Home home, ViewArray<View>& x)
    : NaryPropagator<View,PC_INT_NONE>(home,x), 
      c(home), pos(NULL), run(false), n_subsumed(0) {
    for (int i=x.size(); i--; )
      x[i].subscribe(home, *new (home) Index(home,*this,c,i));
  }

  template<class View, int o>
  ExecStatus
  NaryLqLe<View,o>::post(Home home, ViewArray<View>& x) {
    assert((o == 0) || (o == 1));
    // Check for sharing
    if (x.same(home)) {
      if (o == 1)
        return ES_FAILED;
      /*
       * Eliminate sharing: if a view occurs twice, all views in between
       * must be equal.
       */
      int n = x.size();
      for (int i=0; i<n; i++)
        for (int j=n-1; j>i; j--)
          if (same(x[i],x[j])) {
            if (i+1 != j) {
              // Create equality propagator for elements i+1 ... j
              ViewArray<View> y(home,j-i);
              for (int k=j-i; k--; )
                y[k] = x[i+1+k];
              GECODE_ES_CHECK(NaryEqBnd<View>::post(home,y));
            }
            for (int k=0; k<n-1-j-1+1; k++)
              x[i+1+k]=x[j+1+k];
            n -= j-i;
          }
      x.size(n);
    }
        
    // Propagate one round
    for (int i=1; i<x.size(); i++)
      GECODE_ME_CHECK(x[i].gq(home,x[i-1].min()+o));
    for (int i=x.size()-1; i--;)
      GECODE_ME_CHECK(x[i].lq(home,x[i+1].max()-o));
    // Eliminate redundant variables
    {
      // Eliminate at beginning
      {
        int i=0;
        while ((i+1 < x.size()) && (x[i].max()+o <= x[i+1].min()))
          i++;
        x.drop_fst(i);
      }
      // Eliminate at end
      {
        int i=x.size()-1;
        while ((i > 0) && (x[i-1].max()+o <= x[i].min()))
          i--;
        x.drop_lst(i);
      }
      // Eliminate in the middle
      if (x.size() > 1) {
        int j=1;
        for (int i=1; i+1<x.size(); i++)
          if ((x[j-1].max()+o > x[i].min()) ||
              (x[i].max()+o > x[i+1].min()))
            x[j++]=x[i];
        x[j++]=x[x.size()-1];
        x.size(j);
      }
    }
    if (x.size() == 2) {
      if (o == 0)
        return Lq<View>::post(home,x[0],x[1]);
      else
        return Le<View>::post(home,x[0],x[1]);
    } else if (x.size() >= 2) {
      (void) new (home) NaryLqLe<View,o>(home,x);
    }
    return ES_OK;
  }

  template<class View, int o>
  forceinline
  NaryLqLe<View,o>::NaryLqLe(Space& home, bool share, NaryLqLe<View,o>& p)
    : NaryPropagator<View,PC_INT_NONE>(home,share,p), 
      pos(NULL), run(false), n_subsumed(p.n_subsumed) {
    assert(p.pos == NULL);
    c.update(home, share, p.c);
  }

  template<class View, int o>
  Actor*
  NaryLqLe<View,o>::copy(Space& home, bool share) {
    if (n_subsumed > n_threshold) {
      Region r(home);
      // Record for which views there is an advisor
      Support::BitSet<Region> a(r,static_cast<unsigned int>(x.size()));
      for (Advisors<Index> as(c); as(); ++as)
        a.set(static_cast<unsigned int>(as.advisor().i));
      // Compact view array and compute map for advisors
      int* m = r.alloc<int>(x.size());
      int j=0;
      for (int i=0; i<x.size(); i++)
        if (a.get(static_cast<unsigned int>(i))) {
          m[i] = j; x[j++] = x[i];
        }
      x.size(j);
      // Remap advisors
      for (Advisors<Index> as(c); as(); ++as)
        as.advisor().i = m[as.advisor().i];
      
      n_subsumed = 0;
    }
    return new (home) NaryLqLe<View,o>(home,share,*this);
  }

  template<class View, int o>
  PropCost
  NaryLqLe<View,o>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::binary(PropCost::HI);
  }

  template<class View, int o>
  forceinline size_t
  NaryLqLe<View,o>::dispose(Space& home) {
    for (Advisors<Index> as(c); as(); ++as)
      x[as.advisor().i].cancel(home,as.advisor());
    c.dispose(home);
    while (!empty())
      (void) pop(home);
    (void) NaryPropagator<View,PC_INT_NONE>::dispose(home);
    return sizeof(*this);
  }


  template<class View, int o>
  ExecStatus
  NaryLqLe<View,o>::advise(Space& home, Advisor& _a, const Delta& d) {
    Index& a = static_cast<Index&>(_a);
    const int i = a.i;
    switch (View::modevent(d)) {
    case ME_INT_VAL:
      a.dispose(home,c);
      n_subsumed++;
      break;
    case ME_INT_BND:
      if (((i == 0) || (x[i-1].max()+o <= x[i].min())) &&
          ((i == x.size()-1) || (x[i].max()+o <= x[i+1].min()))) {
        x[i].cancel(home,a);
        a.dispose(home,c);
        n_subsumed++;
        return (run || (n_subsumed + 1 < x.size())) ? ES_FIX : ES_NOFIX;
      }
      break;
    default:
      return ES_FIX;
    }
    if (run)
      return ES_FIX;
    if (((i < x.size()-1) && (x[i+1].min() < x[i].min()+o)) ||
        ((i > 0) && (x[i-1].max() > x[i].max()-o))) {
      push(home,i);
      return ES_NOFIX;
    }
    return (n_subsumed+1 >= x.size()) ? ES_NOFIX : ES_FIX;
  }

  template<class View, int o>
  ExecStatus
  NaryLqLe<View,o>::propagate(Space& home, const ModEventDelta&) {
    run = true;
    int n = x.size();
    while (!empty()) {
      int p = pop(home);
      for (int i=p; i<n-1; i++) {
        ModEvent me = x[i+1].gq(home,x[i].min()+o);
        if (me_failed(me))
          return ES_FAILED;
        if (!me_modified(me))
          break;
      }
      for (int i=p; i>0; i--) {
        ModEvent me = x[i-1].lq(home,x[i].max()-o);
        if (me_failed(me))
          return ES_FAILED;
        if (!me_modified(me))
          break;
      }
    }
#ifdef GECODE_AUDIT
    for (int i=0; i<n-1; i++)
      assert(!me_modified(x[i+1].gq(home,x[i].min()+o)));
    for (int i=n-1; i>0; i--)
      assert(!me_modified(x[i-1].lq(home,x[i].max()-o)));
#endif
    if (n_subsumed+1 >= n)
      return home.ES_SUBSUMED(*this);
    run = false;
    return ES_FIX;
  }



  /*
   * Reified less or equal propagator
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLq<View,CtrlView,rm>::ReLq(Home home, View x0, View x1, CtrlView b)
    : ReBinaryPropagator<View,PC_INT_BND,CtrlView>(home,x0,x1,b) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLq<View,CtrlView,rm>::post(Home home, View x0, View x1, CtrlView b) {
    if (b.one()) {
      if (rm == RM_PMI)
        return ES_OK;
      return Lq<View>::post(home,x0,x1);
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return ES_OK;
      return Le<View>::post(home,x1,x0);
    }
    if (!same(x0,x1)) {
      switch (rtest_lq(x0,x1)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home)); 
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home)); 
        break;
      case RT_MAYBE:
        (void) new (home) ReLq<View,CtrlView,rm>(home,x0,x1,b); 
        break;
      default: GECODE_NEVER;
      }
    } else if (rm != RM_IMP) {
      GECODE_ME_CHECK(b.one_none(home));
    }
    return ES_OK;
  }

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLq<View,CtrlView,rm>::ReLq(Space& home, bool share, ReLq& p)
    : ReBinaryPropagator<View,PC_INT_BND,CtrlView>(home,share,p) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReLq<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReLq<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLq<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_REWRITE(*this,Lq<View>::post(home(*this),x0,x1));
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_REWRITE(*this,Le<View>::post(home(*this),x1,x0));
    } else {
      switch (rtest_lq(x0,x1)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home)); 
        break;
      case RT_MAYBE:
        return ES_FIX;
      default: GECODE_NEVER;
      }
    }
    return home.ES_SUBSUMED(*this);
  }

  /*
   * Reified less or equal propagator involving one variable
   *
   */

  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLqInt<View,CtrlView,rm>::ReLqInt(Home home, View x, int c0, CtrlView b)
    : ReUnaryPropagator<View,PC_INT_BND,CtrlView>(home,x,b), c(c0) {}

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLqInt<View,CtrlView,rm>::post(Home home, View x, int c, CtrlView b) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x.lq(home,c));
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(x.gr(home,c));
    } else {
      switch (rtest_lq(x,c)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
        break;
      case RT_MAYBE:
        (void) new (home) ReLqInt<View,CtrlView,rm>(home,x,c,b);
        break;
      default: GECODE_NEVER;
      }
    }
    return ES_OK;
  }


  template<class View, class CtrlView, ReifyMode rm>
  forceinline
  ReLqInt<View,CtrlView,rm>::ReLqInt(Space& home, bool share, ReLqInt& p)
    : ReUnaryPropagator<View,PC_INT_BND,CtrlView>(home,share,p), c(p.c) {}

  template<class View, class CtrlView, ReifyMode rm>
  Actor*
  ReLqInt<View,CtrlView,rm>::copy(Space& home, bool share) {
    return new (home) ReLqInt<View,CtrlView,rm>(home,share,*this);
  }

  template<class View, class CtrlView, ReifyMode rm>
  ExecStatus
  ReLqInt<View,CtrlView,rm>::propagate(Space& home, const ModEventDelta&) {
    if (b.one()) {
      if (rm != RM_PMI)
        GECODE_ME_CHECK(x0.lq(home,c));
    } else if (b.zero()) {
      if (rm != RM_IMP)
        GECODE_ME_CHECK(x0.gr(home,c));
    } else {
      switch (rtest_lq(x0,c)) {
      case RT_TRUE:
        if (rm != RM_IMP)
          GECODE_ME_CHECK(b.one_none(home));
        break;
      case RT_FALSE:
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero_none(home));
        break;
      case RT_MAYBE:
        return ES_FIX;
      default: GECODE_NEVER;
      }
    }
    return home.ES_SUBSUMED(*this);
  }

}}}

// STATISTICS: int-prop

