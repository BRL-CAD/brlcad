/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
 *     Vincent Barichard, 2012
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

#include <gecode/minimodel.hh>

namespace Gecode {

  /// %Node for Boolean expression
  class BoolExpr::Node {
  public:
    /// Nodes are reference counted
    unsigned int use;
    /// Number of variables in subtree with same type (for AND and OR)
    int same;
    /// Type of expression
    NodeType t;
    /// Subexpressions
    Node *l, *r;
    /// Possibly a variable
    BoolVar x;
    /// Possibly a reified linear relation
    LinIntRel rl;
#ifdef GECODE_HAS_FLOAT_VARS
    /// Possibly a reified float linear relation
    LinFloatRel rfl;
#endif
#ifdef GECODE_HAS_SET_VARS
    /// Possibly a reified set relation
    SetRel rs;
#endif
    /// Possibly a misc Boolean expression
    MiscExpr* m;

    /// Default constructor
    Node(void);
    /// Destructor
    ~Node(void);
    /// Decrement reference count and possibly free memory
    GECODE_MINIMODEL_EXPORT
    bool decrement(void);
    /// Memory management
    static void* operator new(size_t size);
    /// Memory management
    static void  operator delete(void* p, size_t size);
  };


  /*
   * Operations for nodes
   *
   */
  BoolExpr::Node::Node(void) 
    : use(1), l(NULL), r(NULL), m(NULL) {}

  BoolExpr::Node::~Node(void) {    
    delete m;  
  }

  void*
  BoolExpr::Node::operator new(size_t size) {
    return heap.ralloc(size);
  }
  void
  BoolExpr::Node::operator delete(void* p, size_t) {
    heap.rfree(p);
  }

  bool
  BoolExpr::Node::decrement(void) {
    if (--use == 0) {
      if ((l != NULL) && l->decrement())
        delete l;
      if ((r != NULL) && r->decrement())
        delete r;
      return true;
    }
    return false;
  }

  BoolExpr::BoolExpr(void) : n(new Node) {}

  BoolExpr::BoolExpr(const BoolExpr& e) : n(e.n) {
    n->use++;
  }

  BoolExpr::BoolExpr(const BoolVar& x) : n(new Node) {
    n->same = 1;
    n->t    = NT_VAR;
    n->l    = NULL;
    n->r    = NULL;
    n->x    = x;
  }

  BoolExpr::BoolExpr(const BoolExpr& l, NodeType t, const BoolExpr& r)
    : n(new Node) {
    int ls = ((l.n->t == t) || (l.n->t == NT_VAR)) ? l.n->same : 1;
    int rs = ((r.n->t == t) || (r.n->t == NT_VAR)) ? r.n->same : 1;
    n->same = ls+rs;
    n->t    = t;
    n->l    = l.n;
    n->l->use++;
    n->r    = r.n;
    n->r->use++;
  }

  BoolExpr::BoolExpr(const BoolExpr& l, NodeType t) {
    (void) t;
    assert(t == NT_NOT);
    if (l.n->t == NT_NOT) {
      n = l.n->l;
      n->use++;
    } else {
      n = new Node;
      n->same = 1;
      n->t    = NT_NOT;
      n->l    = l.n;
      n->l->use++;
      n->r    = NULL;
    }
  }

  BoolExpr::BoolExpr(const LinIntRel& rl)
    : n(new Node) {
    n->same = 1;
    n->t    = NT_RLIN;
    n->l    = NULL;
    n->r    = NULL;
    n->rl   = rl;
  }

#ifdef GECODE_HAS_FLOAT_VARS
  BoolExpr::BoolExpr(const LinFloatRel& rfl)
    : n(new Node) {
    n->same  = 1;
    n->t     = NT_RLINFLOAT;
    n->l     = NULL;
    n->r     = NULL;
    n->rfl   = rfl;
  }
#endif

#ifdef GECODE_HAS_SET_VARS
  BoolExpr::BoolExpr(const SetRel& rs)
    : n(new Node) {
    n->same = 1;
    n->t    = NT_RSET;
    n->l    = NULL;
    n->r    = NULL;
    n->rs   = rs;
  }

  BoolExpr::BoolExpr(const SetCmpRel& rs)
    : n(new Node) {
    n->same = 1;
    n->t    = NT_RSET;
    n->l    = NULL;
    n->r    = NULL;
    n->rs   = rs;
  }
#endif

