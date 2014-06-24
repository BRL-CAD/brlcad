/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Mikael Lagerkvist, 2007
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

namespace Gecode { namespace Int { namespace Extensional {

  /*
   * Support advisor
   *
   */

  template<class View>
  forceinline
  Incremental<View>::SupportAdvisor::
  SupportAdvisor(Space& home, Propagator& p, Council<SupportAdvisor>& c,
                 int i0)
    : Advisor(home,p,c), i(i0) {}

  template<class View>
  forceinline
  Incremental<View>::SupportAdvisor::
  SupportAdvisor(Space& home, bool share, SupportAdvisor& a)
    : Advisor(home,share,a), i(a.i) {}

  template<class View>
  forceinline void
  Incremental<View>::SupportAdvisor::
  dispose(Space& home, Council<SupportAdvisor>& c) {
    Advisor::dispose(home,c);
  }


  /*
   * Support entries
   *
   */
  template<class View>
  forceinline
  Incremental<View>::SupportEntry::SupportEntry(Tuple t0)
    : t(t0) {}

  template<class View>
  forceinline
  Incremental<View>::SupportEntry::SupportEntry(Tuple t0, SupportEntry* n)
    : FreeList(n), t(t0) {}

  template<class View>
  forceinline typename Incremental<View>::SupportEntry*
  Incremental<View>::SupportEntry::next(void) const {
    return static_cast<SupportEntry*>(FreeList::next());
  }

  template<class View>
  forceinline typename Incremental<View>::SupportEntry**
  Incremental<View>::SupportEntry::nextRef(void) {
    return reinterpret_cast<SupportEntry**>(FreeList::nextRef());
  }

  template<class View>
  forceinline void
  Incremental<View>::SupportEntry::operator delete(void*) {}

  template<class View>
  forceinline void
  Incremental<View>::SupportEntry::operator delete(void*, Space&) {
    GECODE_NEVER;
  }

  template<class View>
  forceinline void*
  Incremental<View>::SupportEntry::operator new(size_t, Space& home) {
    return home.fl_alloc<sizeof(SupportEntry)>();
  }

  template<class View>
  forceinline void
  Incremental<View>::SupportEntry::dispose(Space& home) {
    home.fl_dispose<sizeof(SupportEntry)>(this,this);
  }

  template<class View>
  forceinline void
  Incremental<View>::SupportEntry::dispose(Space& home, SupportEntry* l) {
    home.fl_dispose<sizeof(SupportEntry)>(this,l);
  }


  /*
   * Work entries
   *
   */
  template<class View>
  forceinline
  Incremental<View>::WorkEntry::WorkEntry(int i0, int n0, WorkEntry* n)
    : FreeList(n), i(i0), n(n0) {}

  template<class View>
  forceinline typename Incremental<View>::WorkEntry*
  Incremental<View>::WorkEntry::next(void) const {
    return static_cast<WorkEntry*>(FreeList::next());
  }

  template<class View>
  forceinline void
  Incremental<View>::WorkEntry::next(WorkEntry* n) {
    return FreeList::next(n);
  }

  template<class View>
  forceinline void
  Incremental<View>::WorkEntry::operator delete(void*) {}

  template<class View>
  forceinline void
  Incremental<View>::WorkEntry::operator delete(void*, Space&) {
    GECODE_NEVER;
  }

  template<class View>
  forceinline void*
  Incremental<View>::WorkEntry::operator new(size_t, Space& home) {
    return home.fl_alloc<sizeof(WorkEntry)>();
  }

  template<class View>
  forceinline void
  Incremental<View>::WorkEntry::dispose(Space& home) {
    home.fl_dispose<sizeof(WorkEntry)>(this,this);
  }


  /*
   * Work stack
   *
   */
  template<class View>
  forceinline
  Incremental<View>::Work::Work(void)
    : we(NULL) {}
  template<class View>
  forceinline bool
  Incremental<View>::Work::empty(void) const {
    return we == NULL;
  }
  template<class View>
  forceinline void
  Incremental<View>::Work::push(Space& home, int i, int n) {
    we = new (home) WorkEntry(i,n,we);
  }
  template<class View>
  forceinline void
  Incremental<View>::Work::pop(Space& home, int& i, int& n) {
    WorkEntry* d = we;
    we = we->next();
    i=d->i; n=d->n;
    d->dispose(home);
  }


