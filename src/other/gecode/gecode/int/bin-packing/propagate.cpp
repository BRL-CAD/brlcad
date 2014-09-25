/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2010
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

#include <gecode/int/bin-packing.hh>

namespace Gecode { namespace Int { namespace BinPacking {

  /*
   * Packing propagator
   *
   */

  PropCost 
  Pack::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::HI,bs.size());
  }

  Actor* 
  Pack::copy(Space& home, bool share) {
    return new (home) Pack(home,share,*this);
  }

  /// Record tell information
  class TellCache {
  protected:
    /// Values (sorted) to be pruned from view
    int* _nq;
    /// Number of values to be pruned
    int _n_nq;
    /// Value to which view should be assigned
    int _eq;
  public:
    /// Initialize cache for at most \a m values
    TellCache(Region& region, int m);
    /// Record that view must be different from \a j
    void nq(int j);
    /// Record that view must be equal to \a j, return false if not possible
    void eq(int j);
    /// Perform tell to view \a x and reset cache
    ExecStatus tell(Space& home, IntView x);
  };

  forceinline
  TellCache::TellCache(Region& region, int m) 
    : _nq(region.alloc<int>(m)), _n_nq(0), _eq(-1) {}
  forceinline void 
  TellCache::nq(int j) {
    _nq[_n_nq++] = j;
  }
  forceinline void
  TellCache::eq(int j) {
    // For eq: -1 mean not yet assigned, -2 means failure, positive means value
    if (_eq == -1)
      _eq = j;
    else
      _eq = -2;
  }
  ExecStatus
  TellCache::tell(Space& home, IntView x) {
    if (_eq == -2) {
      return ES_FAILED;
    } else if (_eq >= 0) {
      GECODE_ME_CHECK(x.eq(home,_eq));
    }
    Iter::Values::Array nqi(_nq, _n_nq);
    GECODE_ME_CHECK(x.minus_v(home, nqi));
    _n_nq=0; _eq=-1;
    return ES_OK;
  }


  /*
   * Propagation proper
   *
   */
  ExecStatus 
  Pack::propagate(Space& home, const ModEventDelta& med) {
    // Number of items
    int n = bs.size();
    // Number of bins
    int m = l.size();

    {
      Region region(home);

      // Possible sizes for bins
      int* s = region.alloc<int>(m);

      for (int j=m; j--; )
        s[j] = 0;

      // Compute sizes for bins
      if (OffsetView::me(med) == ME_INT_VAL) {
        // Also eliminate assigned items
        int k=0;
        for (int i=0; i<n; i++)
          if (bs[i].assigned()) {
            int j = bs[i].bin().val();
            l[j].offset(l[j].offset() - bs[i].size());
            t -= bs[i].size();
          } else {
            for (ViewValues<IntView> j(bs[i].bin()); j(); ++j)
              s[j.val()] += bs[i].size();
            bs[k++] = bs[i];
          }
        n=k; bs.size(n);
      } else {
        for (int i=n; i--; ) {
          assert(!bs[i].assigned());
          for (ViewValues<IntView> j(bs[i].bin()); j(); ++j)
            s[j.val()] += bs[i].size();
        }
      }

      // Propagate bin loads and compute lower and upper bound
      int min = t, max = t;
      for (int j=m; j--; ) {
        GECODE_ME_CHECK(l[j].gq(home,0));
        GECODE_ME_CHECK(l[j].lq(home,s[j]));
        min -= l[j].max(); max -= l[j].min();
      }

      // Propagate that load must be equal to total size
      for (bool mod = true; mod; ) {
        mod = false; ModEvent me;
        for (int j=m; j--; ) {
          int lj_min = l[j].min();
          me = l[j].gq(home, min + l[j].max());
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            max += lj_min - l[j].min(); mod = true;
          }
          int lj_max = l[j].max();
          me = l[j].lq(home, max + l[j].min());
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            min += lj_max - l[j].max(); mod = true;
          }
        }
      }

