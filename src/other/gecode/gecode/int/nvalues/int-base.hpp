/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

#include <gecode/int/distinct.hh>

namespace Gecode { namespace Int { namespace NValues {

  template<class VY>
  forceinline
  IntBase<VY>::IntBase(Home home, ValSet& vs0, ViewArray<IntView>& x, VY y)
    : MixNaryOnePropagator<IntView,PC_INT_DOM,VY,PC_INT_BND>(home,x,y),
      vs(vs0) {}

  template<class VY>
  forceinline
  IntBase<VY>::IntBase(Space& home, bool share, IntBase<VY>& p)
    : MixNaryOnePropagator<IntView,PC_INT_DOM,VY,PC_INT_BND>(home, share, p) {
    vs.update(home, share, p.vs);
  }

  template<class VY>
  forceinline size_t
  IntBase<VY>::dispose(Space& home) {
    vs.dispose(home);
    (void) MixNaryOnePropagator<IntView,PC_INT_DOM,VY,PC_INT_BND>
      ::dispose(home);
    return sizeof(*this);
  }

  template<class VY>
  PropCost 
  IntBase<VY>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::HI, x.size());
  }

  template<class VY>
  void
  IntBase<VY>::add(Space& home) {
    int n=x.size();
    for (int i=n; i--; )
      if (x[i].assigned()) {
        vs.add(home, x[i].val());
        x[i] = x[--n];
      }
    x.size(n);
  }

  template<class VY>
  void
  IntBase<VY>::disjoint(Space& home, Region& r, int*& dis, int& n_dis) {
    // Compute positions of disjoint views
    int n=x.size();
    dis = r.alloc<int>(n); n_dis = 0;

    int i=0;
    while (i < n)
      switch (vs.compare(x[i])) {
      case Iter::Ranges::CS_SUBSET:
        // All values are already contained in vs, eliminate x[i]
        x[i].cancel(home, *this, PC_INT_DOM);
        x[i] = x[--n]; 
        break;
      case Iter::Ranges::CS_DISJOINT:
        dis[n_dis++] = i++; 
        break;
      case Iter::Ranges::CS_NONE:
        i++;
        break;
      default:
        GECODE_NEVER;
      }
    x.size(n);
  }

  template<class VY>
  void
  IntBase<VY>::eliminate(Space& home) {
    int n=x.size();
    for (int i=n; i--; )
      if (vs.subset(x[i])) {
        // All values are already contained in vs, eliminate x[i]
        x[i].cancel(home, *this, PC_INT_DOM);
        x[i] = x[--n]; 
      }
    x.size(n);
  }

  template<class VY>
  int
  IntBase<VY>::size(Space& home) const {
    Region r(home);
    assert(x.size() > 0);
    ValSet::Ranges vsr(vs);
    ViewRanges<IntView> xr(x[x.size()-1]);
    Iter::Ranges::NaryUnion u(r,vsr,xr);
    for (int i=x.size()-1; i--; ) {
      ViewRanges<IntView> xir(x[i]);
      u |= xir;
    }
    unsigned int s = Iter::Ranges::size(u);

    // To avoid overflow
    if (static_cast<unsigned int>(x.size()+vs.size()) < s)
      return x.size() + vs.size();
    else
      return static_cast<int>(s);
  }

  template<class VY>
  ExecStatus
  IntBase<VY>::all_in_valset(Space& home) {
    for (int i=x.size(); i--; ) {
      ValSet::Ranges vsr(vs);
      GECODE_ME_CHECK(x[i].inter_r(home, vsr, false));
    }
    return home.ES_SUBSUMED(*this);
  }

