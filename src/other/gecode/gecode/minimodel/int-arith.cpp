/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
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

namespace Gecode { namespace MiniModel {

  /// Non-linear arithmetic expressions over integer variables
  class GECODE_MINIMODEL_EXPORT ArithNonLinIntExpr : public NonLinIntExpr {
  public:
    /// The expression type
    enum ArithNonLinIntExprType {
      ANLE_ABS,   ///< Absolute value expression
      ANLE_MIN,   ///< Minimum expression
      ANLE_MAX,   ///< Maximum expression
      ANLE_MULT,  ///< Multiplication expression
      ANLE_DIV,   ///< Division expression
      ANLE_MOD,   ///< Modulo expression
      ANLE_SQR,   ///< Square expression
      ANLE_SQRT,  ///< Square root expression
      ANLE_POW,   ///< Pow expression
      ANLE_NROOT, ///< Nroot expression
      ANLE_ELMNT, ///< Element expression
      ANLE_ITE    ///< If-then-else expression
    } t;
    /// Expressions
    LinIntExpr* a;
    /// Size of variable array
    int n;
    /// Integer argument (used in nroot for example)
    int aInt;
    /// Boolean expression argument (used in ite for example)
    BoolExpr b;
    /// Constructor
    ArithNonLinIntExpr(ArithNonLinIntExprType t0, int n0)
      : t(t0), a(heap.alloc<LinIntExpr>(n0)), n(n0) {}
    /// Constructor
    ArithNonLinIntExpr(ArithNonLinIntExprType t0, int n0, int a0)
      : t(t0), a(heap.alloc<LinIntExpr>(n0)), n(n0), aInt(a0) {}
    /// Constructor
    ArithNonLinIntExpr(ArithNonLinIntExprType t0, int n0, const BoolExpr& b0)
      : t(t0), a(heap.alloc<LinIntExpr>(n0)), n(n0), b(b0) {}
    /// Destructor
    ~ArithNonLinIntExpr(void) { 
      heap.free<LinIntExpr>(a,n); 
    }
    /// Post expression
    virtual IntVar post(Home home, IntVar* ret, IntConLevel icl) const {
      IntVar y;
      switch (t) {
      case ANLE_ABS:
        {
          IntVar x = a[0].post(home, icl);
          if (x.min() >= 0)
            y = result(home,ret,x);
          else {
            y = result(home,ret);
            abs(home, x, y, icl);
          }
        }
        break;
      case ANLE_MIN:
        if (n==1) {
          y = result(home,ret, a[0].post(home, icl));
        } else if (n==2) {
          IntVar x0 = a[0].post(home, icl);
          IntVar x1 = a[1].post(home, icl);
          if (x0.max() <= x1.min())
            y = result(home,ret,x0);
          else if (x1.max() <= x0.min())
            y = result(home,ret,x1);
          else {
            y = result(home,ret);
            min(home, x0, x1, y, icl);
          }
        } else {
          IntVarArgs x(n);
          for (int i=n; i--;)
            x[i] = a[i].post(home, icl);
          y = result(home,ret);
          min(home, x, y, icl);
        }
        break;
      case ANLE_MAX:
        if (n==1) {
          y = result(home,ret,a[0].post(home, icl));
        } else if (n==2) {
          IntVar x0 = a[0].post(home, icl);
          IntVar x1 = a[1].post(home, icl);
          if (x0.max() <= x1.min())
            y = result(home,ret,x1);
          else if (x1.max() <= x0.min())
            y = result(home,ret,x0);
          else {
            y = result(home,ret);
            max(home, x0, x1, y, icl);
          }
        } else {
          IntVarArgs x(n);
          for (int i=n; i--;)
            x[i] = a[i].post(home, icl);
          y = result(home,ret);
          max(home, x, y, icl);
        }
        break;
      case ANLE_MULT:
        {
          assert(n == 2);
          IntVar x0 = a[0].post(home, icl);
          IntVar x1 = a[1].post(home, icl);
          if (x0.assigned() && (x0.val() == 0))
            y = result(home,ret,x0);
          else if (x0.assigned() && (x0.val() == 1))
            y = result(home,ret,x1);
          else if (x1.assigned() && (x1.val() == 0))
            y = result(home,ret,x1);
          else if (x1.assigned() && (x1.val() == 1))
            y = result(home,ret,x0);
          else {
            y = result(home,ret);
            mult(home, x0, x1, y, icl);
          }
        }
        break;
      case ANLE_DIV:
        {
          assert(n == 2);
          IntVar x0 = a[0].post(home, icl);
          IntVar x1 = a[1].post(home, icl);
          rel(home, x1, IRT_NQ, 0);
          if (x1.assigned() && (x1.val() == 1))
            y = result(home,ret,x0);
          else if (x0.assigned() && (x0.val() == 0))
            y = result(home,ret,x0);
          else {
            y = result(home,ret);
            div(home, x0, x1, y, icl);
          }
        }
        break;
      case ANLE_MOD:
        {
          assert(n == 2);
          IntVar x0 = a[0].post(home, icl);
          IntVar x1 = a[1].post(home, icl);
          y = result(home,ret);
          mod(home, x0, x1, y, icl);
        }
        break;
      case ANLE_SQR:
        {
          assert(n == 1);
          IntVar x = a[0].post(home, icl);
          if (x.assigned() && ((x.val() == 0) || (x.val() == 1)))
            y = x;
          else {
            y = result(home,ret);
            sqr(home, x, y, icl);
          }
        }
        break;
      case ANLE_SQRT:
        {
          assert(n == 1);
          IntVar x = a[0].post(home, icl);
          if (x.assigned() && ((x.val() == 0) || (x.val() == 1)))
            y = result(home,ret,x);
          else {
            y = result(home,ret);
            sqrt(home, x, y, icl);
          }
        }
        break;
      case ANLE_POW:
        {
          assert(n == 1);
          IntVar x = a[0].post(home, icl);
          if (x.assigned() && (aInt > 0) && 
              ((x.val() == 0) || (x.val() == 1)))
            y = x;
          else {
            y = result(home,ret);
            pow(home, x, aInt, y, icl);
          }
        }
        break;
      case ANLE_NROOT:
        {
          assert(n == 1);
          IntVar x = a[0].post(home, icl);
          if (x.assigned() && (aInt > 0) && 
              ((x.val() == 0) || (x.val() == 1)))
            y = result(home,ret,x);
          else {
            y = result(home,ret);
            nroot(home, x, aInt, y, icl);
          }
        }
        break;
      case ANLE_ELMNT:
        {
          IntVar z = a[n-1].post(home, icl);
          if (z.assigned() && z.val() >= 0 && z.val() < n-1) {
            y = result(home,ret,a[z.val()].post(home, icl));
          } else {
            IntVarArgs x(n-1);
            bool assigned = true;
            for (int i=n-1; i--;) {
              x[i] = a[i].post(home, icl);
              if (!x[i].assigned())
                assigned = false;
            }
            y = result(home,ret);
            if (assigned) {
              IntArgs xa(n-1);
              for (int i=n-1; i--;)
                xa[i] = x[i].val();
              element(home, xa, z, y, icl);
            } else {
              element(home, x, z, y, icl);
            }
          }
        }
        break;
      case ANLE_ITE:
        {
          assert(n == 2);
          BoolVar c = b.expr(home, icl); 
          IntVar x0 = a[0].post(home, icl);
          IntVar x1 = a[1].post(home, icl);
          y = result(home,ret);
          ite(home, c, x0, x1, y, icl);
        }
        break;
      default:
        GECODE_NEVER;
      }
      return y;
    }
    virtual void post(Home home, IntRelType irt, int c,
                      IntConLevel icl) const {
      if ( (t == ANLE_MIN && (irt == IRT_GQ || irt == IRT_GR)) ||
           (t == ANLE_MAX && (irt == IRT_LQ || irt == IRT_LE)) ) {
        IntVarArgs x(n);
        for (int i=n; i--;)
          x[i] = a[i].post(home, icl);
        rel(home, x, irt, c);
      } else {
        rel(home, post(home,NULL,icl), irt, c);
      }
    }
    virtual void post(Home home, IntRelType irt, int c,
                      BoolVar b, IntConLevel icl) const {
      rel(home, post(home,NULL,icl), irt, c, b);
    }
  };
  /// Check if \a e is of type \a t
  bool hasType(const LinIntExpr& e, ArithNonLinIntExpr::ArithNonLinIntExprType t) {
    return e.nle() &&
      dynamic_cast<ArithNonLinIntExpr*>(e.nle()) != NULL &&
      dynamic_cast<ArithNonLinIntExpr*>(e.nle())->t == t;
  }

}}