  BoolExpr::BoolExpr(BoolExpr::MiscExpr* m)
    : n(new Node) {
    n->same = 1;
    n->t    = NT_MISC;
    n->l    = NULL;
    n->r    = NULL;
    n->m    = m;
  }

  const BoolExpr&
  BoolExpr::operator =(const BoolExpr& e) {
    if (this != &e) {
      if (n->decrement())
        delete n;
      n = e.n;
      n->use++;
    }
    return *this;
  }

  BoolExpr::MiscExpr::~MiscExpr(void) {}

  BoolExpr::~BoolExpr(void) {
    if (n->decrement())
      delete n;
  }

  namespace {
    /// %Node for negation normalform (%NNF)
    class NNF {
    public:
      typedef BoolExpr::NodeType NodeType;
      typedef BoolExpr::Node Node;
      /// Type of node
      NodeType t;
      /// Number of positive literals for node type
      int p;
      /// Number of negative literals for node type
      int n;
      /// Union depending on nodetype \a t
      union {
        /// For binary nodes (and, or, eqv)
        struct {
          /// Left subtree
          NNF* l;
          /// Right subtree
          NNF* r;
        } b;
        /// For atomic nodes
        struct {
          /// Is atomic formula negative
          bool neg;
          /// Pointer to corresponding Boolean expression node
          Node* x;
        } a;
      } u;
      /// Create negation normalform
      GECODE_MINIMODEL_EXPORT
      static NNF* nnf(Region& r, Node* n, bool neg);
      /// Post propagators for nested conjunctive and disjunctive expression
      GECODE_MINIMODEL_EXPORT
      void post(Home home, NodeType t,
                BoolVarArgs& bp, BoolVarArgs& bn,
                int& ip, int& in,
                IntConLevel icl) const;
      /// Post propagators for expression
      GECODE_MINIMODEL_EXPORT
      BoolVar expr(Home home, IntConLevel icl) const;
      /// Post propagators for relation
      GECODE_MINIMODEL_EXPORT
      void rel(Home home, IntConLevel icl) const;
      /// Allocate memory from region
      static void* operator new(size_t s, Region& r);
      /// No-op (for exceptions)
      static void operator delete(void*);
      /// No-op
      static void operator delete(void*, Region&);
    };

    /*
     * Operations for negation normalform
     *
     */
    forceinline void
    NNF::operator delete(void*) {}

    forceinline void
    NNF::operator delete(void*, Region&) {}

    forceinline void*
    NNF::operator new(size_t s, Region& r) {
      return r.ralloc(s);
    }

    BoolVar
    NNF::expr(Home home, IntConLevel icl) const {
      if ((t == BoolExpr::NT_VAR) && !u.a.neg)
        return u.a.x->x;
      BoolVar b(home,0,1);
      switch (t) {
      case BoolExpr::NT_VAR:
        assert(u.a.neg);
        Gecode::rel(home, u.a.x->x, IRT_NQ, b);
        break;
      case BoolExpr::NT_RLIN:
        u.a.x->rl.post(home, b, !u.a.neg, icl);
        break;
#ifdef GECODE_HAS_FLOAT_VARS
      case BoolExpr::NT_RLINFLOAT:
        u.a.x->rfl.post(home, b, !u.a.neg);
        break;
#endif
#ifdef GECODE_HAS_SET_VARS
      case BoolExpr::NT_RSET:
        u.a.x->rs.post(home, b, !u.a.neg);
        break;
#endif
      case BoolExpr::NT_MISC:
        u.a.x->m->post(home, b, !u.a.neg, icl);
        break;
      case BoolExpr::NT_AND:
        {
          BoolVarArgs bp(p), bn(n);
          int ip=0, in=0;
          post(home, BoolExpr::NT_AND, bp, bn, ip, in, icl);
          clause(home, BOT_AND, bp, bn, b);
        }
        break;
      case BoolExpr::NT_OR:
        {
          BoolVarArgs bp(p), bn(n);
          int ip=0, in=0;
          post(home, BoolExpr::NT_OR, bp, bn, ip, in, icl);
          clause(home, BOT_OR, bp, bn, b);
        }
        break;
      case BoolExpr::NT_EQV:
        {
          bool n = false;
          BoolVar l;
          if (u.b.l->t == BoolExpr::NT_VAR) {
            l = u.b.l->u.a.x->x;
            if (u.b.l->u.a.neg) n = !n;
          } else {
            l = u.b.l->expr(home,icl);
          }
          BoolVar r;
          if (u.b.r->t == BoolExpr::NT_VAR) {
            r = u.b.r->u.a.x->x;
            if (u.b.r->u.a.neg) n = !n;
          } else {
            r = u.b.r->expr(home,icl);
          }
          Gecode::rel(home, l, n ? BOT_XOR : BOT_EQV, r, b, icl);
        }
        break;
      default:
        GECODE_NEVER;
      }
      return b;
    }

