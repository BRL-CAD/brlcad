/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2010
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
#include <gecode/int/linear.hh>

namespace Gecode {

  /// Nodes for linear expressions
  class LinIntExpr::Node {
  public:
    /// Nodes are reference counted
    unsigned int use;
    /// Integer variables in tree
    int n_int;
    /// Boolean variables in tree
    int n_bool;
    /// Type of expression
    NodeType t;
    /// Subexpressions
    Node *l, *r;
    /// Sum of integer or Boolean variables, or non-linear expression
    union {
      /// Integer views and coefficients
      Int::Linear::Term<Int::IntView>* ti;
      /// Bool views and coefficients
      Int::Linear::Term<Int::BoolView>* tb;
      /// Non-linear expression
      NonLinIntExpr* ne;
    } sum;
    /// Coefficient and offset
    int a, c;
    /// Integer variable (potentially)
    IntVar x_int;
    /// Boolean variable (potentially)
    BoolVar x_bool;
    /// Default constructor
    Node(void);
    /// Generate linear terms from expression
    void fill(Home home, IntConLevel icl,
              Int::Linear::Term<Int::IntView>*& ti,
              Int::Linear::Term<Int::BoolView>*& tb,
              long long int m, long long int& d) const;
    /// Generate linear terms for expressions
    int fill(Home home, IntConLevel icl,
             Int::Linear::Term<Int::IntView>* ti,
             Int::Linear::Term<Int::BoolView>* tb) const;
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
  LinIntExpr::Node::Node(void) : use(1) {
  }

  forceinline
  LinIntExpr::Node::~Node(void) {
    switch (t) {
    case NT_SUM_INT:
      if (n_int > 0)
        heap.free<Int::Linear::Term<Int::IntView> >(sum.ti,n_int);
      break;
    case NT_SUM_BOOL:
      if (n_bool > 0)
        heap.free<Int::Linear::Term<Int::BoolView> >(sum.tb,n_bool);
      break;
    case NT_NONLIN:
      delete sum.ne;
      break;
    default: ;
    }
  }

  forceinline void*
  LinIntExpr::Node::operator new(size_t size) {
    return heap.ralloc(size);
  }

