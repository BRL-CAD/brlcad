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

namespace Gecode { namespace Int { namespace Sorted {

  /**
   *   \brief Compute the sccs of the oriented intersection-graph
   *
   *   An y-node \f$y_j\f$ and its corresponding matching mate
   *   \f$x_{\phi(j)}\f$ form the smallest possible scc, since both
   *   edges \f$e_1(y_j, x_{\phi(j)})\f$ and \f$e_2(x_{\phi(j)},y_j)\f$
   *   are both contained in the oriented intersection graph.
   *
   *   Hence a scc containg more than two nodes is represented as an
   *   array of SccComponent entries,
   *   \f$[ y_{j_0},x_{\phi(j_0)},\dots,y_{j_k},x_{\phi(j_k)}]\f$.
   *
   *   Parameters
   *   scclist ~ resulting sccs
   */

  template<class View>
  inline void
  computesccs(Space& home, ViewArray<View>& x, ViewArray<View>& y,
              int phi[], SccComponent sinfo[], int scclist[]) {

    // number of sccs is bounded by xs (number of x-nodes)
    int xs = x.size();
    Region r(home);
    Support::StaticStack<int,Region> cs(r,xs);

    //select an y node from the graph
    for (int j = 0; j < xs; j++) {
      int yjmin = y[j].min();    // the processed min
      while (!cs.empty() && x[phi[sinfo[cs.top()].rightmost]].max() < yjmin) {
        // the topmost scc cannot "reach" y_j or a node to the right of it
        cs.pop();
      }

      // a component has the form C(y-Node, matching x-Node)
      // C is a minimal scc in the oriented intersection graph
      // we only store y_j-Node, since \phi(j) gives the matching X-node
      int i     = phi[j];
      int ximin = x[i].min();
      while (!cs.empty() && ximin <= y[sinfo[cs.top()].rightmost].max()) {
        // y_j can "reach" cs.top() ,
        // i.e. component c can reach component cs.top()
        // merge c and cs.top() into new component
        int top = cs.top();
        // connecting
        sinfo[sinfo[j].leftmost].left        = top;
        sinfo[top].right                     = sinfo[j].leftmost;
        // moving leftmost
        sinfo[j].leftmost                    = sinfo[top].leftmost;
        // moving rightmost
        sinfo[sinfo[top].leftmost].rightmost = j;
        cs.pop();
      }
      cs.push(j);
    }
    cs.reset();


    // now we mark all components with the respective scc-number
    // labeling is bound by O(k) which is bound by O(n)

    for (int i = 0; i < xs; i++) {
      if (sinfo[i].left == i) { // only label variables in sccs
        int scc = sinfo[i].rightmost;
        int z   = i;
        //bound by the size of the largest scc = k
        while (sinfo[z].right != z) {
          sinfo[z].rightmost   = scc;
          scclist[phi[z]]      = scc;
          z                    = sinfo[z].right;
        }
        sinfo[z].rightmost     = scc;
        scclist[phi[z]]        = scc;
      }
    }
  }

  /**
   *   \brief Narrowing the domains of the x variables
   *
   *   Due to the correspondance between perfect matchings in the "reduced"
   *   intersection graph of \a x and \a y views and feasible
   *   assignments for the sorted constraint the new domain bounds for
   *   views in \a x are computed as
   *      - lower bounds:
   *        \f$ S_i \geq E_l \f$
   *        where \f$y_l\f$ is the leftmost neighbour of \f$x_i\f$
   *      - upper bounds:
   *        \f$ S_i \leq  E_h \f$
   *        where \f$y_h\f$ is the rightmost neighbour of \f$x_i\f$
   */

  template<class View, bool Perm>
  inline bool
  narrow_domx(Space& home,
              ViewArray<View>& x,
              ViewArray<View>& y,
              ViewArray<View>& z,
              int tau[],
              int[],
              int scclist[],
              SccComponent sinfo[],
              bool& nofix) {

    int xs = x.size();

    // For every x node
    for (int i = 0; i < xs; i++) {

      int xmin = x[i].min();
      /*
       * take the scc-list for the current x node
       * start from the leftmost reachable y node of the scc
       * and check which Y node in the scc is
       * really the rightmost node intersecting x, i.e.
       * search for the greatest lower bound of x
       */
      int start = sinfo[scclist[i]].leftmost;
      while (y[start].max() < xmin) {
        start = sinfo[start].right;
      }

      if (Perm) {
        // start is the leftmost-position for x_i
        // that denotes the lower bound on p_i

        ModEvent me_plb = z[i].gq(home, start);
        if (me_failed(me_plb)) {
          return false;
        }
        nofix |= (me_modified(me_plb) && start != z[i].min());
      }

      ModEvent me_lb = x[i].gq(home, y[start].min());
      if (me_failed(me_lb)) {
        return false;
      }
      nofix |= (me_modified(me_lb) &&
                y[start].min() != x[i].min());

      int ptau = tau[xs - 1 - i];
      int xmax = x[ptau].max();
      /*
       * take the scc-list for the current x node
       * start from the rightmost reachable node and check which
       * y node in the scc is
       * really the rightmost node intersecting x, i.e.
       * search for the smallest upper bound of x
       */
      start = sinfo[scclist[ptau]].rightmost;
      while (y[start].min() > xmax) {
        start = sinfo[start].left;
      }

      if (Perm) {
        //start is the rightmost-position for x_i
        //that denotes the upper bound on p_i
        ModEvent me_pub = z[ptau].lq(home, start);
        if (me_failed(me_pub)) {
          return false;
        }
        nofix |= (me_modified(me_pub) && start != z[ptau].max());
      }

      ModEvent me_ub = x[ptau].lq(home, y[start].max());
      if (me_failed(me_ub)) {
        return false;
      }
      nofix |= (me_modified(me_ub) &&
                y[start].max() != x[ptau].max());
    }
    return true;
  }

  /**
   * \brief Narrowing the domains of the y views
   *
   * analogously to the x views we take
   *    - for the upper bounds the matching \f$\phi\f$ computed in  glover
   *      and compute the new upper bound by \f$T_j=min(E_j, D_{\phi(j)})\f$
   *    - for the lower bounds the matching \f$\phi'\f$ computed in  revglover
   *      and update the new lower bound by \f$T_j=max(E_j, D_{\phi'(j)})\f$
   */

  template<class View>
  inline bool
  narrow_domy(Space& home,
              ViewArray<View>& x, ViewArray<View>& y,
              int phi[], int phiprime[], bool& nofix) {
    for (int i=x.size(); i--; ) {
      ModEvent me_lb = y[i].gq(home, x[phiprime[i]].min());
      if (me_failed(me_lb)) {
        return false;
      }
      nofix |= (me_modified(me_lb) &&
                x[phiprime[i]].min() != y[i].min());

      ModEvent me_ub = y[i].lq(home, x[phi[i]].max());
      if (me_failed(me_ub)) {
        return false;
      }
      nofix |= (me_modified(me_ub) &&
                x[phi[i]].max() != y[i].max());
    }
    return true;
  }

}}}

// STATISTICS: int-prop

