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

namespace Gecode { namespace Int { namespace NValues {

  forceinline
  Graph::Graph(void)
    : n_matched(0) {}

  forceinline int
  Graph::size(void) const {
    return n_matched;
  }

  forceinline void
  Graph::init(Space& home, const ValSet& vs, const ViewArray<IntView>& x) {
    using namespace ViewValGraph;
    n_view = x.size() + vs.size();
    view = home.alloc<ViewNode<IntView>*>(n_view);

    // Create nodes corresponding to the value set vs
    {
      int i = x.size();
      ValSet::Ranges vsr(vs);
      ValNode<IntView>** v = &val;
      for (Iter::Ranges::ToValues<ValSet::Ranges> n(vsr); n(); ++n) {
        // Create view node
        view[i] = new (home) ViewNode<IntView>();
        // Create and link value node
        ValNode<IntView>* nv = new (home) ValNode<IntView>(n.val());
        *v = nv; v = nv->next_val_ref();
        // Create and link single edge
        Edge<IntView>** e = view[i]->val_edges_ref();
        *e = new (home) Edge<IntView>(nv,view[i],NULL);
        // Match edge
        (*e)->revert(view[i]); nv->matching(*e);
        i++;
      }
      *v = NULL;
      n_val = vs.size();
      n_matched = vs.size();
      assert(i - x.size() == vs.size());
    }

    // Initialize real view nodes
    for (int i=x.size(); i--; ) {
      view[i] = new (home) ViewNode<IntView>(x[i]);
      ViewValGraph::Graph<IntView>::init(home,view[i]);
    }

    // Match the real view nodes, if possible
    Region r(home);
    ViewNodeStack m(r,n_view);
    for (int i = x.size(); i--; )
      if (match(m,view[i]))
        n_matched++;
  }

  forceinline void
  Graph::sync(Space& home) {
    using namespace ViewValGraph;
    Region r(home);

    // Whether to rematch
    bool rematch = false;

    // Synchronize nodes
    for (int i = n_view; i--; ) {
      ViewNode<IntView>* x = view[i];
      // Skip faked view nodes, they correspond to values in the value set
      if (!x->fake()) {
        if (x->changed()) {
          ViewRanges<IntView> r(x->view());
          Edge<IntView>*  m = x->matched() ? x->edge_fst() : NULL;
          Edge<IntView>** p = x->val_edges_ref();
          Edge<IntView>*  e = *p;
          do {
            while (e->val(x)->val() < r.min()) {
              // Skip edge
              e->unlink(); e->mark();
              e = e->next_edge();
            }
            *p = e;
            assert(r.min() == e->val(x)->val());
            // This edges must be kept
            for (unsigned int j=r.width(); j--; ) {
              e->free();
              p = e->next_edge_ref();
              e = e->next_edge();
            }
            ++r;
          } while (r());
          *p = NULL;
          while (e != NULL) {
            e->unlink(); e->mark();
            e = e->next_edge();
          }
          if ((m != NULL) && m->marked()) {
            // Matching has been deleted!
            m->val(x)->matching(NULL);
            rematch = true;
            n_matched--;
          }
        } else {
          // Just free edges
          for (Edge<IntView>* e=x->val_edges(); e != NULL; e = e->next_edge())
            e->free();
        }
      }
    }

    if (rematch) {
      ViewNodeStack m(r,n_view);
      for (int i = n_view; i--; )
        if (!view[i]->matched() && match(m,view[i]))
          n_matched++;
    }
  }

  forceinline bool
  Graph::mark(Space& home) {
    using namespace ViewValGraph;
    Region r(home);

    int n_view_visited = 0;
    {
      // Marks all edges as used that are on simple paths in the graph
      // that start from a free value node by depth-first-search
      Support::StaticStack<ValNode<IntView>*,Region> visit(r,n_val);
      
      // Insert all free value nodes
      count++;
      {
        ValNode<IntView>** v = &val;
        while (*v != NULL)
          // Is the node free?
          if (!(*v)->matching()) {
            // Eliminate empty value nodes
            if ((*v)->empty()) {
              *v = (*v)->next_val();
              n_val--;
            } else {
              (*v)->min = count;
              visit.push(*v);
              v = (*v)->next_val_ref();
            }
          } else {
            v = (*v)->next_val_ref();
          }
      }
      
      // Invariant: only value nodes are on the stack!
      while (!visit.empty()) {
        ValNode<IntView>* n = visit.pop();
        for (Edge<IntView>* e = n->edge_fst(); e != n->edge_lst(); 
             e = e->next()) {
          // Is the view node is matched: the path must be alternating!
          ViewNode<IntView>* x = e->view(n);
          if (x->matched()) {
            // Mark the edge as used
            e->use();
            if (x->min < count) {
              n_view_visited++;
              x->min = count;
              assert(x->edge_fst()->next() == x->edge_lst());
              ValNode<IntView>* m = x->edge_fst()->val(x);
              x->edge_fst()->use();
              if (m->min < count) {
                m->min = count;
                visit.push(m);
              }
            }
          }
        }
      }
      
    }

    if (n_view_visited < n_view) {
      // Mark all edges as used starting from a free view node on
      // an alternating path by depth-first search.
      Support::StaticStack<ViewNode<IntView>*,Region> visit(r,n_view);
      
      // Insert all free view nodes
      count++;
      for (int i = n_view; i--; )
        if (!view[i]->matched()) {
          view[i]->min = count;
          visit.push(view[i]);
        }

      // Invariant: only view nodes are on the stack!
      while (!visit.empty()) {
        n_view_visited++;
        ViewNode<IntView>* x = visit.pop();
        for (Edge<IntView>* e = x->val_edges(); e != NULL; e = e->next_edge())
          // Consider only free edges
          if (e != x->edge_fst()) {
            ValNode<IntView>* n = e->val(x);
            // Is there a matched edge from the value node to a view node?
            if (n->matching() != NULL) {
              e->use();
              n->matching()->use();
              ViewNode<IntView>* y = n->matching()->view(n);
              if (y->min < count) {
                y->min = count;
                visit.push(y);
              }
            }
          }
      }

    }

    if (n_view_visited < n_view) {
      scc(home);
      return true;
    } else {
      return false;
    }
  }

  forceinline ExecStatus
  Graph::prune(Space& home) {
    using namespace ViewValGraph;
    // Tell constraints and also eliminate nodes and edges
    for (int i = n_view; i--; ) {
      ViewNode<IntView>* x = view[i];
      if (!x->fake()) {
        if (x->matched() && !x->edge_fst()->used(x)) {
          GECODE_ME_CHECK(x->view().eq(home,x->edge_fst()->val(x)->val()));
          x->edge_fst()->val(x)->matching(NULL);
          for (Edge<IntView>* e = x->val_edges(); e != NULL; e=e->next_edge())
            e->unlink();
          view[i] = view[--n_view];
        } else {
          IterPruneVal<IntView> pv(x);
          GECODE_ME_CHECK(x->view().minus_v(home,pv,false));
        }
      }
    }

    return ES_OK;
  }

}}}

// STATISTICS: int-prop