      if (n == 0) {
        assert(l.assigned());
        return home.ES_SUBSUMED(*this);
      }

    
      {
        TellCache tc(region,m);

        int k=0;
        for (int i=0; i<n; i++) {
          for (ViewValues<IntView> j(bs[i].bin()); j(); ++j) {
            if (bs[i].size() > l[j.val()].max())
              tc.nq(j.val());
            if (s[j.val()] - bs[i].size() < l[j.val()].min()) 
              tc.eq(j.val());
          }
          GECODE_ES_CHECK(tc.tell(home,bs[i].bin()));
          // Eliminate assigned bin
          if (bs[i].assigned()) {
            int j = bs[i].bin().val();
            l[j].offset(l[j].offset() - bs[i].size());
            t -= bs[i].size();
          } else {
            bs[k++] = bs[i];
          }
        }
        n=k; bs.size(n);
      }

    }

    // Only if the propagator is at fixpoint here, continue with the more
    // expensive stage for propagation.
    if (IntView::me(modeventdelta()) != ME_INT_NONE)
      return ES_NOFIX;

    // Now the invariant holds that no more assigned bins exist!
    {
      Region region(home);

      // Size of items
      SizeSetMinusOne* s = region.alloc<SizeSetMinusOne>(m);

      for (int j=m; j--; )
        s[j] = SizeSetMinusOne(region,n);

      // Set up size information
      for (int i=0; i<n; i++) {
        assert(!bs[i].assigned());
        for (ViewValues<IntView> j(bs[i].bin()); j(); ++j) 
          s[j.val()].add(bs[i].size());
      }

      for (int j=m; j--; ) {
        // Can items still be packed into bin?
        if (nosum(static_cast<SizeSet&>(s[j]), l[j].min(), l[j].max()))
          return ES_FAILED;
        int ap, bp;
        // Must there be packed more items into bin?
        if (nosum(static_cast<SizeSet&>(s[j]), l[j].min(), l[j].min(), 
                  ap, bp))
          GECODE_ME_CHECK(l[j].gq(home,bp));
        // Must there be packed less items into bin?
        if (nosum(static_cast<SizeSet&>(s[j]), l[j].max(), l[j].max(), 
                  ap, bp))
          GECODE_ME_CHECK(l[j].lq(home,ap));
      }

      TellCache tc(region,m);

      int k=0;
      for (int i=0; i<n; i++) {
        assert(!bs[i].assigned());
        for (ViewValues<IntView> j(bs[i].bin()); j(); ++j) {
          // Items must be removed in decreasing size!
          s[j.val()].minus(bs[i].size());
          // Can item i still be packed into bin j?
          if (nosum(s[j.val()], 
                    l[j.val()].min() - bs[i].size(),
                    l[j.val()].max() - bs[i].size()))
            tc.nq(j.val());
          // Must item i be packed into bin j?
          if (nosum(s[j.val()], l[j.val()].min(), l[j.val()].max()))
            tc.eq(j.val());
        }
        GECODE_ES_CHECK(tc.tell(home,bs[i].bin()));
        if (bs[i].assigned()) {
          int j = bs[i].bin().val();
          l[j].offset(l[j].offset() - bs[i].size());
          t -= bs[i].size();
        } else {
          bs[k++] = bs[i];
        }
      }
      n=k; bs.size(n);
    }

    // Perform lower bound checking
    if (n > 0) {
      Region region(home);

      // Find capacity estimate (we start from bs[0] as it might be
      // not packable, actually (will be detected later anyway)!
      int c = bs[0].size();
      for (int j=m; j--; )
        c = std::max(c,l[j].max());

      // Count how many items have a certain size (bucket sort)
      int* n_s = region.alloc<int>(c+1);

      for (int i=c+1; i--; )
        n_s[i] = 0;

      // Count unpacked items
      for (int i=n; i--; )
        n_s[bs[i].size()]++;

      // Number of items and remaining bin load
      int nm = n;

      // Only count positive remaining bin loads
      for (int j=m; j--; ) 
        if (l[j].max() < 0) {
          return ES_FAILED;
        } else if (c > l[j].max()) {
          n_s[c - l[j].max()]++; nm++;
        }

      // Sizes of items and remaining bin loads
      int* s = region.alloc<int>(nm);

      // Setup sorted sizes
      {
        int k=0;
        for (int i=c+1; i--; )
          for (int n=n_s[i]; n--; )
            s[k++]=i;
        assert(k == nm);
      }

      // Items in N1 are from 0 ... n1 - 1
      int n1 = 0;
      // Items in N2 are from n1 ... n12 - 1, we count elements in N1 and N2
      int n12 = 0;
      // Items in N3 are from n12 ... n3 - 1 
      int n3 = 0;
      // Free space in N2
      int f2 = 0;
      // Total size of items in N3
      int s3 = 0;

      // Initialize n12 and f2
      for (; (n12 < nm) && (s[n12] > c/2); n12++)
        f2 += c - s[n12];

      // Initialize n3 and s3
      for (n3 = n12; n3 < nm; n3++)
        s3 += s[n3];
        
      // Compute lower bounds
      for (int k=0; k<=c/2; k++) {
        // Make N1 larger by adding elements and N2 smaller
        for (; (n1 < nm) && (s[n1] > c-k); n1++)
          f2 -= c - s[n1];
        assert(n1 <= n12);
        // Make N3 smaller by removing elements
        for (; (s[n3-1] < k) && (n3 > n12); n3--)
          s3 -= s[n3-1];
        // Overspill
        int o = (s3 > f2) ? ((s3 - f2 + c - 1) / c) : 0;
        if (n12 + o > m)
          return ES_FAILED;
      }
    }

    return ES_NOFIX;
  }

  ExecStatus
  Pack::post(Home home, ViewArray<OffsetView>& l, ViewArray<Item>& bs) {
    // Sort according to size
    Support::quicksort(&bs[0], bs.size());
    // Eliminate zero sized items (which are at the end as the size are sorted)
    {
      int n = bs.size();
      while (bs[n-1].size() == 0)
        n--;
      bs.size(n);
    }
    if (bs.size() == 0) {
      // No items to be packed
      for (int i=l.size(); i--; )
        GECODE_ME_CHECK(l[i].eq(home,0));
      return ES_OK;
    } else if (l.size() == 0) {
      // No bins available
      return ES_FAILED;
    } else {
      int s = 0;
      // Constrain bins 
      for (int i=bs.size(); i--; ) {
        s += bs[i].size();
        GECODE_ME_CHECK(bs[i].bin().gq(home,0));
        GECODE_ME_CHECK(bs[i].bin().le(home,l.size()));
      }
      // Constrain load
      for (int j=l.size(); j--; ) {
        GECODE_ME_CHECK(l[j].gq(home,0));
        GECODE_ME_CHECK(l[j].lq(home,s));
      }
      (void) new (home) Pack(home,l,bs);
      return ES_OK;
    }
  }

}}}

// STATISTICS: int-prop

