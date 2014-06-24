/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
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

#ifdef GECODE_HAS_FLOAT_VARS

#include <gecode/float/linear.hh>

namespace Gecode {

  /// Nodes for linear expressions
  class LinFloatExpr::Node {
  public:
    /// Nodes are reference counted
    unsigned int use;
    /// Float variables in tree
    int n_float;
    /// Type of expression
    NodeType t;
    /// Subexpressions
    Node *l, *r;
    /// Sum of integer or Boolean variables, or non-linear expression
    union {
      /// Integer views and coefficients
      Float::Linear::Term* tf;
      /// Non-linear expression
      NonLinFloatExpr* ne;
    } sum;
    /// Coefficient and offset
    FloatVal a, c;
    /// Float variable (potentially)
    FloatVar x_float;
    /// Default constructor
    Node(void);
    /// Generate linear terms from expression
    GECODE_MINIMODEL_EXPORT
    void fill(Home home, Float::Linear::Term*& tf,
              FloatVal m, FloatVal& d) const;
    /// Generate linear terms for expressions
    FloatVal fill(Home home, Float::Linear::Term* tf) const;
    /// Decrement reference count and possibly free memory
    bool decrement(void);
    /// Destructor
    ~Node(void);
    /// Memory management
    static void* operator new(size_t size);
    /// Memory management
    static void  operator delete(void* p,size_t size);
    
  };

  /*
   * Operations for nodes
   *
   */
  forceinline
  LinFloatExpr::Node::Node(void) : use(1) {
  }

  forceinline
  LinFloatExpr::Node::~Node(void) {
    switch (t) {
    case NT_SUM:
      if (n_float > 0)
        heap.free<Float::Linear::Term>(sum.tf,n_float);
      break;
    case NT_NONLIN:
      delete sum.ne;
      break;
    default: ;
    }
  }

  forceinline void*
  LinFloatExpr::Node::operator new(size_t size) {
    return heap.ralloc(size);
  }

  forceinline void
  LinFloatExpr::Node::operator delete(void* p, size_t) {
    heap.rfree(p);
  }

  bool
  LinFloatExpr::Node::decrement(void) {
    if (--use == 0) {
      if ((l != NULL) && l->decrement())
        delete l;
      if ((r != NULL) && r->decrement())
        delete r;
      return true;
    }
    return false;
  }

  /*
   * Operations for float expressions
   *
   */

  LinFloatExpr::LinFloatExpr(const LinFloatExpr& e)
    : n(e.n) {
    n->use++;
  }

  NonLinFloatExpr*
  LinFloatExpr::nlfe(void) const {
    return n->t == NT_NONLIN ? n->sum.ne : NULL;
  }

  FloatVal
  LinFloatExpr::Node::fill(Home home, 
                           Float::Linear::Term* tf) const {
    FloatVal d=0;
    fill(home,tf,1.0,d);
    Float::Limits::check(d,"MiniModel::LinFloatExpr");
    return d;
  }

  void
  LinFloatExpr::post(Home home, FloatRelType frt) const {
    if (home.failed()) return;
    Region r(home);
    if (n->t==NT_ADD && n->l == NULL && n->r->t==NT_NONLIN) {
      n->r->sum.ne->post(home,frt,-n->c);
    } else if (n->t==NT_SUB && n->r->t==NT_NONLIN && n->l==NULL) {
      switch (frt) {
      case FRT_LQ: frt=FRT_GQ; break;
      case FRT_GQ: frt=FRT_LQ; break;
      default: break;
      }
      n->r->sum.ne->post(home,frt,n->c);
    } else if (frt==FRT_EQ &&
               n->t==NT_SUB && n->r->t==NT_NONLIN &&
               n->l != NULL && n->l->t==NT_VAR
               && n->l->a==1) {
      (void) n->r->sum.ne->post(home,&n->l->x_float);
    } else if (frt==FRT_EQ &&
               n->t==NT_SUB && n->r->t==NT_VAR &&
               n->l != NULL && n->l->t==NT_NONLIN
               && n->r->a==1) {
      (void) n->l->sum.ne->post(home,&n->r->x_float);
    } else {
      Float::Linear::Term* fts =
        r.alloc<Float::Linear::Term>(n->n_float);
      FloatVal c = n->fill(home,fts);
      Float::Linear::post(home, fts, n->n_float, frt, -c);
    }
  }

