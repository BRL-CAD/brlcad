/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
 *     Guido Tack, 2004
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

namespace Gecode { namespace Int {

  /*
   * Range lists
   *
   */

#define GECODE_INT_RL2PD(r) reinterpret_cast<ptrdiff_t>(r)
#define GECODE_INT_PD2RL(p) reinterpret_cast<RangeList*>(p)

  forceinline
  IntVarImp::RangeList::RangeList(void) {}

  forceinline
  IntVarImp::RangeList::RangeList(int min, int max)
    : _min(min), _max(max) {}

  forceinline
  IntVarImp::RangeList::RangeList(int min, int max, RangeList* p, RangeList* n)
    : _min(min), _max(max) {
    _next = GECODE_INT_PD2RL(GECODE_INT_RL2PD(p)^GECODE_INT_RL2PD(n));
  }

  forceinline IntVarImp::RangeList*
  IntVarImp::RangeList::next(const RangeList* p) const {
    return GECODE_INT_PD2RL(GECODE_INT_RL2PD(_next)^GECODE_INT_RL2PD(p));
  }
  forceinline IntVarImp::RangeList*
  IntVarImp::RangeList::prev(const RangeList* n) const {
    return GECODE_INT_PD2RL(GECODE_INT_RL2PD(_next)^GECODE_INT_RL2PD(n));
  }
  forceinline void
  IntVarImp::RangeList::prevnext(RangeList* p, RangeList* n) {
    _next = GECODE_INT_PD2RL(GECODE_INT_RL2PD(p)^GECODE_INT_RL2PD(n));
  }
  forceinline void
  IntVarImp::RangeList::next(RangeList* o, RangeList* n) {
    _next = GECODE_INT_PD2RL(GECODE_INT_RL2PD(_next)^
                             (GECODE_INT_RL2PD(o)^GECODE_INT_RL2PD(n)));
  }
  forceinline void
  IntVarImp::RangeList::prev(RangeList* o, RangeList* n) {
    _next = GECODE_INT_PD2RL(GECODE_INT_RL2PD(_next)^
                             (GECODE_INT_RL2PD(o)^GECODE_INT_RL2PD(n)));
  }
  forceinline void
  IntVarImp::RangeList::fix(RangeList* n) {
    _next = n;
  }

  forceinline void
  IntVarImp::RangeList::min(int n) {
    _min = n;
  }
  forceinline void
  IntVarImp::RangeList::max(int n) {
    _max = n;
  }

  forceinline int
  IntVarImp::RangeList::min(void) const {
    return _min;
  }
  forceinline int
  IntVarImp::RangeList::max(void) const {
    return _max;
  }
  forceinline unsigned int
  IntVarImp::RangeList::width(void) const {
    return static_cast<unsigned int>(_max - _min) + 1;
  }


  forceinline void
  IntVarImp::RangeList::operator delete(void*) {}

  forceinline void
  IntVarImp::RangeList::operator delete(void*, Space&) {
    GECODE_NEVER;
  }
  forceinline void
  IntVarImp::RangeList::operator delete(void*, void*) {
    GECODE_NEVER;
  }

  forceinline void*
  IntVarImp::RangeList::operator new(size_t, Space& home) {
    return home.fl_alloc<sizeof(RangeList)>();
  }

  forceinline void*
  IntVarImp::RangeList::operator new(size_t, void* p) {
    return p;
  }

  forceinline void
  IntVarImp::RangeList::dispose(Space& home, RangeList* p, RangeList* l) {
    RangeList* c = this;
    while (c != l) {
      RangeList* n = c->next(p);
      c->fix(n);
      p=c; c=n;
    }
    home.fl_dispose<sizeof(RangeList)>(this,l);
  }

  forceinline void
  IntVarImp::RangeList::dispose(Space& home, RangeList* l) {
    home.fl_dispose<sizeof(RangeList)>(this,l);
  }

  forceinline void
  IntVarImp::RangeList::dispose(Space& home) {
    home.fl_dispose<sizeof(RangeList)>(this,this);
  }

#undef GECODE_INT_RL2PD
#undef GECODE_INT_PD2RL

  /*
   * Mainitaining range lists for variable domain
   *
   */

