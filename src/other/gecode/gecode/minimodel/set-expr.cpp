/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2010
 *     Christian Schulte, 2004
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

#ifdef GECODE_HAS_SET_VARS

namespace Gecode {

  namespace {
    /// Check if types agree
    static bool same(SetExpr::NodeType t0, SetExpr::NodeType t1) {
      return (t0==t1) || (t1==SetExpr::NT_VAR) ||
             (t1==SetExpr::NT_CONST) || (t1==SetExpr::NT_LEXP);
    }
  }
  
  /// %Node for set expression
  class SetExpr::Node {
  public:
    /// Nodes are reference counted
    unsigned int use;
    /// Number of variables in subtree with same type (for INTER and UNION)
    int same;
    /// Type of expression
    NodeType t;
    /// Subexpressions
    Node *l, *r;
    /// Possibly a variable
    SetVar x;
    /// Possibly a constant
    IntSet s;
    /// Possibly a linear expression
    LinIntExpr e;

    /// Default constructor
    Node(void);
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
  SetExpr::Node::Node(void) : use(1) {}

  void*
  SetExpr::Node::operator new(size_t size) {
    return heap.ralloc(size);
  }
  void
  SetExpr::Node::operator delete(void* p, size_t) {
    heap.rfree(p);
  }

  bool
  SetExpr::Node::decrement(void) {
    if (--use == 0) {
      if ((l != NULL) && l->decrement())
        delete l;
      if ((r != NULL) && r->decrement())
        delete r;
      return true;
    }
    return false;
  }

  namespace {
    /// %Node for negation normalform (%NNF)
    class NNF {
    public:
      typedef SetExpr::NodeType NodeType;
      typedef SetExpr::Node Node;
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
          /// Pointer to corresponding Boolean expression node
          Node* x;
        } a;
      } u;
      /// Is formula negative
      bool neg;
      /// Create negation normalform
      static NNF* nnf(Region& r, Node* n, bool neg);
      /// Post propagators for nested conjunctive and disjunctive expression
      void post(Home home, NodeType t, SetVarArgs& b, int& i) const;
      /// Post propagators for expression
      void post(Home home, SetRelType srt, SetVar s) const;
      /// Post propagators for reified expression
      void post(Home home, SetRelType srt, SetVar s, BoolVar b) const;
      /// Post propagators for relation
      void post(Home home, SetRelType srt, const NNF* n) const;
      /// Post reified propagators for relation (or negated relation if \a t is false)
      void post(Home home, BoolVar b, bool t, SetRelType srt,
                const NNF* n) const;
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

