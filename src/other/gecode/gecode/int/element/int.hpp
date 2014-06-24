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

namespace Gecode { namespace Int { namespace Element {


  // Index value pairs
  template<class V0, class V1, class Idx, class Val>
  forceinline void
  Int<V0,V1,Idx,Val>::IdxVal::mark(void) {
    idx = -1;
  }
  template<class V0, class V1, class Idx, class Val>
  forceinline bool
  Int<V0,V1,Idx,Val>::IdxVal::marked(void) const {
    return idx<0;
  }


  // Index iterator
  template<class V0, class V1, class Idx, class Val>
  forceinline
  Int<V0,V1,Idx,Val>::IterIdxUnmark::IterIdxUnmark(IdxVal* iv0)
    : iv(iv0) {
    Idx p=0;
    i = iv[p].idx_next;
    while ((i != 0) && iv[i].marked())
      i=iv[i].idx_next;
    iv[p].idx_next=i;
  }
  template<class V0, class V1, class Idx, class Val>
  forceinline bool
  Int<V0,V1,Idx,Val>::IterIdxUnmark::operator ()(void) const {
    return i != 0;
  }
  template<class V0, class V1, class Idx, class Val>
  forceinline void
  Int<V0,V1,Idx,Val>::IterIdxUnmark::operator ++(void) {
    int p=i;
    i = iv[p].idx_next;
    while ((i != 0) && iv[i].marked())
      i=iv[i].idx_next;
    iv[p].idx_next=i;
  }
  template<class V0, class V1, class Idx, class Val>
  forceinline Idx
  Int<V0,V1,Idx,Val>::IterIdxUnmark::val(void) const {
    assert(!iv[i].marked());
    return iv[i].idx;
  }



  template<class V0, class V1, class Idx, class Val>
  forceinline
  Int<V0,V1,Idx,Val>::IterVal::IterVal(IdxVal* iv0)
    : iv(iv0), i(iv[0].val_next) {}
  template<class V0, class V1, class Idx, class Val>
  forceinline bool
  Int<V0,V1,Idx,Val>::IterVal::operator ()(void) const {
    return i != 0;
  }
  template<class V0, class V1, class Idx, class Val>
  forceinline void
  Int<V0,V1,Idx,Val>::IterVal::operator ++(void) {
    i=iv[i].val_next;
  }
  template<class V0, class V1, class Idx, class Val>
  forceinline Val
  Int<V0,V1,Idx,Val>::IterVal::val(void) const {
    assert(!iv[i].marked());
    return iv[i].val;
  }



  template<class V0, class V1, class Idx, class Val>
  forceinline
  Int<V0,V1,Idx,Val>::IterValUnmark::IterValUnmark(IdxVal* iv0)
    : iv(iv0) {
    Idx p=0;
    i = iv[p].val_next;
    while ((i != 0) && iv[i].marked())
      i=iv[i].val_next;
    iv[p].val_next=i;
  }
  template<class V0, class V1, class Idx, class Val>
  forceinline bool
  Int<V0,V1,Idx,Val>::IterValUnmark::operator ()(void) const {
    return i != 0;
  }
  template<class V0, class V1, class Idx, class Val>
  forceinline void
  Int<V0,V1,Idx,Val>::IterValUnmark::operator ++(void) {
    int p=i;
    i = iv[p].val_next;
    while ((i != 0) && iv[i].marked())
      i=iv[i].val_next;
    iv[p].val_next=i;
  }
  template<class V0, class V1, class Idx, class Val>
  forceinline Val
  Int<V0,V1,Idx,Val>::IterValUnmark::val(void) const {
    assert(!iv[i].marked());
    return iv[i].val;
  }



  // Sort function
  template<class V0, class V1, class Idx, class Val>
  forceinline
  Int<V0,V1,Idx,Val>::ByVal::ByVal(const IdxVal* iv0)
    : iv(iv0) {}
  template<class V0, class V1, class Idx, class Val>
  forceinline bool
  Int<V0,V1,Idx,Val>::ByVal::operator ()(Idx& i, Idx& j) {
    return iv[i].val < iv[j].val;
  }


  /*
   * Element propagator proper
   *
   */
  template<class V0, class V1, class Idx, class Val>
  forceinline
  Int<V0,V1,Idx,Val>::Int(Home home, IntSharedArray& c0, V0 y0, V1 y1)
    : Propagator(home), x0(y0), s0(0), x1(y1), s1(0), c(c0), iv(NULL) {
    home.notice(*this,AP_DISPOSE);
    x0.subscribe(home,*this,PC_INT_DOM);
    x1.subscribe(home,*this,PC_INT_DOM);
  }