    void
    NNF::post(Home home, NodeType t,
                        BoolVarArgs& bp, BoolVarArgs& bn,
                        int& ip, int& in,
                        IntConLevel icl) const {
      if (this->t != t) {
        switch (this->t) {
        case BoolExpr::NT_VAR:
          if (u.a.neg) {
            bn[in++]=u.a.x->x;
          } else {
            bp[ip++]=u.a.x->x;
          }
          break;
        case BoolExpr::NT_RLIN:
          {
            BoolVar b(home,0,1);
            u.a.x->rl.post(home, b, !u.a.neg, icl);
            bp[ip++]=b;
          }
          break;
#ifdef GECODE_HAS_FLOAT_VARS
        case BoolExpr::NT_RLINFLOAT:
          {
            BoolVar b(home,0,1);
            u.a.x->rfl.post(home, b, !u.a.neg);
            bp[ip++]=b;
          }
          break;
#endif
#ifdef GECODE_HAS_SET_VARS
        case BoolExpr::NT_RSET:
          {
            BoolVar b(home,0,1);
            u.a.x->rs.post(home, b, !u.a.neg);
            bp[ip++]=b;
          }
          break;
#endif
        case BoolExpr::NT_MISC:
          {
            BoolVar b(home,0,1);
            u.a.x->m->post(home, b, !u.a.neg, icl);
            bp[ip++]=b;
          }
          break;      
        default:
          bp[ip++] = expr(home, icl);
          break;
        }
      } else {
        u.b.l->post(home, t, bp, bn, ip, in, icl);
        u.b.r->post(home, t, bp, bn, ip, in, icl);
      }
    }