    void
    NNF::post(Home home, SetRelType srt, SetVar s) const {
      switch (t) {
      case SetExpr::NT_VAR:
        if (neg) {
          switch (srt) {
          case SRT_EQ:
            rel(home, u.a.x->x, SRT_CMPL, s);
            break;
          case SRT_CMPL:
            rel(home, u.a.x->x, SRT_EQ, s);
            break;
          default:
            SetVar bc(home,IntSet::empty,
                      IntSet(Set::Limits::min,Set::Limits::max));
            rel(home, s, SRT_CMPL, bc);
            rel(home, u.a.x->x, srt, bc);
            break;
          }
        } else
          rel(home, u.a.x->x, srt, s);
        break;
      case SetExpr::NT_CONST:
        {
          IntSet ss;
          if (neg) {
            IntSetRanges sr(u.a.x->s);
            Set::RangesCompl<IntSetRanges> src(sr);
            ss = IntSet(src);
          } else {
            ss = u.a.x->s;
          }
          switch (srt) {
          case SRT_SUB: srt = SRT_SUP; break;
          case SRT_SUP: srt = SRT_SUB; break;
          default: break;
          }
          dom(home, s, srt, ss);
        }
        break;
      case SetExpr::NT_LEXP:
        {
          IntVar iv = u.a.x->e.post(home,ICL_DEF);
          if (neg) {
            SetVar ic(home,IntSet::empty,
                      IntSet(Set::Limits::min,Set::Limits::max));
            rel(home, iv, SRT_CMPL, ic);
            rel(home,ic,srt,s);
          } else {
            rel(home,iv,srt,s);
          }
        }
        break;
      case SetExpr::NT_INTER:
        {
          SetVarArgs bs(p+n);
          int i=0;
          post(home, SetExpr::NT_INTER, bs, i);
          if (i == 2) {
            rel(home, bs[0], SOT_INTER, bs[1], srt, s);
          } else {
            if (srt == SRT_EQ)
              rel(home, SOT_INTER, bs, s);
            else {
              SetVar bc(home,IntSet::empty,
                        IntSet(Set::Limits::min,Set::Limits::max));
              rel(home, SOT_INTER, bs, bc);
              rel(home, bc, srt, s);
            }
          }
        }
        break;
      case SetExpr::NT_UNION:
        {
          SetVarArgs bs(p+n);
          int i=0;
          post(home, SetExpr::NT_UNION, bs, i);
          if (i == 2) {
            rel(home, bs[0], SOT_UNION, bs[1], srt, s);
          } else {
            if (srt == SRT_EQ)
              rel(home, SOT_UNION, bs, s);
            else {
              SetVar bc(home,IntSet::empty,
                        IntSet(Set::Limits::min,Set::Limits::max));
              rel(home, SOT_UNION, bs, bc);
              rel(home, bc, srt, s);
            }
          }
        }
        break;
      case SetExpr::NT_DUNION:
        {
          SetVarArgs bs(p+n);
          int i=0;
          post(home, SetExpr::NT_DUNION, bs, i);

          if (i == 2) {
            if (neg) {
              if (srt == SRT_CMPL) {
                rel(home, bs[0], SOT_DUNION, bs[1], srt, s);
              } else {
                SetVar bc(home,IntSet::empty,
                  IntSet(Set::Limits::min,Set::Limits::max));
                rel(home,s,SRT_CMPL,bc);
                rel(home, bs[0], SOT_DUNION, bs[1], srt, bc);
              }
            } else {
              rel(home, bs[0], SOT_DUNION, bs[1], srt, s);
            }
          } else {
            if (neg) {
              if (srt == SRT_CMPL) {
                rel(home, SOT_DUNION, bs, s);
              } else {
                SetVar br(home,IntSet::empty,
                          IntSet(Set::Limits::min,Set::Limits::max));
                rel(home, SOT_DUNION, bs, br);
                if (srt == SRT_EQ)
                  rel(home, br, SRT_CMPL, s);
                else {
                  SetVar bc(home,IntSet::empty,
                            IntSet(Set::Limits::min,Set::Limits::max));
                  rel(home, br, srt, bc);
                  rel(home, bc, SRT_CMPL, s);
                }
              }
            } else {
              if (srt == SRT_EQ)
                rel(home, SOT_DUNION, bs, s);
              else {
                SetVar br(home,IntSet::empty,
                          IntSet(Set::Limits::min,Set::Limits::max));
                rel(home, SOT_DUNION, bs, br);
                rel(home, br, srt, s);
              }
            }
          }
        }
        break;
      default:
        GECODE_NEVER;
      }
    }