  void
  LinFloatExpr::post(Home home, FloatRelType frt, const BoolVar& b) const {
    if (home.failed()) return;
    Region r(home);
    if (n->t==NT_ADD && n->l==NULL && n->r->t==NT_NONLIN) {
      n->r->sum.ne->post(home,frt,-n->c,b);
    } else if (n->t==NT_SUB && n->l==NULL && n->r->t==NT_NONLIN) {
      switch (frt) {
      case FRT_LQ: frt=FRT_GQ; break;
      case FRT_LE: frt=FRT_GR; break;
      case FRT_GQ: frt=FRT_LQ; break;
      case FRT_GR: frt=FRT_LE; break;
      default: break;
      }
      n->r->sum.ne->post(home,frt,n->c,b);
    } else {
      Float::Linear::Term* fts =
        r.alloc<Float::Linear::Term>(n->n_float);
      FloatVal c = n->fill(home,fts);
      Float::Linear::post(home, fts, n->n_float, frt, -c, b);
    }
    
  }

  FloatVar
  LinFloatExpr::post(Home home) const {
    if (home.failed()) return FloatVar(home,0,0);
    Region r(home);
    Float::Linear::Term* fts =
      r.alloc<Float::Linear::Term>(n->n_float+1);
    FloatVal c = n->fill(home,fts);
    if ((n->n_float == 1) && (c == 0) && (fts[0].a == 1))
      return fts[0].x;
    FloatNum min, max;
    Float::Linear::estimate(&fts[0],n->n_float,c,min,max);
    FloatVar x(home, min, max);
    fts[n->n_float].x = x; fts[n->n_float].a = -1;
    Float::Linear::post(home, fts, n->n_float+1, FRT_EQ, -c);
    return x;
    
  }

  LinFloatExpr::LinFloatExpr(void) :
    n(new Node) {
    n->n_float = 0;
    n->t = NT_VAR;
    n->l = n->r = NULL;
    n->a = 0;
  }

  LinFloatExpr::LinFloatExpr(const FloatVal& c) :
    n(new Node) {
    n->n_float = 0;
    n->t = NT_CONST;
    n->l = n->r = NULL;
    n->a = 0;
    Float::Limits::check(c,"MiniModel::LinFloatExpr");
    n->c = c;
  }

  LinFloatExpr::LinFloatExpr(const FloatVar& x) :
    n(new Node) {
    n->n_float = 1;
    n->t = NT_VAR;
    n->l = n->r = NULL;
    n->a = 1.0;
    n->x_float = x;
  }

  LinFloatExpr::LinFloatExpr(const FloatVar& x, FloatVal a) :
    n(new Node) {
    n->n_float = 1;
    n->t = NT_VAR;
    n->l = n->r = NULL;
    n->a = a;
    n->x_float = x;
  }

  LinFloatExpr::LinFloatExpr(const FloatVarArgs& x) :
    n(new Node) {
    n->n_float = x.size();
    n->t = NT_SUM;
    n->l = n->r = NULL;
    if (x.size() > 0) {
      n->sum.tf = heap.alloc<Float::Linear::Term>(x.size());
      for (int i=x.size(); i--; ) {
        n->sum.tf[i].x = x[i];
        n->sum.tf[i].a = 1.0;
      }
    }
  }

  LinFloatExpr::LinFloatExpr(const FloatValArgs& a, const FloatVarArgs& x) :
    n(new Node) {
    if (a.size() != x.size())
      throw Float::ArgumentSizeMismatch("MiniModel::LinFloatExpr");
    n->n_float = x.size();
    n->t = NT_SUM;
    n->l = n->r = NULL;
    if (x.size() > 0) {
      n->sum.tf = heap.alloc<Float::Linear::Term>(x.size());
      for (int i=x.size(); i--; ) {
        n->sum.tf[i].x = x[i];
        n->sum.tf[i].a = a[i];
      }
    }
  }

  LinFloatExpr::LinFloatExpr(const LinFloatExpr& e0, NodeType t, const LinFloatExpr& e1) :
    n(new Node) {
    n->n_float = e0.n->n_float + e1.n->n_float;
    n->t = t;
    n->l = e0.n; n->l->use++;
    n->r = e1.n; n->r->use++;
  }

