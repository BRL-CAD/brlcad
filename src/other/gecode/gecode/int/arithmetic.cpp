/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

#include <gecode/int/arithmetic.hh>

namespace Gecode {

  void
  abs(Home home, IntVar x0, IntVar x1, IntConLevel icl) {
    using namespace Int;
    if (home.failed()) return;
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::AbsDom<IntView>::post(home,x0,x1));
    } else {
      GECODE_ES_FAIL(Arithmetic::AbsBnd<IntView>::post(home,x0,x1));
    }
  }


  void
  max(Home home, IntVar x0, IntVar x1, IntVar x2,
      IntConLevel icl) {
    using namespace Int;
    if (home.failed()) return;
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::MaxDom<IntView>::post(home,x0,x1,x2));
    } else {
      GECODE_ES_FAIL(Arithmetic::MaxBnd<IntView>::post(home,x0,x1,x2));
    }
  }

  void
  max(Home home, const IntVarArgs& x, IntVar y,
      IntConLevel icl) {
    using namespace Int;
    if (x.size() == 0)
      throw TooFewArguments("Int::max");
    if (home.failed()) return;
    ViewArray<IntView> xv(home,x);
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::NaryMaxDom<IntView>::post(home,xv,y));
    } else {
      GECODE_ES_FAIL(Arithmetic::NaryMaxBnd<IntView>::post(home,xv,y));
    }
  }

  void
  min(Home home, IntVar x0, IntVar x1, IntVar x2,
      IntConLevel icl) {
    using namespace Int;
    if (home.failed()) return;
    MinusView m0(x0); MinusView m1(x1); MinusView m2(x2);
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::MaxDom<MinusView>::post(home,m0,m1,m2));
    } else {
      GECODE_ES_FAIL(Arithmetic::MaxBnd<MinusView>::post(home,m0,m1,m2));
    }
  }

  void
  min(Home home, const IntVarArgs& x, IntVar y,
      IntConLevel icl) {
    using namespace Int;
    if (x.size() == 0)
      throw TooFewArguments("Int::min");
    if (home.failed()) return;
    ViewArray<MinusView> m(home,x.size());
    for (int i=x.size(); i--; )
      m[i] = MinusView(x[i]);
    MinusView my(y);
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::NaryMaxDom<MinusView>::post(home,m,my));
    } else {
      GECODE_ES_FAIL(Arithmetic::NaryMaxBnd<MinusView>::post(home,m,my));
    }
  }


  void
  mult(Home home, IntVar x0, IntVar x1, IntVar x2,
       IntConLevel icl) {
    using namespace Int;
    if (home.failed()) return;
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::MultDom::post(home,x0,x1,x2));
    } else {
      GECODE_ES_FAIL(Arithmetic::MultBnd::post(home,x0,x1,x2));
    }
  }


  void
  divmod(Home home, IntVar x0, IntVar x1, IntVar x2, IntVar x3,
         IntConLevel) {
    using namespace Int;
    if (home.failed()) return;

    IntVar prod(home, Int::Limits::min, Int::Limits::max);
    GECODE_ES_FAIL(Arithmetic::MultBnd::post(home,x1,x2,prod));
    Linear::Term<IntView> t[3];
    t[0].a = 1; t[0].x = prod;
    t[1].a = 1; t[1].x = x3;
    int min, max;
    Linear::estimate(t,2,0,min,max);
    IntView x0v(x0);
    GECODE_ME_FAIL(x0v.gq(home,min));
    GECODE_ME_FAIL(x0v.lq(home,max));
    t[2].a=-1; t[2].x=x0;
    Linear::post(home,t,3,IRT_EQ,0);
    if (home.failed()) return;
    IntView x1v(x1);
    GECODE_ES_FAIL(
      Arithmetic::DivMod<IntView>::post(home,x0,x1,x3));
  }

  void
  div(Home home, IntVar x0, IntVar x1, IntVar x2,
      IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    GECODE_ES_FAIL(
      (Arithmetic::DivBnd<IntView>::post(home,x0,x1,x2)));
  }

  void
  mod(Home home, IntVar x0, IntVar x1, IntVar x2,
      IntConLevel icl) {
    using namespace Int;
    if (home.failed()) return;
    IntVar _div(home, Int::Limits::min, Int::Limits::max);
    divmod(home, x0, x1, _div, x2, icl);
  }

  void
  sqr(Home home, IntVar x0, IntVar x1, IntConLevel icl) {
    using namespace Int;
    if (home.failed()) return;
    Arithmetic::SqrOps ops;
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::PowDom<Arithmetic::SqrOps>
                     ::post(home,x0,x1,ops));
    } else {
      GECODE_ES_FAIL(Arithmetic::PowBnd<Arithmetic::SqrOps>
                     ::post(home,x0,x1,ops));
    }
  }

  void
  sqrt(Home home, IntVar x0, IntVar x1, IntConLevel icl) {
    using namespace Int;
    if (home.failed()) return;
    Arithmetic::SqrOps ops;
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::NrootDom<Arithmetic::SqrOps>
                     ::post(home,x0,x1,ops));
    } else {
      GECODE_ES_FAIL(Arithmetic::NrootBnd<Arithmetic::SqrOps>
                     ::post(home,x0,x1,ops));
    }
  }

  void
  pow(Home home, IntVar x0, int n, IntVar x1, IntConLevel icl) {
    using namespace Int;
    Limits::nonnegative(n,"Int::pow");
    if (home.failed()) return;
    if (n == 2) {
      sqr(home, x0, x1, icl);
      return;
    }
    Arithmetic::PowOps ops(n);
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::PowDom<Arithmetic::PowOps>
                     ::post(home,x0,x1,ops));
    } else {
      GECODE_ES_FAIL(Arithmetic::PowBnd<Arithmetic::PowOps>
                     ::post(home,x0,x1,ops));
    }
  }

  void
  nroot(Home home, IntVar x0, int n, IntVar x1, IntConLevel icl) {
    using namespace Int;
    Limits::positive(n,"Int::nroot");
    if (home.failed()) return;
    if (n == 2) {
      sqrt(home, x0, x1, icl);
      return;
    }
    Arithmetic::PowOps ops(n);
    if (icl == ICL_DOM) {
      GECODE_ES_FAIL(Arithmetic::NrootDom<Arithmetic::PowOps>
                     ::post(home,x0,x1,ops));
    } else {
      GECODE_ES_FAIL(Arithmetic::NrootBnd<Arithmetic::PowOps>
                     ::post(home,x0,x1,ops));
    }
  }

}

// STATISTICS: int-post