  forceinline IntVarImp::RangeList*
  IntVarImp::fst(void) const {
    return dom.next(NULL);
  }

  forceinline void
  IntVarImp::fst(IntVarImp::RangeList* f) {
    dom.prevnext(NULL,f);
  }

  forceinline IntVarImp::RangeList*
  IntVarImp::lst(void) const {
    return _lst;
  }

  forceinline void
  IntVarImp::lst(IntVarImp::RangeList* l) {
    _lst = l;
  }

  /*
   * Creation of new variable implementations
   *
   */

  forceinline
  IntVarImp::IntVarImp(Space& home, int min, int max)
    : IntVarImpBase(home), dom(min,max,NULL,NULL), holes(0) {}

  forceinline
  IntVarImp::IntVarImp(Space& home, const IntSet& d)
    : IntVarImpBase(home), dom(d.min(),d.max()) {
    if (d.ranges() > 1) {
      int n = d.ranges();
      assert(n >= 2);
      RangeList* r = home.alloc<RangeList>(n);
      fst(r); lst(r+n-1);
      unsigned int h = static_cast<unsigned int>(d.max()-d.min())+1;
      h -= d.width(0);
      r[0].min(d.min(0)); r[0].max(d.max(0));
      r[0].prevnext(NULL,&r[1]);
      for (int i = 1; i < n-1; i++) {
        h -= d.width(i);
        r[i].min(d.min(i)); r[i].max(d.max(i));
        r[i].prevnext(&r[i-1],&r[i+1]);
      }
      h -= d.width(n-1);
      r[n-1].min(d.min(n-1)); r[n-1].max(d.max(n-1));
      r[n-1].prevnext(&r[n-2],NULL);
      holes = h;
    } else {
      fst(NULL); holes = 0;
    }
  }


  /*
   * Operations on integer variable implementations
   *
   */

  forceinline int
  IntVarImp::min(void) const {
    return dom.min();
  }
  forceinline int
  IntVarImp::max(void) const {
    return dom.max();
  }
  forceinline int
  IntVarImp::val(void) const {
    assert(dom.min() == dom.max());
    return dom.min();
  }

  forceinline bool
  IntVarImp::range(void) const {
    return fst() == NULL;
  }
  forceinline bool
  IntVarImp::assigned(void) const {
    return dom.min() == dom.max();
  }


  forceinline unsigned int
  IntVarImp::width(void) const {
    return dom.width();
  }

  forceinline unsigned int
  IntVarImp::size(void) const {
    return dom.width() - holes;
  }

  forceinline unsigned int
  IntVarImp::regret_min(void) const {
    if (fst() == NULL) {
      return (dom.min() == dom.max()) ? 0U : 1U;
    } else if (dom.min() == fst()->max()) {
      return static_cast<unsigned int>(fst()->next(NULL)->min()-dom.min());
    } else {
      return 1U;
    }
  }
  forceinline unsigned int
  IntVarImp::regret_max(void) const {
    if (fst() == NULL) {
      return (dom.min() == dom.max()) ? 0U : 1U;
    } else if (dom.max() == lst()->min()) {
      return static_cast<unsigned int>(dom.max()-lst()->prev(NULL)->max());
    } else {
      return 1U;
    }
  }



  /*
   * Tests
   *
   */

  forceinline bool
  IntVarImp::in(int n) const {
    if ((n < dom.min()) || (n > dom.max()))
      return false;
    return (fst() == NULL) || in_full(n);
  }
  forceinline bool
  IntVarImp::in(long long int n) const {
    if ((n < dom.min()) || (n > dom.max()))
      return false;
    return (fst() == NULL) || in_full(static_cast<int>(n));
  }


  /*
   * Accessing rangelists for iteration
   *
   */

  forceinline const IntVarImp::RangeList*
  IntVarImp::ranges_fwd(void) const {
    return (fst() == NULL) ? &dom : fst();
  }

  forceinline const IntVarImp::RangeList*
  IntVarImp::ranges_bwd(void) const {
    return (fst() == NULL) ? &dom : lst();
  }