    void
    NNF::post(Home home, SetRelType srt, SetVar s, BoolVar b) const {
      switch (t) {
      case SetExpr::NT_VAR:
        if (neg) {
          switch (srt) {
          case SRT_EQ:
            rel(home, u.a.x->x, SRT_CMPL, s, b);
            break;
          case SRT_CMPL:
            rel(home, u.a.x->x, SRT_EQ, s, b);
            break;
          default:
            SetVar bc(home,IntSet::empty,
                      IntSet(Set::Limits::min,Set::Limits::max));
            rel(home, s, SRT_CMPL, bc);
            rel(home, u.a.x->x, srt, bc, b);
            break;
          }
        } else
          rel(home, u.a.x->x, srt, s, b);
        break;
      case SetExpr::NT_CONST:
        {
          IntSet ss;
          if (neg) {
            IntSetRanges sr(u.a.x->s);
            Set::RangesCompl<IntSetRanges> src(sr);
            ss = IntSet(src);
          } else {
            ss = u.a.x->s;
          }
          SetRelType invsrt;
          switch (srt) {
          case SRT_SUB: invsrt = SRT_SUP; break;
          case SRT_SUP: invsrt = SRT_SUB; break;
          case SRT_LQ:  invsrt = SRT_GQ; break;
          case SRT_LE:  invsrt = SRT_GR; break;
          case SRT_GQ:  invsrt = SRT_LQ; break;
          case SRT_GR:  invsrt = SRT_LE; break;
          case SRT_EQ:
          case SRT_NQ:
          case SRT_DISJ:
          case SRT_CMPL:
            invsrt = srt;
            break;
          }
          dom(home, s, invsrt, ss, b);
        }
        break;
      case SetExpr::NT_LEXP:
        {
          IntVar iv = u.a.x->e.post(home,ICL_DEF);
          if (neg) {
            SetVar ic(home,IntSet::empty,
                      IntSet(Set::Limits::min,Set::Limits::max));
            rel(home, iv, SRT_CMPL, ic);
            rel(home,ic,srt,s,b);
          } else {
            rel(home,iv,srt,s,b);
          }
        }
      case SetExpr::NT_INTER:
        {
          SetVarArgs bs(p+n);
          int i=0;
          post(home, SetExpr::NT_INTER, bs, i);
          SetVar br(home,IntSet::empty,
                    IntSet(Set::Limits::min,Set::Limits::max));
          rel(home, SOT_INTER, bs, br);
          rel(home, br, srt, s, b);
        }
        break;
      case SetExpr::NT_UNION:
        {
          SetVarArgs bs(p+n);
          int i=0;
          post(home, SetExpr::NT_UNION, bs, i);
          SetVar br(home,IntSet::empty,
                    IntSet(Set::Limits::min,Set::Limits::max));
          rel(home, SOT_UNION, bs, br);
          rel(home, br, srt, s, b);
        }
        break;
      case SetExpr::NT_DUNION:
        {
          SetVarArgs bs(p+n);
          int i=0;
          post(home, SetExpr::NT_DUNION, bs, i);

          if (neg) {
            SetVar br(home,IntSet::empty,
                      IntSet(Set::Limits::min,Set::Limits::max));
            rel(home, SOT_DUNION, bs, br);
            if (srt == SRT_CMPL)
              rel(home, br, SRT_EQ, s, b);
            else if (srt == SRT_EQ)
              rel(home, br, SRT_CMPL, s, b);
            else {
              SetVar bc(home,IntSet::empty,
                        IntSet(Set::Limits::min,Set::Limits::max));
              rel(home, br, srt, bc);
              rel(home, bc, SRT_CMPL, s, b);
            }
          } else {
            SetVar br(home,IntSet::empty,
                      IntSet(Set::Limits::min,Set::Limits::max));
            rel(home, SOT_DUNION, bs, br);
            rel(home, br, srt, s, b);
          }
        }
        break;
      default:
        GECODE_NEVER;
      }
    }

    void
    NNF::post(Home home, NodeType t, SetVarArgs& b, int& i) const {
      if (this->t != t) {
        switch (this->t) {
        case SetExpr::NT_VAR:
          if (neg) {
            SetVar xc(home,IntSet::empty,
                      IntSet(Set::Limits::min,Set::Limits::max));
            rel(home, xc, SRT_CMPL, u.a.x->x);
            b[i++]=xc;
          } else {
            b[i++]=u.a.x->x;
          }
          break;
        default:
          {
            SetVar s(home,IntSet::empty,
                     IntSet(Set::Limits::min,Set::Limits::max));
            post(home,SRT_EQ,s);
            b[i++] = s;
          }
          break;
        }
      } else {
        u.b.l->post(home, t, b, i);
        u.b.r->post(home, t, b, i);
      }
    }