  /*
   * Support management
   *
   */
  template<class View>
  forceinline typename Incremental<View>::SupportEntry*
  Incremental<View>::support(int i, int n) {
    return support_data[(i*(ts()->domsize)) + (n - ts()->min)];
  }

  template<class View>
  forceinline void
  Incremental<View>::init_support(Space& home) {
    assert(support_data == NULL);
    int literals = static_cast<int>(ts()->domsize*x.size());
    support_data = home.alloc<SupportEntry*>(literals);
    for (int i = literals; i--; )
      support_data[i] = NULL;
  }

  template<class View>
  forceinline void
  Incremental<View>::add_support(Space& home, Tuple l) {
    for (int i = x.size(); i--; ) {
      int pos = (i*static_cast<int>(ts()->domsize)) + (l[i] - ts()->min);
      support_data[pos] = new (home) SupportEntry(l, support_data[pos]);
    }
  }

  template<class View>
  forceinline void
  Incremental<View>::find_support(Space& home, Domain dom, int i, int n) {
    if (support(i,n) == NULL) {
      // Find support for value vv.val() in view i
      Tuple l = Base<View,false>::find_support(dom,i,n - ts()->min);
      if (l == NULL) {
        // No possible supports left
        w_remove.push(home,i,n);
      } else {
        // Mark values in support as supported
        add_support(home,l);
      }
    }
  }

  template<class View>
  forceinline void
  Incremental<View>::remove_support(Space& home, Tuple l, int i, int n) {
    (void) n;
    for (int j = x.size(); j--; ) {
      int v = l[j];
      int ov = v - ts()->min;
      int pos = (j*(static_cast<int>(ts()->domsize))) + ov;

      assert(support_data[pos] != NULL);

      SupportEntry** a = &(support_data[pos]);
      while ((*a)->t != l) {
        assert((*a)->next() != NULL);
        a = (*a)->nextRef();
      }
      SupportEntry* old = *a;
      *a = (*a)->next();

      old->dispose(home);
      if ((i != j) && (support_data[pos] == NULL))
        w_support.push(home, j, v);
    }
  }



  /*
   * The propagator proper
   *
   */

  template<class View>
  forceinline
  Incremental<View>::Incremental(Home home, ViewArray<View>& x,
                                 const TupleSet& t)
    : Base<View,false>(home,x,t), support_data(NULL),
      unassigned(x.size()), ac(home) {
    init_support(home);

    // Post advisors
    for (int i = x.size(); i--; )
      if (x[i].assigned()) {
        --unassigned;
      } else {
        x[i].subscribe(home,*new (home) SupportAdvisor(home,*this,ac,i));
      }

    Region r(home);

    // Add initial supports
    BitSet* dom = r.alloc<BitSet>(x.size());
    init_dom(home, dom);
    for (int i = x.size(); i--; )
      for (ViewValues<View> vv(x[i]); vv(); ++vv)
        find_support(home, dom, i, vv.val());

    // Work to be done or subsumption
    if (!w_support.empty() || !w_remove.empty() || (unassigned == 0))
      View::schedule(home,*this,
                     (unassigned != x.size()) ? ME_INT_VAL : ME_INT_DOM);
  }

  template<class View>
  forceinline ExecStatus
  Incremental<View>::post(Home home, ViewArray<View>& x, const TupleSet& t) {
    // All variables in the correct domain
    for (int i = x.size(); i--; ) {
      GECODE_ME_CHECK(x[i].gq(home, t.min()));
      GECODE_ME_CHECK(x[i].lq(home, t.max()));
    }
    (void) new (home) Incremental<View>(home,x,t);
    return ES_OK;
  }

