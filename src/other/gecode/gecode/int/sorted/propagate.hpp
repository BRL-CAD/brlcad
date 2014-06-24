/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Patrick Pekczynski <pekczynski@ps.uni-sb.de>
 *
 *  Copyright:
 *     Patrick Pekczynski, 2004
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

#include <gecode/int/rel.hh>
#include <gecode/int/distinct.hh>

namespace Gecode { namespace Int { namespace Sorted {


  /*
   * Summary of the propagation algorithm as implemented in the
   * propagate method below:
   *
   * STEP 1: Normalize the domains of the y variables
   * STEP 2: Sort the domains of the x variables according to their lower
   *         and upper endpoints
   * STEP 3: Compute the matchings phi and phiprime with
   *         Glover's matching algorithm
   * STEP 4: Compute the strongly connected components in
   *         the oriented intersection graph
   * STEP 5: Narrow the domains of the variables
   *
   */

  /**
   * \brief Perform bounds consistent sortedness propagation
   *
   * Implements the propagation algorithm for Sorted::Sorted
   * and is provided as seperate function, because a second pass of
   * the propagation algorithm is needed in order to achieve idempotency
   * in case explicit permutation variables are provided.
   *
   * If \a Perm is true, permutation variables form the
   * third argument which implies additional inferences,
   * consistency check on the permutation variables and eventually a
   * second pass of the propagation algorithm.
   * Otherwise, the algorithm does not take care of the permutation
   * variables resulting in a better performance.
   */

