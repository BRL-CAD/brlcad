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

#include <gecode/int/linear.hh>

namespace Gecode {

  using namespace Int;

  void
  linear(Home home,
         const IntVarArgs& x, IntRelType irt, int c,
         IntConLevel icl) {
    if (home.failed()) return;
    Region re(home);
    Linear::Term<IntView>* t = re.alloc<Linear::Term<IntView> >(x.size());
    for (int i = x.size(); i--; ) {
      t[i].a=1; t[i].x=x[i];
    }
    Linear::post(home,t,x.size(),irt,c,icl);
  }

  void
  linear(Home home,
         const IntVarArgs& x, IntRelType irt, int c, Reify r,
         IntConLevel) {
    if (home.failed()) return;
    Region re(home);
    Linear::Term<IntView>* t = re.alloc<Linear::Term<IntView> >(x.size());
    for (int i = x.size(); i--; ) {
      t[i].a=1; t[i].x=x[i];
    }
    Linear::post(home,t,x.size(),irt,c,r);
  }

  void
  linear(Home home,
         const IntArgs& a, const IntVarArgs& x, IntRelType irt, int c,
         IntConLevel icl) {
    if (a.size() != x.size())
      throw ArgumentSizeMismatch("Int::linear");
    if (home.failed()) return;
    Region re(home);
    Linear::Term<IntView>* t = re.alloc<Linear::Term<IntView> >(x.size());
    for (int i = x.size(); i--; ) {
      t[i].a=a[i]; t[i].x=x[i];
    }
    Linear::post(home,t,x.size(),irt,c,icl);
  }

  void
  linear(Home home,
         const IntArgs& a, const IntVarArgs& x, IntRelType irt, int c, 
         Reify r, IntConLevel) {
    if (a.size() != x.size())
      throw ArgumentSizeMismatch("Int::linear");
    if (home.failed()) return;
    Region re(home);
    Linear::Term<IntView>* t = re.alloc<Linear::Term<IntView> >(x.size());
    for (int i = x.size(); i--; ) {
      t[i].a=a[i]; t[i].x=x[i];
    }
    Linear::post(home,t,x.size(),irt,c,r);
  }

  void
  linear(Home home,
         const IntVarArgs& x, IntRelType irt, IntVar y,
         IntConLevel icl) {
    if (home.failed()) return;
    Region re(home);
    Linear::Term<IntView>* t = re.alloc<Linear::Term<IntView> >(x.size()+1);
    for (int i = x.size(); i--; ) {
      t[i].a=1; t[i].x=x[i];
    }
    int min, max;
    estimate(t,x.size(),0,min,max);
    IntView v(y);
    switch (irt) {
    case IRT_EQ:
      GECODE_ME_FAIL(v.gq(home,min)); GECODE_ME_FAIL(v.lq(home,max));
      break;
    case IRT_GQ:
      GECODE_ME_FAIL(v.lq(home,max));
      break;
    case IRT_LQ:
      GECODE_ME_FAIL(v.gq(home,min));
      break;
    default: ;
    }
    if (home.failed()) return;
    t[x.size()].a=-1; t[x.size()].x=y;
    Linear::post(home,t,x.size()+1,irt,0,icl);
  }

  void
  linear(Home home,
         const IntVarArgs& x, IntRelType irt, IntVar y, Reify r,
         IntConLevel) {
    if (home.failed()) return;
    Region re(home);
    Linear::Term<IntView>* t = re.alloc<Linear::Term<IntView> >(x.size()+1);
    for (int i = x.size(); i--; ) {
      t[i].a=1; t[i].x=x[i];
    }
    t[x.size()].a=-1; t[x.size()].x=y;
    Linear::post(home,t,x.size()+1,irt,0,r);
  }

  void
  linear(Home home,
         const IntArgs& a, const IntVarArgs& x, IntRelType irt, IntVar y,
         IntConLevel icl) {
    if (a.size() != x.size())
      throw ArgumentSizeMismatch("Int::linear");
    if (home.failed()) return;
    Region re(home);
    Linear::Term<IntView>* t = re.alloc<Linear::Term<IntView> >(x.size()+1);
    for (int i = x.size(); i--; ) {
      t[i].a=a[i]; t[i].x=x[i];
    }
    int min, max;
    estimate(t,x.size(),0,min,max);
    IntView v(y);
    switch (irt) {
    case IRT_EQ:
      GECODE_ME_FAIL(v.gq(home,min)); GECODE_ME_FAIL(v.lq(home,max));
      break;
    case IRT_GQ:
      GECODE_ME_FAIL(v.lq(home,max));
      break;
    case IRT_LQ:
      GECODE_ME_FAIL(v.gq(home,min));
      break;
    default: ;
    }
    if (home.failed()) return;
    t[x.size()].a=-1; t[x.size()].x=y;
    Linear::post(home,t,x.size()+1,irt,0,icl);
  }

  void
  linear(Home home,
         const IntArgs& a, const IntVarArgs& x, IntRelType irt, IntVar y,
         Reify r, IntConLevel) {
    if (a.size() != x.size())
      throw ArgumentSizeMismatch("Int::linear");
    if (home.failed()) return;
    Region re(home);
    Linear::Term<IntView>* t = re.alloc<Linear::Term<IntView> >(x.size()+1);
    for (int i = x.size(); i--; ) {
      t[i].a=a[i]; t[i].x=x[i];
    }
    t[x.size()].a=-1; t[x.size()].x=y;
    Linear::post(home,t,x.size()+1,irt,0,r);
  }

}

// STATISTICS: int-post
