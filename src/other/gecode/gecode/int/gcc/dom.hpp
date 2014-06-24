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
 *     Patrick Pekczynski, 2004
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

  /*
   * Analogously to "gcc/bnd.hpp" we split the algorithm
   * in two parts:
   *   1) the UBC (Upper Bound Constraint) stating that there are
   *      at most k[i].max() occurences of the value v_i
   *   2) the LBC (Lower Bound Constraint) stating that there are
   *      at least k[i].min() occurences of the value v_i
   *
   * The algorithm proceeds in 5 STEPS:
   *
   * STEP 1:
   *    Build the bipartite value-graph G=(<X,D>,E),
   *    with X = all variable nodes (each variable forms a node)
   *         D = all value nodes (union over all domains of the variables)
   *    and (x_i,v) is an edge in G iff value v is in the domain D_i of x_i
   *
   * STEP 2:   Compute a matching in the value graph.
   * STEP 3:   Compute all even alternating paths from unmatched  nodes
   * STEP 4:   Compute strongly connected components in the merged graph
   * STEP 5:   Update the Domains according to the computed edges
   *
   */

  template<class Card>
  inline
  Dom<Card>::Dom(Home home, ViewArray<IntView>& x0,
                 ViewArray<Card>& k0, bool cf)
    : Propagator(home), x(x0),  y(home, x0),
      k(k0), vvg(NULL), card_fixed(cf){
    // y is used for bounds propagation since prop_bnd needs all variables
    // values within the domain bounds
    x.subscribe(home, *this, PC_INT_DOM);
    k.subscribe(home, *this, PC_INT_DOM);
  }

  template<class Card>
  forceinline
  Dom<Card>::Dom(Space& home, bool share, Dom<Card>& p)
    : Propagator(home, share, p), vvg(NULL), card_fixed(p.card_fixed) {
    x.update(home, share, p.x);
    y.update(home, share, p.y);
    k.update(home, share, p.k);
  }

  template<class Card>
  forceinline size_t
  Dom<Card>::dispose(Space& home) {
    x.cancel(home,*this, PC_INT_DOM);
    k.cancel(home,*this, PC_INT_DOM);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class Card>
  Actor*
  Dom<Card>::copy(Space& home, bool share) {
    return new (home) Dom<Card>(home, share, *this);
  }

  template<class Card>
  PropCost
  Dom<Card>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::cubic(PropCost::LO, x.size());
  }

  template<class Card>
  ExecStatus
  Dom<Card>::propagate(Space& home, const ModEventDelta&) {
    Region r(home);

    int* count = r.alloc<int>(k.size());
    for (int i = k.size(); i--; )
      count[i] = 0;

    // total number of assigned views
    int noa = 0;
    for (int i = y.size(); i--; )
      if (y[i].assigned()) {
        noa++;
        int idx;
        if (!lookupValue(k,y[i].val(),idx))
          return ES_FAILED;
        count[idx]++;
        if (Card::propagate && (k[idx].max() == 0))
          return ES_FAILED;
      }

    if (noa == y.size()) {
      // All views are assigned
      for (int i = k.size(); i--; ) {
        if ((k[i].min() > count[i]) || (count[i] > k[i].max()))
          return ES_FAILED;
        // the solution contains ci occurences of value k[i].card();
        if (Card::propagate)
          GECODE_ME_CHECK(k[i].eq(home, count[i]));
      }
      return home.ES_SUBSUMED(*this);
    }

    // before propagation performs inferences on cardinality variables:
    if (Card::propagate) {
      if (noa > 0)
        for (int i = k.size(); i--; )
          if (!k[i].assigned()) {
            GECODE_ME_CHECK(k[i].lq(home, y.size() - (noa - count[i])));
            GECODE_ME_CHECK(k[i].gq(home, count[i]));
          }

      GECODE_ES_CHECK(prop_card<Card>(home,y,k));
      if (!card_consistent<Card>(y,k))
        return ES_FAILED;
    }

    if (x.size() == 0) {
      for (int j = k.size(); j--; )
        if ((k[j].min() > k[j].counter()) || (k[j].max() < k[j].counter()))
          return ES_FAILED;
      return home.ES_SUBSUMED(*this);
    } else if ((x.size() == 1) && (x[0].assigned())) {
      int idx;
      if (!lookupValue(k,x[0].val(),idx))
        return ES_FAILED;
      GECODE_ME_CHECK(k[idx].inc());
      for (int j = k.size(); j--; )
        if ((k[j].min() > k[j].counter()) || (k[j].max() < k[j].counter()))
          return ES_FAILED;
      return home.ES_SUBSUMED(*this);
    }

    if (vvg == NULL) {
      int smin = 0;
      int smax = 0;
      for (int i=k.size(); i--; )
        if (k[i].counter() > k[i].max() ) {
          return ES_FAILED;
        } else {
          smax += (k[i].max() - k[i].counter());
          if (k[i].counter() < k[i].min())
            smin += (k[i].min() - k[i].counter());
        }

      if ((x.size() < smin) || (smax < x.size()))
        return ES_FAILED;

      vvg = new (home) VarValGraph<Card>(home, x, k, smin, smax);
      GECODE_ES_CHECK(vvg->min_require(home,x,k));
      GECODE_ES_CHECK(vvg->template maximum_matching<UBC>(home));
      if (!card_fixed)
        GECODE_ES_CHECK(vvg->template maximum_matching<LBC>(home));
    } else {
      GECODE_ES_CHECK(vvg->sync(home,x,k));
    }

    vvg->template free_alternating_paths<UBC>(home);
    vvg->template strongly_connected_components<UBC>(home);

    GECODE_ES_CHECK(vvg->template narrow<UBC>(home,x,k));

    if (!card_fixed) {
      if (Card::propagate)
        GECODE_ES_CHECK(vvg->sync(home,x,k));

      vvg->template free_alternating_paths<LBC>(home);
      vvg->template strongly_connected_components<LBC>(home);

      GECODE_ES_CHECK(vvg->template narrow<LBC>(home,x,k));
    }

    {
      bool card_assigned = true;
      if (Card::propagate) {
        GECODE_ES_CHECK(prop_card<Card>(home, y, k));
        card_assigned = k.assigned();
      }
      
      if (card_assigned) {
        if (x.size() == 0) {
          for (int j=k.size(); j--; )
            if ((k[j].min() > k[j].counter()) || 
                (k[j].max() < k[j].counter()))
              return ES_FAILED;
          return home.ES_SUBSUMED(*this);
        } else if ((x.size() == 1) && x[0].assigned()) {
          int idx;
          if (!lookupValue(k,x[0].val(),idx))
            return ES_FAILED;
          GECODE_ME_CHECK(k[idx].inc());
          
          for (int j = k.size(); j--; )
            if ((k[j].min() > k[j].counter()) || 
                (k[j].max() < k[j].counter()))
              return ES_FAILED;
          return home.ES_SUBSUMED(*this);
        }
      }
    }

    for (int i = k.size(); i--; )
      count[i] = 0;

    bool all_assigned = true;
    // total number of assigned views
    for (int i = y.size(); i--; )
      if (y[i].assigned()) {
        int idx;
        if (!lookupValue(k,y[i].val(),idx))
          return ES_FAILED;
        count[idx]++;
        if (Card::propagate && (k[idx].max() == 0))
          return ES_FAILED;
      } else {
        all_assigned = false;
      }

    if (Card::propagate)
      GECODE_ES_CHECK(prop_card<Card>(home, y, k));

    if (all_assigned) {
      for (int i = k.size(); i--; ) {
        if ((k[i].min() > count[i]) || (count[i] > k[i].max()))
          return ES_FAILED;
        // the solution contains count[i] occurences of value k[i].card();
        if (Card::propagate)
          GECODE_ME_CHECK(k[i].eq(home,count[i]));
      }
      return home.ES_SUBSUMED(*this);
    }

    if (Card::propagate) {
      int ysmax = y.size();
      for (int i=k.size(); i--; )
        ysmax -= k[i].max();
      int smax = 0;
      bool card_ass = true;
      for (int i = k.size(); i--; ) {
        GECODE_ME_CHECK(k[i].gq(home, ysmax + k[i].max()));
        smax += k[i].max();
        GECODE_ME_CHECK(k[i].lq(home, y.size() + k[i].min()));
        if (!k[i].assigned())
          card_ass = false;
      }
      if (card_ass && (smax != y.size()))
        return ES_FAILED;
    }

    return Card::propagate ? ES_NOFIX : ES_FIX;
  }

  template<class Card>
  inline ExecStatus
  Dom<Card>::post(Home home, 
                  ViewArray<IntView>& x, ViewArray<Card>& k) {
    GECODE_ES_CHECK((postSideConstraints<Card>(home,x,k)));

    if (isDistinct<Card>(home, x, k))
      return Distinct::Dom<IntView>::post(home,x);

    bool cardfix = true;
    for (int i = k.size(); i--; )
      if (!k[i].assigned()) {
        cardfix = false; break;
      }

    (void) new (home) Dom<Card>(home,x,k,cardfix);
    return ES_OK;
  }

}}}

// STATISTICS: int-prop