    void
    NNF::post(Home home, SetRelType srt, const NNF* n) const {
      if (n->t == SetExpr::NT_VAR && !n->neg) {
        post(home,srt,n->u.a.x->x);
      } else if (t == SetExpr::NT_VAR && !neg) {
        SetRelType n_srt;
        switch (srt) {
        case SRT_SUB: n_srt = SRT_SUP; break;
        case SRT_SUP: n_srt = SRT_SUB; break;
        default: n_srt = srt;
        }
        n->post(home,n_srt,this);
      } else {
        SetVar nx(home,IntSet::empty,
                  IntSet(Set::Limits::min,Set::Limits::max));
        n->post(home,SRT_EQ,nx);
        post(home,srt,nx);
      }
    }

    void
    NNF::post(Home home, BoolVar b, bool pt,
              SetRelType srt, const NNF* n) const {
      if (pt) {
        if (n->t == SetExpr::NT_VAR && !n->neg) {
          post(home,srt,n->u.a.x->x,b);
        } else if (t == SetExpr::NT_VAR && !neg) {
          SetRelType n_srt;
          switch (srt) {
          case SRT_SUB: n_srt = SRT_SUP; break;
          case SRT_SUP: n_srt = SRT_SUB; break;
          default: n_srt = srt;
          }
          n->post(home,b,true,n_srt,this);
        } else {
          SetVar nx(home,IntSet::empty,
                    IntSet(Set::Limits::min,Set::Limits::max));
          n->post(home,SRT_EQ,nx);
          post(home,srt,nx,b);
        }
      } else if (srt == SRT_EQ) {
        post(home,b,true,SRT_NQ,n);
      } else if (srt == SRT_NQ) {
        post(home,b,true,SRT_EQ,n);
      } else {
        BoolVar nb(home,0,1);
        rel(home,b,IRT_NQ,nb);
        post(home,nb,true,srt,n);
      }
    }

    NNF*
    NNF::nnf(Region& r, Node* n, bool neg) {
      switch (n->t) {
      case SetExpr::NT_VAR:
      case SetExpr::NT_CONST:
      case SetExpr::NT_LEXP:
        {
          NNF* x = new (r) NNF;
          x->t = n->t; x->neg = neg; x->u.a.x = n;
          if (neg) {
            x->p = 0; x->n = 1;
          } else {
            x->p = 1; x->n = 0;
          }
          return x;
        }
      case SetExpr::NT_CMPL:
        return nnf(r,n->l,!neg);
      case SetExpr::NT_INTER:
      case SetExpr::NT_UNION:
      case SetExpr::NT_DUNION:
        {
          NodeType t; bool xneg;
          if (n->t == SetExpr::NT_DUNION) {
            t = n->t; xneg = neg; neg = false;
          } else {
            t = ((n->t == SetExpr::NT_INTER) == neg) ?
              SetExpr::NT_UNION : SetExpr::NT_INTER;
            xneg = false;
          }
          NNF* x = new (r) NNF;
          x->neg = xneg;
          x->t = t;
          x->u.b.l = nnf(r,n->l,neg);
          x->u.b.r = nnf(r,n->r,neg);
          int p_l, n_l;
          if ((x->u.b.l->t == t) || (x->u.b.l->t == SetExpr::NT_VAR)) {
            p_l=x->u.b.l->p; n_l=x->u.b.l->n;
          } else {
            p_l=1; n_l=0;
          }
          int p_r, n_r;
          if ((x->u.b.r->t == t) || (x->u.b.r->t == SetExpr::NT_VAR)) {
            p_r=x->u.b.r->p; n_r=x->u.b.r->n;
          } else {
            p_r=1; n_r=0;
          }
          x->p = p_l+p_r;
          x->n = n_l+n_r;
          return x;
        }
      default:
        GECODE_NEVER;
      }
      GECODE_NEVER;
      return NULL;
    }
  }

  SetExpr::SetExpr(const SetVar& x) : n(new Node) {
    n->same = 1;
    n->t    = NT_VAR;
    n->l    = NULL;
    n->r    = NULL;
    n->x    = x;
  }

  SetExpr::SetExpr(const IntSet& s) : n(new Node) {
    n->same = 1;
    n->t    = NT_CONST;
    n->l    = NULL;
    n->r    = NULL;
    n->s    = s;
  }

