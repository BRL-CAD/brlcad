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
 *     Patrick Pekczynski, 2004/2005
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
 *
 */

namespace Gecode { namespace Int { namespace GCC {

  template<class Card>
  forceinline
  Bnd<Card>::
  Bnd(Home home, ViewArray<IntView>& x0, ViewArray<Card>& k0,
         bool cf, bool nolbc) :
    Propagator(home), x(x0), y(home, x0), k(k0),
    card_fixed(cf), skip_lbc(nolbc) {
    y.subscribe(home, *this, PC_INT_BND);
    k.subscribe(home, *this, PC_INT_BND);
  }

  template<class Card>
  forceinline
  Bnd<Card>::
  Bnd(Space& home, bool share, Bnd<Card>& p)
    : Propagator(home, share, p),
      card_fixed(p.card_fixed), skip_lbc(p.skip_lbc) {
    x.update(home, share, p.x);
    y.update(home, share, p.y);
    k.update(home, share, p.k);
  }

  template<class Card>
  forceinline size_t
  Bnd<Card>::dispose(Space& home) {
    y.cancel(home,*this, PC_INT_BND);
    k.cancel(home,*this, PC_INT_BND);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class Card>
  Actor*
  Bnd<Card>::copy(Space& home, bool share) {
    return new (home) Bnd<Card>(home,share,*this);
  }

  template<class Card>
  PropCost
  Bnd<Card>::cost(const Space&,
                            const ModEventDelta& med) const {
    int n_k = Card::propagate ? k.size() : 0;
    if (IntView::me(med) == ME_INT_VAL)
      return PropCost::linear(PropCost::LO, y.size() + n_k);
    else
      return PropCost::quadratic(PropCost::LO, x.size() + n_k);
  }


  template<class Card>
  forceinline ExecStatus
  Bnd<Card>::lbc(Space& home, int& nb,
                           HallInfo hall[], Rank rank[], int mu[], int nu[]) {
    int n = x.size();

    /*
     *  Let I(S) denote the number of variables whose domain intersects
     *  the set S and C(S) the number of variables whose domain is containded
     *  in S. Let further  min_cap(S) be the minimal number of variables
     *  that must be assigned to values, that is
     *  min_cap(S) is the sum over all l[i] for a value v_i that is an
     *  element of S.
     *
     *  A failure set is a set F if
     *       I(F) < min_cap(F)
     *  An unstable set is a set U if
     *       I(U) = min_cap(U)
     *  A stable set is a set S if
     *      C(S) > min_cap(S) and S intersetcs nor
     *      any failure set nor any unstable set
     *      forall unstable and failure sets
     *
     *  failure sets determine the satisfiability of the LBC
     *  unstable sets have to be pruned
     *  stable set do not have to be pruned
     *
     * hall[].ps ~ stores the unstable
     *             sets that have to be pruned
     * hall[].s  ~ stores sets that must not be pruned
     * hall[].h  ~ contains stable and unstable sets
     * hall[].d  ~ contains the difference between interval bounds, i.e.
     *             the minimal capacity of the interval
     * hall[].t  ~ contains the critical capacity pointer, pointing to the
     *             values
     */

    // LBC lower bounds

    int i = 0;
    int j = 0;
    int w = 0;
    int z = 0;
    int v = 0;

    //initialization of the tree structure
    int rightmost = nb + 1; // rightmost accesible value in bounds
    int bsize     = nb + 2;
    w = rightmost;

    // test
    // unused but uninitialized
    hall[0].d = 0;
    hall[0].s = 0;
    hall[0].ps = 0;

    for (i = bsize; --i; ) { // i must not be zero
      int pred = i - 1;
      hall[i].s = pred;
      hall[i].ps = pred;
      hall[i].d = lps.sumup(hall[pred].bounds, hall[i].bounds - 1);

      /* Let [hall[i].bounds,hall[i-1].bounds]=:I
       * If the capacity is zero => min_cap(I) = 0
       * => I cannot be a failure set
       * => I is an unstable set
       */
      if (hall[i].d == 0) {
        hall[pred].h = w;
      } else {
        hall[w].h = pred;
        w = pred;
      }
    }

    w = rightmost;
    for (i = bsize; i--; ) { // i can be zero
      hall[i].t = i - 1;
      if (hall[i].d == 0) {
        hall[i].t = w;
      } else {
        hall[w].t = i;
        w = i;
      }
    }

    /*
     * The algorithm assigns to each value v in bounds
     * empty buckets corresponding to the minimal capacity l[i] to be
     * filled for v. (the buckets correspond to hall[].d containing the
     * difference between the interval bounds) Processing it
     * searches for the smallest value v in dom(x_i) that has an
     * empty bucket, i.e. if all buckets are filled it is guaranteed
     * that there are at least l[i] variables that will be
     * instantiated to v. Since the buckets are initially empty,
     * they are considered as FAILURE SETS
     */

    for (i = 0; i < n; i++) {
      // visit intervals in increasing max order
      int x0 = rank[mu[i]].min;
      int y = rank[mu[i]].max;
      int succ = x0 + 1;
      z = pathmax_t(hall, succ);
      j = hall[z].t;

      /*
       * POTENTIALLY STABLE SET:
       *  z \neq succ \Leftrigharrow z>succ, i.e.
       *  min(D_{\mu(i)}) is guaranteed to occur min(K_i) times
       *  \Rightarrow [x0, min(y,z)] is potentially stable
       */

      if (z != succ) {
        w = pathmax_ps(hall, succ);
        v = hall[w].ps;
        pathset_ps(hall, succ, w, w);
        w = std::min(y, z);
        pathset_ps(hall, hall[w].ps, v, w);
        hall[w].ps = v;
      }

      /*
       * STABLE SET:
       *   being stable implies being potentially stable, i.e.
       *   [hall[y].ps, hall[y].bounds-1] is the largest stable subset of
       *   [hall[j].bounds, hall[y].bounds-1].
       */

      if (hall[z].d <= lps.sumup(hall[y].bounds, hall[z].bounds - 1)) {
        w = pathmax_s(hall, hall[y].ps);
        pathset_s(hall, hall[y].ps, w, w);
        // Path compression
        v = hall[w].s;
        pathset_s(hall, hall[y].s, v, y);
        hall[y].s = v;
      } else {
        /*
         * FAILURE SET:
         * If the considered interval [x0,y] is neither POTENTIALLY STABLE
         * nor STABLE there are still buckets that can be filled,
         * therefore d can be decreased. If d equals zero the intervals
         * minimum capacity is met and thepath can be compressed to the
         * next value having an empty bucket.
         * see DOMINATION in "ubc"
         */
        if (--hall[z].d == 0) {
          hall[z].t = z + 1;
          z = pathmax_t(hall, hall[z].t);
          hall[z].t = j;
        }

        /*
         * FINDING NEW LOWER BOUND:
         * If the lower bound belongs to an unstable or a stable set,
         * remind the new value we might assigned to the lower bound
         * in case the variable doesn't belong to a stable set.
         */
        if (hall[x0].h > x0) {
          hall[i].newBound = pathmax_h(hall, x0);
          w = hall[i].newBound;
          pathset_h(hall, x0, w, w); // path compression
        } else {
          // Do not shrink the variable: take old min as new min
          hall[i].newBound = x0;
        }

        /* UNSTABLE SET
         * If an unstable set is discovered
         * the difference between the interval bounds is equal to the
         * number of variables whose domain intersect the interval
         * (see ZEROTEST in "gcc")
         */
        // CLEARLY THIS WAS NOT STABLE == UNSTABLE
        if (hall[z].d == lps.sumup(hall[y].bounds, hall[z].bounds - 1)) {
          if (hall[y].h > y)
            /*
             * y is not the end of the potentially stable set
             * thus ensure that the potentially stable superset is marked
             */
            y = hall[y].h;
          // Equivalent to pathmax since the path is fully compressed
          pathset_h(hall, hall[y].h, j-1, y);
          // mark the new unstable set [j,y]
          hall[y].h = j-1;
        }
      }
      pathset_t(hall, succ, z, z); // path compression
    }

    /* If there is a FAILURE SET left the minimum occurences of the values
     * are not guaranteed. In order to satisfy the LBC the last value
     * in the stable and unstable datastructure hall[].h must point to
     * the sentinel at the beginning of bounds.
     */
    if (hall[nb].h != 0)
      return ES_FAILED;

    // Perform path compression over all elements in
    // the stable interval data structure. This data
    // structure will no longer be modified and will be
    // accessed n or 2n times. Therefore, we can afford
    // a linear time compression.
    for (i = bsize; --i;)
      if (hall[i].s > i)
        hall[i].s = w;
      else
        w = i;

    /*
     * UPDATING LOWER BOUND:
     * For all variables that are not a subset of a stable set,
     * shrink the lower bound, i.e. forall stable sets S we have:
     * x0 < S_min <= y <=S_max  or S_min <= x0 <= S_max < y
     * that is [x0,y] is NOT a proper subset of any stable set S
     */
    for (i = n; i--; ) {
      int x0 = rank[mu[i]].min;
      int y = rank[mu[i]].max;
      // update only those variables that are not contained in a stable set
      if ((hall[x0].s <= x0) || (y > hall[x0].s)) {
        // still have to check this out, how skipping works (consider dominated indices)
        int m = lps.skipNonNullElementsRight(hall[hall[i].newBound].bounds);
        GECODE_ME_CHECK(x[mu[i]].gq(home, m));
      }
    }

    // LBC narrow upper bounds
    w = 0;
    for (i = 0; i <= nb; i++) {
      hall[i].d = lps.sumup(hall[i].bounds, hall[i + 1].bounds - 1);
      if (hall[i].d == 0) {
        hall[i].t = w;
      } else {
        hall[w].t = i;
        w = i;
      }
    }
    hall[w].t = i;

    w = 0;
    for (i = 1; i <= nb; i++)
      if (hall[i - 1].d == 0) {
        hall[i].h = w;
      } else {
        hall[w].h = i;
        w = i;
      }
    hall[w].h = i;

    for (i = n; i--; ) {
      // visit intervals in decreasing min order
      // i.e. minsorted from right to left
      int x0 = rank[nu[i]].max;
      int y = rank[nu[i]].min;
      int pred = x0 - 1; // predecessor of x0 in the indices
      z = pathmin_t(hall, pred);
      j = hall[z].t;

      /* If the variable is not in a discovered stable set
       * (see above condition for STABLE SET)
       */
      if (hall[z].d > lps.sumup(hall[z].bounds, hall[y].bounds - 1)) {
        // FAILURE SET
        if (--hall[z].d == 0) {
          hall[z].t = z - 1;
          z = pathmin_t(hall, hall[z].t);
          hall[z].t = j;
        }
        // FINDING NEW UPPER BOUND
        if (hall[x0].h < x0) {
          w = pathmin_h(hall, hall[x0].h);
          hall[i].newBound = w;
          pathset_h(hall, x0, w, w); // path compression
        } else {
          hall[i].newBound = x0;
        }
        // UNSTABLE SET
        if (hall[z].d == lps.sumup(hall[z].bounds, hall[y].bounds - 1)) {
          if (hall[y].h < y) {
            y = hall[y].h;
          }
          int succj = j + 1;
          // mark new unstable set [y,j]
          pathset_h(hall, hall[y].h, succj, y);
          hall[y].h = succj;
        }
      }
      pathset_t(hall, pred, z, z);
    }

    // UPDATING UPPER BOUND
    for (i = n; i--; ) {
      int x0 =  rank[nu[i]].min;
      int y  =  rank[nu[i]].max;
      if ((hall[x0].s <= x0) || (y > hall[x0].s)) {
        int m = lps.skipNonNullElementsLeft(hall[hall[i].newBound].bounds - 1);
        GECODE_ME_CHECK(x[nu[i]].lq(home, m));
      }
    }
    return ES_OK;
  }


  template<class Card>
  forceinline ExecStatus
  Bnd<Card>::ubc(Space& home, int& nb,
                           HallInfo hall[], Rank rank[], int mu[], int nu[]) {
    int rightmost = nb + 1; // rightmost accesible value in bounds
    int bsize = nb + 2; // number of unique bounds including sentinels

    //Narrow lower bounds (UBC)

    /*
     * Initializing tree structure with the values from bounds
     * and the interval capacities of neighboured values
     * from left to right
     */


    hall[0].h = 0;
    hall[0].t = 0;
    hall[0].d = 0;

    for (int i = bsize; --i; ) {
      hall[i].h = hall[i].t = i-1;
      hall[i].d = ups.sumup(hall[i-1].bounds, hall[i].bounds - 1);
    }

    int n = x.size();

    for (int i = 0; i < n; i++) {
      // visit intervals in increasing max order
      int x0   = rank[mu[i]].min;
      int succ = x0 + 1;
      int y    = rank[mu[i]].max;
      int z    = pathmax_t(hall, succ);
      int j    = hall[z].t;

      /* DOMINATION:
       *     v^i_j denotes
       *     unused values in the current interval. If the difference d
       *     between to critical capacities v^i_j and v^i_z
       *     is equal to zero, j dominates z
       *
       *     i.e. [hall[l].bounds, hall[nb+1].bounds] is not left-maximal and
       *     [hall[j].bounds, hall[l].bounds] is a Hall set iff
       *     [hall[j].bounds, hall[l].bounds] processing a variable x_i uses up a value in the interval
       *     [hall[z].bounds,hall[z+1].bounds] according to the intervals
       *     capacity. Therefore, if d = 0
       *     the considered value has already been used by processed variables
       *     m-times, where m = u[i] for value v_i. Since this value must not
       *     be reconsidered the path can be compressed
       */
      if (--hall[z].d == 0) {
        hall[z].t = z + 1;
        z = pathmax_t(hall, hall[z].t);
        if (z >= bsize)
          z--;
        hall[z].t = j;
      }
      pathset_t(hall, succ, z, z); // path compression

      /* NEGATIVE CAPACITY:
       *     A negative capacity results in a failure.Since a
       *     negative capacity signals that the number of variables
       *     whose domain is contained in the set S is larger than
       *     the maximum capacity of S => UBC is not satisfiable,
       *     i.e. there are more variables than values to instantiate them
       */
      if (hall[z].d < ups.sumup(hall[y].bounds, hall[z].bounds - 1))
        return ES_FAILED;
      
      /* UPDATING LOWER BOUND:
       *   If the lower bound min_i lies inside a Hall interval [a,b]
       *   i.e. a <= min_i <=b <= max_i
       *   min_i is set to  min_i := b+1
       */
      if (hall[x0].h > x0) {
        int w = pathmax_h(hall, hall[x0].h);
        int m = hall[w].bounds;
        GECODE_ME_CHECK(x[mu[i]].gq(home, m));
        pathset_h(hall, x0, w, w); // path compression
      }

      /* ZEROTEST:
       *     (using the difference between capacity pointers)
       *     zero capacity => the difference between critical capacity
       *     pointers is equal to the maximum capacity of the interval,i.e.
       *     the number of variables whose domain is contained in the
       *     interval is equal to the sum over all u[i] for a value v_i that
       *     lies in the Hall-Intervall which can also be thought of as a
       *     Hall-Set
       *
       *    ZeroTestLemma: Let k and l be succesive critical indices.
       *          v^i_k=0  =>  v^i_k = max_i+1-l+d
       *                   <=> v^i_k = y + 1 - z + d
       *                   <=> d = z-1-y
       *    if this equation holds the interval [j,z-1] is a hall intervall
       */

      if (hall[z].d == ups.sumup(hall[y].bounds, hall[z].bounds - 1)) {
        /*
         *mark hall interval [j,z-1]
         * hall pointers form a path to the upper bound of the interval
         */
        int predj = j - 1;
        pathset_h(hall, hall[y].h, predj, y);
        hall[y].h = predj;
      }
    }

    /* Narrow upper bounds (UBC)
     * symmetric to the narrowing of the lower bounds
     */
    for (int i = 0; i < rightmost; i++) {
      hall[i].h = hall[i].t = i+1;
      hall[i].d = ups.sumup(hall[i].bounds, hall[i+1].bounds - 1);
    }
        
    for (int i = n; i--; ) {
      // visit intervals in decreasing min order
      int x0 = rank[nu[i]].max;
      int pred = x0 - 1;
      int y = rank[nu[i]].min;
      int z = pathmin_t(hall, pred);
      int j = hall[z].t;
    
      // DOMINATION:
      if (--hall[z].d == 0) {
        hall[z].t = z - 1;
        z = pathmin_t(hall, hall[z].t);
        hall[z].t = j;
      }
      pathset_t(hall, pred, z, z);
    
      // NEGATIVE CAPACITY:
      if (hall[z].d < ups.sumup(hall[z].bounds,hall[y].bounds-1))
        return ES_FAILED;
    
      /* UPDATING UPPER BOUND:
       *   If the upper bound max_i lies inside a Hall interval [a,b]
       *   i.e. min_i <= a <= max_i < b
       *   max_i is set to  max_i := a-1
       */
      if (hall[x0].h < x0) {
        int w = pathmin_h(hall, hall[x0].h);
        int m = hall[w].bounds - 1;
        GECODE_ME_CHECK(x[nu[i]].lq(home, m));
        pathset_h(hall, x0, w, w);
      }

      // ZEROTEST
      if (hall[z].d == ups.sumup(hall[z].bounds, hall[y].bounds - 1)) {
        //mark hall interval [y,j]
        pathset_h(hall, hall[y].h, j+1, y);
        hall[y].h = j+1;
      }
    }
    return ES_OK;
  }

  template<class Card>
  ExecStatus
  Bnd<Card>::pruneCards(Space& home) {
    // Remove all values with 0 max occurrence
    // and remove corresponding occurrence variables from k
    
    // The number of zeroes
    int n_z = 0;
    for (int i=k.size(); i--;)
      if (k[i].max() == 0)
        n_z++;

    if (n_z > 0) {
      Region r(home);
      int* z = r.alloc<int>(n_z);
      n_z = 0;
      int n_k = 0;
      for (int i=0; i<k.size(); i++)
        if (k[i].max() == 0) {
          z[n_z++] = k[i].card();            
        } else {
          k[n_k++] = k[i];
        }
      k.size(n_k);
      Support::quicksort(z,n_z);
      for (int i=x.size(); i--;) {
        Iter::Values::Array zi(z,n_z);
        GECODE_ME_CHECK(x[i].minus_v(home,zi,false));
      }
      lps.reinit(); ups.reinit();
    }
    return ES_OK;
  }

  template<class Card>
  ExecStatus
  Bnd<Card>::propagate(Space& home, const ModEventDelta& med) {
    if (IntView::me(med) == ME_INT_VAL) {
      GECODE_ES_CHECK(prop_val<Card>(home,*this,y,k));
      return home.ES_NOFIX_PARTIAL(*this,IntView::med(ME_INT_BND));
    }

    if (Card::propagate)
      GECODE_ES_CHECK(pruneCards(home));

    Region r(home);
    int* count = r.alloc<int>(k.size());

    for (int i = k.size(); i--; )
      count[i] = 0;
    bool all_assigned = true;
    int noa = 0;
    for (int i = x.size(); i--; ) {
      if (x[i].assigned()) {
        noa++;
        int idx;
        // reduction is essential for order on value nodes in dom
        // hence introduce test for failed lookup
        if (!lookupValue(k,x[i].val(),idx))
          return ES_FAILED;
        count[idx]++;
      } else {
        all_assigned = false;
        // We only need the counts in the view case or when all
        // x are assigned
        if (!Card::propagate)
          break;
      }
    }

    if (Card::propagate) {
      // before propagation performs inferences on cardinality variables:
      if (noa > 0)
        for (int i = k.size(); i--; )
          if (!k[i].assigned()) {
            GECODE_ME_CHECK(k[i].lq(home, x.size() - (noa - count[i])));
            GECODE_ME_CHECK(k[i].gq(home, count[i]));
          }

      if (!card_consistent<Card>(x, k))
        return ES_FAILED;

      GECODE_ES_CHECK(prop_card<Card>(home, x, k));

      // Cardinalities may have been modified, so recompute
      // count and all_assigned
      for (int i = k.size(); i--; )
        count[i] = 0;
      all_assigned = true;
      for (int i = x.size(); i--; ) {
        if (x[i].assigned()) {
          int idx;
          // reduction is essential for order on value nodes in dom
          // hence introduce test for failed lookup
          if (!lookupValue(k,x[i].val(),idx))
            return ES_FAILED;
          count[idx]++;
        } else {
          // We won't need the remaining counts, they're only used when
          // all x are assigned
          all_assigned = false;
          break;
        }
      }
    }

    if (all_assigned) {
      for (int i = k.size(); i--; )
        GECODE_ME_CHECK(k[i].eq(home, count[i]));
      return home.ES_SUBSUMED(*this);
    }

    if (Card::propagate)
      GECODE_ES_CHECK(pruneCards(home));

    int n = x.size();

    int* mu = r.alloc<int>(n);
    int* nu = r.alloc<int>(n);

    for (int i = n; i--; )
      nu[i] = mu[i] = i;

    // Create sorting permutation mu according to the variables upper bounds
    MaxInc<IntView> max_inc(x);
    Support::quicksort<int, MaxInc<IntView> >(mu, n, max_inc);

    // Create sorting permutation nu according to the variables lower bounds
    MinInc<IntView> min_inc(x);
    Support::quicksort<int, MinInc<IntView> >(nu, n, min_inc);

    // Sort the cardinality bounds by index
    MinIdx<Card> min_idx;
    Support::quicksort<Card, MinIdx<Card> >(&k[0], k.size(), min_idx);

    if (!lps.initialized()) {
      assert (!ups.initialized());
      lps.init(home, k, false);
      ups.init(home, k, true);
    } else if (Card::propagate) {
      // if there has been a change to the cardinality variables
      // reconstruction of the partial sum structure is necessary
      if (lps.check_update_min(k))
        lps.init(home, k, false);
      if (ups.check_update_max(k))
        ups.init(home, k, true);
    }

    // assert that the minimal value of the partial sum structure for
    // LBC is consistent with the smallest value a variable can take
    assert(lps.minValue() <= x[nu[0]].min());
    // assert that the maximal value of the partial sum structure for
    // UBC is consistent with the largest value a variable can take

    /*
     *  Setup rank and bounds info
     *  Since this implementation is based on the theory of Hall Intervals
     *  additional datastructures are needed in order to represent these
     *  intervals and the "partial-sum" data structure (cf."gcc/bnd-sup.hpp")
     *
     */

    HallInfo* hall = r.alloc<HallInfo>(2 * n + 2);
    Rank* rank = r.alloc<Rank>(n);

    int nb = 0;
    // setup bounds and rank
    int min        = x[nu[0]].min();
    int max        = x[mu[0]].max() + 1;
    int last       = lps.firstValue + 1; //equivalent to last = min -2
    hall[0].bounds = last;

    /*
     * First the algorithm merges the arrays minsorted and maxsorted
     * into bounds i.e. hall[].bounds contains the ordered union
     * of the lower and upper domain bounds including two sentinels
     * at the beginning and at the end of the set
     * ( the upper variable bounds in this union are increased by 1 )
     */

    {
      int i = 0;
      int j = 0;
      while (true) {
        if (i < n && min < max) {
          if (min != last) {
            last = min;
            hall[++nb].bounds = last;
          }
          rank[nu[i]].min = nb;
          if (++i < n)
            min = x[nu[i]].min();
        } else {
          if (max != last) {
            last = max;
            hall[++nb].bounds = last;
          }
          rank[mu[j]].max = nb;
          if (++j == n)
            break;
          max = x[mu[j]].max() + 1;
        }
      }
    }

    int rightmost = nb + 1; // rightmost accesible value in bounds
    hall[rightmost].bounds = ups.lastValue + 1 ;

    if (Card::propagate) {
      skip_lbc = true;
      for (int i = k.size(); i--; )
        if (k[i].min() != 0) {
          skip_lbc = false;
          break;
        }
    }

    if (!card_fixed && !skip_lbc)
      GECODE_ES_CHECK((lbc(home, nb, hall, rank, mu, nu)));

    GECODE_ES_CHECK((ubc(home, nb, hall, rank, mu, nu)));

    if (Card::propagate)
      GECODE_ES_CHECK(prop_card<Card>(home, x, k));

    for (int i = k.size(); i--; )
      count[i] = 0;
    for (int i = x.size(); i--; )
      if (x[i].assigned()) {
        int idx;
        if (!lookupValue(k,x[i].val(),idx))
          return ES_FAILED;
        count[idx]++;
      } else {
        // We won't need the remaining counts, they're only used when
        // all x are assigned
        return ES_NOFIX;
      }

    for (int i = k.size(); i--; )
      GECODE_ME_CHECK(k[i].eq(home, count[i]));

    return home.ES_SUBSUMED(*this);
  }


  template<class Card>
  ExecStatus
  Bnd<Card>::post(Home home,
                  ViewArray<IntView>& x, ViewArray<Card>& k) {
    bool cardfix = true;
    for (int i=k.size(); i--; )
      if (!k[i].assigned()) {
        cardfix = false; break;
      }
    bool nolbc = true;
    for (int i=k.size(); i--; )
      if (k[i].min() != 0) {
        nolbc = false; break;
      }

    GECODE_ES_CHECK(postSideConstraints<Card>(home, x, k));

    if (isDistinct<Card>(home,x,k))
      return Distinct::Bnd<IntView>::post(home,x);

    (void) new (home) Bnd<Card>(home,x,k,cardfix,nolbc);
    return ES_OK;
  }

}}}

// STATISTICS: int-prop
