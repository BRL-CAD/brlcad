/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Guido Tack, 2004
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

#ifndef __GECODE_INT_VIEW_VAL_GRAPH_HH__
#define __GECODE_INT_VIEW_VAL_GRAPH_HH__

#include <gecode/int.hh>

/**
 * \namespace Gecode::Int::ViewValGraph
 * \brief Support classes for propagators using a view-value graph
 */
namespace Gecode { namespace Int { namespace ViewValGraph {

  /**
   * \brief Class for combining two pointers with a flag
   *
   * When one pointer is given, the other can be retrieved.
   *
   */
  template<class T>
  class CombPtrFlag {
  private:
    /// Store pointer and flag
    ptrdiff_t cpf;
  public:
    /// Initialize with pointer \a p1 and \a p2
    CombPtrFlag(T* p1, T* p2);
    /// Initialize with pointer \a p1 and \a p2
    void init(T* p1, T* p2);
    /// Return the other pointer when \a p is given
    T* ptr(T* p) const;
    /// Check whether flag is set
    int is_set(void) const;
    /// Set flag
    void set(void);
    /// Clear flag
    void unset(void);
  };

  /// Bidirectional links for edges and anchors in nodes of view-value graph
  class BiLink {
  private:
    /// Link to previous element
    BiLink* _prev; 
    /// Link to next element
    BiLink* _next;
  public:
    /// Initialize as empty (self referenced)
    BiLink(void);
    /// Return previous element
    BiLink* prev(void) const;
    /// Return next element
    BiLink* next(void) const; 
    /// Set previous element to \a l
    void prev(BiLink* l);
    /// Set next element to \a l
    void next(BiLink* l);
    /// Add \a l after this element
    void add(BiLink* l);
    /// Unlink this element
    void unlink(void);
    /// Mark element (invalidates next element pointer)
    void mark(void);
    /// Whether element is marked
    bool marked(void) const;
    /// Whether element has no previous and next element
    bool empty(void) const;
  };


  template<class View> class Edge;

  /**
   * \brief Base-class for nodes (both view and value nodes)
   *
   * Note: the obvious ill-design to have also nodes and edges
   * parametric wrt View is because the right design (having template
   * function members) gets miscompiled (and actually not even compiled
   * with some C++ compilers). Duh!
   *
   */
  template<class View>
  class Node : public BiLink {
  public:
    /// Next edge for computing strongly connected components
    Edge<View>* iter;
    /// Values for computing strongly connected components
    unsigned int low, min, comp;
    /// Initialize
    Node(void);
    /// Return first edge (organized by bi-links)
    Edge<View>* edge_fst(void) const;
    /// Return last edge (organized by bi-links)
    Edge<View>* edge_lst(void) const;

    /// Allocate memory from space
    static void* operator new(size_t, Space&);
    /// Needed for exceptions
    static void  operator delete(void*, size_t);
    /// Needed for exceptions
    static void  operator delete(void*,Space&);
  };

  /**
   * \brief Value nodes in view-value graph
   *
   */
  template<class View>
  class ValNode : public Node<View> {
  protected:
    /// The value of the node
    const int      _val;
    /// The matching edge
    Edge<View>*    _matching;
    /// The next value node
    ValNode<View>* _next_val;
  public:
    /// Initialize with value \a v
    ValNode(int v);
    /// Initialize with value \a v and successor \a n
    ValNode(int v, ValNode<View>* n);
    /// Return value of node
    int val(void) const;
    /// Set matching edge to \a m
    void matching(Edge<View>* m);
    /// Return matching edge (NULL if unmatched)
    Edge<View>* matching(void) const;
    /// Return pointer to next value node fields
    ValNode<View>** next_val_ref(void);
    /// Return next value node
    ValNode<View>* next_val(void) const;
    /// Set next value node to \a v
    void next_val(ValNode<View>* v);
  };