  SetExpr::SetExpr(const LinIntExpr& e) : n(new Node) {
    n->same = 1;
    n->t    = NT_LEXP;
    n->l    = NULL;
    n->r    = NULL;
    n->e    = e;
  }
  
  SetExpr::SetExpr(const SetExpr& l, NodeType t, const SetExpr& r)
    : n(new Node) {
    int ls = same(t,l.n->t) ? l.n->same : 1;
    int rs = same(t,r.n->t) ? r.n->same : 1;
    n->same = ls+rs;
    n->t    = t;
    n->l    = l.n;
    n->l->use++;
    n->r    = r.n;
    n->r->use++;
  }

  SetExpr::SetExpr(const SetExpr& l, NodeType t) {
    (void) t;
    assert(t == NT_CMPL);
    if (l.n->t == NT_CMPL) {
      n = l.n->l;
      n->use++;
    } else {
      n = new Node;
      n->same = 1;
      n->t    = NT_CMPL;
      n->l    = l.n;
      n->l->use++;
      n->r    = NULL;
    }
  }

  const SetExpr&
  SetExpr::operator =(const SetExpr& e) {
    if (this != &e) {
      if (n != NULL && n->decrement())
        delete n;
      n = e.n;
      n->use++;
    }
    return *this;
  }

  SetExpr::~SetExpr(void) {
    if (n != NULL && n->decrement())
      delete n;
  }

  SetExpr::SetExpr(const SetExpr& e) : n(e.n) {
    n->use++;
  }

  SetVar
  SetExpr::post(Home home) const {
    Region r(home);
    SetVar s(home,IntSet::empty,
             IntSet(Set::Limits::min,Set::Limits::max));
    NNF::nnf(r,n,false)->post(home,SRT_EQ,s);
    return s;
  }

  void
  SetExpr::post(Home home, SetRelType srt, const SetExpr& e) const {
    Region r(home);
    return NNF::nnf(r,n,false)->post(home,srt,NNF::nnf(r,e.n,false));
  }
  void
  SetExpr::post(Home home, BoolVar b, bool t,
                SetRelType srt, const SetExpr& e) const {
    Region r(home);
    return NNF::nnf(r,n,false)->post(home,b,t,srt,
                                     NNF::nnf(r,e.n,false));
  }

  SetExpr
  operator &(const SetExpr& l, const SetExpr& r) {
    return SetExpr(l,SetExpr::NT_INTER,r);
  }
  SetExpr
  operator |(const SetExpr& l, const SetExpr& r) {
    return SetExpr(l,SetExpr::NT_UNION,r);
  }
  SetExpr
  operator +(const SetExpr& l, const SetExpr& r) {
    return SetExpr(l,SetExpr::NT_DUNION,r);
  }
  SetExpr
  operator -(const SetExpr& e) {
    return SetExpr(e,SetExpr::NT_CMPL);
  }
  SetExpr
  operator -(const SetExpr& l, const SetExpr& r) {
    return SetExpr(l,SetExpr::NT_INTER,-r);
  }
  SetExpr
  singleton(const LinIntExpr& e) {
    return SetExpr(e);
  }

  SetExpr
  inter(const SetVarArgs& x) {
    if (x.size() == 0)
      return SetExpr(IntSet(Set::Limits::min,Set::Limits::max));
    SetExpr r(x[0]);
    for (int i=1; i<x.size(); i++)
      r = (r & x[i]);
    return r;
  }
  SetExpr
  setunion(const SetVarArgs& x) {
    if (x.size() == 0)
      return SetExpr(IntSet::empty);
    SetExpr r(x[0]);
    for (int i=1; i<x.size(); i++)
      r = (r | x[i]);
    return r;    
  }
  SetExpr
  setdunion(const SetVarArgs& x) {
    if (x.size() == 0)
      return SetExpr(IntSet::empty);
    SetExpr r(x[0]);
    for (int i=1; i<x.size(); i++)
      r = (r + x[i]);
    return r;    
  }

