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
 *     Patrick Pekczynski, 2005
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

  /**
   * \brief Bounds constraint (BC) type
   *
   * If BC = UBC, then we argue about the Upper Bounds Constraint
   * else we use the functions for the Lower Bounds Constraint
   */
  enum BC {UBC = 1, LBC = 0};

  class Edge;
  /// Base class for nodes in the variable-value-graph
  class Node {
  protected:
    /// Stores all incident edges on the node
    Edge* e;
    /// First edge
    Edge* fst;
    /// Last edge
    Edge* lst;
    /// Single incoming edge used for storing a path in the algorithms
    Edge* ie;
    /// Index
    int idx;
    /// Flags for nodes
    enum NodeFlag {
      /// No flags set
      NF_NONE  = 0,
      /// Whether node is a value node
      NF_VAL   = 1 << 0,
      /// Whether matched for LBC
      NF_M_LBC = 1 << 1,
      /// Whether matched for UBC
      NF_M_UBC = 1 << 2
    };
    /// Flags for node
    unsigned char nf;
  public:
    /// stores the number of incident edges on the node
    int noe;

    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Node(void);
    /// Constructor for index \a i that sets type to \a t
    Node(NodeFlag nf, int i);
    //@}

    /// \name Access
    //@{
    /// Return the type of the node (false for a variable node)
    bool type(void) const;
    /// Return reference to the incident edges
    Edge** adj(void);
    /// Return pointer to the first incident edge
    Edge* first(void) const;
    /// Return pointer to the last incident edge
    Edge* last(void) const;
    /// Return pointer to the node's inedge
    Edge* inedge(void) const;
    /// Get index of either variable or value
    int index(void) const;
    /// check whether a node has been removed from the graph
    bool removed(void) const;
    //@}

    /// \name Update
    //@{
    /// Set the first edge pointer to \a p
    void first(Edge* p);
    /// Set the last edge pointer to \a p
    void last(Edge* p);
    /// Set the inedge pointer to \a p
    void inedge(Edge* p);
    /// Set index of either variable or value
    void index(int i);
    //@}

    /// \name Memory management
    //@{
    /// Allocate memory from space
    static void* operator new(size_t s, Space& home);
    /// Free memory (unused)
    static void operator delete(void*, Space&) {};
    /// Needed for exceptions
    static void  operator delete(void*) {};
    //@}
  };

  /// %Variable node
  class VarNode : public Node {
  protected:
    /// Stores the matching edge on this node in the UBC
    Edge* ubm;
    /// Stores the matching edge on this node in the LBC
    Edge* lbm;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    VarNode(void);
    /// Creates a variable node with index \a i
    VarNode(int i);
    //@}

    /// \name Access
    //@{
    /// Return the matching edge on the node
    Edge* get_match(BC bc) const;
    /// tests whether the node is matched or not
    bool matched(BC bc) const;
    //@}

    /// \name Update
    //@{
    /// Set the pointer of the matching edge to m
    void set_match(BC bc, Edge* m);
    /// Set node to matched
    void match(BC bc);
    /// Unmatch the node
    void unmatch(BC bc);
    //@}
  };

  /// Value node
  class ValNode : public Node {
  protected:
    /// Minimal required occurence of the value as stored in k
    int _klb;
    /// Maximal required occurence of the value as stored in k
    int _kub;
    /// Index to acces the value via cardinality array k
    int _kidx;
    /// Stores the current number of occurences of the value
    int _kcount;
    /// Store numbre of conflicting matching edges
    int noc;
    /// Minimal capacity of the value node
    int lb;
    /// Smallest maximal capacity of the value node
    int ublow;
    /// Maximal capacity of the value node
    int ub;
  public:
    /// Stores the value of the node
    int val;

    /// \name Constructors and destructors
    //@{
    /// Default constructor
    ValNode(void);
    /**
     * \brief Constructor for value node
     *
     * with minimal capacity \a min,
     * maximal capacity \a max,
     * the value \a value and the index \a k_access in \a k
     */
    ValNode(int min, int max, int value, int kidx, int kshift, int count);
    //@}

    /// \name Access
    //@{
    /// get max cap for LBC
    int maxlow(void) const;
    /// Mark the value node as conflicting in case of variable cardinalities
    void card_conflict(int c);
    /// Check whether the value node is conflicting
    int card_conflict(void) const;
    /// Reduce the conflict counter
    void red_conflict(void);
    /// increases the value counter
    void inc(void);
    /// returns the current number of occurences of the value
    int kcount(void) const;
    /// returns the number of incident matching edges on a value node
    int incid_match(BC bc) const;
    /// returns the index in cardinality array k
    int kindex(void) const;
    /// returns \a true if the node is matched in BC, \a false otherwise
    bool matched(BC bc) const;
    /// tests whether the node is a sink
    bool sink(void) const;
    /// tests whether the node is a source
    bool source(void) const;
    /// return the minimal node capacity as stored in \a k
    int kmin(void) const;
    /// return the maximal node capacity as stored in \a k
    int kmax(void) const;
    /// return minimal or maximal capacity
    int kbound(BC bc) const;
    //@}

    /// \name Update
    //@{
    /// set the max cap for LBC
    void maxlow(int i);
    /// Set how often value occurs
    void kcount(int);
    /// changes the index in the cardinality array k
    void kindex(int);
    /// decrease the node-capacity
    void dec(BC bc);
    /// increase the node-capacity
    void inc(BC bc);
    /// return the the node-capacity
    int cap(BC bc) const;
    /// set the node-capacity to \a c
    void cap(BC bc, int c);
    /// match the node
    void match(BC bc);
    /// unmatch the node
    void unmatch(BC bc);
    /// node reset to original capacity values
    void reset(void);
    /// set the minimal k-capacity to min
    void kmin(int min);
    /// set the maximal k-capacity to max
    void kmax(int max);
    //@}
  };

  /// Class for edges \f$ e(x,v) \f$ in the variable-value-graph
  class Edge {
  private:
    /// pointer to the variable node
    VarNode* x;
    /// pointer to the value node
    ValNode* v;
    /// pointer to the next edge incident on the same variable node
    Edge* next_edge;
    /// pointer to the previous edge incident on the same variable node
    Edge* prev_edge;
    /// pointer to the next edge on the same value node
    Edge* next_vedge;
    /// pointer to the previous edge on the same value node
    Edge* prev_vedge;
    /// Flags for edges
    enum EdgeFlag {
      /// No flags set
      EF_NONE  = 0,
      /// Whether edge is used in LBC
      EF_MRKLB = 1 << 0,
      /// Whether edge is used in UBC
      EF_MRKUB = 1 << 1,
      /// Whether edge is matched in LBC
      EF_LM    = 1 << 2,
      /// Whether edge is matched in UBC
      EF_UM    = 1 << 3,
      /// Whether edge has been deleted
      EF_DEL   = 1 << 4
    };
    /// Flags for edges
    unsigned char ef;
  public:
    /// \name Constructors
    //@{
    /// Default constructor
    Edge(void) {}
    /**
     * \brief Construct edge \f$e(x,v)\f$ from variable node \a x
     *  and value node \a y
     */
    Edge(VarNode* x, ValNode* v);
    //@}

    /// \name Access
    //@{
    /// Whether the edge is used
    bool used(BC bc) const;
    /// return whether the edge is matched
    bool matched(BC bc) const;
    /// return whether the edge has been deleted from the graph
    bool deleted(void) const;
    /**
     * \brief return a pointer to the next edge
     *  If \a t is false the function returns the next edge incident on \a x
     *  otherwise it returns the next edge incident on \a v
     */
    Edge* next(bool t) const;
    /// return the pointer to the next edge incident on \a x
    Edge* next(void) const;
    /// return the pointer to the previous edge incident on \a x
    Edge* prev(void) const;
    /// return the pointer to the next edge incident on \a v
    Edge* vnext(void) const;
    /// return the pointer to the previous edge incident on \a v
    Edge* vprev(void) const;
    /// return the pointer to the variable node \a x of this edge
    VarNode* getVar(void) const;
    /// return the pointer to the value node \a v of this edge
    ValNode* getVal(void) const;
    /**
     * \brief return pointer to \a x if \a t = true otherwise return \a v
     *
     */
    Node* getMate(bool t) const;
    //@}

    /// Update
    //@{
    /// Mark the edge as used
    void use(BC bc);
    /// Mark the edge as unused
    void free(BC bc);
    /// Reset the edge (free the edge, and unmatch the edge)
    void reset(BC bc);
    /// Match the edge
    void match(BC bc);
    /// Unmatch the edge and the incident nodes
    void unmatch(BC bc);
    /// Unmatch the edge and  ( \a x if t=false,  \a v otherwise )
    void unmatch(BC bc, bool t);
    /// Unlink the edge from the linked list of edges
    void unlink(void);
    /// Mark the edge as deleted during synchronization
    void del_edge(void);
    /// Insert the edge again
    void insert_edge(void);
    /// return the reference to the next edge incident on \a x
    Edge** next_ref(void);
    /// return the reference to the previous edge incident on \a x
    Edge** prev_ref(void);
    /// return the reference to the next edge incident on \a v
    Edge** vnext_ref(void);
    /// return the reference to the previous edge incident on \a v
    Edge** vprev_ref(void);
    //@}

    /// \name Memory management
    //@{
    /// Allocate memory from space
    static void* operator new(size_t s, Space& home);
    /// Free memory (unused)
    static void operator delete(void*, Space&) {};
    /// Needed for exceptions
    static void operator delete(void*) {};
    //@}
  };


  /**
   * \brief Variable-value-graph used during propagation
   *
   */
  template<class Card>
  class VarValGraph {
  private:
    /// Temporary stack for nodes
    typedef Support::StaticStack<Node*,Region> NodeStack;
    /// Bitset
    typedef Support::BitSet<Region> BitSet;
    /// Variable partition representing the problem variables
    VarNode** vars;
    /**
     * \brief Value partition
     *  For each value
     *  \f$ v_i\in V=\left(\bigcup_\{0, \dots, |x|-1\}\right) D_i \f$
     *  in the domains of the
     *  problem variables there is a node in the graph.
     */
    ValNode** vals;
    /// Cardinality of the variable partition
    int n_var;
    /**
     * \brief  Cardinality of the value partition
     *
     * Computed as \f$ |V| = \left(\bigcup_\{0, \dots, |x|-1\}\right) D_i \f$
     */
    int n_val;
    /// Total number of nodes in the graph
    int n_node;
    /**
     * \brief The sum over the minimal capacities of all value nodes
     *
     *  \f$sum_min = \sum_{v_i \in V} l_i= k[i].min() \f$
     */
    int sum_min;
    /**
     * \brief The sum over the maximal capacities of all value nodes
     *
     * \f$sum_max = \sum_{v_i \in V} l_i= k[i].max() \f$
     */
    int sum_max;
  public:
    /// \name Constructors and Destructors
    //@{
    /**
     * \brief Constructor for the variable-value-graph
     *
     * The variable parition is initialized with the variables from \a x,
     * the value partition is initialized with the values from \a k.
     **/
    VarValGraph(Space& home,
                ViewArray<IntView>& x, ViewArray<Card>& k,
                int smin, int smax);
    //@}
    /// \name Graph-interface
    //@{
    /// Check whether minimum requirements shrink variable domains
    ExecStatus min_require(Space& home, 
                           ViewArray<IntView>& x, ViewArray<Card>& k);

    /**
     * \brief Synchronization of the graph
     *
     * If the graph has already been constructed and some edges have
     * been removed during propagation, this function removes those edges
     * that do not longer belong to the graph associated with the current
     * variable domains.
     */
    ExecStatus sync(Space& home,
                    ViewArray<IntView>& x, ViewArray<Card>& k);
    /// Remove edges that do not belong to any maximal matching
    template<BC>
    ExecStatus narrow(Space& home,
                      ViewArray<IntView>& x, ViewArray<Card>& k);

    /** \brief Compute a maximum matching M on the graph
     *
     *  - If BC=UBC then \f$|M|= |X|\f$
     *  - If BC=LBC then \f$|M|= \sum_{i\in \{ 0, \dots, |X|-1\}}
     *    k[i].min()\f$
     */
    template<BC>
    ExecStatus maximum_matching(Space& home);

    /// Compute possible free alternating paths in the graph
    template<BC>
    void free_alternating_paths(Space& home);
    /// Compute possible strongly connected components of the graph
    template<BC>
    void strongly_connected_components(Space& home);
    /**
     * \brief Test whether the current maximal matching on the graph
     * can be augmented by an alternating path starting and ending with
     * a free node.
     */
    template<BC>
    bool augmenting_path(Space& home, Node*);

  protected:
    /**
     * \brief Perform depth-first search on the graph
     *
     * Depth first search used to compute the
     * strongly connected components of the graph.
     */
    template<BC>
    void dfs(Node*, BitSet&, BitSet&, int[],
             NodeStack&, NodeStack&, int&);

    //@}
  public:
    /// Allocate memory for the graph
    void* operator new(size_t t, Space& home);
    /// Deallocation (void)
    void operator delete(void*, Space&) {}
  };



  /*
   * Nodes
   *
   */
  forceinline
  Node::Node(void) {}
  forceinline
  Node::Node(NodeFlag nf0, int i) 
    : e(NULL), fst(NULL), lst(NULL), ie(NULL), idx(i), 
      nf(static_cast<unsigned char>(nf0)), noe(0) {}

  forceinline Edge**
  Node::adj(void) {
    return &e;
  }
  forceinline  Edge*
  Node::first(void) const {
    return fst;
  }
  forceinline Edge*
  Node::last(void) const {
    return lst;
  }
  forceinline void
  Node::first(Edge* p) {
    fst = p;
  }
  forceinline void
  Node::last(Edge* p) {
    lst = p;
  }
  forceinline bool
  Node::type(void) const {
    return (nf & NF_VAL) != 0;
  }
  forceinline Edge*
  Node::inedge(void) const {
    return ie;
  }
  forceinline void
  Node::inedge(Edge* p) {
    ie = p;
  }
  forceinline bool
  Node::removed(void) const {
    return noe == 0;
  }
  forceinline void
  Node::index(int i) {
    idx = i;
  }
  forceinline int
  Node::index(void) const {
    return idx;
  }

  forceinline void*
  Node::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }



  /*
   * Variable nodes
   *
   */
  forceinline
  VarNode::VarNode(void) {}

  forceinline
  VarNode::VarNode(int x) :
    Node(NF_NONE,x), ubm(NULL), lbm(NULL) {}

  forceinline bool
  VarNode::matched(BC bc) const {
    if (bc == UBC)
      return (nf & NF_M_UBC) != 0;
    else
      return (nf & NF_M_LBC) != 0;
  }

  forceinline void
  VarNode::match(BC bc) {
    if (bc == UBC)
      nf |= NF_M_UBC;
    else
      nf |= NF_M_LBC;
  }

  forceinline void
  VarNode::set_match(BC bc, Edge* p) {
    if (bc == UBC)
      ubm = p;
    else
      lbm = p;
  }

  forceinline void
  VarNode::unmatch(BC bc) {
    if (bc == UBC) {
      nf &= ~NF_M_UBC; ubm = NULL;
    } else {
      nf &= ~NF_M_LBC; lbm = NULL;
    }
  }

  forceinline Edge*
  VarNode::get_match(BC bc) const {
    if (bc == UBC)
      return ubm;
    else
      return lbm;
  }




  /*
   * Value nodes
   *
   */
  forceinline
  ValNode::ValNode(void) {}

  forceinline
  ValNode::ValNode(int min, int max, int value,
                   int kidx, int kshift, int count) :
    Node(NF_VAL,kshift), _klb(min), _kub(max), _kidx(kidx), _kcount(count),
    noc(0),
    lb(min), ublow(max), ub(max),
    val(value) {}

  forceinline void
  ValNode::maxlow(int i) {
    assert(i >= lb);
    ublow = i;
  }

  forceinline int
  ValNode::maxlow(void) const {
    if (_klb == _kub) {
      assert(ublow == lb);
    }
    return ublow;
  }


  forceinline void
  ValNode::card_conflict(int c) {
    noc = c;
  }

  forceinline void
  ValNode::red_conflict(void) {
    noc--;
    assert(noc >= 0);
  }

  forceinline int
  ValNode::card_conflict(void) const {
    return noc;
  }

  forceinline int
  ValNode::cap(BC bc) const {
    if (bc == UBC)
      return ub;
    else
      return lb;
  }
  forceinline bool
  ValNode::matched(BC bc) const {
    return cap(bc) == 0;
  }

  forceinline void
  ValNode::reset(void) {
    lb = _klb;
    ublow = _kub;
    ub = _kub;
    noe = 0;
  }

  forceinline int
  ValNode::kbound(BC bc) const {
    if (bc == UBC) {
      return _kub;
    } else {
      return _klb;
    }
  }

  forceinline int
  ValNode::kmax(void) const {
    return _kub;
  }

  forceinline int
  ValNode::kmin(void) const {
    return _klb;
  }

  forceinline void
  ValNode::kmin(int klb) {
    _klb = klb;
  }

  forceinline void
  ValNode::kmax(int kub) {
    _kub = kub;
  }


  forceinline void
  ValNode::dec(BC bc) {
    if (bc == UBC) {
      ub--;
    } else {
      lb--; ublow--;
    }
  }

  forceinline void
  ValNode::inc(BC bc) {
    if (bc == UBC) {
      ub++;
    } else {
      lb++; ublow++;
    }
  }

  forceinline void
  ValNode::match(BC bc) {
    dec(bc);
  }

  forceinline void
  ValNode::unmatch(BC bc) {
    inc(bc);
  }

  forceinline void
  ValNode::cap(BC bc, int c) {
    if (bc == UBC)
      ub = c;
    else
      lb = c;
  }

  forceinline void
  ValNode::inc(void) {
    _kcount++;
  }

  forceinline int
  ValNode::kcount(void) const {
    return _kcount;
  }

  forceinline void
  ValNode::kcount(int c) {
    _kcount = c;
  }

  forceinline void
  ValNode::kindex(int i) {
    _kidx = i;
  }

  forceinline int
  ValNode::kindex(void) const {
    return _kidx;
  }

  /// Returs the number of incident matching edges on the node
  forceinline int
  ValNode::incid_match(BC bc) const {
    if (bc == LBC)
      return _kub - ublow + _kcount;
    else
      return _kub - ub + _kcount;
  }


  forceinline bool
  ValNode::sink(void) const {
    // there are only incoming edges
    // in case of the UBC-matching
    return _kub - ub == noe;
  }

  forceinline bool
  ValNode::source(void) const {
    // there are only incoming edges
    // in case of the UBC-matching
    return _klb - lb == noe;
  }



  /*
   * Edges
   *
   */
  forceinline void
  Edge::unlink(void) {
    // unlink from variable side
    Edge* p = prev_edge;
    Edge* n = next_edge;

    if (p != NULL)
      *p->next_ref() = n;
    if (n != NULL)
      *n->prev_ref() = p;

    if (this == x->first()) {
      Edge** ref = x->adj();
      *ref = n;
      x->first(n);
    }

    if (this == x->last())
      x->last(p);

    // unlink from value side
    Edge* pv = prev_vedge;
    Edge* nv = next_vedge;

    if (pv != NULL)
      *pv->vnext_ref() = nv;
    if (nv != NULL)
      *nv->vprev_ref() = pv;
    if (this == v->first()) {
      Edge** ref = v->adj();
      *ref = nv;
      v->first(nv);
    }
    if (this == v->last())
      v->last(pv);
  }

  forceinline
  Edge::Edge(VarNode* var, ValNode* val) :
    x(var), v(val),
    next_edge(NULL), prev_edge(NULL),
    next_vedge(NULL), prev_vedge(NULL), ef(EF_NONE) {}

  forceinline void
  Edge::use(BC bc) {
    if (bc == UBC)
      ef |= EF_MRKUB;
    else
      ef |= EF_MRKLB;
  }
  forceinline void
  Edge::free(BC bc) {
    if (bc == UBC)
      ef &= ~EF_MRKUB;
    else
      ef &= ~EF_MRKLB;
  }
  forceinline bool
  Edge::used(BC bc) const {
    if (bc == UBC)
      return (ef & EF_MRKUB) != 0;
    else
      return (ef & EF_MRKLB) != 0;
  }
  forceinline Edge*
  Edge::next(void) const {
    return next_edge;
  }
  forceinline Edge*
  Edge::next(bool t) const {
    if (t) {
      return next_vedge;
    } else {
      return next_edge;
    }
  }

  forceinline Edge*
  Edge::vnext(void) const {
    return next_vedge;
  }
  forceinline Edge**
  Edge::vnext_ref(void) {
    return &next_vedge;
  }
  forceinline Edge*
  Edge::prev(void) const {
    return prev_edge;
  }
  forceinline Edge**
  Edge::prev_ref(void) {
    return &prev_edge;
  }
  forceinline Edge*
  Edge::vprev(void) const {
    return prev_vedge;
  }
  forceinline Edge**
  Edge::vprev_ref(void) {
    return &prev_vedge;
  }
  forceinline Edge**
  Edge::next_ref(void) {
    return &next_edge;
  }
  forceinline VarNode*
  Edge::getVar(void) const {
    assert(x != NULL);
    return x;
  }

  forceinline ValNode*
  Edge::getVal(void) const {
    assert(v != NULL);
    return v;
  }

  forceinline Node*
  Edge::getMate(bool type) const {
    if (type)
      return x;
    else
      return v;
  }

  forceinline void
  Edge::unmatch(BC bc) {
    if (bc == UBC)
      ef &= ~EF_UM;
    else
      ef &= ~EF_LM;
    x->unmatch(bc); v->unmatch(bc);
  }

  forceinline void
  Edge::unmatch(BC bc, bool node) {
    if (bc == UBC)
      ef &= ~EF_UM;
    else
      ef &= ~EF_LM;
    if (node)
      v->unmatch(bc);
    else
      x->unmatch(bc);
  }

  forceinline void
  Edge::reset(BC bc) {
    free(bc); unmatch(bc);
  }

  forceinline void
  Edge::match(BC bc) {
    if (bc == UBC)
      ef |= EF_UM;
    else
      ef |= EF_LM;
    x->match(bc);
    x->set_match(bc,this);
    v->match(bc);
  }

  forceinline bool
  Edge::matched(BC bc) const {
    if (bc == UBC)
      return (ef & EF_UM) != 0;
    else
      return (ef & EF_LM) != 0;
  }

  forceinline void
  Edge::del_edge(void) {
    ef |= EF_DEL;
  }

  forceinline void
  Edge::insert_edge(void) {
    ef &= ~EF_DEL;
  }


  forceinline bool
  Edge::deleted(void) const {
    return (ef & EF_DEL) != 0;
  }

  forceinline void*
  Edge::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }


  /*
   * Variable value graph
   *
   */
  template<class Card>
  VarValGraph<Card>::VarValGraph(Space& home,
                                 ViewArray<IntView>& x, ViewArray<Card>& k,
                                 int smin, int smax)
    : n_var(x.size()),
      n_val(k.size()),
      n_node(n_var + n_val),
      sum_min(smin),
      sum_max(smax) {

    unsigned int noe = 0;
    for (int i=x.size(); i--; )
      noe += x[i].size();

    vars = home.alloc<VarNode*>(n_var);
    vals = home.alloc<ValNode*>(n_val);

    for (int i = n_val; i--; ) {
      int kmi = k[i].min();
      int kma = k[i].max();
      int kc  = k[i].counter();
      if (kc != kma) {
        if (kmi >= kc) {
          kmi -=kc;
          assert(kmi >=0);
        } else {
          kmi = 0;
        }
        kma -= kc;
        assert (kma > 0);
        vals[i] = new (home)
          ValNode(kmi, kma, k[i].card(), i, i + n_var, kc);
      } else {
        vals[i] = new (home)
          ValNode(0, 0, k[i].card(), i, i + n_var, kc);
      }
    }

    for (int i = n_var; i--; ) {
      vars[i] = new (home) VarNode(i);
      // get the space for the edges of the varnode
      Edge** xadjacent = vars[i]->adj();

      int j = 0;
      for (ViewValues<IntView> xi(x[i]); xi(); ++xi) {
        // get the correct index for the value
        while(vals[j]->val < xi.val())
          j++;
        *xadjacent = new (home) Edge(vars[i],vals[j]);
        vars[i]->noe++;
        if (vars[i]->first() == NULL)
          vars[i]->first(*xadjacent);
        Edge* oldprev  = vars[i]->last();
        vars[i]->last(*xadjacent);
        *vars[i]->last()->prev_ref() = oldprev;

        if (vals[j]->first() == NULL) {
          vals[j]->first(*xadjacent);
          vals[j]->last(*xadjacent);
        } else {
          Edge* old = vals[j]->first();
          vals[j]->first(*xadjacent);
          *vals[j]->first()->vnext_ref() = old;
          *old->vprev_ref() = vals[j]->first();
        }
        vals[j]->noe++;
        xadjacent = (*xadjacent)->next_ref();
      }
      *xadjacent = NULL;
    }
  }


  template<class Card>
  inline ExecStatus
  VarValGraph<Card>::min_require(Space& home, 
                                 ViewArray<IntView>& x,
                                 ViewArray<Card>& k) {
    for (int i = n_val; i--; ) {
      ValNode* vln = vals[i];
      if (vln->noe > 0) {
        if (k[i].min() == vln->noe) {
          // all variable nodes reachable from vln should be equal to vln->val
          for (Edge* e = vln->first(); e != NULL; e = e->vnext()) {
            VarNode* vrn = e->getVar();
            for (Edge* f = vrn->first(); f != NULL; f = f->next())
              if (f != e) {
                ValNode* w = f->getVal();
                w->noe--;
                vrn->noe--;
                f->del_edge();
                f->unlink();
              }
            assert(vrn->noe == 1);

            int vi = vrn->index();
            GECODE_ME_CHECK(x[vi].eq(home, vln->val));

            vars[vi] = vars[--n_var];
            vars[vi]->index(vi);
            x.move_lst(vi);
            n_node--;
            vln->noe--;
          }


          int vidx = vln->kindex();
          if (Card::propagate)
            GECODE_ME_CHECK(k[vidx].eq(home, k[vidx].min()));

          k[vidx].counter(k[vidx].min());

          vln->cap(UBC,0);
          vln->cap(LBC,0);
          vln->maxlow(0);

          if (sum_min >= k[vidx].min())
            sum_min -= k[vidx].min();
          if (sum_max >= k[vidx].max())
            sum_max -= k[vidx].max();
        }
      } else {
        vals[i]->cap(UBC,0);
        vals[i]->cap(LBC,0);
        vals[i]->maxlow(0);
        vals[i]->kmax(0);
        vals[i]->kmin(0);
      }

      if (Card::propagate && (k[i].counter() == 0))
        GECODE_ME_CHECK(k[i].lq(home, vals[i]->noe));
    }

    for (int i = n_val; i--; )
      vals[i]->index(n_var + i);

    return ES_OK;
  }

  template<class Card>
  inline ExecStatus
  VarValGraph<Card>::sync(Space& home,
                          ViewArray<IntView>& x, ViewArray<Card>& k) {
    Region r(home);
    // A node can be pushed twice (once when checking cardinality and later again)
    NodeStack re(r,2*n_node);

    // synchronize cardinality variables
    if (Card::propagate) {
      for (int i = n_val; i--; ) {
        ValNode* v = vals[i];
        int inc_ubc = v->incid_match(UBC);
        int inc_lbc = v->incid_match(LBC);
        if (v->noe == 0) {
          inc_ubc = 0;
          inc_lbc = 0;
        }
        int rm = v->kmax() - k[i].max();
        // the cardinality bounds have been modified
        if ((k[i].max() < v->kmax()) || (k[i].min() > v->kmin())) {
          if ((k[i].max() != k[i].counter()) || (k[i].max() == 0)) {
            // update the bounds
            v->kmax(k[i].max());
            v->kmin(k[i].min());

            //everything is fine
            if (inc_ubc <= k[i].max()) {
              // adjust capacities
              v->cap(UBC, k[i].max() - inc_ubc);
              v->maxlow(k[i].max() - inc_lbc);
              if (v->kmin() == v->kmax())
                v->cap(LBC, k[i].max() - inc_lbc);
            } else {
              // set cap to max and resolve conflicts on view side
              // set to full capacity for later rescheduling
              if (v->cap(UBC))
                v->cap(UBC,k[i].max());
              v->maxlow(k[i].max() - (inc_lbc));
              if (v->kmin() == v->kmax())
                v->cap(LBC,k[i].max() - (inc_lbc));
              v->card_conflict(rm);
            }
          }
        }
        if (inc_lbc < k[i].min() && v->noe > 0) {
          v->cap(LBC, k[i].min() - inc_lbc);
          re.push(v);
        }
      }

      for (int i = n_var; i--; ) {
        Edge* mub = vars[i]->get_match(UBC);
        if (mub != NULL) {
          ValNode* vu = mub->getVal();
          if ((vars[i]->noe != 1) && vu->card_conflict()) {
            vu->red_conflict();
            mub->unmatch(UBC,vars[i]->type());
            re.push(vars[i]);
          }
        }
      }
    }

    // go on with synchronization
    assert(x.size() == n_var);
    for (int i = n_var; i--; ) {

      VarNode* vrn = vars[i];
      if (static_cast<int>(x[i].size()) != vrn->noe) {
        // if the variable is already assigned
        if (x[i].assigned()) {
          int  v = x[i].val();
          Edge* mub = vrn->get_match(UBC);
          if ((mub != NULL) && (v != mub->getVal()->val)) {
            mub->unmatch(UBC);
            re.push(vars[i]);
          }

          Edge* mlb = vrn->get_match(LBC);
          if (mlb != NULL) {
            ValNode* vln = mlb->getVal();
            if (v != vln->val) {
              mlb->unmatch(LBC);
              if (vln->incid_match(LBC) < vln->kmin())
                re.push(vln);
            }
          }

          for (Edge* e = vrn->first(); e != NULL; e = e->next()) {
            ValNode* vln = e->getVal();
            if (vln->val != v) {
              vrn->noe--;
              e->getVal()->noe--;
              e->del_edge();
              e->unlink();
            }
          }
        } else {

          // delete the edge
          ViewValues<IntView> xiter(x[i]);
          Edge*  mub = vrn->get_match(UBC);
          Edge*  mlb = vrn->get_match(LBC);
          Edge** p   = vrn->adj();
          Edge*  e   = *p;
          do {
            // search the edge that has to be deleted
            while (e != NULL && (e->getVal()->val < xiter.val())) {
              // Skip edge
              e->getVal()->noe--;
              vrn->noe--;
              e->del_edge();
              e->unlink();
              e = e ->next();
              *p = e;
            }

            assert(xiter.val() == e->getVal()->val);

            // This edge must be kept
            e->free(UBC);
            e->free(LBC);
            ++xiter;
            p = e->next_ref();
            e = e->next();
          } while (xiter());
          *p = NULL;
          while (e) {
            e->getVar()->noe--;
            e->getVal()->noe--;
            e->del_edge();
            e->unlink();
            e = e->next();
          }

          if ((mub != NULL) && mub->deleted()) {
            mub->unmatch(UBC);
            re.push(vars[i]);
          }

          //lower bound matching can be zero
          if ((mlb != NULL) && mlb->deleted()) {
            ValNode* vln = mlb->getVal();
            mlb->unmatch(LBC);
            if (vln->incid_match(LBC) < vln->kmin())
              re.push(vln);
          }
        }
      }
      vars[i]->index(i);
    }

    for (int i = n_val; i--; ) {
      if ((k[i].min() > vals[i]->noe) && (k[i].counter() == 0))
        return ES_FAILED;
      vals[i]->index(n_var + i);
    }

    // start repair
    while (!re.empty()) {
      Node* n = re.pop();
      if (!n->removed()) {
        if (!n->type()) {
          VarNode* vrn = static_cast<VarNode*>(n);
          if (!vrn->matched(UBC) && !augmenting_path<UBC>(home,vrn))
            return ES_FAILED;
        } else {
          ValNode* vln = static_cast<ValNode*>(n);
          while (!vln->matched(LBC))
            if (!augmenting_path<LBC>(home,vln))
              return ES_FAILED;
        }
      }
    }

    return ES_OK;
  }

  template<class Card> template<BC bc>
  inline ExecStatus
  VarValGraph<Card>::narrow(Space& home, 
                            ViewArray<IntView>& x, ViewArray<Card>& k) {
    for (int i = n_var; i--; )
      if (vars[i]->noe == 1) {
        ValNode* v = vars[i]->first()->getVal();
        vars[i]->first()->free(bc);
        GECODE_ME_CHECK(x[i].eq(home, v->val));
        v->inc();
      }

    for (int i = n_val; i--; ) {
      ValNode* v = vals[i];
      if (Card::propagate && (k[i].counter() == 0))
        GECODE_ME_CHECK(k[i].lq(home, v->noe));
      if (v->noe > 0) {
        if (Card::propagate)
          GECODE_ME_CHECK(k[i].lq(home, v->noe));

        // If the maximum number of occurences of a value is reached
        // it cannot be consumed by another view

        if (v->kcount() == v->kmax()) {
          int vidx = v->kindex();

          k[i].counter(v->kcount());

          if (Card::propagate)
            GECODE_ME_CHECK(k[i].eq(home, k[i].counter()));

          bool delall = v->card_conflict() && (v->noe > v->kmax());

          for (Edge* e = v->last(); e != NULL; e = e->vprev()) {
            VarNode* vrn = e->getVar();
            if (vrn->noe == 1) {
              vrn->noe--;
              v->noe--;
              int vi= vrn->index();

              x.move_lst(vi);
              vars[vi] = vars[--n_var];
              vars[vi]->index(vi);
              n_node--;
              e->del_edge();
              e->unlink();

            } else if (delall) {
              GECODE_ME_CHECK(x[vrn->index()].nq(home, v->val));
              vrn->noe--;
              v->noe--;
              e->del_edge();
              e->unlink();
            }
          }
          v->cap(UBC,0);
          v->cap(LBC,0);
          v->maxlow(0);
          if (sum_min >= k[vidx].min())
            sum_min -= k[vidx].min();
          if (sum_max >= k[vidx].max())
            sum_max -= k[vidx].max();

        } else if (v->kcount() > 0) {
          v->kcount(0);
        }
      }
    }
    for (int i = n_var; i--; )
      vars[i]->index(i);

    for (int i = n_val; i--; ) {
      if (vals[i]->noe == 0) {
        vals[i]->cap(UBC,0);
        vals[i]->cap(LBC,0);
        vals[i]->maxlow(0);
      }
      vals[i]->index(n_var + i);
    }

    for (int i = n_var; i--; ) {
      if (vars[i]->noe > 1) {
        for (Edge* e = vars[i]->first(); e != NULL; e = e->next()) {
          if (!e->matched(bc) && !e->used(bc)) {
            GECODE_ME_CHECK(x[i].nq(home, e->getVal()->val));
          } else {
            e->free(bc);
          }
        }
      }
    }
    return ES_OK;
  }

  template<class Card> template<BC bc>
  forceinline bool
  VarValGraph<Card>::augmenting_path(Space& home, Node* v) {
    Region r(home);
    NodeStack ns(r,n_node);
    BitSet visited(r,static_cast<unsigned int>(n_node));
    Edge** start = r.alloc<Edge*>(n_node);

    // keep track of the nodes that have already been visited
    Node* sn = v;

    // mark the start partition
    bool sp = sn->type();

    // nodes in sp only follow free edges
    // nodes in V - sp only follow matched edges

    for (int i = n_node; i--; )
      if (i >= n_var) {
        vals[i-n_var]->inedge(NULL);
        start[i] = vals[i-n_var]->first();
      } else {
        vars[i]->inedge(NULL);
        start[i] = vars[i]->first();
      }

    v->inedge(NULL);
    ns.push(v);
    visited.set(static_cast<unsigned int>(v->index()));
    while (!ns.empty()) {
      Node* v = ns.top();
      Edge* e = NULL;
      if (v->type() == sp) {
        e = start[v->index()];
        while ((e != NULL) && e->matched(bc))
          e = e->next(v->type());
      } else {
        e = start[v->index()];
        while ((e != NULL) && !e->matched(bc))
          e = e->next(v->type());
        start[v->index()] = e;
      }
      if (e != NULL) {
        start[v->index()] = e->next(v->type());
        Node* w = e->getMate(v->type());
        if (!visited.get(static_cast<unsigned int>(w->index()))) {
          // unexplored path
          bool m = w->type() ? 
            static_cast<ValNode*>(w)->matched(bc) :
            static_cast<VarNode*>(w)->matched(bc);
          if (!m && w->type() != sp) {
            if (v->inedge() != NULL) {
              // augmenting path of length l > 1
              e->match(bc);
              break;
            } else {
              // augmenting path of length l = 1
              e->match(bc);
              ns.pop();
              return true;
            }
          } else {
            w->inedge(e);
            visited.set(static_cast<unsigned int>(w->index()));
            // find matching edge m incident with w
            ns.push(w);
          }
        }
      } else {
        // tried all outgoing edges without finding an augmenting path
        ns.pop();
      }
    }

    bool pathfound = !ns.empty();

    while (!ns.empty()) {
      Node* t = ns.pop();
      if (t != sn) {
        Edge* in = t->inedge();
        if (t->type() != sp) {
          in->match(bc);
        } else if (!sp) {
          in->unmatch(bc,!sp);
        } else {
          in->unmatch(bc);
        }
      }
    }
    return pathfound;
  }

  template<class Card>  template<BC bc>
  inline ExecStatus
  VarValGraph<Card>::maximum_matching(Space& home) {
    int card_match = 0;
    // find an intial matching in O(n*d)
    // greedy algorithm
    for (int i = n_val; i--; )
      for (Edge* e = vals[i]->first(); e != NULL ; e = e->vnext())
        if (!e->getVar()->matched(bc) && !vals[i]->matched(bc)) {
          e->match(bc); card_match++;
        }

    Region r(home);
    switch (bc) {
    case LBC: 
      if (card_match < sum_min) {
        Support::StaticStack<ValNode*,Region> free(r,n_val);

        // find failed nodes
        for (int i = n_val; i--; )
          if (!vals[i]->matched(LBC))
            free.push(vals[i]);
        
        while (!free.empty()) {
          ValNode* v = free.pop();
          while (!v->matched(LBC))
            if (augmenting_path<LBC>(home,v))
              card_match++;
            else
              break;
        }

        return (card_match >= sum_min) ? ES_OK : ES_FAILED;
      } else {
        return ES_OK;
      }
      break;
    case UBC:
      if (card_match < n_var) {
        Support::StaticStack<VarNode*,Region> free(r,n_var);

        // find failed nodes
        for (int i = n_var; i--; )
          if (!vars[i]->matched(UBC))
            free.push(vars[i]);

        while (!free.empty()) {
          VarNode* v = free.pop();
          if (!v->matched(UBC) && augmenting_path<UBC>(home,v))
            card_match++;
        }

        return (card_match >= n_var) ? ES_OK : ES_FAILED;
      } else {
        return ES_OK;
      }
      break;
    default: GECODE_NEVER;
    }
    GECODE_NEVER;
    return ES_FAILED;
  }


  template<class Card> template<BC bc>
  forceinline void
  VarValGraph<Card>::free_alternating_paths(Space& home) {
    Region r(home);
    NodeStack ns(r,n_node);
    BitSet visited(r,static_cast<unsigned int>(n_node));

    switch (bc) {
    case LBC:
      // after a maximum matching on the value nodes there still can be
      // free value nodes, hence we have to consider ALL nodes whether
      // they are the starting point of an even alternating path in G
      for (int i = n_var; i--; )
        if (!vars[i]->matched(LBC)) {
          ns.push(vars[i]); 
          visited.set(static_cast<unsigned int>(vars[i]->index()));
        }
      for (int i = n_val; i--; )
        if (!vals[i]->matched(LBC)) {
          ns.push(vals[i]); 
          visited.set(static_cast<unsigned int>(vals[i]->index()));
        }
      break;
    case UBC:
      // clearly, after a maximum matching on the x variables
      // corresponding to a set cover on x there are NO free var nodes
      for (int i = n_val; i--; )
        if (!vals[i]->matched(UBC)) {
          ns.push(vals[i]); 
          visited.set(static_cast<unsigned int>(vals[i]->index()));
        }
      break;
    default: GECODE_NEVER;
    }

    while (!ns.empty()) {
      Node* node = ns.pop();
      if (node->type()) {
        // ValNode
        ValNode* vln = static_cast<ValNode*>(node);

        for (Edge* cur = vln->first(); cur != NULL; cur = cur->vnext()) {
          VarNode* mate = cur->getVar();
          switch (bc) {
          case LBC:
            if (cur->matched(LBC)) {
              // mark the edge
              cur->use(LBC);
              if (!visited.get(static_cast<unsigned int>(mate->index()))) {
                ns.push(mate); 
                visited.set(static_cast<unsigned int>(mate->index()));
              }
            }
            break;
          case UBC:
            if (!cur->matched(UBC)) {
              // mark the edge
              cur->use(UBC);
              if (!visited.get(static_cast<unsigned int>(mate->index()))) {
                ns.push(mate); 
                visited.set(static_cast<unsigned int>(mate->index()));
              }
            }
            break;
          default: GECODE_NEVER;
          }
        }

      } else {
        // VarNode
        VarNode* vrn = static_cast<VarNode*>(node);

        switch (bc) {
        case LBC: 
          // after LBC-matching we can follow every unmatched edge
          for (Edge* cur = vrn->first(); cur != NULL; cur = cur->next()) {
            ValNode* mate = cur->getVal();
            if (!cur->matched(LBC)) {
              cur->use(LBC);
              if (!visited.get(static_cast<unsigned int>(mate->index()))) {
                ns.push(mate); 
                visited.set(static_cast<unsigned int>(mate->index()));
              }
            }
          }
          break;
        case UBC: 
          // after UBC-matching we can only follow a matched edge
          {
            Edge* cur = vrn->get_match(UBC);
            if (cur != NULL) {
              cur->use(UBC);
              ValNode* mate = cur->getVal();
              if (!visited.get(static_cast<unsigned int>(mate->index()))) {
                ns.push(mate); 
                visited.set(static_cast<unsigned int>(mate->index()));
              }
            }
          }
          break;
        default: GECODE_NEVER;
        }
      }
    }
  }

  template<class Card> template<BC bc>
  void
  VarValGraph<Card>::dfs(Node* v,
                         BitSet& inscc, BitSet& in_unfinished, int dfsnum[],
                         NodeStack& roots, NodeStack& unfinished,
                         int& count) {
    count++;
    int v_index            = v->index();
    dfsnum[v_index]        = count;
    inscc.set(static_cast<unsigned int>(v_index));
    in_unfinished.set(static_cast<unsigned int>(v_index));

    unfinished.push(v);
    roots.push(v);
    for (Edge* e = v->first(); e != NULL; e = e->next(v->type())) {
      bool m;
      switch (bc) {
      case LBC:
        m = v->type() ? e->matched(LBC) : !e->matched(LBC);
        break;
      case UBC:
        m = v->type() ? !e->matched(UBC) : e->matched(UBC);
        break;
      default: GECODE_NEVER;
      }
      if (m) {
        Node* w = e->getMate(v->type());
        int w_index = w->index();

        assert(w_index < n_node);
        if (!inscc.get(static_cast<unsigned int>(w_index))) {
          // w is an uncompleted scc
          w->inedge(e);
          dfs<bc>(w, inscc, in_unfinished, dfsnum,
                  roots, unfinished, count);
        } else if (in_unfinished.get(static_cast<unsigned int>(w_index))) {
          // even alternating cycle found mark the edge closing the cycle,
          // completing the scc
          e->use(bc);
          // if w belongs to an scc we detected earlier
          // merge components
          assert(roots.top()->index() < n_node);
          while (dfsnum[roots.top()->index()] > dfsnum[w_index]) {
            roots.pop();
          }
        }
      }
    }

    if (v == roots.top()) {
      while (v != unfinished.top()) {
        // w belongs to the scc with root v
        Node* w = unfinished.top();
        w->inedge()->use(bc);
        in_unfinished.clear(static_cast<unsigned int>(w->index()));
        unfinished.pop();
      }
      assert(v == unfinished.top());
      in_unfinished.clear(static_cast<unsigned int>(v_index));
      roots.pop();
      unfinished.pop();
    }
  }

  template<class Card> template<BC bc>
  forceinline void
  VarValGraph<Card>::strongly_connected_components(Space& home) {
    Region r(home);
    BitSet inscc(r,static_cast<unsigned int>(n_node));
    BitSet in_unfinished(r,static_cast<unsigned int>(n_node));
    int* dfsnum = r.alloc<int>(n_node);

    for (int i = n_node; i--; )
      dfsnum[i]=0;

    int count = 0;
    NodeStack roots(r,n_node);
    NodeStack unfinished(r,n_node);

    for (int i = n_var; i--; )
      dfs<bc>(vars[i], inscc, in_unfinished, dfsnum,
              roots, unfinished, count);
  }

  template<class Card>
  forceinline void*
  VarValGraph<Card>::operator new(size_t t, Space& home) {
    return home.ralloc(t);
  }

}}}

// STATISTICS: int-prop