  /*
   * Support for delta information
   *
   */
  forceinline int
  IntVarImp::min(const Delta& d) {
    return static_cast<const IntDelta&>(d).min();
  }
  forceinline int
  IntVarImp::max(const Delta& d) {
    return static_cast<const IntDelta&>(d).max();
  }
  forceinline bool
  IntVarImp::any(const Delta& d) {
    return static_cast<const IntDelta&>(d).any();
  }


  /*
   * Tell operations (to be inlined: performing bounds checks first)
   *
   */

  forceinline ModEvent
  IntVarImp::gq(Space& home, int n) {
    if (n <= dom.min()) return ME_INT_NONE;
    if (n > dom.max())  return ME_INT_FAILED;
    ModEvent me = gq_full(home,n);
    GECODE_ASSUME((me == ME_INT_FAILED) |
                  (me == ME_INT_VAL) |
                  (me == ME_INT_BND));
    return me;
  }
  forceinline ModEvent
  IntVarImp::gq(Space& home, long long int n) {
    if (n <= dom.min()) return ME_INT_NONE;
    if (n > dom.max())  return ME_INT_FAILED;
    ModEvent me = gq_full(home,static_cast<int>(n));
    GECODE_ASSUME((me == ME_INT_FAILED) |
                  (me == ME_INT_VAL) |
                  (me == ME_INT_BND));
    return me;
  }

  forceinline ModEvent
  IntVarImp::lq(Space& home, int n) {
    if (n >= dom.max()) return ME_INT_NONE;
    if (n < dom.min())  return ME_INT_FAILED;
    ModEvent me = lq_full(home,n);
    GECODE_ASSUME((me == ME_INT_FAILED) |
                  (me == ME_INT_VAL) |
                  (me == ME_INT_BND));
    return me;
  }
  forceinline ModEvent
  IntVarImp::lq(Space& home, long long int n) {
    if (n >= dom.max()) return ME_INT_NONE;
    if (n < dom.min())  return ME_INT_FAILED;
    ModEvent me = lq_full(home,static_cast<int>(n));
    GECODE_ASSUME((me == ME_INT_FAILED) |
                  (me == ME_INT_VAL) |
                  (me == ME_INT_BND));
    return me;
  }

  forceinline ModEvent
  IntVarImp::eq(Space& home, int n) {
    if ((n < dom.min()) || (n > dom.max()))
      return ME_INT_FAILED;
    if ((n == dom.min()) && (n == dom.max()))
      return ME_INT_NONE;
    ModEvent me = eq_full(home,n);
    GECODE_ASSUME((me == ME_INT_FAILED) | (me == ME_INT_VAL));
    return me;
  }
  forceinline ModEvent
  IntVarImp::eq(Space& home, long long int m) {
    if ((m < dom.min()) || (m > dom.max()))
      return ME_INT_FAILED;
    int n = static_cast<int>(m);
    if ((n == dom.min()) && (n == dom.max()))
      return ME_INT_NONE;
    ModEvent me = eq_full(home,n);
    GECODE_ASSUME((me == ME_INT_FAILED) | (me == ME_INT_VAL));
    return me;
  }

  forceinline ModEvent
  IntVarImp::nq(Space& home, int n) {
    if ((n < dom.min()) || (n > dom.max()))
      return ME_INT_NONE;
    return nq_full(home,n);
  }
  forceinline ModEvent
  IntVarImp::nq(Space& home, long long int d) {
    if ((d < dom.min()) || (d > dom.max()))
      return ME_INT_NONE;
    return nq_full(home,static_cast<int>(d));
  }


  /*
   * Forward range iterator for rangelists
   *
   */

  forceinline
  IntVarImpFwd::IntVarImpFwd(void) {}
  forceinline
  IntVarImpFwd::IntVarImpFwd(const IntVarImp* x)
    : p(NULL), c(x->ranges_fwd()) {}
  forceinline void
  IntVarImpFwd::init(const IntVarImp* x) {
    p=NULL; c=x->ranges_fwd();
  }

  forceinline bool
  IntVarImpFwd::operator ()(void) const {
    return c != NULL;
  }
  forceinline void
  IntVarImpFwd::operator ++(void) {
    const IntVarImp::RangeList* n=c->next(p); p=c; c=n;
  }

  forceinline int
  IntVarImpFwd::min(void) const {
    return c->min();
  }
  forceinline int
  IntVarImpFwd::max(void) const {
    return c->max();
  }
  forceinline unsigned int
  IntVarImpFwd::width(void) const {
    return c->width();
  }


