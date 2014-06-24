/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2006
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

namespace Gecode { namespace MiniModel {

  /// Non-linear float arithmetic expressions
  class GECODE_MINIMODEL_EXPORT ArithNonLinFloatExpr : 
    public NonLinFloatExpr {
  public:
    /// The expression type
    enum ArithNonLinFloatExprType {
      ANLFE_ABS,   ///< Absolute value expression
      ANLFE_MIN,   ///< Minimum expression
      ANLFE_MAX,   ///< Maximum expression
      ANLFE_MULT,  ///< Multiplication expression
      ANLFE_DIV,   ///< Division expression
      ANLFE_SQR,   ///< Square expression
      ANLFE_SQRT,  ///< Square root expression
#ifdef GECODE_HAS_MPFR
      ANLFE_EXP,   ///< Exponential expression
      ANLFE_LOG,   ///< Logarithm root expression
      ANLFE_ASIN,  ///< expression
      ANLFE_SIN,   ///< expression
      ANLFE_ACOS,  ///< expression
      ANLFE_COS,   ///< expression
      ANLFE_ATAN,  ///< expression
      ANLFE_TAN,   ///< expression
#endif          
      ANLFE_POW,   ///< Pow expression
      ANLFE_NROOT  ///< Nth root expression
    } t;
    /// Expressions
    LinFloatExpr* a;
    /// Size of variable array
    int n;
    /// Integer argument (used in nroot for example)
    int aInt;
    /// Constructors
    ArithNonLinFloatExpr(ArithNonLinFloatExprType t0, int n0)
      : t(t0), a(heap.alloc<LinFloatExpr>(n0)), n(n0), aInt(-1) {}
    ArithNonLinFloatExpr(ArithNonLinFloatExprType t0, int n0, int a0)
      : t(t0), a(heap.alloc<LinFloatExpr>(n0)), n(n0), aInt(a0) {}
    /// Destructor
    ~ArithNonLinFloatExpr(void) { heap.free<LinFloatExpr>(a,n); }
    /// Post expression
    virtual FloatVar post(Home home, FloatVar* ret) const {
      FloatVar y;
      switch (t) {
      case ANLFE_ABS:
        {
          FloatVar x = a[0].post(home);
          if (x.min() >= 0)
            y = result(home,ret,x);
          else {
            y = result(home,ret);
            abs(home, x, y);
          }
        }
        break;
      case ANLFE_MIN:
        if (n==1) {
          y = result(home,ret, a[0].post(home));
        } else if (n==2) {
          FloatVar x0 = a[0].post(home);
          FloatVar x1 = a[1].post(home);
          if (x0.max() <= x1.min())
            y = result(home,ret,x0);
          else if (x1.max() <= x0.min())
            y = result(home,ret,x1);
          else {
            y = result(home,ret);
            min(home, x0, x1, y);
          }
        } else {
          FloatVarArgs x(n);
          for (int i=n; i--;)
            x[i] = a[i].post(home);
          y = result(home,ret);
          min(home, x, y);
        }
        break;
      case ANLFE_MAX:
        if (n==1) {
          y = result(home,ret,a[0].post(home));
        } else if (n==2) {
          FloatVar x0 = a[0].post(home);
          FloatVar x1 = a[1].post(home);
          if (x0.max() <= x1.min())
            y = result(home,ret,x1);
          else if (x1.max() <= x0.min())
            y = result(home,ret,x0);
          else {
            y = result(home,ret);
            max(home, x0, x1, y);
          }
        } else {
          FloatVarArgs x(n);
          for (int i=n; i--;)
            x[i] = a[i].post(home);
          y = result(home,ret);
          max(home, x, y);
        }
        break;
      case ANLFE_MULT:
        {
          assert(n == 2);
          FloatVar x0 = a[0].post(home);
          FloatVar x1 = a[1].post(home);
          if (x0.assigned() && (x0.val() == 0.0))
            y = result(home,ret,x0);
          else if (x0.assigned() && (x0.val() == 1.0))
            y = result(home,ret,x1);
          else if (x1.assigned() && (x1.val() == 0.0))
            y = result(home,ret,x1);
          else if (x1.assigned() && (x1.val() == 1.0))
            y = result(home,ret,x0);
          else {
            y = result(home,ret);
            mult(home, x0, x1, y);
          }
        }
        break;
      case ANLFE_DIV:
        {
          assert(n == 2);
          FloatVar x0 = a[0].post(home);
          FloatVar x1 = a[1].post(home);
          if (x1.assigned() && (x1.val() == 1.0))
            y = result(home,ret,x0);
          else if (x0.assigned() && (x0.val() == 0.0))
            y = result(home,ret,x0);
          else {
            y = result(home,ret);
            div(home, x0, x1, y);
          }
        }
        break;
      case ANLFE_SQR:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          if (x.assigned() && ((x.val() == 0.0) || (x.val() == 1.0)))
            y = x;
          else {
            y = result(home,ret);
            sqr(home, x, y);
          }
        }
        break;
      case ANLFE_SQRT:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          if (x.assigned() && ((x.val() == 0.0) || (x.val() == 1.0)))
            y = result(home,ret,x);
          else {
            y = result(home,ret);
            sqrt(home, x, y);
          }
        }
        break;
      case ANLFE_POW:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          if (x.assigned() && ((x.val() == 0.0) || (x.val() == 1.0)))
            y = result(home,ret,x);
          else {
            y = result(home,ret);
            pow(home, x, aInt, y);
          }
        }
        break;
      case ANLFE_NROOT:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          if (x.assigned() && ((x.val() == 0.0) || (x.val() == 1.0)))
            y = result(home,ret,x);
          else {
            y = result(home,ret);
            nroot(home, x, aInt, y);
          }
        }
        break;
#ifdef GECODE_HAS_MPFR
      case ANLFE_EXP:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          if (x.assigned() && (x.val() == 0.0))
            y = result(home,ret,x);
          else {
            y = result(home,ret);
            exp(home, x, y);
          }
        }
        break;
      case ANLFE_LOG:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          y = result(home,ret);
          log(home, x, y);
        }
        break;
      case ANLFE_ASIN:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          y = result(home,ret);
          asin(home, x, y);
        }
        break;
      case ANLFE_SIN:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          y = result(home,ret);
          sin(home, x, y);
        }
        break;
      case ANLFE_ACOS:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          y = result(home,ret);
          acos(home, x, y);
        }
        break;
      case ANLFE_COS:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          y = result(home,ret);
          cos(home, x, y);
        }
        break;
      case ANLFE_ATAN:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          y = result(home,ret);
          atan(home, x, y);
        }
        break;
      case ANLFE_TAN:
        {
          assert(n == 1);
          FloatVar x = a[0].post(home);
          y = result(home,ret);
          tan(home, x, y);
          }
        break;
