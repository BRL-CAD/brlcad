/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
o *  Main authors:
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

#include <gecode/float/linear.hh>

namespace Gecode {

  void
  linear(Home home,
         const FloatVarArgs& x, FloatRelType frt, FloatNum c) {
    using namespace Float;
    if (home.failed()) return;
    Region re(home);
    Linear::Term* t = re.alloc<Linear::Term>(x.size());
    for (int i = x.size(); i--; ) {
      t[i].a=1.0; t[i].x=x[i];
    }
    Linear::post(home,t,x.size(),frt,c);
  }

  void
  linear(Home home,
         const FloatVarArgs& x, FloatRelType frt, FloatNum c, Reify r) {
    using namespace Float;
    if (home.failed()) return;
    Region re(home);
    Linear::Term* t = re.alloc<Linear::Term>(x.size());
    for (int i = x.size(); i--; ) {
      t[i].a=1.0; t[i].x=x[i];
    }
    Linear::post(home,t,x.size(),frt,c,r);
  }

  void
  linear(Home home,
         const FloatValArgs& a, const FloatVarArgs& x, FloatRelType frt, 
         FloatNum c) {
    using namespace Float;
    if (a.size() != x.size())
      throw ArgumentSizeMismatch("Float::linear");
    if (home.failed()) return;
    Region re(home);
    Linear::Term* t = re.alloc<Linear::Term>(x.size());
    for (int i = x.size(); i--; ) {
      t[i].a=a[i]; t[i].x=x[i];
    }
    Linear::post(home,t,x.size(),frt,c);
  }

  void
  linear(Home home,
         const FloatValArgs& a, const FloatVarArgs& x, FloatRelType frt, 
         FloatNum c, Reify r) {
    using namespace Float;
    if (a.size() != x.size())
      throw ArgumentSizeMismatch("Float::linear");
    if (home.failed()) return;
    Region re(home);
    Linear::Term* t = re.alloc<Linear::Term >(x.size());
    for (int i = x.size(); i--; ) {
      t[i].a=a[i]; t[i].x=x[i];
    }
    Linear::post(home,t,x.size(),frt,c,r);
  }

  void
  linear(Home home,
         const FloatVarArgs& x, FloatRelType frt, FloatVar y) {
    using namespace Float;
    if (home.failed()) return;
    Region re(home);
    Linear::Term* t = re.alloc<Linear::Term>(x.size()+1);
    for (int i = x.size(); i--; ) {
      t[i].a=1.0; t[i].x=x[i];
    }
    FloatNum min, max;
    estimate(t,x.size(),0.0,min,max);
    FloatView v(y);
    switch (frt) {
    case FRT_EQ:
      GECODE_ME_FAIL(v.gq(home,min)); GECODE_ME_FAIL(v.lq(home,max));
      break;
    case FRT_GQ: case FRT_GR:
      GECODE_ME_FAIL(v.lq(home,max));
      break;
    case FRT_LQ: case FRT_LE:
      GECODE_ME_FAIL(v.gq(home,min));
      break;
    default: ;
    }
    if (home.failed()) return;
    t[x.size()].a=-1.0; t[x.size()].x=y;
    Linear::post(home,t,x.size()+1,frt,0.0);
  }

  void
  linear(Home home,
         const FloatVarArgs& x, FloatRelType frt, FloatVar y, Reify r) {
    using namespace Float;
    if (home.failed()) return;
    Region re(home);
    Linear::Term* t = re.alloc<Linear::Term>(x.size()+1);
    for (int i = x.size(); i--; ) {
      t[i].a=1.0; t[i].x=x[i];
    }
    t[x.size()].a=-1; t[x.size()].x=y;
    Linear::post(home,t,x.size()+1,frt,0.0,r);
  }

  void
  linear(Home home,
         const FloatValArgs& a, const FloatVarArgs& x, FloatRelType frt, 
         FloatVar y) {
    using namespace Float;
    if (a.size() != x.size())
      throw ArgumentSizeMismatch("Float::linear");
    if (home.failed()) return;
    Region re(home);
    Linear::Term* t = re.alloc<Linear::Term>(x.size()+1);
    for (int i = x.size(); i--; ) {
      t[i].a=a[i]; t[i].x=x[i];
    }
    FloatNum min, max;
    estimate(t,x.size(),0.0,min,max);
    FloatView v(y);
    switch (frt) {
    case FRT_EQ:
      GECODE_ME_FAIL(v.gq(home,min)); GECODE_ME_FAIL(v.lq(home,max));
      break;
    case FRT_GQ: case FRT_GR:
      GECODE_ME_FAIL(v.lq(home,max));
      break;
    case FRT_LQ: case FRT_LE:
      GECODE_ME_FAIL(v.gq(home,min));
      break;
    default: ;
    }
    if (home.failed()) return;
    t[x.size()].a=-1.0; t[x.size()].x=y;
    Linear::post(home,t,x.size()+1,frt,0.0);
  }

  void
  linear(Home home,
         const FloatValArgs& a, const FloatVarArgs& x, FloatRelType frt, 
         FloatVar y, Reify r) {
    using namespace Float;
    if (a.size() != x.size())
      throw ArgumentSizeMismatch("Float::linear");
    if (home.failed()) return;
    Region re(home);
    Linear::Term* t = re.alloc<Linear::Term>(x.size()+1);
    for (int i = x.size(); i--; ) {
      t[i].a=a[i]; t[i].x=x[i];
    }
    t[x.size()].a=-1.0; t[x.size()].x=y;
    Linear::post(home,t,x.size()+1,frt,0.0,r);
  }

}

// STATISTICS: float-post