  LinFloatExpr::LinFloatExpr(const LinFloatExpr& e, NodeType t, const FloatVal& c) :
    n(new Node) {
    n->n_float = e.n->n_float;
    n->t = t;
    n->l = NULL;
    n->r = e.n; n->r->use++;
    n->c = c;
  }

  LinFloatExpr::LinFloatExpr(FloatVal a, const LinFloatExpr& e) :
    n(new Node) {
    n->n_float = e.n->n_float;
    n->t = NT_MUL;
    n->l = e.n; n->l->use++;
    n->r = NULL;
    n->a = a;
  }

  LinFloatExpr::LinFloatExpr(NonLinFloatExpr* e) :
    n(new Node) {
    n->n_float = 1;
    n->t = NT_NONLIN;
    n->l = n->r = NULL;
    n->a = 0;
    n->sum.ne = e;
  }

  const LinFloatExpr&
  LinFloatExpr::operator =(const LinFloatExpr& e) {
    if (this != &e) {
      if (n->decrement())
        delete n;
      n = e.n; n->use++;
    }
    return *this;
  }

  LinFloatExpr::~LinFloatExpr(void) {
    if (n->decrement())
      delete n;
  }


  void
  LinFloatExpr::Node::fill(Home home, 
                           Float::Linear::Term*& tf, 
                           FloatVal m, FloatVal& d) const {
    switch (this->t) {
    case NT_CONST:
      Float::Limits::check(m*c,"MiniModel::LinFloatExpr");
      d += m*c;
      break;
    case NT_VAR:
      Float::Limits::check(m*a,"MiniModel::LinFloatExpr");
      tf->a=m*a; tf->x=x_float; tf++;
      break;
    case NT_NONLIN:
      tf->a=m; tf->x=sum.ne->post(home, NULL); tf++;
      break;
    case NT_SUM:
      for (int i=n_float; i--; ) {
        Float::Limits::check(m*sum.tf[i].a,"MiniModel::LinFloatExpr");
        tf[i].x = sum.tf[i].x; tf[i].a = m*sum.tf[i].a;
      }
      tf += n_float;
      break;
    case NT_ADD:
      if (l == NULL) {
        Float::Limits::check(m*c,"MiniModel::LinFloatExpr");
        d += m*c;
      } else {
        l->fill(home,tf,m,d);
      }
      r->fill(home,tf,m,d);
      break;
    case NT_SUB:
      if (l == NULL) {
        Float::Limits::check(m*c,"MiniModel::LinFloatExpr");
        d += m*c;
      } else {
        l->fill(home,tf,m,d);
      }
      r->fill(home,tf,-m,d);
      break;
    case NT_MUL:
      Float::Limits::check(m*a,"MiniModel::LinFloatExpr");
      l->fill(home,tf,m*a,d);
      break;
    default:
      GECODE_NEVER;
    }
  }


  /*
   * Operators
   *
   */
  LinFloatExpr
  operator +(const FloatVal& c, const FloatVar& x) {
    if (x.assigned() && Float::Limits::valid(c+x.val()))
      return LinFloatExpr(c+x.val());
    else
      return LinFloatExpr(x,LinFloatExpr::NT_ADD,c);
  }
  LinFloatExpr
  operator +(const FloatVal& c, const LinFloatExpr& e) {
    return LinFloatExpr(e,LinFloatExpr::NT_ADD,c);
  }
  LinFloatExpr
  operator +(const FloatVar& x, const FloatVal& c) {
    if (x.assigned() && Float::Limits::valid(c+x.val()))
      return LinFloatExpr(c+x.val());
    else
      return LinFloatExpr(x,LinFloatExpr::NT_ADD,c);
  }
  LinFloatExpr
  operator +(const LinFloatExpr& e, const FloatVal& c) {
    return LinFloatExpr(e,LinFloatExpr::NT_ADD,c);
  }
  LinFloatExpr
  operator +(const FloatVar& x, const FloatVar& y) {
    if (x.assigned())
      return x.val() + y;
    else if (y.assigned())
      return x + y.val();
    else
      return LinFloatExpr(x,LinFloatExpr::NT_ADD,(const LinFloatExpr&)y);
  }
  LinFloatExpr
  operator +(const FloatVar& x, const LinFloatExpr& e) {
    if (x.assigned())
      return x.val() + e;
    else
      return LinFloatExpr(x,LinFloatExpr::NT_ADD,e);
  }
  LinFloatExpr
  operator +(const LinFloatExpr& e, const FloatVar& x) {
    if (x.assigned())
      return e + x.val();
    else
      return LinFloatExpr(e,LinFloatExpr::NT_ADD,(const LinFloatExpr&)x);
  }
  LinFloatExpr
  operator +(const LinFloatExpr& e1, const LinFloatExpr& e2) {
    return LinFloatExpr(e1,LinFloatExpr::NT_ADD,e2);
  }