  template<class View, bool Perm>
  ExecStatus
  bounds_propagation(Space& home, Propagator& p,
                     ViewArray<View>& x,
                     ViewArray<View>& y,
                     ViewArray<View>& z,
                     bool& repairpass,
                     bool& nofix,
                     bool& match_fixed){

    int n = x.size();

    Region r(home);
    int* tau = r.alloc<int>(n);
    int* phi = r.alloc<int>(n);
    int* phiprime = r.alloc<int>(n);
    OfflineMinItem* sequence = r.alloc<OfflineMinItem>(n);
    int* vertices = r.alloc<int>(n);

    if (match_fixed) {
      // sorting is determined, sigma and tau coincide
      for (int i=n; i--; )
        tau[z[i].val()] = i;
    } else {
      for (int i = n; i--; )
        tau[i] = i;
    }

    if (Perm) {
      // normalized and sorted
      // collect all bounds

      Rank* allbnd = r.alloc<Rank>(x.size());
#ifndef NDEBUG
      for (int i=n; i--;)
        allbnd[i].min = allbnd[i].max = -1;
#endif
      for (int i=n; i--;) {
        int min = x[i].min();
        int max = x[i].max();
        for (int j=0; j<n; j++) {
          if ( (y[j].min() > min) ||
               (y[j].min() <= min && min <= y[j].max()) ) {
            allbnd[i].min = j;
            break;
          }
        }
        for (int j=n; j--;) {
          if (y[j].min() > max) {
            allbnd[i].max = j-1;
            break;
          } else if (y[j].min() <= max && min <= y[j].max()) {
            allbnd[i].max = j;
            break;
          }
        }
      }
      
      for (int i = n; i--; ) {
        // minimum reachable y-variable
        int minr = allbnd[i].min;
        assert(minr != -1);
        int maxr = allbnd[i].max;
        assert(maxr != -1);

        ModEvent me = x[i].gq(home, y[minr].min());
        if (me_failed(me))
          return ES_FAILED;
        nofix |= (me_modified(me) && (x[i].min() != y[minr].min()));

        me = x[i].lq(home, y[maxr].max());
        if (me_failed(me))
          return ES_FAILED;
        nofix |= (me_modified(me) && (x[i].min() != y[maxr].max()));

        me = z[i].gq(home, minr);
        if (me_failed(me))
          return ES_FAILED;
        nofix |= (me_modified(me) &&  (z[i].min() != minr));

        me = z[i].lq(home, maxr);
        if (me_failed(me))
          return ES_FAILED;
        nofix |= (me_modified(me) &&  (z[i].max() != maxr));
      }

      // channel information from x to y through permutation variables in z
      if (!channel(home,x,y,z,nofix))
        return ES_FAILED;
      if (nofix)
        return ES_NOFIX;
    }

    /*
     * STEP 1:
     *  normalization is implemented in "order.hpp"
     *    o  setting the lower bounds of the y_i domains (\lb E_i)
     *       to max(\lb E_{i-1},\lb E_i)
     *    o  setting the upper bounds of the y_i domains (\ub E_i)
     *       to min(\ub E_i,\ub E_{i+1})
     */

    if (!normalize(home, y, x, nofix))
      return ES_FAILED;

    if (Perm) {
      // check consistency of channeling after normalization
      if (!channel(home,x,y,z,nofix))
        return ES_FAILED;
      if (nofix)
        return ES_NOFIX;
    }


    // if bounds have changed we have to recreate sigma to restore
    // optimized dropping of variables

    sort_sigma<View,Perm>(home,x,z);

    bool subsumed   = true;
    bool array_subs = false;
    int  dropfst  = 0;
    bool noperm_bc = false;

    if (!check_subsumption<View,Perm>(x,y,z,subsumed,dropfst) ||
        !array_assigned<View,Perm>(home,x,y,z,array_subs,match_fixed,nofix,noperm_bc))
      return ES_FAILED;

    if (subsumed || array_subs)
      return home.ES_SUBSUMED(p);

    /*
     * STEP 2: creating tau
     * Sort the domains of the x variables according
     * to their lower bounds, where we use an
     * intermediate array of integers for sorting
     */
    sort_tau<View,Perm>(x,z,tau);

    /*
     * STEP 3:
     *  Compute the matchings \phi and \phi' between
     *  the x and the y variables
     *  with Glover's matching algorithm.
     *        o  phi is computed with the glover function
     *        o  phiprime is computed with the revglover function
     *  glover and revglover are implemented in "matching.hpp"
     */

    if (!match_fixed) {
      if (!glover(x,y,tau,phi,sequence,vertices))
        return ES_FAILED;
    } else {
      for (int i = x.size(); i--; ) {
        phi[i]      = z[i].val();
        phiprime[i] = phi[i];
      }
    }

    for (int i = n; i--; )
      if (!y[i].assigned()) {
        // phiprime is not needed to narrow the domains of the x-variables
        if (!match_fixed &&
            !revglover(x,y,tau,phiprime,sequence,vertices))
          return ES_FAILED;

        if (!narrow_domy(home,x,y,phi,phiprime,nofix))
          return ES_FAILED;

        if (nofix && !match_fixed) {
          // data structures (matching) destroyed by domains with holes

          for (int j = y.size(); j--; )
            phi[j]=phiprime[j]=0;

          if (!glover(x,y,tau,phi,sequence,vertices))
            return ES_FAILED;

          if (!revglover(x,y,tau,phiprime,sequence,vertices))
            return ES_FAILED;

          if (!narrow_domy(home,x,y,phi,phiprime,nofix))
            return ES_FAILED;
        }
        break;
      }

    if (Perm) {
      // check consistency of channeling after normalization
      if (!channel(home,x,y,z,nofix))
        return ES_FAILED;
      if (nofix)
        return ES_NOFIX;
    }

    /*
     * STEP 4:
     *  Compute the strongly connected components in
     *  the oriented intersection graph
     *  the computation of the sccs is implemented in
     *  "narrowing.hpp" in the function narrow_domx
     */

    int* scclist = r.alloc<int>(n);
    SccComponent* sinfo = r.alloc<SccComponent>(n);

    for(int i = n; i--; )
      sinfo[i].left=sinfo[i].right=sinfo[i].rightmost=sinfo[i].leftmost= i;

    computesccs(home,x,y,phi,sinfo,scclist);

    /*
     * STEP 5:
     *  Narrow the domains of the variables
     *  Also implemented in "narrowing.hpp"
     *  in the functions narrow_domx and narrow_domy
     */

    if (!narrow_domx<View,Perm>(home,x,y,z,tau,phi,scclist,sinfo,nofix))
      return ES_FAILED;

    if (Perm) {
      if (!noperm_bc &&
          !perm_bc<View>
          (home, tau, sinfo, scclist, x,z, repairpass, nofix))
          return ES_FAILED;

      // channeling also needed after normal propagation steps
      // in order to ensure consistency after possible modification in perm_bc
      if (!channel(home,x,y,z,nofix))
        return ES_FAILED;
      if (nofix)
        return ES_NOFIX;
    }

    sort_tau<View,Perm>(x,z,tau);

    if (Perm) {
      // special case of sccs of size 2 denoted by permutation variables
      // used to enforce consistency from x to y
      // case of the upper bound ordering tau
      for (int i = x.size() - 1; i--; ) {
        // two x variables are in the same scc of size 2
        if (z[tau[i]].min() == z[tau[i+1]].min() &&
            z[tau[i]].max() == z[tau[i+1]].max() &&
            z[tau[i]].size() == 2 && z[tau[i]].range()) {
          // if bounds are strictly smaller
          if (x[tau[i]].max() < x[tau[i+1]].max()) {
            ModEvent me = y[z[tau[i]].min()].lq(home, x[tau[i]].max());
            if (me_failed(me))
              return ES_FAILED;
            nofix |= (me_modified(me) &&
                      y[z[tau[i]].min()].max() != x[tau[i]].max());

            me = y[z[tau[i+1]].max()].lq(home, x[tau[i+1]].max());
            if (me_failed(me))
              return ES_FAILED;
            nofix |= (me_modified(me) &&
                      y[z[tau[i+1]].max()].max() != x[tau[i+1]].max());
          }
        }
      }
    }
    return nofix ? ES_NOFIX : ES_FIX;
  }