  /**
   * \brief View nodes in view-value graph
   *
   */
  template<class View>
  class ViewNode : public Node<View> {
  protected:
    /// The size of the view after last change
    unsigned int _size;
    /// The node's view
    View        _view;
    /// The first value edge
    Edge<View>* _val_edges;
  public:
    /// Initialize node for a non-view
    ViewNode(void);
    /// Initialize new node for view \a x
    ViewNode(View x);
    /// Return first edge of all value edges
    Edge<View>*  val_edges(void) const;
    /// Return pointer to first edge fields of all value edges
    Edge<View>** val_edges_ref(void);
    /// Test whether node has a fake view
    bool fake(void) const;
    /// Return view
    View view(void) const;
    /// Update size of view after change
    void update(void);
    /// Return whether view has changed its size
    bool changed(void) const;
    /// Whether the node is matched
    bool matched(void) const;
  };

  /**
   * \brief Edges in view-value graph
   *
   */
  template<class View>
  class Edge : public BiLink {
  protected:
    /// Next edge in chain of value edges
    Edge<View>* _next_edge;
    /// Combine source and destination node and flag
    CombPtrFlag<Node<View> > sd;
  public:
    /// Construct new edge between \a x and \a v
    Edge(ValNode<View>* v, ViewNode<View>* x);
    /// Construct new edge between \a x and \a v with next edge \a n
    Edge(ValNode<View>* v, ViewNode<View>* x, Edge<View>* n);
    /// Return destination of edge when source \a s is given
    Node<View>* dst(Node<View>* s) const;

    /// Return view node when value node \a v is given
    ViewNode<View>* view(ValNode<View>* v) const;
    /// Return value node when view node \a x is given
    ValNode<View>* val(ViewNode<View>* x) const;

    /// Whether edge is used (marked or between nodes from the same scc)
    bool used(Node<View>* v) const;
    /// Mark node as used
    void use(void);
    /// Unmark node as used
    void free(void);

    /// Revert edge to node \a d for matching
    void revert(Node<View>* d);

    /// Return next edge in list of value edges
    Edge<View>*  next_edge(void) const;
    /// Return reference to next edge in list of value edges
    Edge<View>** next_edge_ref(void);
    /// Return next edge in list of edges per node
    Edge<View>* next(void) const;

    /// Allocate memory from space
    static void* operator new(size_t, Space&);
    /// Needed for exceptions
    static void  operator delete(void*, size_t);
    /// Needed for exceptions
    static void  operator delete(void*,Space&);
  };

  /// Iterates the values to be pruned from a view node
  template<class View>
  class IterPruneVal {
  protected:
    /// View node
    ViewNode<View>* x;
    /// Current value edge
    Edge<View>* e;
  public:
    /// \name Constructors and initialization
    //@{
    /// Initialize with edges for view node \a x
    IterPruneVal(ViewNode<View>* x);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a value or done
    bool operator ()(void) const;
    /// Move iterator to next value (if possible)
    void operator ++(void);
    //@}

    /// \name Value access
    //@{
    /// Return current value
    int val(void) const;
    //@}
  };

}}}

#include <gecode/int/view-val-graph/comb-ptr-flag.hpp>
#include <gecode/int/view-val-graph/bi-link.hpp>
#include <gecode/int/view-val-graph/edge.hpp>
#include <gecode/int/view-val-graph/node.hpp>
#include <gecode/int/view-val-graph/iter-prune-val.hpp>

namespace Gecode { namespace Int { namespace ViewValGraph {

  /// View-value graph base class 
  template<class View>
  class Graph {
  protected:
    /// Array of view nodes
    ViewNode<View>** view;
    /// Array of value nodes
    ValNode<View>* val;
    /// Number of view nodes
    int n_view;
    /// Number of value nodes
    int n_val;
    /// Marking counter
    unsigned int count;
    /// Stack used during matching
    typedef Support::StaticStack<ViewNode<View>*,Region> ViewNodeStack;
    /// Initialize the edges for the view node \a x
    void init(Space& home, ViewNode<View>* x);
    /// Find a matching for node \a x
    bool match(ViewNodeStack& m, ViewNode<View>* x);
    /// Compute the strongly connected components
    void scc(Space& home);
  public:
    /// Construct graph as not yet initialized
    Graph(void);
    /// Test whether graph has been initialized
    bool initialized(void) const;
    /// Purge graph if necessary (reset information to avoid overflow)
    void purge(void);
  };

}}}

#include <gecode/int/view-val-graph/graph.hpp>

#endif

// STATISTICS: int-prop