  /*
   * Backward range iterator for rangelists
   *
   */

  forceinline
  IntVarImpBwd::IntVarImpBwd(void) {}
  forceinline
  IntVarImpBwd::IntVarImpBwd(const IntVarImp* x)
    : n(NULL), c(x->ranges_bwd()) {}
  forceinline void
  IntVarImpBwd::init(const IntVarImp* x) {
    n=NULL; c=x->ranges_bwd();
  }

  forceinline bool
  IntVarImpBwd::operator ()(void) const {
    return c != NULL;
  }
  forceinline void
  IntVarImpBwd::operator ++(void) {
    const IntVarImp::RangeList* p=c->prev(n); n=c; c=p;
  }

  forceinline int
  IntVarImpBwd::min(void) const {
    return c->min();
  }
  forceinline int
  IntVarImpBwd::max(void) const {
    return c->max();
  }
  forceinline unsigned int
  IntVarImpBwd::width(void) const {
    return c->width();
  }


  /*
   * Iterator-based domain operations
   *
   */
  template<class I>
  forceinline ModEvent
  IntVarImp::narrow_r(Space& home, I& ri, bool depends) {
    // Is new domain empty?
    if (!ri())
      return ME_INT_FAILED;

    int min0 = ri.min();
    int max0 = ri.max();
    ++ri;

    ModEvent me;

    // Is new domain range?
    if (!ri()) {
      // Remove possible rangelist (if it was not a range, the domain
      // must have been narrowed!)
      if (fst()) {
        fst()->dispose(home,NULL,lst());
        fst(NULL); holes = 0;
      }
      const int min1 = dom.min(); dom.min(min0);
      const int max1 = dom.max(); dom.max(max0);
      if ((min0 == min1) && (max0 == max1))
        return ME_INT_NONE;
      me = (min0 == max0) ? ME_INT_VAL : ME_INT_BND;
      goto notify;
    }

    if (depends || range()) {
      // Construct new rangelist
      RangeList*   f = new (home) RangeList(min0,max0,NULL,NULL);
      RangeList*   l = f;
      unsigned int s = static_cast<unsigned int>(max0-min0+1);
      do {
        RangeList* n = new (home) RangeList(ri.min(),ri.max(),l,NULL);
        l->next(NULL,n);
        l = n;
        s += ri.width();
        ++ri;
      } while (ri());
      if (fst() != NULL)
        fst()->dispose(home,NULL,lst());
      fst(f); lst(l);

      // Check for modification
      if (size() == s)
        return ME_INT_NONE;

      const int min1 = dom.min(); min0 = f->min(); dom.min(min0);
      const int max1 = dom.max(); max0 = l->max(); dom.max(max0);
      holes = width() - s;

      me = ((min0 == min1) && (max0 == max1)) ? ME_INT_DOM : ME_INT_BND;
      goto notify;
    } else {
      // Set up two sentinel elements
      RangeList f, l;
      // Put all ranges between sentinels
      f.prevnext(NULL,fst()); l.prevnext(lst(),NULL);
      fst()->prev(NULL,&f);   lst()->next(NULL,&l);

      // Number of values removed (potential holes)
      unsigned int h = 0;
      // The previous range
      RangeList* p = &f;
      // The current range
      RangeList* r = f.next(NULL);

      while (true) {
        assert((r != &f) && (r != &l));
        if (r->max() < min0) {
          // Entire range removed
          h += r->width();
          RangeList* n=r->next(p);
          p->next(r,n); n->prev(r,p);
          r->dispose(home);
          r=n;
          if (r == &l)
            goto done;
        } else if ((r->min() == min0) && (r->max() == max0)) {
          // Range unchanged
          RangeList* n=r->next(p); p=r; r=n;
          if (r == &l)
            goto done;
          if (!ri())
            goto done;
          min0=ri.min(); max0=ri.max(); ++ri;
        } else {
          // Range might have been split into many small ranges
          assert((r->min() <= min0) && (max0 <= r->max()));
          h += r->width();
          int end = r->max();
          // Copy first range
          r->min(min0); r->max(max0);
          assert(h > r->width());
          h -= r->width();
          {
            RangeList* n=r->next(p); p=r; r=n;
          }
          while (true) {
            if (!ri())
              goto done;
            min0=ri.min(); max0=ri.max(); ++ri;
            if (max0 > end)
              break;
            assert(h > static_cast<unsigned int>(max0-min0+1));
            h -= max0-min0+1;
            RangeList* n = new (home) RangeList(min0,max0,p,r);
            p->next(r,n); r->prev(p,n);
            p=n;
          }
          if (r == &l)
            goto done;
        }
      }
    done:

      // Remove remaining ranges
      while (r != &l) {
        h += r->width();
        RangeList* n=r->next(p);
        p->next(r,n); n->prev(r,p);
        r->dispose(home);
        r=n;
      }

      assert((r == &l) && !ri());

      // New first and last ranges
      RangeList* fn = f.next(NULL);
      RangeList* ln = l.prev(NULL);

      // All ranges pruned?
      assert(fn != &l);

      // Only a single range left?
      assert(fn != ln);

      // The number of removed values
      holes += h;
      // Unlink sentinel ranges
      fn->prev(&f,NULL); ln->next(&l,NULL);
      // How many values where removed at the bounds
      unsigned int b = (static_cast<unsigned int>(fn->min()-dom.min()) +
                        static_cast<unsigned int>(dom.max()-ln->max()));
      // Set new first and last ranges
      fst(fn); lst(ln);

      if (b > 0) {
        assert((dom.min() != fn->min()) || (dom.max() != ln->max()));
        dom.min(fn->min()); dom.max(ln->max());
        holes -= b;
        me = ME_INT_BND; goto notify;
      }

      if (h > 0) {
        assert((dom.min() == fn->min()) && (dom.max() == ln->max()));
        me = ME_INT_DOM; goto notify;
      }
      return ME_INT_NONE;
    }
  notify:
    IntDelta d;
    return notify(home,me,d);
  }

