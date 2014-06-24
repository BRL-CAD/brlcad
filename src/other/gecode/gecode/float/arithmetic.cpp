/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

#include <gecode/float/arithmetic.hh>
#ifdef GECODE_HAS_MPFR
#include <gecode/float/transcendental.hh>
#include <gecode/float/trigonometric.hh>
#endif

namespace Gecode {

  void
  abs(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::Abs<FloatView,FloatView>::post(home,x0,x1)));
  }


  void
  max(Home home, FloatVar x0, FloatVar x1, FloatVar x2) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::Max<FloatView,FloatView,FloatView>::post(home,x0,x1,x2)));
  }

  void
  max(Home home, const FloatVarArgs& x, FloatVar y) {
    using namespace Float;
    if (x.size() == 0)
      throw TooFewArguments("Float::max");
    if (home.failed()) return;
    ViewArray<FloatView> xv(home,x);
    GECODE_ES_FAIL(Arithmetic::NaryMax<FloatView>::post(home,xv,y));
  }


  void
  min(Home home, FloatVar x0, FloatVar x1, FloatVar x2) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::Min<FloatView,FloatView,FloatView>::post(home,x0,x1,x2)));
  }

  void
  min(Home home, const FloatVarArgs& x, FloatVar y) {
    using namespace Float;
    if (x.size() == 0)
      throw TooFewArguments("Float::min");
    if (home.failed()) return;
    ViewArray<MinusView> m(home,x.size());
    for (int i=x.size(); i--; )
      m[i] = MinusView(x[i]);
    MinusView my(y);
    GECODE_ES_FAIL(Arithmetic::NaryMax<MinusView>::post(home,m,my));
  }


  void
  mult(Home home, FloatVar x0, FloatVar x1, FloatVar x2) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::Mult<FloatView>::post(home,x0,x1,x2)));
  }

  void
  sqr(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::Sqr<FloatView>::post(home,x0,x1)));
  }

  void
  sqrt(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::Sqrt<FloatView,FloatView>::post(home,x0,x1)));
  }

  void
  pow(Home home, FloatVar x0, int n, FloatVar x1) {
    using namespace Float;
    if (n < 0)
      throw OutOfLimits("nroot");
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::Pow<FloatView,FloatView>::post(home,x0,x1,n)));
  }

  void
  nroot(Home home, FloatVar x0, int n, FloatVar x1) {
    using namespace Float;
    if (n < 0)
      throw OutOfLimits("nroot");
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::NthRoot<FloatView,FloatView>::post(home,x0,x1,n)));
  }

  void
  div(Home home, FloatVar x0, FloatVar x1, FloatVar x2) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL(
      (Arithmetic::Div<FloatView,FloatView,FloatView>::post(home,x0,x1,x2)));
  }

#ifdef GECODE_HAS_MPFR
  void
  exp(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Transcendental::Exp<FloatView,FloatView>::post(home,x0,x1)));
  }

  void
  log(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Transcendental::Exp<FloatView,FloatView>
      ::post(home,x1,x0)));
  }

  void
  log(Home home, FloatNum base, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Transcendental::Pow<FloatView,FloatView>
      ::post(home,base,x1,x0)));
  }

  void
  pow(Home home, FloatNum base, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Transcendental::Pow<FloatView,FloatView>
      ::post(home,base,x0,x1)));
  }

  void
  asin(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Trigonometric::ASin<FloatView,FloatView>::post(home,x0,x1)));
  }

  void
  sin(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Trigonometric::Sin<FloatView,FloatView>::post(home,x0,x1)));
  }

  void
  acos(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Trigonometric::ACos<FloatView,FloatView>::post(home,x0,x1)));
  }

  void
  cos(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Trigonometric::Cos<FloatView,FloatView>::post(home,x0,x1)));
  }

  void
  atan(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Trigonometric::ATan<FloatView,FloatView>::post(home,x0,x1)));
  }

  void
  tan(Home home, FloatVar x0, FloatVar x1) {
    using namespace Float;
    if (home.failed()) return;
    GECODE_ES_FAIL((Trigonometric::Tan<FloatView,FloatView>::post(home,x0,x1)));
  }
#endif

  void
  channel(Home home, FloatVar x0, IntVar x1) {
    using namespace Float;
    using namespace Int;
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::Channel<FloatView,IntView>::post(home,x0,x1)));
  }

  void
  channel(Home home, IntVar x0, FloatVar x1) {
    using namespace Float;
    using namespace Int;
    if (home.failed()) return;
    GECODE_ES_FAIL((Arithmetic::Channel<FloatView,IntView>::post(home,x1,x0)));
  }

}

// STATISTICS: float-post