namespace Gecode {

  LinIntExpr
  abs(const LinIntExpr& e) {
    using namespace MiniModel;
    if (hasType(e, ArithNonLinIntExpr::ANLE_ABS))
      return e;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_ABS,1);
    ae->a[0] = e;
    return LinIntExpr(ae);
  }

  LinIntExpr
  min(const LinIntExpr& e0, const LinIntExpr& e1) {
    using namespace MiniModel;
    int n = 0;
    if (hasType(e0, ArithNonLinIntExpr::ANLE_MIN))
      n += static_cast<ArithNonLinIntExpr*>(e0.nle())->n;
    else
      n += 1;
    if (hasType(e1, ArithNonLinIntExpr::ANLE_MIN))
      n += static_cast<ArithNonLinIntExpr*>(e1.nle())->n;
    else
      n += 1;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_MIN,n);
    int i=0;
    if (hasType(e0, ArithNonLinIntExpr::ANLE_MIN)) {
      ArithNonLinIntExpr* e0e = static_cast<ArithNonLinIntExpr*>(e0.nle());
      for (; i<e0e->n; i++)
        ae->a[i] = e0e->a[i];
    } else {
      ae->a[i++] = e0;
    }
    if (hasType(e1, ArithNonLinIntExpr::ANLE_MIN)) {
      ArithNonLinIntExpr* e1e = static_cast<ArithNonLinIntExpr*>(e1.nle());
      int curN = i;
      for (; i<curN+e1e->n; i++)
        ae->a[i] = e1e->a[i-curN];
    } else {
      ae->a[i++] = e1;
    }
    return LinIntExpr(ae);
  }

  LinIntExpr
  max(const LinIntExpr& e0, const LinIntExpr& e1) {
    using namespace MiniModel;
    int n = 0;
    if (hasType(e0, ArithNonLinIntExpr::ANLE_MAX))
      n += static_cast<ArithNonLinIntExpr*>(e0.nle())->n;
    else
      n += 1;
    if (hasType(e1, ArithNonLinIntExpr::ANLE_MAX))
      n += static_cast<ArithNonLinIntExpr*>(e1.nle())->n;
    else
      n += 1;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_MAX,n);
    int i=0;
    if (hasType(e0, ArithNonLinIntExpr::ANLE_MAX)) {
      ArithNonLinIntExpr* e0e = static_cast<ArithNonLinIntExpr*>(e0.nle());
      for (; i<e0e->n; i++)
        ae->a[i] = e0e->a[i];
    } else {
      ae->a[i++] = e0;
    }
    if (hasType(e1, ArithNonLinIntExpr::ANLE_MAX)) {
      ArithNonLinIntExpr* e1e = static_cast<ArithNonLinIntExpr*>(e1.nle());
      int curN = i;
      for (; i<curN+e1e->n; i++)
        ae->a[i] = e1e->a[i-curN];
    } else {
      ae->a[i++] = e1;
    }
    return LinIntExpr(ae);
  }

  LinIntExpr
  min(const IntVarArgs& x) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_MIN,x.size());
    for (int i=x.size(); i--;)
      ae->a[i] = x[i];
    return LinIntExpr(ae);
  }

  LinIntExpr
  max(const IntVarArgs& x) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_MAX,x.size());
    for (int i=x.size(); i--;)
      ae->a[i] = x[i];
    return LinIntExpr(ae);
  }

  LinIntExpr
  operator *(const LinIntExpr& e0, const LinIntExpr& e1) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_MULT,2);
    ae->a[0] = e0;
    ae->a[1] = e1;
    return LinIntExpr(ae);
  }

  LinIntExpr
  sqr(const LinIntExpr& e) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_SQR,1);
    ae->a[0] = e;
    return LinIntExpr(ae);
  }

  LinIntExpr
  sqrt(const LinIntExpr& e) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_SQRT,1);
    ae->a[0] = e;
    return LinIntExpr(ae);
  }

  LinIntExpr
  pow(const LinIntExpr& e, int n) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_POW,1,n);
    ae->a[0] = e;
    return LinIntExpr(ae);
  }

  LinIntExpr
  nroot(const LinIntExpr& e, int n) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_NROOT,1,n);
    ae->a[0] = e;
    return LinIntExpr(ae);
  }

  LinIntExpr
  operator /(const LinIntExpr& e0, const LinIntExpr& e1) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_DIV,2);
    ae->a[0] = e0;
    ae->a[1] = e1;
    return LinIntExpr(ae);
  }

  LinIntExpr
  operator %(const LinIntExpr& e0, const LinIntExpr& e1) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_MOD,2);
    ae->a[0] = e0;
    ae->a[1] = e1;
    return LinIntExpr(ae);
  }

  LinIntExpr
  element(const IntVarArgs& x, const LinIntExpr& e) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_ELMNT,x.size()+1);
    for (int i=x.size(); i--;)
      ae->a[i] = x[i];
    ae->a[x.size()] = e;
    return LinIntExpr(ae);
  }

  LinIntExpr
  element(const IntArgs& x, const LinIntExpr& e) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_ELMNT,x.size()+1);
    for (int i=x.size(); i--;)
      ae->a[i] = x[i];
    ae->a[x.size()] = e;
    return LinIntExpr(ae);
  }

  LinIntExpr
  ite(const BoolExpr& b, const LinIntExpr& e0, const LinIntExpr& e1) {
    using namespace MiniModel;
    ArithNonLinIntExpr* ae =
      new ArithNonLinIntExpr(ArithNonLinIntExpr::ANLE_ITE,2,b);
    ae->a[0] = e0;
    ae->a[1] = e1;
    return LinIntExpr(ae);
  }

}

// STATISTICS: minimodel-any