  template<class View, bool Perm>
  forceinline Sorted<View,Perm>::
  Sorted(Space& home, bool share, Sorted<View,Perm>& p):
    Propagator(home, share, p),
    reachable(p.reachable) {
    x.update(home, share, p.x);
    y.update(home, share, p.y);
    z.update(home, share, p.z);
    w.update(home, share, p.w);
  }

  template<class View, bool Perm>
  Sorted<View,Perm>::
  Sorted(Home home,
         ViewArray<View>& x0, ViewArray<View>& y0, ViewArray<View>& z0) :
    Propagator(home), x(x0), y(y0), z(z0), w(home,y0), reachable(-1) {
    x.subscribe(home, *this, PC_INT_BND);
    y.subscribe(home, *this, PC_INT_BND);
    if (Perm)
      z.subscribe(home, *this, PC_INT_BND);
  }

  template<class View, bool Perm>
  forceinline size_t
  Sorted<View,Perm>::dispose(Space& home) {
    x.cancel(home,*this, PC_INT_BND);
    y.cancel(home,*this, PC_INT_BND);
    if (Perm)
      z.cancel(home,*this, PC_INT_BND);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View, bool Perm>
  Actor* Sorted<View,Perm>::copy(Space& home, bool share) {
    return new (home) Sorted<View,Perm>(home, share, *this);
  }

  template<class View, bool Perm>
  PropCost Sorted<View,Perm>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO, x.size());
  }