  template<class I>
  forceinline ModEvent
  IntVarImp::inter_r(Space& home, I& i, bool) {
    IntVarImpFwd j(this);
    Iter::Ranges::Inter<I,IntVarImpFwd> ij(i,j);
    return narrow_r(home,ij,true);
  }

  template<class I>
  forceinline ModEvent
  IntVarImp::minus_r(Space& home, I& i, bool depends) {
    if (depends) {
      IntVarImpFwd j(this);
      Iter::Ranges::Diff<IntVarImpFwd,I> ij(j,i);
      return narrow_r(home,ij,true);
    }

    // Skip all ranges that are too small
    while (i() && (i.max() < dom.min()))
      ++i;

    // Is there no range left or all are too large?
    if (!i() || (i.min() > dom.max()))
      return ME_INT_NONE;

    int i_min = i.min();
    int i_max = i.max();
    ++i;

    if ((i_min <= dom.min()) && (i_max >= dom.max()))
      return ME_INT_FAILED;

    if ((i_min > dom.min()) && (i_max >= dom.max()))
      return lq(home,i_min-1);
    
    if ((i_min <= dom.min()) && (i_max < dom.max()) &&
        (!i() || (i.min() > dom.max())))
      return gq(home,i_max+1);

    // Set up two sentinel elements
    RangeList f, l;
    // Put all ranges between sentinels
    if (range()) {
      // Create a new rangelist just for simplicity
      RangeList* n = new (home) RangeList(min(),max(),&f,&l);
      f.prevnext(NULL,n); l.prevnext(n,NULL);
    } else {
      // Link the two sentinel elements
      f.prevnext(NULL,fst()); l.prevnext(lst(),NULL);
      fst()->prev(NULL,&f);   lst()->next(NULL,&l);
    }

    // Number of values removed (potential holes)
    unsigned int h = 0;
    // The previous range
    RangeList* p = &f;
    // The current range
    RangeList* r = f.next(NULL);

    while (true) {
      assert((r != &f) && (r != &l));
      if (i_min > r->max()) {
        RangeList* n=r->next(p); p=r; r=n;
        if (r == &l)
          break;
      } else if (i_max < r->min()) {
        if (!i())
          break;
        i_min = i.min();
        i_max = i.max();
        ++i;
      } else if ((i_min <= r->min()) && (r->max() <= i_max)) {
        // r is included in i: remove entire range r
        h += r->width();
        RangeList* n=r->next(p);
        p->next(r,n); n->prev(r,p);
        r->dispose(home);
        r=n;
        if (r == &l)
          break;
      } else if ((i_min > r->min()) && (i_max < r->max())) {
        // i is included in r: create new range before the current one
        h += static_cast<unsigned int>(i_max - i_min) + 1;
        RangeList* n = new (home) RangeList(r->min(),i_min-1,p,r);
        r->min(i_max+1);
        p->next(r,n); r->prev(p,n);
        p=n;
        if (!i())
          break;
        i_min = i.min();
        i_max = i.max();
        ++i;
      } else if (i_max < r->max()) {
        assert(i_min <= r->min());
        // i ends before r: adjust minimum of r
        h += i_max-r->min()+1;
        r->min(i_max+1);
        if (!i())
          break;
        i_min = i.min();
        i_max = i.max();
        ++i;
      } else {
        assert((i_max >= r->max()) && (r->min() < i_min));
        // r ends before i: adjust maximum of r
        h += r->max()-i_min+1;
        r->max(i_min-1);
        RangeList* n=r->next(p); p=r; r=n;
        if (r == &l)
          break;
      }
    }

    // New first and last ranges
    RangeList* fn = f.next(NULL);
    RangeList* ln = l.prev(NULL);

    // All ranges pruned?
    if (fn == &l) {
      fst(NULL); lst(NULL); holes=0;
      return ME_INT_FAILED;
    }

    ModEvent me;
    unsigned int b;

    // Only a single range left?
    if (fn == ln) {
      assert(h > 0);
      dom.min(fn->min()); dom.max(fn->max());
      fn->dispose(home);
      fst(NULL); lst(NULL);
      holes = 0;
      me = assigned() ? ME_INT_VAL : ME_INT_BND;
      goto notify;
    }

    // The number of removed values
    holes += h;
    // Unlink sentinel ranges
    fn->prev(&f,NULL); ln->next(&l,NULL);
    // How many values where removed at the bounds
    b = (static_cast<unsigned int>(fn->min()-dom.min()) +
         static_cast<unsigned int>(dom.max()-ln->max()));
    // Set new first and last ranges
    fst(fn); lst(ln);

    if (b > 0) {
      assert((dom.min() != fn->min()) || (dom.max() != ln->max()));
      dom.min(fn->min()); dom.max(ln->max());
      holes -= b;
      me = ME_INT_BND; goto notify;
    }

    if (h > 0) {
      assert((dom.min() == fn->min()) && (dom.max() == ln->max()));
      me = ME_INT_DOM; goto notify;
    }

    return ME_INT_NONE;
  notify:
    IntDelta d;
    return notify(home,me,d);
  }

