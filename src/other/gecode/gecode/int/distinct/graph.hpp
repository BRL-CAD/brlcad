/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

#include <climits>

namespace Gecode { namespace Int { namespace Distinct {

  template<class View>
  forceinline
  Graph<View>::Graph(void) {}

  template<class View>
  forceinline ExecStatus
  Graph<View>::init(Space& home, ViewArray<View>& x) {
    using namespace ViewValGraph;
    n_view = x.size();
    view = home.alloc<ViewNode<View>*>(n_view);

    // Find value information for construction of view value graph
    int min = x[n_view-1].min();
    int max = x[n_view-1].max();
    for (int i=n_view-1; i--; ) {
      min = std::min(min,x[i].min());
      max = std::max(max,x[i].max());
    }

    unsigned int width = static_cast<unsigned int>(max-min+1);

    // Definitly not enough values
    if (width < static_cast<unsigned int>(n_view))
      return ES_FAILED;

    // Initialize view nodes
    for (int i=n_view; i--; )
      view[i] = new (home) ViewNode<View>(x[i]);

    Region r(home);

    if (static_cast<unsigned int>(n_view)*4 >= width) {
      // Values are dense: use a mapping
      ValNode<View>** val2node = r.alloc<ValNode<View>* >(width);

      for (unsigned int i=width; i--; )
        val2node[i]=NULL;

      for (int i=n_view; i--; ) {
        Edge<View>** edge_p = view[i]->val_edges_ref();
        for (ViewValues<View> xi(x[i]); xi(); ++xi) {
          if (val2node[xi.val()-min] == NULL)
            val2node[xi.val()-min] = new (home) ValNode<View>(xi.val());
          *edge_p = new (home) Edge<View>(val2node[xi.val()-min],view[i]);
          edge_p = (*edge_p)->next_edge_ref();
        }
        *edge_p = NULL;
      }

      for (unsigned int i=width; i--; )
        if (val2node[i] != NULL) {
          val2node[i]->next_val(val);
          val = val2node[i];
          n_val++;
        }

    } else {
      // Values are sparse
      for (int i=n_view; i--; )
        ViewValGraph::Graph<View>::init(home,view[i]);
    }

    if (n_val < n_view)
      return ES_FAILED;

    typename ViewValGraph::Graph<View>::ViewNodeStack m(r,n_view);
    for (int i = n_view; i--; )
      if (!match(m,view[i]))
        return ES_FAILED;
    return ES_OK;
  }

  template<class View>
  forceinline bool
  Graph<View>::sync(Space& home) {
    using namespace ViewValGraph;
    Region r(home);
    // Stack for view nodes to be rematched
    typename ViewValGraph::Graph<View>::ViewNodeStack re(r,n_view);
    // Synchronize nodes
    for (int i = n_view; i--; ) {
      ViewNode<View>* x = view[i];
      if (x->view().assigned()) {
        x->edge_fst()->val(x)->matching(NULL);
        for (Edge<View>* e = x->val_edges(); e != NULL; e = e->next_edge())
          e->unlink();
        view[i] = view[--n_view];
      } else if (x->changed()) {
        ViewRanges<View> r(x->view());
        Edge<View>*  m = x->edge_fst();      // Matching edge
        Edge<View>** p = x->val_edges_ref();
        Edge<View>*  e = *p;
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
        if (m->marked()) {
          // Matching has been deleted!
          m->val(x)->matching(NULL);
          re.push(x);
        }
        x->update();
      } else {
        // Just free edges
        for (Edge<View>* e = x->val_edges(); e != NULL; e = e->next_edge())
          e->free();
      }
    }

    typename ViewValGraph::Graph<View>::ViewNodeStack m(r,n_view);
    while (!re.empty())
      if (!match(m,re.pop()))
        return false;
    return true;
  }


  template<class View>
  forceinline bool
  Graph<View>::mark(Space& home) {
    using namespace ViewValGraph;

    Region r(home);

    int n_view_visited = 0;
    {
      // Marks all edges as used that are on simple paths in the graph
      // that start from a free (unmatched node) by depth-first-search
      Support::StaticStack<ValNode<View>*,Region> visit(r,n_val);
      
      // Insert all free nodes: they can be only value nodes as we
      // have a maximum matching covering all view nodes
      count++;
      {
        ValNode<View>** v = &val;
        while (*v != NULL)
          if (!(*v)->matching()) {
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
        ValNode<View>* n = visit.pop();
        for (Edge<View>* e = n->edge_fst(); e != n->edge_lst(); e=e->next()) {
          // Get the value node
          e->use();
          ViewNode<View>* x = e->view(n);
          if (x->min < count) {
            n_view_visited++;
            x->min = count;
            assert(x->edge_fst()->next() == x->edge_lst());
            ValNode<View>* m = x->edge_fst()->val(x);
            x->edge_fst()->use();
            if (m->min < count) {
              m->min = count;
              visit.push(m);
            }
          }
        }
      }
    }

    // If all view nodes have been visited, also all edges are used!
    if (n_view_visited < n_view) {
      scc(home);
      return true;
    } else {
      return false;
    }
  }

  template<class View>
  forceinline ExecStatus
  Graph<View>::prune(Space& home, bool& assigned) {
    using namespace ViewValGraph;
    assigned = false;
    // Tell constraints and also eliminate nodes and edges
    for (int i = n_view; i--; ) {
      ViewNode<View>* x = view[i];
      if (!x->edge_fst()->used(x)) {
        GECODE_ME_CHECK(x->view().eq(home,x->edge_fst()->val(x)->val()));
        x->edge_fst()->val(x)->matching(NULL);
        for (Edge<View>* e = x->val_edges(); e != NULL; e = e->next_edge())
          e->unlink();
        view[i] = view[--n_view];
        assigned = true;
      } else {
        IterPruneVal<View> pv(view[i]);
        GECODE_ME_CHECK(view[i]->view().minus_v(home,pv,false));
      }
    }
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