  template<class VY>
  ExecStatus
  IntBase<VY>::prune_lower(Space& home, int* dis, int n_dis) {
    assert(n_dis > 0);

    // At least one more value will be needed
    GECODE_ME_CHECK(y.gq(home,vs.size() + 1));

    Region r(home);

    // Only one additional value is allowed
    if (y.max() == vs.size() + 1) {
      // Compute possible values
      ViewRanges<IntView>* r_dis = r.alloc<ViewRanges<IntView> >(n_dis);
      for (int i=n_dis; i--; )
        r_dis[i] = ViewRanges<IntView>(x[dis[i]]);
      Iter::Ranges::NaryInter iv(r, r_dis, n_dis);
      // Is there a common value at all?
      if (!iv())
        return ES_FAILED;
      ValSet::Ranges vsr(vs);
      Iter::Ranges::NaryUnion pv(r,iv,vsr);
      // Enforce common values
      for (int i=x.size(); i--; ) {
        pv.reset();
        GECODE_ME_CHECK(x[i].inter_r(home, pv, false));
      }
      return ES_OK;
    }

    // Compute independent set for lower bound
    // ovl is a bit-matrix defining whether two views overlap
    SymBitMatrix ovl(r,x.size());
    // deg[i] is the degree of x[i]
    int* deg = r.alloc<int>(x.size());
    // ovl_i[i] is an array of indices j such that x[j] overlaps with x[i]
    int** ovl_i = r.alloc<int*>(x.size());
    // n_ovl_i[i] defines how many integers are stored for ovl_i[i]
    int* n_ovl_i = r.alloc<int>(x.size());
    {
#ifndef NDEBUG
      // Initialize all to null pointers so that things crash ;-)
      for (int i=x.size(); i--; )
        ovl_i[i] = NULL;
#endif
      // For each i there can be at most n_dis-1 entries in ovl_i[i]
      int* m = r.alloc<int>(n_dis*(n_dis-1));
      for (int i=n_dis; i--; ) {
        deg[dis[i]] = 0; 
        ovl_i[dis[i]] = m; m += n_dis-1;
      }
    }
    
    // Initialize overlap matrix by analyzing the view ranges
    {
      // Compute how many events are needed
      // One event for the end marker
      int n_re = 1;
      // Two events for each range
      for (int i=n_dis; i--; )
        for (ViewRanges<IntView> rx(x[dis[i]]); rx(); ++rx)
          n_re += 2;

      // Allocate and initialize events
      RangeEvent* re = r.alloc<RangeEvent>(n_re);
      int j=0;
      for (int i=n_dis; i--; )
        for (ViewRanges<IntView> rx(x[dis[i]]); rx(); ++rx) {
          // Event when a range starts
          re[j].ret=RET_FST; re[j].val=rx.min(); re[j].view=dis[i]; j++;
          // Event when a range ends
          re[j].ret=RET_LST; re[j].val=rx.max(); re[j].view=dis[i]; j++;
        }
      // Make this the last event
      re[j].ret=RET_END; re[j].val=Int::Limits::infinity;
      assert(j+1 == n_re);
      // Sort and process events
      Support::quicksort(re,n_re);

      // Current views with a range being active
      Support::BitSet<Region> cur(r,static_cast<unsigned int>(x.size()));
      // Process all events
      for (int i=0; true; i++)
        switch (re[i].ret) {
        case RET_FST:
          // Process all overlapping views
          for (Iter::Values::BitSet<Support::BitSet<Region> > j(cur); 
               j(); ++j) {
            int di = re[i].view, dj = j.val();
            if (!ovl.get(di,dj))  {
              ovl.set(di,dj);
              ovl_i[di][deg[di]++] = dj; 
              ovl_i[dj][deg[dj]++] = di; 
            }
          }
          cur.set(static_cast<unsigned int>(re[i].view));
          break;
        case RET_LST:
          cur.clear(static_cast<unsigned int>(re[i].view));
          break;
        case RET_END:
          goto done;
        default:
          GECODE_NEVER;
        }
    done:
      r.free<RangeEvent>(re,n_re);
    }

    // While deg changes, n_ovl_i remains unchanged and is needed, so copy it
    for (int i=n_dis; i--; ) {
      assert(deg[dis[i]] < n_dis);
      n_ovl_i[dis[i]] = deg[dis[i]];
    }
    
    // Views in the independent set
    int* ind = r.alloc<int>(n_dis);
    int n_ind = 0;

    while (n_dis > 0) {
      int i_min = n_dis-1;
      int d_min = deg[dis[i_min]];
      unsigned int s_min = x[dis[i_min]].size(); 

      // Find view with smallest (degree,size)
      for (int i=n_dis-1; i--; )
        if ((d_min > deg[dis[i]]) || 
            ((d_min == deg[dis[i]]) && (s_min > x[dis[i]].size()))) {
          i_min = i;
          d_min = deg[dis[i]];
          s_min = x[dis[i]].size();
        }

      // i_min refers to view with smallest (degree,size)
      ind[n_ind++] = dis[i_min]; dis[i_min] = dis[--n_dis];
      
      // Filter out non disjoint views
      for (int i=n_dis; i--; )
        if (ovl.get(dis[i],ind[n_ind-1])) {
          // Update degree information
          for (int j=n_ovl_i[dis[i]]; j--; )
            deg[ovl_i[dis[i]][j]]--;
          // Eliminate view
          dis[i] = dis[--n_dis];
        }
    }
    // Enforce lower bound
    GECODE_ME_CHECK(y.gq(home,vs.size() + n_ind));

    // Prune, if possible
    if (vs.size() + n_ind == y.max()) {
      // Only values from the indepent set a can be taken
      ViewRanges<IntView>* r_ind = r.alloc<ViewRanges<IntView> >(n_ind);
      for (int i=n_ind; i--; )
        r_ind[i] = ViewRanges<IntView>(x[ind[i]]);
      Iter::Ranges::NaryUnion v_ind(r, r_ind, n_ind);
      ValSet::Ranges vsr(vs);
      v_ind |= vsr;
      for (int i=x.size(); i--; ) {
        v_ind.reset();
        GECODE_ME_CHECK(x[i].inter_r(home,v_ind,false));
      }
    } 
    return ES_OK;
  }

  template<class VY>
  forceinline ExecStatus
  IntBase<VY>::prune_upper(Space& home, Graph& g) {
    if (!g.initialized()) {
      g.init(home,vs,x);
    } else {
      g.purge();
      g.sync(home);
    }
    GECODE_ME_CHECK(y.lq(home, g.size()));
    if (y.min() == g.size()) {
      // All values must be taken on
      if (vs.size() + x.size() == g.size()) {
        // This is in fact a distinct, simplify and rewrite
        for (int i=x.size(); i--; ) {
          ValSet::Ranges vsr(vs);
          GECODE_ME_CHECK(x[i].minus_r(home, vsr, false));
        }
        GECODE_REWRITE(*this,Distinct::Dom<IntView>::post(home,x));
      }
      if (g.mark(home))
        GECODE_ES_CHECK(g.prune(home));
    }
    return ES_OK;
  }

}}}

// STATISTICS: int-prop