    void
    NNF::rel(Home home, IntConLevel icl) const {
      switch (t) {
      case BoolExpr::NT_VAR:
        Gecode::rel(home, u.a.x->x, IRT_EQ, u.a.neg ? 0 : 1);
        break;
      case BoolExpr::NT_RLIN:
        u.a.x->rl.post(home, !u.a.neg, icl);
        break;
#ifdef GECODE_HAS_FLOAT_VARS
      case BoolExpr::NT_RLINFLOAT:
        u.a.x->rfl.post(home, !u.a.neg);
        break;
#endif
#ifdef GECODE_HAS_SET_VARS
      case BoolExpr::NT_RSET:
        u.a.x->rs.post(home, !u.a.neg);
        break;
#endif
      case BoolExpr::NT_MISC:
        {
          BoolVar b(home,!u.a.neg,!u.a.neg);
          u.a.x->m->post(home, b, false, icl);
        }
        break;
      case BoolExpr::NT_AND:
        u.b.l->rel(home, icl);
        u.b.r->rel(home, icl);
        break;
      case BoolExpr::NT_OR:
        {
          BoolVarArgs bp(p), bn(n);
          int ip=0, in=0;
          post(home, BoolExpr::NT_OR, bp, bn, ip, in, icl);
          clause(home, BOT_OR, bp, bn, 1);
        }
        break;
      case BoolExpr::NT_EQV:
        if (u.b.l->t==BoolExpr::NT_VAR &&
            u.b.r->t==BoolExpr::NT_RLIN) {
          u.b.r->u.a.x->rl.post(home, u.b.l->u.a.x->x,
                                u.b.l->u.a.neg==u.b.r->u.a.neg, icl);
        } else if (u.b.r->t==BoolExpr::NT_VAR && 
                   u.b.l->t==BoolExpr::NT_RLIN) {
          u.b.l->u.a.x->rl.post(home, u.b.r->u.a.x->x,
                                u.b.l->u.a.neg==u.b.r->u.a.neg, icl);
        } else if (u.b.l->t==BoolExpr::NT_RLIN) {
          u.b.l->u.a.x->rl.post(home, u.b.r->expr(home,icl),
                                !u.b.l->u.a.neg,icl);
        } else if (u.b.r->t==BoolExpr::NT_RLIN) {
          u.b.r->u.a.x->rl.post(home, u.b.l->expr(home,icl),
                                !u.b.r->u.a.neg,icl);
#ifdef GECODE_HAS_FLOAT_VARS
        } else if (u.b.l->t==BoolExpr::NT_VAR && 
                   u.b.r->t==BoolExpr::NT_RLINFLOAT) {
          u.b.r->u.a.x->rfl.post(home, u.b.l->u.a.x->x,
                                 u.b.l->u.a.neg==u.b.r->u.a.neg);
        } else if (u.b.r->t==BoolExpr::NT_VAR && 
                   u.b.l->t==BoolExpr::NT_RLINFLOAT) {
          u.b.l->u.a.x->rfl.post(home, u.b.r->u.a.x->x,
                                 u.b.l->u.a.neg==u.b.r->u.a.neg);
        } else if (u.b.l->t==BoolExpr::NT_RLINFLOAT) {
          u.b.l->u.a.x->rfl.post(home, u.b.r->expr(home,icl),
                                 !u.b.l->u.a.neg);
        } else if (u.b.r->t==BoolExpr::NT_RLINFLOAT) {
          u.b.r->u.a.x->rfl.post(home, u.b.l->expr(home,icl),
                                 !u.b.r->u.a.neg);
#endif
#ifdef GECODE_HAS_SET_VARS
        } else if (u.b.l->t==BoolExpr::NT_VAR && 
                   u.b.r->t==BoolExpr::NT_RSET) {
          u.b.r->u.a.x->rs.post(home, u.b.l->u.a.x->x,
                                u.b.l->u.a.neg==u.b.r->u.a.neg);
        } else if (u.b.r->t==BoolExpr::NT_VAR && 
                   u.b.l->t==BoolExpr::NT_RSET) {
          u.b.l->u.a.x->rs.post(home, u.b.r->u.a.x->x,
                                u.b.l->u.a.neg==u.b.r->u.a.neg);
        } else if (u.b.l->t==BoolExpr::NT_RSET) {
          u.b.l->u.a.x->rs.post(home, u.b.r->expr(home,icl),
                                !u.b.l->u.a.neg);
        } else if (u.b.r->t==BoolExpr::NT_RSET) {
          u.b.r->u.a.x->rs.post(home, u.b.l->expr(home,icl),
                                !u.b.r->u.a.neg);
#endif
        } else {
          Gecode::rel(home, expr(home, icl), IRT_EQ, 1);
        }
        break;
      default:
        GECODE_NEVER;
      }
    }

    NNF*
    NNF::nnf(Region& r, Node* n, bool neg) {
      switch (n->t) {
      case BoolExpr::NT_VAR:
      case BoolExpr::NT_RLIN:
      case BoolExpr::NT_MISC:
  #ifdef GECODE_HAS_FLOAT_VARS
      case BoolExpr::NT_RLINFLOAT:
  #endif
  #ifdef GECODE_HAS_SET_VARS
      case BoolExpr::NT_RSET:
  #endif
        {
          NNF* x = new (r) NNF;
          x->t = n->t; x->u.a.neg = neg; x->u.a.x = n;
          if (neg) {
            x->p = 0; x->n = 1;
          } else {
            x->p = 1; x->n = 0;
          }
          return x;
        }
      case BoolExpr::NT_NOT:
        return nnf(r,n->l,!neg);
      case BoolExpr::NT_AND: case BoolExpr::NT_OR:
        {
          NodeType t = ((n->t == BoolExpr::NT_AND) == neg) ? 
            BoolExpr::NT_OR : BoolExpr::NT_AND;
          NNF* x = new (r) NNF;
          x->t = t;
          x->u.b.l = nnf(r,n->l,neg);
          x->u.b.r = nnf(r,n->r,neg);
          int p_l, n_l;
          if ((x->u.b.l->t == t) ||
              (x->u.b.l->t == BoolExpr::NT_VAR)) {
            p_l=x->u.b.l->p; n_l=x->u.b.l->n;
          } else {
            p_l=1; n_l=0;
          }
          int p_r, n_r;
          if ((x->u.b.r->t == t) ||
              (x->u.b.r->t == BoolExpr::NT_VAR)) {
            p_r=x->u.b.r->p; n_r=x->u.b.r->n;
          } else {
            p_r=1; n_r=0;
          }
          x->p = p_l+p_r;
          x->n = n_l+n_r;
          return x;
        }
      case BoolExpr::NT_EQV:
        {
          NNF* x = new (r) NNF;
          x->t = BoolExpr::NT_EQV;
          x->u.b.l = nnf(r,n->l,neg);
          x->u.b.r = nnf(r,n->r,false);
          x->p = 2; x->n = 0;
          return x;
        }
      default:
        GECODE_NEVER;
      }
      GECODE_NEVER;
      return NULL;
    }
  }
  
