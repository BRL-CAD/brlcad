/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2007
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

namespace Gecode { namespace Int { namespace Circuit {

  template<class View, class Offset>
  forceinline
  Base<View,Offset>::Base(Home home, ViewArray<View>& x, Offset& o0)
    : NaryPropagator<View,Int::PC_INT_DOM>(home,x), y(home,x), o(o0) {
    home.notice(*this,AP_WEAKLY);
  }

  template<class View, class Offset>
  forceinline
  Base<View,Offset>::Base(Space& home, bool share, Base<View,Offset>& p)
    : NaryPropagator<View,Int::PC_INT_DOM>(home,share,p) {
    o.update(p.o);
    y.update(home,share,p.y);
  }

  /// Information required for non-recursive checking for a single scc
  template<class View>
  class SsccInfo {
  public:
    int min, low, pre;
    Int::ViewValues<View> v;
  };

  /// Information for performing a recorded tell
  template<class View>
  class TellInfo {
  public:
    View x; int n;
  };

  template<class View, class Offset>
  ExecStatus
  Base<View,Offset>::connected(Space& home) {
    int n = x.size();

    /// First non-assigned node.
    int start = 0;
    while (x[start].assigned()) {
      start = o(x[start]).val();
      if (start == 0) break;
    }

    /// Information needed for checking scc's
    Region r(home);
    typedef typename Offset::ViewType OView;
    SsccInfo<OView>* si = r.alloc<SsccInfo<OView> >(n);
    unsigned int n_edges = 0;
    for (int i=n; i--; ) {
      n_edges += x[i].size();
      si[i].pre=-1;
    }

    // Stack to remember which nodes have not been processed completely
    Support::StaticStack<int,Region> next(r,n);

    // Array to remember which mandatory tells need to be done
    TellInfo<OView>* eq = r.alloc<TellInfo<OView> >(n);
    int n_eq = 0;

    // Array to remember which edges need to be pruned
    TellInfo<OView>* nq = r.alloc<TellInfo<OView> >(n_edges);
    int n_nq = 0;

    /*
     * Check whether there is a single strongly connected component.
     * This is a downstripped version of Tarjan's algorithm as
     * the computation of sccs proper is not needed. In addition, it
     * checks a mandatory condition for a graph to be Hamiltonian
     * (due to Mats Carlsson).
     *
     * To quote Mats: Suppose you do a depth-first search of the graph.
     * In that search, the root node will have a number of child subtrees
     * T1, ..., Tn. By construction, if i<j then there is no edge from
     * Ti to Tj. The necessary condition for Hamiltonianicity is that
     * there be an edge from Ti+1 to Ti, for 0 < i < n.
     *
     * In addition, we do the following: if there is only a single edge
     * from Ti+1 to Ti, then it must be mandatory and the variable must
     * be assigned to that value.
     *
     * The same holds true for a back edge from T0 to the root node.
     *
     * Then, all edges that reach from Ti+k+1 to Ti can be pruned.
     *
     */

    {
      // Start always at node start
      int i = start;
      // Counter for scc
      int cnt = 0;
      // Smallest preorder number of last subtree (initially, the root node)
      int subtree_min = 0;
      // Largest preorder number of last subtree (initially, the root node)
      int subtree_max = 0;
      // Number of back edges into last subtree or root
      int back = 0;
    start:
      si[i].min = si[i].pre = si[i].low = cnt++;
      si[i].v.init(o(x[i]));
      do {
        if (si[si[i].v.val()].pre < 0) {
          next.push(i);
          i=si[i].v.val();
          goto start;
        } else if ((subtree_min <= si[si[i].v.val()].pre) &&
                 (si[si[i].v.val()].pre <= subtree_max)) {
          back++;
          eq[n_eq].x = o(x[i]);
          eq[n_eq].n = si[i].v.val();
        } else if (si[si[i].v.val()].pre < subtree_min) {
          nq[n_nq].x = o(x[i]);
          nq[n_nq].n = si[i].v.val();
          n_nq++;
        }
      cont:
        if (si[si[i].v.val()].low < si[i].min)
          si[i].min = si[si[i].v.val()].low;
        ++si[i].v;
      } while (si[i].v());
      if (si[i].min < si[i].low) {
        si[i].low = si[i].min;
      } else if (i != start) {
        // If it is not the first node visited, there is more than one SCC
        return ES_FAILED;
      }
      if (!next.empty()) {
        i=next.pop();
        if (i == start) {
          // No back edge
          if (back == 0)
            return ES_FAILED;
          // Exactly one back edge, make it mandatory (keep topmost entry)
          if (back == 1)
            n_eq++;
          back        = 0;
          subtree_min = subtree_max+1;
          subtree_max = cnt-1;
        }
        goto cont;
      }
      // Whether all nodes have been visited
      if (cnt != n)
        return ES_FAILED;
      ExecStatus es = ES_FIX;
      // Assign all mandatory edges
      while (n_eq-- > 0) {
        ModEvent me = eq[n_eq].x.eq(home,eq[n_eq].n);
        if (me_failed(me))
          return ES_FAILED;
        if (me_modified(me))
          es = ES_NOFIX;
      }
      // Remove all edges that would require a non-simple cycle
      while (n_nq-- > 0) {
        ModEvent me = nq[n_nq].x.nq(home,nq[n_nq].n);
        if (me_failed(me))
          return ES_FAILED;
        if (me_modified(me))
          es = ES_NOFIX;
      }
      return es;
    }
  }

  template<class View, class Offset>
  ExecStatus
  Base<View,Offset>::path(Space& home) {
    // Prunes that partial assigned paths are not completed to cycles

    int n=x.size();

    Region r(home);

    // The path starting at assigned x[i] ends at x[end[j]] which is
    // not assigned.
    int* end = r.alloc<int>(n);
    for (int i=n; i--; )
      end[i]=-1;

    // A stack that records all indices i such that end[i] != -1
    Support::StaticStack<int,Region> tell(r,n);

    typedef typename Offset::ViewType OView;
    for (int i=y.size(); i--; ) {
      assert(!y[i].assigned());
      // Non-assigned views serve as starting points for assigned paths
      Int::ViewValues<OView> v(o(y[i]));
      // Try all connected values
      do {
        int j0=v.val();
        // Starting point for not yet followed assigned path found
        if (x[j0].assigned() && (end[j0] < 0)) {
          // Follow assigned path until non-assigned view:
          // all assigned view on the paths can be skipped, as
          // if x[i] is assigned to j, then x[j] will only have
          // x[i] as predecessor due to propagating distinct.
          int j = j0;
          do {
            j=o(x[j]).val();
          } while (x[j].assigned());
          // Now there cannot be a cycle from x[j] to x[v.val()]!
          // However, the tell cannot be done here as j might be
          // equal to i and might hence kill the iterator v!
          end[j0]=j; tell.push(j0);
        }
        ++v;
      } while (v());
    }

    // Now do the tells based on the end information
    while (!tell.empty()) {
      int i = tell.pop();
      assert(end[i] >= 0);
      GECODE_ME_CHECK(o(x[end[i]]).nq(home,i));
    }
    return ES_NOFIX;
  }

  template<class View, class Offset>
  forceinline size_t
  Base<View,Offset>::dispose(Space& home) {
    home.ignore(*this,AP_WEAKLY);
    (void) NaryPropagator<View,Int::PC_INT_DOM>::dispose(home);
    return sizeof(*this);
  }

}}}

// STATISTICS: int-prop

