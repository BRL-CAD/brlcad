/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Patrick Pekczynski <pekczynski@ps.uni-sb.de>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Patrick Pekczynski, 2005
 *     Christian Schulte, 2009
 *     Guido Tack, 2009
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
 */

namespace Gecode { namespace Int { namespace GCC {

  template<class Card>
  forceinline
  Val<Card>::Val(Home home, 
                 ViewArray<IntView>& x0, ViewArray<Card>& k0)
    : Propagator(home), x(x0), k(k0){
    x.subscribe(home, *this, PC_INT_VAL);
    k.subscribe(home, *this, PC_INT_VAL);
  }

  template<class Card>
  forceinline
  Val<Card>::Val(Space& home, bool share, Val<Card>& p)
    : Propagator(home,share,p) {
    x.update(home,share, p.x);
    k.update(home,share, p.k);
  }

  template<class Card>
  forceinline size_t
  Val<Card>::dispose(Space& home) {
    x.cancel(home,*this, PC_INT_VAL);
    k.cancel(home,*this, PC_INT_VAL);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class Card>
  Actor*
  Val<Card>::copy(Space& home, bool share) {
    return new (home) Val<Card>(home,share,*this);
  }

  template<class Card>
  PropCost
  Val<Card>::cost(const Space&, const ModEventDelta&) const {
    /*
     * Complexity depends on the time needed for value lookup in \a k
     * which is O(n log n).
     */
    return PropCost::linear(PropCost::HI,x.size());
  }

  template<class Card>
  ExecStatus
  prop_val(Space& home, Propagator& p, 
           ViewArray<IntView>& x, ViewArray<Card>& k) {
    assert(x.size() > 0);
    
    Region r(home);
    // count[i] denotes how often value k[i].card() occurs in x
    int* count = r.alloc<int>(k.size());

    // initialization
    int sum_min = 0;
    int removed = 0;
    for (int i = k.size(); i--; ) {
      removed += k[i].counter();
      sum_min += k[i].min();
      count[i] = 0;
    }

    // less than or equal than the total number of free variables
    // to satisfy the required occurences
    for (int i = k.size(); i--; )
      GECODE_ME_CHECK(k[i].lq(home, x.size()+removed-(sum_min - k[i].min())));

    // number of unassigned views
    int non = x.size();

    for (int i = x.size(); i--; )
      if (x[i].assigned()) {
        int idx;
        if (!lookupValue(k,x[i].val(),idx)) {
          return ES_FAILED;
        }
        count[idx]++;
        non--;
      }

    // check for subsumption
    if (non == 0) {
      for (int i = k.size(); i--; )
        GECODE_ME_CHECK((k[i].eq(home, count[i] + k[i].counter())));
      return home.ES_SUBSUMED(p);
    }

    // total number of unsatisfied miminum occurences
    int req = 0;
    // number of values whose min requirements are not yet met
    int n_r = 0;
    // if only one value is unsatisified single holds the index of that value
    int single = -1;
    // total number of assigned views wrt. the original probem size
    int t_noa = 0;

    for (int i = k.size(); i--; ) {
      int ci = count[i] + k[i].counter();
      t_noa += ci;
      if (ci == 0) { // this works
        req += k[i].min();
        n_r++;
        single = i;
      }

      // number of unassigned views cannot satisfy
      // the required minimum occurence
      if (req > non) {
        return ES_FAILED;
      }
    }

    // if only one unsatisfied occurences is left
    if ((req == non) && (n_r == 1)) {
      // This works as the x are not shared!
      for (int i = x.size(); i--; ) {
        // try to assign it
        if (!x[i].assigned()) {
          GECODE_ME_CHECK(x[i].eq(home, k[single].card()));
          assert((single >= 0) && (single < k.size()));
          count[single]++;
        }
      }
      assert((single >= 0) && (single < k.size()));

      for (int i = k.size(); i--; )
        GECODE_ME_CHECK(k[i].eq(home, count[i] + k[i].counter()));
      return home.ES_SUBSUMED(p);
    }

    // Bitset for indexes that can be removed
    Support::BitSet<Region> rem(r,static_cast<unsigned int>(k.size()));

    for (int i = k.size(); i--; ) {
      int ci = count[i] + k[i].counter();
      if (ci == k[i].max()) {
        assert(!rem.get(i));
        rem.set(static_cast<unsigned int>(i));
        k[i].counter(ci);
        // the solution contains ci occurences of value k[i].card();
        GECODE_ME_CHECK(k[i].eq(home, ci));
      } else {
        if (ci > k[i].max()) {
          return ES_FAILED;
        }
        
        // in case of variable cardinalities
        if (Card::propagate) {
          GECODE_ME_CHECK(k[i].gq(home, ci));
          int occupied = t_noa - ci;
          GECODE_ME_CHECK(k[i].lq(home, x.size() + removed - occupied));
        }
      }
      // reset counter
      count[i] = 0;
    }

    // reduce the problem size
    {
      int n_x = x.size();
      for (int i = n_x; i--; ) {
        if (x[i].assigned()) {
          int idx;
          if (!lookupValue(k,x[i].val(),idx)) {
            return ES_FAILED;
          }
          if (rem.get(static_cast<unsigned int>(idx)))
            x[i]=x[--n_x];
        }
      }
      x.size(n_x);
    }

    // remove values
    {
      // Values to prune
      int* p = r.alloc<int>(k.size());
      // Number of values to prune
      int n_p = 0;
      for (Iter::Values::BitSet<Support::BitSet<Region> > i(rem); i(); ++i)
        p[n_p++] = k[i.val()].card();
      Support::quicksort(p,n_p);
      for (int i = x.size(); i--;) {
        Iter::Values::Array pi(p,n_p);
        GECODE_ME_CHECK(x[i].minus_v(home, pi, false));
      }
    }

    {
      bool all_assigned = true;

      for (int i = x.size(); i--; ) {
        if (x[i].assigned()) {
          int idx;
          if (!lookupValue(k,x[i].val(),idx)) {
            return ES_FAILED;
          }
          count[idx]++;
        } else {
          all_assigned = false;
        }
      }
      
      if (all_assigned) {
        for (int i = k.size(); i--; )
          GECODE_ME_CHECK((k[i].eq(home, count[i] + k[i].counter())));
        return home.ES_SUBSUMED(p);
      }
    }

    if (Card::propagate) {
      // check again consistency of cardinalities
      int reqmin = 0;
      int allmax = 0;
      for (int i = k.size(); i--; ) {
        if (k[i].counter() > k[i].max()) {
          return ES_FAILED;
        }
        allmax += k[i].max() - k[i].counter();
        if (k[i].counter() < k[i].min())
          reqmin += k[i].min() - k[i].counter();
        GECODE_ME_CHECK((k[i].lq(home, x.size()+k[i].counter())));
      }
    
      if ((x.size() < reqmin) || (allmax < x.size())) {
        return ES_FAILED;
      }
    }

    return ES_NOFIX;
  }

  template<class Card>
  ExecStatus
  Val<Card>::propagate(Space& home, const ModEventDelta&) {
    return prop_val<Card>(home, *this, x, k);
  }

  template<class Card>
  ExecStatus
  Val<Card>::post(Home home,
                  ViewArray<IntView>& x, ViewArray<Card>& k) {
    GECODE_ES_CHECK((postSideConstraints<Card>(home,x,k)));

    if (isDistinct<Card>(home,x,k))
      return Distinct::Val<IntView>::post(home,x);
   
    (void) new (home) Val<Card>(home,x,k);
    return ES_OK;
  }


}}}

// STATISTICS: int-prop