  BoolVar
  BoolExpr::expr(Home home, IntConLevel icl) const {
    Region r(home);
    return NNF::nnf(r,n,false)->expr(home,icl);
  }

  void
  BoolExpr::rel(Home home, IntConLevel icl) const {
    Region r(home);
    return NNF::nnf(r,n,false)->rel(home,icl);
  }


  BoolExpr
  operator &&(const BoolExpr& l, const BoolExpr& r) {
    return BoolExpr(l,BoolExpr::NT_AND,r);
  }
  BoolExpr
  operator ||(const BoolExpr& l, const BoolExpr& r) {
    return BoolExpr(l,BoolExpr::NT_OR,r);
  }
  BoolExpr
  operator ^(const BoolExpr& l, const BoolExpr& r) {
    return BoolExpr(BoolExpr(l,BoolExpr::NT_EQV,r),BoolExpr::NT_NOT);
  }

  BoolExpr
  operator !(const BoolExpr& e) {
    return BoolExpr(e,BoolExpr::NT_NOT);
  }

  BoolExpr
  operator !=(const BoolExpr& l, const BoolExpr& r) {
    return !BoolExpr(l, BoolExpr::NT_EQV, r);
  }
  BoolExpr
  operator ==(const BoolExpr& l, const BoolExpr& r) {
    return BoolExpr(l, BoolExpr::NT_EQV, r);
  }
  BoolExpr
  operator >>(const BoolExpr& l, const BoolExpr& r) {
    return BoolExpr(BoolExpr(l,BoolExpr::NT_NOT),
                    BoolExpr::NT_OR,r);
  }
  BoolExpr
  operator <<(const BoolExpr& l, const BoolExpr& r) {
    return BoolExpr(BoolExpr(r,BoolExpr::NT_NOT),
                    BoolExpr::NT_OR,l);
  }
  /*
   * Posting Boolean expressions and relations
   *
   */
  BoolVar
  expr(Home home, const BoolExpr& e, IntConLevel icl) {
    if (!home.failed())
      return e.expr(home,icl);
    BoolVar x(home,0,1);
    return x;
  }

  void
  rel(Home home, const BoolExpr& e, IntConLevel icl) {
    if (home.failed()) return;
    e.rel(home,icl);
  }

  /*
   * Boolean element constraints
   *
   */
  
  /// \brief Boolean element expressions
  class BElementExpr : public BoolExpr::MiscExpr {
  public:
    /// The Boolean expressions
    BoolExpr* a;
    /// The number of Boolean expressions
    int n;
    /// The linear expression for the index
    LinIntExpr idx;
    /// Constructor
    BElementExpr(int size);
    /// Destructor
    virtual ~BElementExpr(void);
    /// Constrain \a b to be equivalent to the expression (negated if \a neg)
    virtual void post(Space& home, BoolVar b, bool neg, IntConLevel icl);
  };

  BElementExpr::BElementExpr(int size)
    : a(heap.alloc<BoolExpr>(size)), n(size) {}

  BElementExpr::~BElementExpr(void) {
    heap.free<BoolExpr>(a,n);
  }
  
  void
  BElementExpr::post(Space& home, BoolVar b, bool pos, IntConLevel icl) {
    IntVar z = idx.post(home, icl);
    if (z.assigned() && z.val() >= 0 && z.val() < n) {
      BoolExpr be = pos ? (a[z.val()] == b) : (a[z.val()] == !b);
      be.rel(home,icl);
    } else {
      BoolVarArgs x(n);
      for (int i=n; i--;)
        x[i] = a[i].expr(home,icl);
      BoolVar res = pos ? b : (!b).expr(home,icl);
      element(home, x, z, res, icl);
    }
  }

  BoolExpr
  element(const BoolVarArgs& b, const LinIntExpr& idx) {
    BElementExpr* be = new BElementExpr(b.size());
    for (int i=b.size(); i--;)
      new (&be->a[i]) BoolExpr(b[i]);
    be->idx = idx;
    return BoolExpr(be);
  }

}

// STATISTICS: minimodel-any