  template<class V0, class V1, class Idx, class Val>
  forceinline size_t
  Int<V0,V1,Idx,Val>::dispose(Space& home) {
    home.ignore(*this,AP_DISPOSE);
    x0.cancel(home,*this,PC_INT_DOM);
    x1.cancel(home,*this,PC_INT_DOM);
    c.~IntSharedArray();
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class V0, class V1, class Idx, class Val>
  ExecStatus
  Int<V0,V1,Idx,Val>::post(Home home, IntSharedArray& c, V0 x0, V1 x1) {
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.eq(home,c[x0.val()]));
    } else if (x1.assigned()) {
      GECODE_ES_CHECK(assigned_val(home,c,x0,x1));
    } else {
      (void) new (home) Int<V0,V1,Idx,Val>(home,c,x0,x1);
    }
    return ES_OK;
  }

  template<class V0, class V1, class Idx, class Val>
  forceinline
  Int<V0,V1,Idx,Val>::Int(Space& home, bool share, Int& p)
    : Propagator(home,share,p), s0(0), s1(0), iv(NULL) {
    c.update(home,share,p.c);
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
  }

  template<class V0, class V1, class Idx, class Val>
  Actor*
  Int<V0,V1,Idx,Val>::copy(Space& home, bool share) {
    return new (home) Int<V0,V1,Idx,Val>(home,share,*this);
  }

  template<class V0, class V1, class Idx, class Val>
  PropCost
  Int<V0,V1,Idx,Val>::cost(const Space&, const ModEventDelta& med) const {
    if ((V0::me(med) == ME_INT_VAL) ||
        (V1::me(med) == ME_INT_VAL))
      return PropCost::unary(PropCost::LO);
    else
      return PropCost::binary(PropCost::HI);
  }

  template<class V0, class V1, class Idx, class Val>
  void
  Int<V0,V1,Idx,Val>::prune_idx(void) {
    Idx p = 0;
    Idx i = iv[p].idx_next;
    ViewRanges<V0> v(x0);
    while (v() && (i != 0)) {
      assert(!iv[i].marked());
      if (iv[i].idx < v.min()) {
        iv[i].mark(); i=iv[i].idx_next; iv[p].idx_next=i;
      } else if (iv[i].idx > v.max()) {
        ++v;
      } else {
        p=i; i=iv[i].idx_next;
      }
    }
    iv[p].idx_next = 0;
    while (i != 0) { 
      iv[i].mark(); i=iv[i].idx_next; 
    }
  }

  template<class V0, class V1, class Idx, class Val>
  void
  Int<V0,V1,Idx,Val>::prune_val(void) {
    Idx p = 0;
    Idx i = iv[p].val_next;
    ViewRanges<V1> v(x1);
    while (v() && (i != 0)) {
      if (iv[i].marked()) {
        i=iv[i].val_next; iv[p].val_next=i;
      } else if (iv[i].val < v.min()) {
        iv[i].mark(); i=iv[i].val_next; iv[p].val_next=i;
      } else if (iv[i].val > v.max()) {
        ++v;
      } else {
        p=i; i=iv[i].val_next;
      }
    }
    iv[p].val_next = 0;
    while (i != 0) { 
      iv[i].mark(); i=iv[i].val_next; 
    }
  }

  template<class V0, class V1, class Idx, class Val>
  ExecStatus
  Int<V0,V1,Idx,Val>::assigned_val(Space& home, IntSharedArray& c, 
                                   V0 x0, V1 x1) {
    Region r(home);
    int* v = r.alloc<int>(x0.size());
    int n = 0;
    for (ViewValues<V0> i(x0); i(); ++i)
      if (c[i.val()] != x1.val())
        v[n++]=i.val();
    Iter::Values::Array iv(v,n);
    GECODE_ME_CHECK(x0.minus_v(home,iv,false));
    return ES_OK;
  }