  template<class View>
  forceinline
  Incremental<View>::Incremental(Space& home, bool share, Incremental<View>& p)
    : Base<View,false>(home,share,p), support_data(NULL),
      unassigned(p.unassigned) {
    ac.update(home,share,p.ac);

    init_support(home);
    for (int i = static_cast<int>(ts()->domsize*x.size()); i--; ) {
      SupportEntry** n = &(support_data[i]);
      SupportEntry*  o = p.support_data[i];
      while (o != NULL) {
        // Allocate new support entry
        SupportEntry* s =
          new (home) SupportEntry(ts()->data+(o->t-p.ts()->data));
        // Link in support entry
        (*n) = s; n = s->nextRef();
        // move to next one
        o = o->next();
      }
      *n = NULL;
    }
  }

  template<class View>
  PropCost
  Incremental<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_VAL)
      return PropCost::quadratic(PropCost::HI,x.size());
    else
      return PropCost::cubic(PropCost::HI,x.size());
  }

  template<class View>
  Actor*
  Incremental<View>::copy(Space& home, bool share) {
    return new (home) Incremental<View>(home,share,*this);
  }

  template<class View>
  forceinline size_t
  Incremental<View>::dispose(Space& home) {
    if (!home.failed()) {
      int literals = static_cast<int>(ts()->domsize*x.size());
      for (int i = literals; i--; )
        if (support_data[i]) {
          SupportEntry* lastse = support_data[i];
          while (lastse->next() != NULL)
            lastse = lastse->next();
          support_data[i]->dispose(home, lastse);
        }
      home.rfree(support_data, sizeof(SupportEntry*)*literals);
    }
    ac.dispose(home);
    (void) Base<View,false>::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  ExecStatus
  Incremental<View>::propagate(Space& home, const ModEventDelta&) {
    assert(!w_support.empty() || !w_remove.empty() || unassigned==0);
    // Set up datastructures
    // Bit-sets for amortized O(1) access to domains
    Region r(home);
    // Add initial supports
    BitSet* dom = r.alloc<BitSet>(x.size());
    init_dom(home, dom);

    // Work loop
    while (!w_support.empty() || !w_remove.empty()) {
      while (!w_remove.empty()) {
        int i, n;
        w_remove.pop(home,i,n);
        // Work is still relevant
        if (dom[i].get(static_cast<unsigned int>(n-ts()->min))) {
          GECODE_ME_CHECK(x[i].nq(home,n));
          dom[i].clear(static_cast<unsigned int>(n-ts()->min));
        }
      }
      while (!w_support.empty()) {
        int i, n;
        w_support.pop(home,i,n);
        // Work is still relevant
        if (dom[i].get(static_cast<unsigned int>(n-ts()->min)))
          find_support(home, dom, i, n);
      }
    }
    if (unassigned != 0)
      return ES_FIX;

    return home.ES_SUBSUMED(*this);
  }


  template<class View>
  ExecStatus
  Incremental<View>::advise(Space& home, Advisor& _a, const Delta& d) {
    SupportAdvisor& a = static_cast<SupportAdvisor&>(_a);
    ModEvent me = View::modevent(d);
    bool scheduled = !w_support.empty() || !w_remove.empty();

    if (x[a.i].any(d)) {
      ViewValues<View> vv(x[a.i]);
      for (int n = ts()->min; n <= ts()->max; n++) {
        if (vv() && (n == vv.val())) {
          ++vv;
          continue;
        }
        while (SupportEntry* s = support(a.i,n))
          remove_support(home, s->t, a.i, n);
      }
    } else {
      for (int n = x[a.i].min(d); n <= x[a.i].max(d); n++)
        while (SupportEntry* s = support(a.i,n))
          remove_support(home, s->t, a.i, n);
    }

    if (me == ME_INT_VAL) {
      --unassigned;
      // nothing to do or already scheduled
      // propagator is not subsumed since unassigned!=0
      if (((w_support.empty() && w_remove.empty()) || scheduled) &&
          (unassigned != 0))
        return home.ES_FIX_DISPOSE(ac,a);
      else
        return home.ES_NOFIX_DISPOSE(ac,a);
    } else if ((w_support.empty() && w_remove.empty()) || scheduled) {
      // nothing to do or already scheduled
      return ES_FIX;
    }
    return ES_NOFIX;
  }


}}}

// STATISTICS: int-prop