  template<class View, bool Perm>
  ExecStatus
  Sorted<View,Perm>::propagate(Space& home, const ModEventDelta&) {
    int  n           = x.size();
    bool secondpass  = false;
    bool nofix       = false;
    int  dropfst     = 0;

    bool subsumed    = false;
    bool array_subs  = false;
    bool match_fixed = false;

    // normalization of x and y
    if (!normalize(home, y, x, nofix))
      return ES_FAILED;

    // create sigma sorting
    sort_sigma<View,Perm>(home,x,z);

    bool noperm_bc = false;
    if (!array_assigned<View,Perm>
        (home, x, y, z, array_subs, match_fixed, nofix, noperm_bc))
      return ES_FAILED;

    if (array_subs)
      return home.ES_SUBSUMED(*this);

    sort_sigma<View,Perm>(home,x,z);

    // in this case check_subsumptions is guaranteed to find
    // the xs ordered by sigma

    if (!check_subsumption<View,Perm>(x,y,z,subsumed,dropfst))
      return ES_FAILED;

    if (subsumed)
      return home.ES_SUBSUMED(*this);

    if (Perm) {
      // dropping possibly yields inconsistent indices on permutation variables
      if (dropfst) {
        reachable = w[dropfst - 1].max();
        bool unreachable = true;
        for (int i = x.size(); unreachable && i-- ; ) {
          unreachable &= (reachable < x[i].min());
        }

        if (unreachable) {
          x.drop_fst(dropfst, home, *this, PC_INT_BND);
          y.drop_fst(dropfst, home, *this, PC_INT_BND);
          z.drop_fst(dropfst, home, *this, PC_INT_BND);
        } else {
          dropfst = 0;
        }
      }

      n = x.size();

      if (n < 2) {
        if (x[0].max() < y[0].min() || y[0].max() < x[0].min())
          return ES_FAILED;
        if (Perm) {
          GECODE_ME_CHECK(z[0].eq(home, w.size() - 1));
        }
        GECODE_REWRITE(*this,(Rel::EqBnd<View,View>::post(home(*this), x[0], y[0])));
      }

      // check whether shifting the permutation variables
      // is necessary after dropping x and y vars
      // highest reachable index
      int  valid = n - 1;
      int  index = 0;
      int  shift = 0;

      for (int i = n; i--; ){
        if (z[i].max() > index)
          index = z[i].max();
        if (index > valid)
          shift = index - valid;
      }

      if (shift) {
        ViewArray<OffsetView> ox(home,n), oy(home,n), oz(home,n);

        for (int i = n; i--; ) {
          GECODE_ME_CHECK(z[i].gq(home, shift));

          oz[i] = OffsetView(z[i], -shift);
          ox[i] = OffsetView(x[i], 0);
          oy[i] = OffsetView(y[i], 0);
        }

        GECODE_ES_CHECK((bounds_propagation<OffsetView,Perm>
                         (home,*this,ox,oy,oz,secondpass,nofix,match_fixed)));

        if (secondpass) {
          GECODE_ES_CHECK((bounds_propagation<OffsetView,Perm>
                           (home,*this,ox,oy,oz,secondpass,nofix,match_fixed)));
        }
      } else {
        GECODE_ES_CHECK((bounds_propagation<View,Perm>
                         (home,*this,x,y,z,secondpass,nofix,match_fixed)));

        if (secondpass) {
          GECODE_ES_CHECK((bounds_propagation<View,Perm>
                           (home,*this,x,y,z,secondpass,nofix,match_fixed)));
        }
      }
    } else {
      // dropping has no consequences
      if (dropfst) {
        x.drop_fst(dropfst, home, *this, PC_INT_BND);
        y.drop_fst(dropfst, home, *this, PC_INT_BND);
      }

      n = x.size();

      if (n < 2) {
        if (x[0].max() < y[0].min() || y[0].max() < x[0].min())
          return ES_FAILED;
        GECODE_REWRITE(*this,(Rel::EqBnd<View,View>::post(home(*this), x[0], y[0])));
      }

      GECODE_ES_CHECK((bounds_propagation<View,Perm>
                       (home, *this, x, y, z,secondpass, nofix, match_fixed)));
      // no second pass possible if there are no permvars
    }

    if (!normalize(home, y, x, nofix))
      return ES_FAILED;

    Region r(home);
    int* tau = r.alloc<int>(n);
    if (match_fixed) {
      // sorting is determined
      // sigma and tau coincide
      for (int i = x.size(); i--; ) {
        int pi = z[i].val();
        tau[pi] = i;
      }
    } else {
      for (int i = n; i--; ) {
        tau[i] = i;
      }
    }

    sort_tau<View,Perm>(x,z,tau);
    // recreate consistency for already assigned subparts
    // in order of the upper bounds starting at the end of the array
    bool xbassigned = true;
    for (int i = x.size(); i--; ) {
      if (x[tau[i]].assigned() && xbassigned) {
        GECODE_ME_CHECK(y[i].eq(home, x[tau[i]].val()));
      } else {
        xbassigned = false;
      }
    }

    subsumed   = true;
    array_subs = false;
    noperm_bc  = false;

    // creating sorting anew
    sort_sigma<View,Perm>(home,x,z);

    if (Perm) {
      for (int i = 0; i < x.size() - 1; i++) {
        // special case of subsccs of size2 for the lower bounds
        // two x variables are in the same scc of size 2
        if (z[i].min() == z[i+1].min() &&
            z[i].max() == z[i+1].max() &&
            z[i].size() == 2 && z[i].range()) {
          if (x[i].min() < x[i+1].min()) {
            ModEvent me = y[z[i].min()].gq(home, x[i].min());
            GECODE_ME_CHECK(me);
            nofix |= (me_modified(me) &&
                      y[z[i].min()].min() != x[i].min());

            me = y[z[i+1].max()].gq(home, x[i+1].min());
            GECODE_ME_CHECK(me);
            nofix |= (me_modified(me) &&
                      y[z[i+1].max()].min() != x[i+1].min());
          }
        }
      }
    }

    // check assigned
    // should be sorted
    bool xassigned = true;
    for (int i = 0; i < x.size(); i++) {
      if (x[i].assigned() && xassigned) {
        GECODE_ME_CHECK(y[i].eq(home,x[i].val()));
      } else {
        xassigned = false;
      }
    }

    // sorted check bounds
    // final check that variables are consitent with least and greatest possible
    // values
    int tlb = std::min(x[0].min(), y[0].min());
    int tub = std::max(x[x.size() - 1].max(), y[y.size() - 1].max());
    for (int i = x.size(); i--; ) {
      ModEvent me = y[i].lq(home, tub);
      GECODE_ME_CHECK(me);
      nofix |= me_modified(me) && (y[i].max() != tub);

      me = y[i].gq(home, tlb);
      GECODE_ME_CHECK(me);
      nofix |= me_modified(me) && (y[i].min() != tlb);

      me = x[i].lq(home, tub);
      GECODE_ME_CHECK(me);
      nofix |= me_modified(me) && (x[i].max() != tub);

      me = x[i].gq(home, tlb);
      GECODE_ME_CHECK(me);
      nofix |= me_modified(me) && (x[i].min() != tlb);
    }

    if (!array_assigned<View,Perm>
        (home, x, y, z, array_subs, match_fixed, nofix, noperm_bc))
      return ES_FAILED;

    if (array_subs)
      return home.ES_SUBSUMED(*this);

    if (!check_subsumption<View,Perm>(x,y,z,subsumed,dropfst))
      return ES_FAILED;

    if (subsumed)
      return home.ES_SUBSUMED(*this);

    return nofix ? ES_NOFIX : ES_FIX;
  }

  template<class View, bool Perm>
  ExecStatus
  Sorted<View,Perm>::
  post(Home home,
       ViewArray<View>& x0, ViewArray<View>& y0, ViewArray<View>& z0) {
    int n = x0.size();
    if (n < 2) {
      if ((x0[0].max() < y0[0].min()) || (y0[0].max() < x0[0].min()))
        return ES_FAILED;
      GECODE_ES_CHECK((Rel::EqBnd<View,View>::post(home,x0[0],y0[0])));
      if (Perm) {
        GECODE_ME_CHECK(z0[0].eq(home,0));
      }
    } else {
      if (Perm) {
        ViewArray<View> z(home,n);
        for (int i=n; i--; ) {
          z[i]=z0[i];
          GECODE_ME_CHECK(z[i].gq(home,0));
          GECODE_ME_CHECK(z[i].lq(home,n-1));
        }
        GECODE_ES_CHECK(Distinct::Bnd<View>::post(home,z));
      }
      new (home) Sorted<View,Perm>(home,x0,y0,z0);
    }
    return ES_OK;
  }

}}}

// STATISTICS: int-prop