  template<class V0, class V1, class Idx, class Val>
  ExecStatus
  Int<V0,V1,Idx,Val>::propagate(Space& home, const ModEventDelta&) {
    if (x0.assigned()) {
      GECODE_ME_CHECK(x1.eq(home,c[x0.val()]));
      return home.ES_SUBSUMED(*this);
    }

    if (x1.assigned() && (iv == NULL)) {
      GECODE_ES_CHECK(assigned_val(home,c,x0,x1));
      return home.ES_SUBSUMED(*this);
    }

    if ((static_cast<ValSize>(x1.size()) == s1) &&
        (static_cast<IdxSize>(x0.size()) != s0)) {
      assert(iv != NULL);
      assert(!shared(x0,x1));

      prune_idx();

      IterValUnmark v(iv);
      GECODE_ME_CHECK(x1.narrow_v(home,v,false));
      
      s1=static_cast<ValSize>(x1.size());
      
      assert(!x0.assigned());
      return x1.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
    }

    if ((static_cast<IdxSize>(x0.size()) == s0) &&
        (static_cast<ValSize>(x1.size()) != s1)) {
      assert(iv != NULL);
      assert(!shared(x0,x1));

      prune_val();

      IterIdxUnmark i(iv);
      GECODE_ME_CHECK(x0.narrow_v(home,i,false));
      
      s0=static_cast<IdxSize>(x0.size()); 
      
      return (x0.assigned() || x1.assigned()) ?
          home.ES_SUBSUMED(*this) : ES_FIX;
    }

    bool assigned = x0.assigned() && x1.assigned();
    if (iv == NULL) {
      // Initialize data structure
      iv = home.alloc<IdxVal>(x0.size() + 1);
      
      // The first element in iv[0] is used as sentinel
      // Enter information sorted by idx
      IdxVal* by_idx = &iv[1];
      Idx size = 0;
      for (ViewValues<V0> v(x0); v(); ++v)
        if ((x1.min() <= c[v.val()]) && (x1.max() >= c[v.val()])) {
          by_idx[size].idx = static_cast<Idx>(v.val());
          by_idx[size].val = static_cast<Val>(c[v.val()]);
          size++;
        }
      if (size == 0)
        return ES_FAILED;
      // Create val links sorted by val
      Region r(home);
      Idx* by_val = r.alloc<Idx>(size);
      if (x1.width() <= 128) {
        int n_buckets = static_cast<int>(x1.width());
        int* pos = r.alloc<int>(n_buckets);
        int* buckets = pos - x1.min(); 
        for (int i=n_buckets; i--; )
          pos[i]=0;
        for (Idx i=size; i--; )
          buckets[by_idx[i].val]++;
        int p=0;
        for (int i=0; i<n_buckets; i++) {
          int n=pos[i]; pos[i]=p; p+=n;
        }
        assert(p == size);
        for (Idx i=size; i--; )
          by_val[buckets[by_idx[i].val]++] = i+1;
      } else {
        for (Idx i = size; i--; )
          by_val[i] = i+1;
        ByVal less(iv);
        Support::quicksort<Idx>(by_val,size,less);
      }
      // Create val links
      for (Idx i = size-1; i--; ) {
        by_idx[i].idx_next = i+2;
        iv[by_val[i]].val_next = by_val[i+1];
      }
      by_idx[size-1].idx_next = 0;
      iv[by_val[size-1]].val_next = 0;
      // Set up sentinel element: iv[0]
      iv[0].idx_next = 1;
      iv[0].val_next = by_val[0];
    } else {
      prune_idx();
    }
      
    // Prune values
    prune_val();
    
    // Peform tell
    {
      IterIdxUnmark i(iv);
      GECODE_ME_CHECK(x0.narrow_v(home,i,false));
      IterVal v(iv);
      if (shared(x0,x1)) {
        GECODE_ME_CHECK(x1.inter_v(home,v,false));
        s0=static_cast<IdxSize>(x0.size()); 
        s1=static_cast<ValSize>(x1.size());
        return assigned ? home.ES_SUBSUMED(*this) : ES_NOFIX;
      } else {
        GECODE_ME_CHECK(x1.narrow_v(home,v,false));
        s0=static_cast<IdxSize>(x0.size()); 
        s1=static_cast<ValSize>(x1.size());
        return (x0.assigned() || x1.assigned()) ?
          home.ES_SUBSUMED(*this) : ES_FIX;
      }
    }
  }

  template<class V0, class V1>
  forceinline ExecStatus
  post_int(Home home, IntSharedArray& c, V0 x0, V1 x1) {
    assert(c.size() > 0);
    GECODE_ME_CHECK(x0.gq(home,0));
    GECODE_ME_CHECK(x0.le(home,c.size()));
    Support::IntType idx_type = Support::s_type(c.size());
    int min = c[0];
    int max = c[0];
    for (int i=1; i<c.size(); i++) {
      min = std::min(c[i],min); max = std::max(c[i],max);
    }
    GECODE_ME_CHECK(x1.gq(home,min));
    GECODE_ME_CHECK(x1.lq(home,max));
    Support::IntType val_type = 
      std::max(Support::s_type(min),Support::s_type(max));
    switch (idx_type) {
    case Support::IT_CHAR:
      switch (val_type) {
      case Support::IT_CHAR:
        return Int<V0,V1,signed char,signed char>::post(home,c,x0,x1);
      case Support::IT_SHRT:
        return Int<V0,V1,signed char,signed short int>::post(home,c,x0,x1);
      default: break;
      }
      break;
    case Support::IT_SHRT:
      switch (val_type) {
      case Support::IT_CHAR:
      case Support::IT_SHRT:
        return Int<V0,V1,signed short int,signed short int>::post(home,c,x0,x1);
      default: break;
      }
      break;
    default: break;
    }
    return Int<V0,V1,signed int,signed int>::post(home,c,x0,x1);
  }

}}}

// STATISTICS: int-prop