  LinFloatExpr
  operator -(const FloatVal& c, const FloatVar& x) {
    if (x.assigned() && Float::Limits::valid(c-x.val()))
      return LinFloatExpr(c-x.val());
    else
      return LinFloatExpr(x,LinFloatExpr::NT_SUB,c);
  }
  LinFloatExpr
  operator -(const FloatVal& c, const LinFloatExpr& e) {
    return LinFloatExpr(e,LinFloatExpr::NT_SUB,c);
  }
  LinFloatExpr
  operator -(const FloatVar& x, const FloatVal& c) {
    if (x.assigned() && Float::Limits::valid(x.val()-c))
      return LinFloatExpr(x.val()-c);
    else
      return LinFloatExpr(x,LinFloatExpr::NT_ADD,-c);
  }
  LinFloatExpr
  operator -(const LinFloatExpr& e, const FloatVal& c) {
    return LinFloatExpr(e,LinFloatExpr::NT_ADD,-c);
  }
  LinFloatExpr
  operator -(const FloatVar& x, const FloatVar& y) {
    if (x.assigned())
      return x.val() - y;
    else if (y.assigned())
      return x - y.val();
    else
      return LinFloatExpr(x,LinFloatExpr::NT_SUB,(const LinFloatExpr&)y);
  }
  LinFloatExpr
  operator -(const FloatVar& x, const LinFloatExpr& e) {
    if (x.assigned())
      return x.val() - e;
    else
      return LinFloatExpr(x,LinFloatExpr::NT_SUB,e);
  }
  LinFloatExpr
  operator -(const LinFloatExpr& e, const FloatVar& x) {
    if (x.assigned())
      return e - x.val();
    else
      return LinFloatExpr(e,LinFloatExpr::NT_SUB,(const LinFloatExpr&)x);
  }
  LinFloatExpr
  operator -(const LinFloatExpr& e1, const LinFloatExpr& e2) {
    return LinFloatExpr(e1,LinFloatExpr::NT_SUB,e2);
  }

  LinFloatExpr
  operator -(const FloatVar& x) {
    if (x.assigned())
      return LinFloatExpr(-x.val());
    else
      return LinFloatExpr(x,LinFloatExpr::NT_SUB,0);
  }
  LinFloatExpr
  operator -(const LinFloatExpr& e) {
    return LinFloatExpr(e,LinFloatExpr::NT_SUB,0);
  }

  LinFloatExpr
  operator *(const FloatVal& a, const FloatVar& x) {
    if (a == 0)
      return LinFloatExpr(0.0);
    else if (x.assigned() && 
             Float::Limits::valid(a*x.val()))
      return LinFloatExpr(a*x.val());
    else
      return LinFloatExpr(x,a);
  }
  LinFloatExpr
  operator *(const FloatVar& x, const FloatVal& a) {
    if (a == 0)
      return LinFloatExpr(0.0);
    else if (x.assigned() && 
             Float::Limits::valid(a*x.val()))
      return LinFloatExpr(a*x.val());
    else
      return LinFloatExpr(x,a);
  }
  LinFloatExpr
  operator *(const LinFloatExpr& e, const FloatVal& a) {
    if (a == 0)
      return LinFloatExpr(0.0);
    else
      return LinFloatExpr(a,e);
  }
  LinFloatExpr
  operator *(const FloatVal& a, const LinFloatExpr& e) {
    if (a == 0)
      return LinFloatExpr(0.0);
    else
      return LinFloatExpr(a,e);
  }

  LinFloatExpr
  sum(const FloatVarArgs& x) {
    return LinFloatExpr(x);
  }
  LinFloatExpr
  sum(const FloatValArgs& a, const FloatVarArgs& x) {
    return LinFloatExpr(a,x);
  }

  FloatVar
  expr(Home home, const LinFloatExpr& e) {
    if (!home.failed())
      return e.post(home);
    FloatVar x(home,Float::Limits::min,Float::Limits::max);
    return x;
  }

}

#endif

// STATISTICS: minimodel-any
