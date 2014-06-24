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

namespace Gecode { namespace Int { namespace ViewValGraph {

  template<class View>
  forceinline
  Graph<View>::Graph(void)
    : view(NULL), val(NULL), n_view(0), n_val(0), count(1U) {}

  template<class View>
  forceinline bool
  Graph<View>::initialized(void) const {
    return view != NULL;
  }

  template<class View>
  forceinline void
  Graph<View>::init(Space& home, ViewNode<View>* x) {
    Edge<View>** edge_p = x->val_edges_ref();
    ViewValues<View> xi(x->view());
    ValNode<View>** v = &val;
    while (xi() && (*v != NULL)) {
      if ((*v)->val() == xi.val()) {
        // Value node does already exist, create new edge
        *edge_p = new (home) Edge<View>(*v,x);
        edge_p = (*edge_p)->next_edge_ref();
        v = (*v)->next_val_ref();
        ++xi;
      } else if ((*v)->val() < xi.val()) {
        // Skip to next value node
        v = (*v)->next_val_ref();
      } else {
        // Value node does not yet exist, create and link it
        ValNode<View>* nv = new (home) ValNode<View>(xi.val(),*v);
        *v = nv; v = nv->next_val_ref();
        *edge_p = new (home) Edge<View>(nv,x);
        edge_p = (*edge_p)->next_edge_ref();
        ++xi; n_val++;
      }
    }
    // Create missing value nodes
    while (xi()) {
      ValNode<View>* nv = new (home) ValNode<View>(xi.val(),*v);
      *v = nv; v = nv->next_val_ref();
      *edge_p = new (home) Edge<View>(nv,x);
      edge_p = (*edge_p)->next_edge_ref();
      ++xi; n_val++;
    }
    *edge_p = NULL;
  }

  template<class View>
  forceinline bool
  Graph<View>::match(ViewNodeStack& m, ViewNode<View>* x) {
    count++;
  start:
    // Try to find matching edge cheaply: is there a free edge around?
    {
      Edge<View>* e = x->val_edges();
      // This holds true as domains are never empty
      assert(e != NULL);
      do {
        if (!e->val(x)->matching()) {
          e->revert(x); e->val(x)->matching(e);
          // Found a matching, revert all edges on stack
          while (!m.empty()) {
            x = m.pop(); e = x->iter;
            e->val(x)->matching()->revert(e->val(x));
            e->revert(x); e->val(x)->matching(e);
          }
          return true;
        }
        e = e->next_edge();
      } while (e != NULL);
    }
    // No, find matching edge by augmenting path method
    Edge<View>* e = x->val_edges();
    do {
      if (e->val(x)->matching()->view(e->val(x))->min < count) {
        e->val(x)->matching()->view(e->val(x))->min = count;
        m.push(x); x->iter = e;
        x = e->val(x)->matching()->view(e->val(x));
        goto start;
      }
    next:
      e = e->next_edge();
    } while (e != NULL);
    if (!m.empty()) {
      x = m.pop(); e = x->iter; goto next;
    }
    // All nodes and edges unsuccessfully tried
    return false;
  }

  template<class View>
  forceinline void
  Graph<View>::purge(void) {
    if (count > (UINT_MAX >> 1)) {
      count = 1;
      for (int i=n_view; i--; )
        view[i]->min = 0;
      for (ValNode<View>* v = val; v != NULL; v = v->next_val())
        v->min = 0;
    }
  }

  template<class View>
  forceinline void
  Graph<View>::scc(Space& home) {
    Region r(home);

    Support::StaticStack<Node<View>*,Region> scc(r,n_val+n_view);
    Support::StaticStack<Node<View>*,Region> visit(r,n_val+n_view);
      
    count++;
    unsigned int cnt0 = count;
    unsigned int cnt1 = count;
    
    for (int i = n_view; i--; )
      /*
       * The following test is subtle: for scc, the test should be:
       *   view[i]->min < count
       * However, if view[i] < count-1, then the node has already been
       * reached on a path and all edges connected to the node have been
       * marked anyway! So just ignore this node altogether for scc.
       */ 
      if (view[i]->min < count-1) {
        Node<View>* w = view[i];
      start:
        w->low = w->min = cnt0++;
        scc.push(w);
        Edge<View>* e = w->edge_fst();
        while (e != w->edge_lst()) {
          if (e->dst(w)->min < count) {
            visit.push(w); w->iter = e;
            w=e->dst(w);
            goto start;
          }
        next:
          if (e->dst(w)->low < w->min)
            w->min = e->dst(w)->low;
          e = e->next();
        }
        if (w->min < w->low) {
          w->low = w->min;
        } else {
          Node<View>* v;
          do {
            v = scc.pop();
            v->comp = cnt1;
            v->low  = UINT_MAX;
          } while (v != w);
          cnt1++;
        }
        if (!visit.empty()) {
          w=visit.pop(); e=w->iter; goto next;
        }
      }
    count = cnt0+1;
  }


}}}

// STATISTICS: int-prop