  template<class I>
  forceinline ModEvent
  IntVarImp::narrow_v(Space& home, I& i, bool depends) {
    Iter::Values::ToRanges<I> r(i);
    return narrow_r(home,r,depends);
  }

  template<class I>
  forceinline ModEvent
  IntVarImp::inter_v(Space& home, I& i, bool depends) {
    Iter::Values::ToRanges<I> r(i);
    return inter_r(home,r,depends);
  }

  template<class I>
  forceinline ModEvent
  IntVarImp::minus_v(Space& home, I& i, bool depends) {
    if (depends) {
      Iter::Values::ToRanges<I> r(i);
      return minus_r(home, r, true);
    }

    // Skip all values that are too small
    while (i() && (i.val() < dom.min()))
      ++i;

    // Is there no value left or all are too large?
    if (!i() || (i.val() > dom.max()))
      return ME_INT_NONE;

    int v = i.val();
    // Skip values that are the same
    do {
      ++i;
    } while (i() && (i.val() == v));

    // Is there only a single value to be pruned?
    if (!i() || (i.val() > dom.max()))
      return nq_full(home,v);

    // Set up two sentinel elements
    RangeList f, l;
    // Put all ranges between sentinels
    if (range()) {
      // Create a new rangelist just for simplicity
      RangeList* n = new (home) RangeList(min(),max(),&f,&l);
      f.prevnext(NULL,n); l.prevnext(n,NULL);
    } else {
      // Link the two sentinel elements
      f.prevnext(NULL,fst()); l.prevnext(lst(),NULL);
      fst()->prev(NULL,&f);   lst()->next(NULL,&l);
    }

    // Number of values removed (potential holes)
    unsigned int h = 0;
    // The previous range
    RangeList* p = &f;
    // The current range
    RangeList* r = f.next(NULL);

    while (true) {
      assert((r != &f) && (r != &l));
      if (v > r->max()) {
        // Move to next range
        RangeList* n=r->next(p); p=r; r=n;
        if (r == &l)
          break;
      } else {
        if ((v == r->min()) && (v == r->max())) {
          // Remove range
          h++;
          RangeList* n=r->next(p);
          p->next(r,n); n->prev(r,p);
          r->dispose(home);
          r=n;
          if (r == &l)
            break;
        } else if (v == r->min()) {
          h++; r->min(v+1);
        } else if (v == r->max()) {
          h++; r->max(v-1);
          RangeList* n=r->next(p); p=r; r=n;
          if (r == &l)
            break;
        } else if (v > r->min()) {
          // Create new range before the current one
          assert(v < r->max());
          h++;
          RangeList* n = new (home) RangeList(r->min(),v-1,p,r);
          r->min(v+1);
          p->next(r,n); r->prev(p,n);
          p=n;
        }
        if (!i())
          break;
        // Move to next value
        v = i.val(); ++i;
      }
    }
    assert((r == &l) || !i());

    // New first and last ranges
    RangeList* fn = f.next(NULL);
    RangeList* ln = l.prev(NULL);

    // All ranges pruned?
    if (fn == &l) {
      fst(NULL); lst(NULL); holes=0;
      return ME_INT_FAILED;
    }

    IntDelta d;

    // Only a single range left?
    if (fn == ln) {
      assert(h > 0);
      dom.min(fn->min()); dom.max(fn->max());
      fn->dispose(home);
      fst(NULL); lst(NULL);
      holes = 0;
      if (assigned())
        return notify(home,ME_INT_VAL,d);
      else
        return notify(home,ME_INT_BND,d);
    }

    // The number of removed values
    holes += h;
    // Unlink sentinel ranges
    fn->prev(&f,NULL); ln->next(&l,NULL);
    // How many values where removed at the bounds
    unsigned int b = (static_cast<unsigned int>(fn->min()-dom.min()) +
                      static_cast<unsigned int>(dom.max()-ln->max()));
    // Set new first and last ranges
    fst(fn); lst(ln);

    if (b > 0) {
      assert((dom.min() != fn->min()) || (dom.max() != ln->max()));
      dom.min(fn->min()); dom.max(ln->max());
      holes -= b;
      return notify(home,ME_INT_BND,d);
    }

    if (h > 0) {
      assert((dom.min() == fn->min()) && (dom.max() == ln->max()));
      return notify(home,ME_INT_DOM,d);
    }

    return ME_INT_NONE;
  }


  /*
   * Copying a variable
   *
   */

  forceinline IntVarImp*
  IntVarImp::copy(Space& home, bool share) {
    return copied() ? static_cast<IntVarImp*>(forward())
      : perform_copy(home,share);
  }


  /*
   * Dependencies
   *
   */
  forceinline void
  IntVarImp::subscribe(Space& home, Propagator& p, PropCond pc, bool schedule) {
    IntVarImpBase::subscribe(home,p,pc,dom.min()==dom.max(),schedule);
  }
  forceinline void
  IntVarImp::cancel(Space& home, Propagator& p, PropCond pc) {
    IntVarImpBase::cancel(home,p,pc,dom.min()==dom.max());
  }

  forceinline void
  IntVarImp::subscribe(Space& home, Advisor& a) {
    IntVarImpBase::subscribe(home,a,dom.min()==dom.max());
  }
  forceinline void
  IntVarImp::cancel(Space& home, Advisor& a) {
    IntVarImpBase::cancel(home,a,dom.min()==dom.max());
  }

  forceinline ModEventDelta
  IntVarImp::med(ModEvent me) {
    return IntVarImpBase::med(me);
  }

}}

// STATISTICS: int-var