#endif
      default:
        GECODE_NEVER;
      }
      return y;
    }
    virtual void post(Home home, FloatRelType frt, FloatVal c) const {
      if ((t == ANLFE_MIN && frt == FRT_GQ) ||
          (t == ANLFE_MAX && frt == FRT_LQ)) {
        FloatVarArgs x(n);
        for (int i=n; i--;)
          x[i] = a[i].post(home);
        rel(home, x, frt, c);
      } else {
        rel(home, post(home,NULL), frt, c);
      }
    }
    virtual void post(Home home, FloatRelType frt, FloatVal c,
                      BoolVar b) const {
      rel(home, post(home,NULL), frt, c, b);
    }
  };
  /// Check if \a e is of type \a t
  bool hasType(const LinFloatExpr& e, ArithNonLinFloatExpr::ArithNonLinFloatExprType t) {
    return e.nlfe() &&
      dynamic_cast<ArithNonLinFloatExpr*>(e.nlfe()) != NULL &&
      dynamic_cast<ArithNonLinFloatExpr*>(e.nlfe())->t == t;
  }
  
}}

namespace Gecode {

  LinFloatExpr
  abs(const LinFloatExpr& e) {
    using namespace MiniModel;
    if (hasType(e, ArithNonLinFloatExpr::ANLFE_ABS))
      return e;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_ABS,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  min(const LinFloatExpr& e0, const LinFloatExpr& e1) {
    using namespace MiniModel;
    int n = 0;
    if (hasType(e0, ArithNonLinFloatExpr::ANLFE_MIN))
      n += static_cast<ArithNonLinFloatExpr*>(e0.nlfe())->n;
    else
      n += 1;
    if (hasType(e1, ArithNonLinFloatExpr::ANLFE_MIN))
      n += static_cast<ArithNonLinFloatExpr*>(e1.nlfe())->n;
    else
      n += 1;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_MIN,n);
    int i=0;
    if (hasType(e0, ArithNonLinFloatExpr::ANLFE_MIN)) {
      ArithNonLinFloatExpr* e0e = static_cast<ArithNonLinFloatExpr*>(e0.nlfe());
      for (; i<e0e->n; i++)
        ae->a[i] = e0e->a[i];
    } else {
      ae->a[i++] = e0;
    }
    if (hasType(e1, ArithNonLinFloatExpr::ANLFE_MIN)) {
      ArithNonLinFloatExpr* e1e = static_cast<ArithNonLinFloatExpr*>(e1.nlfe());
      int curN = i;
      for (; i<curN+e1e->n; i++)
        ae->a[i] = e1e->a[i-curN];
    } else {
      ae->a[i++] = e1;
    }
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  min(const FloatVarArgs& x) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_MIN,x.size());
    for (int i=x.size(); i--;)
      ae->a[i] = x[i];
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  max(const LinFloatExpr& e0, const LinFloatExpr& e1) {
    using namespace MiniModel;
    int n = 0;
    if (hasType(e0, ArithNonLinFloatExpr::ANLFE_MAX))
      n += static_cast<ArithNonLinFloatExpr*>(e0.nlfe())->n;
    else
      n += 1;
    if (hasType(e1, ArithNonLinFloatExpr::ANLFE_MAX))
      n += static_cast<ArithNonLinFloatExpr*>(e1.nlfe())->n;
    else
      n += 1;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_MAX,n);
    int i=0;
    if (hasType(e0, ArithNonLinFloatExpr::ANLFE_MAX)) {
      ArithNonLinFloatExpr* e0e = static_cast<ArithNonLinFloatExpr*>(e0.nlfe());
      for (; i<e0e->n; i++)
        ae->a[i] = e0e->a[i];
    } else {
      ae->a[i++] = e0;
    }
    if (hasType(e1, ArithNonLinFloatExpr::ANLFE_MAX)) {
      ArithNonLinFloatExpr* e1e = static_cast<ArithNonLinFloatExpr*>(e1.nlfe());
      int curN = i;
      for (; i<curN+e1e->n; i++)
        ae->a[i] = e1e->a[i-curN];
    } else {
      ae->a[i++] = e1;
    }
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  max(const FloatVarArgs& x) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_MAX,x.size());
    for (int i=x.size(); i--;)
      ae->a[i] = x[i];
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  operator *(const FloatVar& e0, const FloatVar& e1) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_MULT,2);
    ae->a[0] = e0;
    ae->a[1] = e1;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  operator *(const LinFloatExpr& e0, const FloatVar& e1) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_MULT,2);
    ae->a[0] = e0;
    ae->a[1] = e1;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  operator *(const FloatVar& e0, const LinFloatExpr& e1) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_MULT,2);
    ae->a[0] = e0;
    ae->a[1] = e1;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  operator *(const LinFloatExpr& e0, const LinFloatExpr& e1) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_MULT,2);
    ae->a[0] = e0;
    ae->a[1] = e1;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  operator /(const LinFloatExpr& e0, const LinFloatExpr& e1) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_DIV,2);
    ae->a[0] = e0;
    ae->a[1] = e1;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  sqr(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_SQR,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  sqrt(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_SQRT,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  pow(const LinFloatExpr& e, int exp) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_POW,1,exp);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  nroot(const LinFloatExpr& e, int exp) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_NROOT,1,exp);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

#ifdef GECODE_HAS_MPFR

  LinFloatExpr
  exp(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_EXP,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  log(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_LOG,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  asin(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_ASIN,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  sin(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_SIN,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  acos(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_ACOS,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  cos(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_COS,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  atan(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_ATAN,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

  LinFloatExpr
  tan(const LinFloatExpr& e) {
    using namespace MiniModel;
    ArithNonLinFloatExpr* ae =
      new ArithNonLinFloatExpr(ArithNonLinFloatExpr::ANLFE_TAN,1);
    ae->a[0] = e;
    return LinFloatExpr(ae);
  }

#endif

}

#endif

// STATISTICS: minimodel-any
