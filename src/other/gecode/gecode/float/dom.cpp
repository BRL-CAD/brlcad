/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2013
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


#include <gecode/float/rel.hh>

namespace Gecode {

  void
  dom(Home home, FloatVar x, FloatVal n) {
    using namespace Float;
    Limits::check(n,"Float::dom");
    if (home.failed()) return;
    FloatView xv(x);
    GECODE_ME_FAIL(xv.eq(home,n));
  }

  void
  dom(Home home, const FloatVarArgs& x, FloatVal n) {
    using namespace Float;
    Limits::check(n,"Float::dom");
    if (home.failed()) return;
    for (int i=x.size(); i--; ) {
      FloatView xv(x[i]);
      GECODE_ME_FAIL(xv.eq(home,n));
    }
  }

  void
  dom(Home home, FloatVar x, FloatNum min, FloatNum max) {
    using namespace Float;
    Limits::check(min,"Float::dom");
    Limits::check(max,"Float::dom");
    if (home.failed()) return;
    FloatView xv(x);
    GECODE_ME_FAIL(xv.gq(home,min));
    GECODE_ME_FAIL(xv.lq(home,max));
  }

  void
  dom(Home home, const FloatVarArgs& x, FloatNum min, FloatNum max) {
    using namespace Float;
    Limits::check(min,"Float::dom");
    Limits::check(max,"Float::dom");
    if (home.failed()) return;
    for (int i=x.size(); i--; ) {
      FloatView xv(x[i]);
      GECODE_ME_FAIL(xv.gq(home,min));
      GECODE_ME_FAIL(xv.lq(home,max));
    }
  }

  void
  dom(Home home, FloatVar x, FloatVal n, Reify r) {
    using namespace Float;
    Limits::check(n,"Float::dom");
    if (home.failed()) return;
    switch (r.mode()) {
    case RM_EQV:
      GECODE_ES_FAIL((Rel::ReEqFloat<FloatView,Int::BoolView,RM_EQV>
                      ::post(home,x,n,r.var())));
      break;
    case RM_IMP:
      GECODE_ES_FAIL((Rel::ReEqFloat<FloatView,Int::BoolView,RM_IMP>
                      ::post(home,x,n,r.var())));
      break;
    case RM_PMI:
      GECODE_ES_FAIL((Rel::ReEqFloat<FloatView,Int::BoolView,RM_PMI>
                      ::post(home,x,n,r.var())));
      break;
    default: throw Int::UnknownReifyMode("Float::dom");
    }
  }

  void
  dom(Home home, FloatVar x, FloatNum min, FloatNum max, Reify r) {
    using namespace Float;
    if (min > max) {
      Int::BoolView b(r.var());
      switch (r.mode()) {
      case RM_EQV:
      case RM_IMP:
        GECODE_ME_FAIL(b.zero(home));
        break;
      case RM_PMI:
        break;
      default: throw Int::UnknownReifyMode("Float::dom");
      }
    } else {
      FloatVal n(min,max);
      dom(home,x,n,r);
    }
  }

  void
  dom(Home home, FloatVar x, FloatVar d) {
    using namespace Float;    
    if (home.failed()) return;
    FloatView xv(x), dv(d);
    if (!same(xv,dv)) {
      GECODE_ME_FAIL(xv.lq(home,dv.max()));
      GECODE_ME_FAIL(xv.gq(home,dv.min()));
    }
  }

  void
  dom(Home home, const FloatVarArgs& x, const FloatVarArgs& d) {
    using namespace Float;    
    if (x.size() != d.size())
      throw ArgumentSizeMismatch("Float::dom");
    for (int i=x.size(); i--; ) {
      if (home.failed()) return;
      FloatView xv(x[i]), dv(d[i]);
      if (!same(xv,dv)) {
        GECODE_ME_FAIL(xv.lq(home,dv.max()));
        GECODE_ME_FAIL(xv.gq(home,dv.min()));
      }
    }
  }

}

// STATISTICS: float-post