  forceinline void
  LinIntExpr::Node::operator delete(void* p, size_t) {
    heap.rfree(p);
  }
  bool
  LinIntExpr::Node::decrement(void) {
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
   * Operations for expressions
   *
   */

  LinIntExpr::LinIntExpr(const LinIntExpr& e)
    : n(e.n) {
    n->use++;
  }

  int
  LinIntExpr::Node::fill(Home home, IntConLevel icl,
                      Int::Linear::Term<Int::IntView>* ti, 
                      Int::Linear::Term<Int::BoolView>* tb) const {
    long long int d=0;
    fill(home,icl,ti,tb,1,d);
    Int::Limits::check(d,"MiniModel::LinIntExpr");
    return static_cast<int>(d);
  }

  void
  LinIntExpr::post(Home home, IntRelType irt, IntConLevel icl) const {
    if (home.failed()) return;
    Region r(home);
    if (n->n_bool == 0) {
      // Only integer variables
      if (n->t==NT_ADD && n->l == NULL && n->r->t==NT_NONLIN) {
        n->r->sum.ne->post(home,irt,-n->c,icl);
      } else if (n->t==NT_SUB && n->r->t==NT_NONLIN && n->l==NULL) {
        switch (irt) {
        case IRT_LQ: irt=IRT_GQ; break;
        case IRT_LE: irt=IRT_GR; break;
        case IRT_GQ: irt=IRT_LQ; break;
        case IRT_GR: irt=IRT_LE; break;
        default: break;
        }
        n->r->sum.ne->post(home,irt,n->c,icl);
      } else if (irt==IRT_EQ &&
                 n->t==NT_SUB && n->r->t==NT_NONLIN &&
                 n->l != NULL && n->l->t==NT_VAR_INT
                 && n->l->a==1) {
        (void) n->r->sum.ne->post(home,&n->l->x_int,icl);
      } else if (irt==IRT_EQ &&
                 n->t==NT_SUB && n->r->t==NT_VAR_INT &&
                 n->l != NULL && n->l->t==NT_NONLIN
                 && n->r->a==1) {
        (void) n->l->sum.ne->post(home,&n->r->x_int,icl);
      } else {
        Int::Linear::Term<Int::IntView>* its =
          r.alloc<Int::Linear::Term<Int::IntView> >(n->n_int);
        int c = n->fill(home,icl,its,NULL);
        Int::Linear::post(home, its, n->n_int, irt, -c, icl);
      }
    } else if (n->n_int == 0) {
      // Only Boolean variables
      Int::Linear::Term<Int::BoolView>* bts =
        r.alloc<Int::Linear::Term<Int::BoolView> >(n->n_bool);
      int c = n->fill(home,icl,NULL,bts);
      Int::Linear::post(home, bts, n->n_bool, irt, -c, icl);
    } else if (n->n_bool == 1) {
      // Integer variables and only one Boolean variable
      Int::Linear::Term<Int::IntView>* its =
        r.alloc<Int::Linear::Term<Int::IntView> >(n->n_int+1);
      Int::Linear::Term<Int::BoolView>* bts =
        r.alloc<Int::Linear::Term<Int::BoolView> >(1);
      int c = n->fill(home,icl,its,bts);
      IntVar x(home,0,1);
      channel(home,bts[0].x,x);
      its[n->n_int].x = x;
      its[n->n_int].a = bts[0].a;
      Int::Linear::post(home, its, n->n_int+1, irt, -c, icl);
    } else {
      // Both integer and Boolean variables
      Int::Linear::Term<Int::IntView>* its =
        r.alloc<Int::Linear::Term<Int::IntView> >(n->n_int+1);
      Int::Linear::Term<Int::BoolView>* bts =
        r.alloc<Int::Linear::Term<Int::BoolView> >(n->n_bool);
      int c = n->fill(home,icl,its,bts);
      int min, max;
      Int::Linear::estimate(&bts[0],n->n_bool,0,min,max);
      IntVar x(home,min,max);
      its[n->n_int].x = x; its[n->n_int].a = 1;
      Int::Linear::post(home, bts, n->n_bool, IRT_EQ, x, 0, icl);
      Int::Linear::post(home, its, n->n_int+1, irt, -c, icl);
    }
  }

  void
  LinIntExpr::post(Home home, IntRelType irt, const BoolVar& b,
                IntConLevel icl) const {
    if (home.failed()) return;
    Region r(home);
    if (n->n_bool == 0) {
      // Only integer variables
      if (n->t==NT_ADD && n->l==NULL && n->r->t==NT_NONLIN) {
        n->r->sum.ne->post(home,irt,-n->c,b,icl);
      } else if (n->t==NT_SUB && n->l==NULL && n->r->t==NT_NONLIN) {
        switch (irt) {
        case IRT_LQ: irt=IRT_GQ; break;
        case IRT_LE: irt=IRT_GR; break;
        case IRT_GQ: irt=IRT_LQ; break;
        case IRT_GR: irt=IRT_LE; break;
        default: break;
        }
        n->r->sum.ne->post(home,irt,n->c,b,icl);
      } else {
        Int::Linear::Term<Int::IntView>* its =
          r.alloc<Int::Linear::Term<Int::IntView> >(n->n_int);
        int c = n->fill(home,icl,its,NULL);
        Int::Linear::post(home, its, n->n_int, irt, -c, b, icl);
      }
    } else if (n->n_int == 0) {
      // Only Boolean variables
      Int::Linear::Term<Int::BoolView>* bts =
        r.alloc<Int::Linear::Term<Int::BoolView> >(n->n_bool);
      int c = n->fill(home,icl,NULL,bts);
      Int::Linear::post(home, bts, n->n_bool, irt, -c, b, icl);
    } else if (n->n_bool == 1) {
      // Integer variables and only one Boolean variable
      Int::Linear::Term<Int::IntView>* its =
        r.alloc<Int::Linear::Term<Int::IntView> >(n->n_int+1);
      Int::Linear::Term<Int::BoolView>* bts =
        r.alloc<Int::Linear::Term<Int::BoolView> >(1);
      int c = n->fill(home,icl,its,bts);
      IntVar x(home,0,1);
      channel(home,bts[0].x,x);
      its[n->n_int].x = x;
      its[n->n_int].a = bts[0].a;
      Int::Linear::post(home, its, n->n_int+1, irt, -c, b, icl);
    } else {
      // Both integer and Boolean variables
      Int::Linear::Term<Int::IntView>* its =
        r.alloc<Int::Linear::Term<Int::IntView> >(n->n_int+1);
      Int::Linear::Term<Int::BoolView>* bts =
        r.alloc<Int::Linear::Term<Int::BoolView> >(n->n_bool);
      int c = n->fill(home,icl,its,bts);
      int min, max;
      Int::Linear::estimate(&bts[0],n->n_bool,0,min,max);
      IntVar x(home,min,max);
      its[n->n_int].x = x; its[n->n_int].a = 1;
      Int::Linear::post(home, bts, n->n_bool, IRT_EQ, x, 0, icl);
      Int::Linear::post(home, its, n->n_int+1, irt, -c, b, icl);
    }
  }

  IntVar
  LinIntExpr::post(Home home, IntConLevel icl) const {
    if (home.failed()) return IntVar(home,0,0);
    Region r(home);
    if (n->n_bool == 0) {
      // Only integer variables
      Int::Linear::Term<Int::IntView>* its =
        r.alloc<Int::Linear::Term<Int::IntView> >(n->n_int+1);
      int c = n->fill(home,icl,its,NULL);
      if ((n->n_int == 1) && (c == 0) && (its[0].a == 1))
        return its[0].x;
      int min, max;
      Int::Linear::estimate(&its[0],n->n_int,c,min,max);
      IntVar x(home, min, max);
      its[n->n_int].x = x; its[n->n_int].a = -1;
      Int::Linear::post(home, its, n->n_int+1, IRT_EQ, -c, icl);
      return x;
    } else if (n->n_int == 0) {
      // Only Boolean variables
      Int::Linear::Term<Int::BoolView>* bts =
        r.alloc<Int::Linear::Term<Int::BoolView> >(n->n_bool);
      int c = n->fill(home,icl,NULL,bts);
      int min, max;
      Int::Linear::estimate(&bts[0],n->n_bool,c,min,max);
      IntVar x(home, min, max);
      Int::Linear::post(home, bts, n->n_bool, IRT_EQ, x, -c, icl);
      return x;
    } else if (n->n_bool == 1) {
      // Integer variables and single Boolean variable
      Int::Linear::Term<Int::IntView>* its =
        r.alloc<Int::Linear::Term<Int::IntView> >(n->n_int+2);
      Int::Linear::Term<Int::BoolView>* bts =
        r.alloc<Int::Linear::Term<Int::BoolView> >(1);
      int c = n->fill(home,icl,its,bts);
      IntVar x(home, 0, 1);
      channel(home, x, bts[0].x);
      its[n->n_int].x = x; its[n->n_int].a = bts[0].a;
      int y_min, y_max;
      Int::Linear::estimate(&its[0],n->n_int+1,c,y_min,y_max);
      IntVar y(home, y_min, y_max);
      its[n->n_int+1].x = y; its[n->n_int+1].a = -1;
      Int::Linear::post(home, its, n->n_int+2, IRT_EQ, -c, icl);
      return y;
    } else {
      // Both integer and Boolean variables
      Int::Linear::Term<Int::IntView>* its =
        r.alloc<Int::Linear::Term<Int::IntView> >(n->n_int+2);
      Int::Linear::Term<Int::BoolView>* bts =
        r.alloc<Int::Linear::Term<Int::BoolView> >(n->n_bool);
      int c = n->fill(home,icl,its,bts);
      int x_min, x_max;
      Int::Linear::estimate(&bts[0],n->n_bool,0,x_min,x_max);
      IntVar x(home, x_min, x_max);
      Int::Linear::post(home, bts, n->n_bool, IRT_EQ, x, 0, icl);
      its[n->n_int].x = x; its[n->n_int].a = 1;
      int y_min, y_max;
      Int::Linear::estimate(&its[0],n->n_int+1,c,y_min,y_max);
      IntVar y(home, y_min, y_max);
      its[n->n_int+1].x = y; its[n->n_int+1].a = -1;
      Int::Linear::post(home, its, n->n_int+2, IRT_EQ, -c, icl);
      return y;
    }
  }

  NonLinIntExpr*
  LinIntExpr::nle(void) const {
    return n->t == NT_NONLIN ? n->sum.ne : NULL;
  }

  LinIntExpr::LinIntExpr(void) :
    n(new Node) {
    n->n_int = n->n_bool = 0;
    n->t = NT_VAR_INT;
    n->l = n->r = NULL;
    n->a = 0;
  }

  LinIntExpr::LinIntExpr(int c) :
    n(new Node) {
    n->n_int = n->n_bool = 0;
    n->t = NT_CONST;
    n->l = n->r = NULL;
    n->a = 0;
    Int::Limits::check(c,"MiniModel::LinIntExpr");
    n->c = c;
  }

  LinIntExpr::LinIntExpr(const IntVar& x, int a) :
    n(new Node) {
    n->n_int = 1;
    n->n_bool = 0;
    n->t = NT_VAR_INT;
    n->l = n->r = NULL;
    n->a = a;
    n->x_int = x;
  }

  LinIntExpr::LinIntExpr(const BoolVar& x, int a) :
    n(new Node) {
    n->n_int = 0;
    n->n_bool = 1;
    n->t = NT_VAR_BOOL;
    n->l = n->r = NULL;
    n->a = a;
    n->x_bool = x;
  }

  LinIntExpr::LinIntExpr(const IntVarArgs& x) :
    n(new Node) {
    n->n_int = x.size();
    n->n_bool = 0;
    n->t = NT_SUM_INT;
    n->l = n->r = NULL;
    if (x.size() > 0) {
      n->sum.ti = heap.alloc<Int::Linear::Term<Int::IntView> >(x.size());
      for (int i=x.size(); i--; ) {
        n->sum.ti[i].x = x[i];
        n->sum.ti[i].a = 1;
      }
    }
  }

  LinIntExpr::LinIntExpr(const IntArgs& a, const IntVarArgs& x) :
    n(new Node) {
    if (a.size() != x.size())
      throw Int::ArgumentSizeMismatch("MiniModel::LinIntExpr");
    n->n_int = x.size();
    n->n_bool = 0;
    n->t = NT_SUM_INT;
    n->l = n->r = NULL;
    if (x.size() > 0) {
      n->sum.ti = heap.alloc<Int::Linear::Term<Int::IntView> >(x.size());
      for (int i=x.size(); i--; ) {
        n->sum.ti[i].x = x[i];
        n->sum.ti[i].a = a[i];
      }
    }
  }

  LinIntExpr::LinIntExpr(const BoolVarArgs& x) :
    n(new Node) {
    n->n_int = 0;
    n->n_bool = x.size();
    n->t = NT_SUM_BOOL;
    n->l = n->r = NULL;
    if (x.size() > 0) {
      n->sum.tb = heap.alloc<Int::Linear::Term<Int::BoolView> >(x.size());
      for (int i=x.size(); i--; ) {
        n->sum.tb[i].x = x[i];
        n->sum.tb[i].a = 1;
      }
    }
  }

  LinIntExpr::LinIntExpr(const IntArgs& a, const BoolVarArgs& x) :
    n(new Node) {
    if (a.size() != x.size())
      throw Int::ArgumentSizeMismatch("MiniModel::LinIntExpr");
    n->n_int = 0;
    n->n_bool = x.size();
    n->t = NT_SUM_BOOL;
    n->l = n->r = NULL;
    if (x.size() > 0) {
      n->sum.tb = heap.alloc<Int::Linear::Term<Int::BoolView> >(x.size());
      for (int i=x.size(); i--; ) {
        n->sum.tb[i].x = x[i];
        n->sum.tb[i].a = a[i];
      }
    }
  }

  LinIntExpr::LinIntExpr(const LinIntExpr& e0, NodeType t, const LinIntExpr& e1) :
    n(new Node) {
    n->n_int = e0.n->n_int + e1.n->n_int;
    n->n_bool = e0.n->n_bool + e1.n->n_bool;
    n->t = t;
    n->l = e0.n; n->l->use++;
    n->r = e1.n; n->r->use++;
  }

  LinIntExpr::LinIntExpr(const LinIntExpr& e, NodeType t, int c) :
    n(new Node) {
    n->n_int = e.n->n_int;
    n->n_bool = e.n->n_bool;
    n->t = t;
    n->l = NULL;
    n->r = e.n; n->r->use++;
    n->c = c;
  }

  LinIntExpr::LinIntExpr(int a, const LinIntExpr& e) :
    n(new Node) {
    n->n_int = e.n->n_int;
    n->n_bool = e.n->n_bool;
    n->t = NT_MUL;
    n->l = e.n; n->l->use++;
    n->r = NULL;
    n->a = a;
  }

  LinIntExpr::LinIntExpr(NonLinIntExpr* e) :
    n(new Node) {
    n->n_int = 1;
    n->n_bool = 0;
    n->t = NT_NONLIN;
    n->l = n->r = NULL;
    n->a = 0;
    n->sum.ne = e;
  }

  const LinIntExpr&
  LinIntExpr::operator =(const LinIntExpr& e) {
    if (this != &e) {
      if (n->decrement())
        delete n;
      n = e.n; n->use++;
    }
    return *this;
  }

  LinIntExpr::~LinIntExpr(void) {
    if (n->decrement())
      delete n;
  }


  void
  LinIntExpr::Node::fill(Home home, IntConLevel icl,
                      Int::Linear::Term<Int::IntView>*& ti, 
                      Int::Linear::Term<Int::BoolView>*& tb,
                      long long int m, long long int& d) const {
    switch (this->t) {
    case NT_CONST:
      Int::Limits::check(m*c,"MiniModel::LinIntExpr");
      d += m*c;
      break;
    case NT_VAR_INT:
      Int::Limits::check(m*a,"MiniModel::LinIntExpr");
      ti->a=static_cast<int>(m*a); ti->x=x_int; ti++;
      break;
    case NT_NONLIN:
      ti->a=static_cast<int>(m); ti->x=sum.ne->post(home, NULL, icl); ti++;
      break;
    case NT_VAR_BOOL:
      Int::Limits::check(m*a,"MiniModel::LinIntExpr");
      tb->a=static_cast<int>(m*a); tb->x=x_bool; tb++;
      break;
    case NT_SUM_INT:
      for (int i=n_int; i--; ) {
        Int::Limits::check(m*sum.ti[i].a,"MiniModel::LinIntExpr");
        ti[i].x = sum.ti[i].x; ti[i].a = static_cast<int>(m*sum.ti[i].a);
      }
      ti += n_int;
      break;
    case NT_SUM_BOOL:
      for (int i=n_bool; i--; ) {
        Int::Limits::check(m*sum.tb[i].a,"MiniModel::LinIntExpr");
        tb[i].x = sum.tb[i].x; tb[i].a = static_cast<int>(m*sum.tb[i].a);
      }
      tb += n_bool;
      break;
    case NT_ADD:
      if (l == NULL) {
        Int::Limits::check(m*c,"MiniModel::LinIntExpr");
        d += m*c;
      } else {
        l->fill(home,icl,ti,tb,m,d);
      }
      r->fill(home,icl,ti,tb,m,d);
      break;
    case NT_SUB:
      if (l == NULL) {
        Int::Limits::check(m*c,"MiniModel::LinIntExpr");
        d += m*c;
      } else {
        l->fill(home,icl,ti,tb,m,d);
      }
      r->fill(home,icl,ti,tb,-m,d);
      break;
    case NT_MUL:
      Int::Limits::check(m*a,"MiniModel::LinIntExpr");
      l->fill(home,icl,ti,tb,m*a,d);
      break;
    default:
      GECODE_NEVER;
    }
  }


  /*
   * Operators
   *
   */
  LinIntExpr
  operator +(int c, const IntVar& x) {
    if (x.assigned() && 
        Int::Limits::valid(static_cast<long long int>(c)+x.val()))
      return LinIntExpr(c+x.val());
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,c);
  }
  LinIntExpr
  operator +(int c, const BoolVar& x) {
    if (x.assigned() && 
        Int::Limits::valid(static_cast<long long int>(c)+x.val()))
      return LinIntExpr(c+x.val());
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,c);
  }
  LinIntExpr
  operator +(int c, const LinIntExpr& e) {
    return LinIntExpr(e,LinIntExpr::NT_ADD,c);
  }
  LinIntExpr
  operator +(const IntVar& x, int c) {
    if (x.assigned() && 
        Int::Limits::valid(static_cast<long long int>(c)+x.val()))
      return LinIntExpr(c+x.val());
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,c);
  }
  LinIntExpr
  operator +(const BoolVar& x, int c) {
    if (x.assigned() && 
        Int::Limits::valid(static_cast<long long int>(c)+x.val()))
      return LinIntExpr(c+x.val());
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,c);
  }
  LinIntExpr
  operator +(const LinIntExpr& e, int c) {
    return LinIntExpr(e,LinIntExpr::NT_ADD,c);
  }
  LinIntExpr
  operator +(const IntVar& x, const IntVar& y) {
    if (x.assigned())
      return x.val() + y;
    else if (y.assigned())
      return x + y.val();
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,y);
  }
  LinIntExpr
  operator +(const IntVar& x, const BoolVar& y) {
    if (x.assigned())
      return x.val() + y;
    else if (y.assigned())
      return x + y.val();
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,y);
  }
  LinIntExpr
  operator +(const BoolVar& x, const IntVar& y) {
    if (x.assigned())
      return x.val() + y;
    else if (y.assigned())
      return x + y.val();
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,y);
  }
  LinIntExpr
  operator +(const BoolVar& x, const BoolVar& y) {
    if (x.assigned())
      return x.val() + y;
    else if (y.assigned())
      return x + y.val();
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,y);
  }
  LinIntExpr
  operator +(const IntVar& x, const LinIntExpr& e) {
    if (x.assigned())
      return x.val() + e;
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,e);
  }
  LinIntExpr
  operator +(const BoolVar& x, const LinIntExpr& e) {
    if (x.assigned())
      return x.val() + e;
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,e);
  }
  LinIntExpr
  operator +(const LinIntExpr& e, const IntVar& x) {
    if (x.assigned())
      return e + x.val();
    else
      return LinIntExpr(e,LinIntExpr::NT_ADD,x);
  }
  LinIntExpr
  operator +(const LinIntExpr& e, const BoolVar& x) {
    if (x.assigned())
      return e + x.val();
    else
      return LinIntExpr(e,LinIntExpr::NT_ADD,x);
  }
  LinIntExpr
  operator +(const LinIntExpr& e1, const LinIntExpr& e2) {
    return LinIntExpr(e1,LinIntExpr::NT_ADD,e2);
  }

  LinIntExpr
  operator -(int c, const IntVar& x) {
    if (x.assigned() && 
        Int::Limits::valid(static_cast<long long int>(c)-x.val()))
      return LinIntExpr(c-x.val());
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,c);
  }
  LinIntExpr
  operator -(int c, const BoolVar& x) {
    if (x.assigned() && 
        Int::Limits::valid(static_cast<long long int>(c)-x.val()))
      return LinIntExpr(c-x.val());
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,c);
  }
  LinIntExpr
  operator -(int c, const LinIntExpr& e) {
    return LinIntExpr(e,LinIntExpr::NT_SUB,c);
  }
  LinIntExpr
  operator -(const IntVar& x, int c) {
    if (x.assigned() && 
        Int::Limits::valid(x.val()-static_cast<long long int>(c)))
      return LinIntExpr(x.val()-c);
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,-c);
  }
  LinIntExpr
  operator -(const BoolVar& x, int c) {
    if (x.assigned() && 
        Int::Limits::valid(x.val()-static_cast<long long int>(c)))
      return LinIntExpr(x.val()-c);
    else
      return LinIntExpr(x,LinIntExpr::NT_ADD,-c);
  }
  LinIntExpr
  operator -(const LinIntExpr& e, int c) {
    return LinIntExpr(e,LinIntExpr::NT_ADD,-c);
  }
  LinIntExpr
  operator -(const IntVar& x, const IntVar& y) {
    if (x.assigned())
      return x.val() - y;
    else if (y.assigned())
      return x - y.val();
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,y);
  }
  LinIntExpr
  operator -(const IntVar& x, const BoolVar& y) {
    if (x.assigned())
      return x.val() - y;
    else if (y.assigned())
      return x - y.val();
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,y);
  }
  LinIntExpr
  operator -(const BoolVar& x, const IntVar& y) {
    if (x.assigned())
      return x.val() - y;
    else if (y.assigned())
      return x - y.val();
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,y);
  }
  LinIntExpr
  operator -(const BoolVar& x, const BoolVar& y) {
    if (x.assigned())
      return x.val() - y;
    else if (y.assigned())
      return x - y.val();
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,y);
  }
  LinIntExpr
  operator -(const IntVar& x, const LinIntExpr& e) {
    if (x.assigned())
      return x.val() - e;
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,e);
  }
  LinIntExpr
  operator -(const BoolVar& x, const LinIntExpr& e) {
    if (x.assigned())
      return x.val() - e;
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,e);
  }
  LinIntExpr
  operator -(const LinIntExpr& e, const IntVar& x) {
    if (x.assigned())
      return e - x.val();
    else
      return LinIntExpr(e,LinIntExpr::NT_SUB,x);
  }
  LinIntExpr
  operator -(const LinIntExpr& e, const BoolVar& x) {
    if (x.assigned())
      return e - x.val();
    else
      return LinIntExpr(e,LinIntExpr::NT_SUB,x);
  }
  LinIntExpr
  operator -(const LinIntExpr& e1, const LinIntExpr& e2) {
    return LinIntExpr(e1,LinIntExpr::NT_SUB,e2);
  }

  LinIntExpr
  operator -(const IntVar& x) {
    if (x.assigned())
      return LinIntExpr(-x.val());
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,0);
  }
  LinIntExpr
  operator -(const BoolVar& x) {
    if (x.assigned())
      return LinIntExpr(-x.val());
    else
      return LinIntExpr(x,LinIntExpr::NT_SUB,0);
  }
  LinIntExpr
  operator -(const LinIntExpr& e) {
    return LinIntExpr(e,LinIntExpr::NT_SUB,0);
  }

  LinIntExpr
  operator *(int a, const IntVar& x) {
    if (a == 0)
      return LinIntExpr(0.0);
    else if (x.assigned() && 
             Int::Limits::valid(static_cast<long long int>(a)*x.val()))
      return LinIntExpr(a*x.val());
    else
      return LinIntExpr(x,a);
  }
  LinIntExpr
  operator *(int a, const BoolVar& x) {
    if (a == 0)
      return LinIntExpr(0.0);
    else if (x.assigned() && 
             Int::Limits::valid(static_cast<long long int>(a)*x.val()))
      return LinIntExpr(a*x.val());
    else
      return LinIntExpr(x,a);
  }
  LinIntExpr
  operator *(const IntVar& x, int a) {
    if (a == 0)
      return LinIntExpr(0.0);
    else if (x.assigned() && 
             Int::Limits::valid(static_cast<long long int>(a)*x.val()))
      return LinIntExpr(a*x.val());
    else
      return LinIntExpr(x,a);
  }
  LinIntExpr
  operator *(const BoolVar& x, int a) {
    if (a == 0)
      return LinIntExpr(0.0);
    else if (x.assigned() && 
             Int::Limits::valid(static_cast<long long int>(a)*x.val()))
      return LinIntExpr(a*x.val());
    else
      return LinIntExpr(x,a);
  }
  LinIntExpr
  operator *(const LinIntExpr& e, int a) {
    if (a == 0)
      return LinIntExpr(0.0);
    else
      return LinIntExpr(a,e);
  }
  LinIntExpr
  operator *(int a, const LinIntExpr& e) {
    if (a == 0)
      return LinIntExpr(0.0);
    else
      return LinIntExpr(a,e);
  }

  LinIntExpr
  sum(const IntVarArgs& x) {
    return LinIntExpr(x);
  }
  LinIntExpr
  sum(const IntArgs& a, const IntVarArgs& x) {
    return LinIntExpr(a,x);
  }
  LinIntExpr
  sum(const BoolVarArgs& x) {
    return LinIntExpr(x);
  }
  LinIntExpr
  sum(const IntArgs& a, const BoolVarArgs& x) {
    return LinIntExpr(a,x);
  }
  LinIntExpr 
  sum(const Slice<IntArgs>& slice) {
    const Slice<IntArgs>::ArgsType & args = slice;
    return sum(args);
  }
  LinIntExpr 
  sum(const Matrix<IntArgs>& matrix) {
    const Matrix<IntArgs>::ArgsType & args = matrix.get_array();
    return sum(args);
  }
  LinIntExpr
  sum(const IntArgs& args) {
    int i, sum = 0;
    const int size = args.size();

    for (i = 0 ; i < size ; ++i)    
    {
      sum += args[i];
    }

    return LinIntExpr(sum);
  }


  IntVar
  expr(Home home, const LinIntExpr& e, IntConLevel icl) {
    if (!home.failed())
      return e.post(home,icl);
    IntVar x(home,Int::Limits::min,Int::Limits::max);
    return x;
  }

}

// STATISTICS: minimodel-any