  namespace MiniModel {
    /// Integer valued set expressions
    class GECODE_MINIMODEL_EXPORT SetNonLinIntExpr : public NonLinIntExpr {
    public:
      /// The expression type
      enum SetNonLinIntExprType {
        SNLE_CARD, ///< Cardinality expression
        SNLE_MIN,  ///< Minimum element expression
        SNLE_MAX   ///< Maximum element expression
      } t;
      /// The expression
      SetExpr e;
      /// Constructor
      SetNonLinIntExpr(const SetExpr& e0, SetNonLinIntExprType t0)
        : t(t0), e(e0) {}
      /// Post expression
      virtual IntVar post(Home home, IntVar* ret, IntConLevel) const {
        IntVar m = result(home,ret);
        switch (t) {
        case SNLE_CARD:
          cardinality(home, e.post(home), m);
          break;
        case SNLE_MIN:
          min(home, e.post(home), m);
          break;
        case SNLE_MAX:
          max(home, e.post(home), m);
          break;
        default:
          GECODE_NEVER;
          break;
        }
        return m;
      }
      virtual void post(Home home, IntRelType irt, int c,
                        IntConLevel icl) const {
        if (t==SNLE_CARD && irt!=IRT_NQ) {
          switch (irt) {
          case IRT_LQ:
            cardinality(home, e.post(home), 
                        0U, 
                        static_cast<unsigned int>(c));
            break;
          case IRT_LE:
            cardinality(home, e.post(home), 
                        0U, 
                        static_cast<unsigned int>(c-1));
            break;
          case IRT_GQ:
            cardinality(home, e.post(home), 
                        static_cast<unsigned int>(c), 
                        Set::Limits::card);
            break;
          case IRT_GR:
            cardinality(home, e.post(home), 
                        static_cast<unsigned int>(c+1), 
                        Set::Limits::card);
            break;
          case IRT_EQ:
            cardinality(home, e.post(home), 
                        static_cast<unsigned int>(c),
                        static_cast<unsigned int>(c));
            break;
          default:
            GECODE_NEVER;
          }
        } else if (t==SNLE_MIN && (irt==IRT_GR || irt==IRT_GQ)) {
          c = (irt==IRT_GQ ? c : c+1);
          dom(home, e.post(home), SRT_SUB, c, Set::Limits::max);
        } else if (t==SNLE_MAX && (irt==IRT_LE || irt==IRT_LQ)) {
          c = (irt==IRT_LQ ? c : c-1);
          dom(home, e.post(home), SRT_SUB, Set::Limits::min, c);
        } else {
          rel(home, post(home,NULL,icl), irt, c);
        }
      }
      virtual void post(Home home, IntRelType irt, int c,
                        BoolVar b, IntConLevel icl) const {
        if (t==SNLE_MIN && (irt==IRT_GR || irt==IRT_GQ)) {
          c = (irt==IRT_GQ ? c : c+1);
          dom(home, e.post(home), SRT_SUB, c, Set::Limits::max, b);
        } else if (t==SNLE_MAX && (irt==IRT_LE || irt==IRT_LQ)) {
          c = (irt==IRT_LQ ? c : c-1);
          dom(home, e.post(home), SRT_SUB, Set::Limits::min, c, b);
        } else {
          rel(home, post(home,NULL,icl), irt, c, b);
        }
      }
    };
  }

  LinIntExpr
  cardinality(const SetExpr& e) {
    return LinIntExpr(new MiniModel::SetNonLinIntExpr(e,
      MiniModel::SetNonLinIntExpr::SNLE_CARD));
  }
  LinIntExpr
  min(const SetExpr& e) {
    return LinIntExpr(new MiniModel::SetNonLinIntExpr(e,
      MiniModel::SetNonLinIntExpr::SNLE_MIN));
  }
  LinIntExpr
  max(const SetExpr& e) {
    return LinIntExpr(new MiniModel::SetNonLinIntExpr(e,
      MiniModel::SetNonLinIntExpr::SNLE_MAX));
  }

  /*
   * Posting set expressions
   *
   */
  SetVar
  expr(Home home, const SetExpr& e) {
    if (!home.failed())
      return e.post(home);
    SetVar x(home,IntSet::empty,IntSet::empty);
    return x;
  }


}

#endif

// STATISTICS: minimodel-any
