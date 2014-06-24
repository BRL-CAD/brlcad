/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
 *     Guido Tack, 2011
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

#include <gecode/int/circuit.hh>

namespace Gecode {

  void
  circuit(Home home, int offset, const IntVarArgs& x, IntConLevel icl) {
    Int::Limits::nonnegative(offset,"Int::circuit");
    if (x.size() == 0)
      throw Int::TooFewArguments("Int::circuit");
    if (x.same(home))
      throw Int::ArgumentSame("Int::circuit");
    if (home.failed()) return;
    ViewArray<Int::IntView> xv(home,x);
    
    if (offset == 0) {
      typedef Int::NoOffset<Int::IntView> NOV;
      NOV no;
      if (icl == ICL_DOM) {
        GECODE_ES_FAIL((Int::Circuit::Dom<Int::IntView,NOV>
                        ::post(home,xv,no)));
      } else {
        GECODE_ES_FAIL((Int::Circuit::Val<Int::IntView,NOV>
                        ::post(home,xv,no)));
      }
    } else {
      typedef Int::Offset OV;
      OV off(-offset);
      if (icl == ICL_DOM) {
        GECODE_ES_FAIL((Int::Circuit::Dom<Int::IntView,OV>
                        ::post(home,xv,off)));
      } else {
        GECODE_ES_FAIL((Int::Circuit::Val<Int::IntView,OV>
                        ::post(home,xv,off)));
      }
    }
  }
  void
  circuit(Home home, const IntVarArgs& x, IntConLevel icl) {
    circuit(home,0,x,icl);
  }
  
  void
  circuit(Home home, const IntArgs& c, int offset,
          const IntVarArgs& x, const IntVarArgs& y, IntVar z,
          IntConLevel icl) {
    Int::Limits::nonnegative(offset,"Int::circuit");
    int n = x.size();
    if (n == 0)
      throw Int::TooFewArguments("Int::circuit");
    if (x.same(home))
      throw Int::ArgumentSame("Int::circuit");
    if ((y.size() != n) || (c.size() != n*n))
      throw Int::ArgumentSizeMismatch("Int::circuit");
    circuit(home, offset, x, icl);
    if (home.failed()) return;
    IntArgs cx(offset+n);
    for (int i=0; i<offset; i++)
      cx[i] = 0;
    for (int i=n; i--; ) {
      for (int j=0; j<n; j++)
        cx[offset+j] = c[i*n+j];
      element(home, cx, x[i], y[i]);
    }
    linear(home, y, IRT_EQ, z);
  }
  void
  circuit(Home home, const IntArgs& c,
          const IntVarArgs& x, const IntVarArgs& y, IntVar z,
          IntConLevel icl) {
    circuit(home,c,0,x,y,z,icl);
  }
  void
  circuit(Home home, const IntArgs& c, int offset,
          const IntVarArgs& x, IntVar z, 
          IntConLevel icl) {
    Int::Limits::nonnegative(offset,"Int::circuit");
    if (home.failed()) return;
    IntVarArgs y(home, x.size(), Int::Limits::min, Int::Limits::max);
    circuit(home, c, offset, x, y, z, icl);
  }
  void
  circuit(Home home, const IntArgs& c,
          const IntVarArgs& x, IntVar z, 
          IntConLevel icl) {
    circuit(home,c,0,x,z,icl);
  }

  void
  path(Home home, int offset, const IntVarArgs& x, IntVar s, IntVar e,
       IntConLevel icl) {
    Int::Limits::nonnegative(offset,"Int::path");
    int n=x.size();
    if (n == 0)
      throw Int::TooFewArguments("Int::path");
    if (x.same(home))
      throw Int::ArgumentSame("Int::path");
    if (home.failed()) return;
    ViewArray<Int::IntView> xv(home,n+1);
    for (int i=n; i--; )
      xv[i] = Int::IntView(x[i]);
    xv[n] = s;

    if (offset == 0) {
      element(home, x, e, n);
      typedef Int::NoOffset<Int::IntView> NOV;
      NOV no;
      if (icl == ICL_DOM) {
        GECODE_ES_FAIL((Int::Circuit::Dom<Int::IntView,NOV>
                        ::post(home,xv,no)));
      } else {
        GECODE_ES_FAIL((Int::Circuit::Val<Int::IntView,NOV>
                        ::post(home,xv,no)));
      }
    } else {
      IntVarArgs ox(n+offset);
      IntVar y(home, -1,-1);
      for (int i=offset; i--; )
        ox[i] = y;
      for (int i=n; i--; )
        ox[offset + i] = x[i];
      element(home, ox, e, offset+n);
      typedef Int::Offset OV;
      OV off(-offset);
      if (icl == ICL_DOM) {
        GECODE_ES_FAIL((Int::Circuit::Dom<Int::IntView,OV>
                        ::post(home,xv,off)));
      } else {
        GECODE_ES_FAIL((Int::Circuit::Val<Int::IntView,OV>
                        ::post(home,xv,off)));
      }
    }
  }
  void
  path(Home home, const IntVarArgs& x, IntVar s, IntVar e,
       IntConLevel icl) {
    path(home,0,x,s,e,icl);
  }
  
  void
  path(Home home, const IntArgs& c, int offset,
       const IntVarArgs& x, IntVar s, IntVar e,
       const IntVarArgs& y, IntVar z,
       IntConLevel icl) {
    Int::Limits::nonnegative(offset,"Int::path");
    int n = x.size();
    if (n == 0)
      throw Int::TooFewArguments("Int::path");
    if (x.same(home))
      throw Int::ArgumentSame("Int::path");
    if ((y.size() != n) || (c.size() != n*n))
      throw Int::ArgumentSizeMismatch("Int::path");
    if (home.failed()) return;
    path(home, offset, x, s, e, icl);
    IntArgs cx(offset+n+1);
    for (int i=0; i<offset; i++)
      cx[i] = 0;
    cx[offset+n] = 0;
    for (int i=n; i--; ) {
      for (int j=0; j<n; j++)
        cx[offset+j] = c[i*n+j];
      element(home, cx, x[i], y[i]);
    }
    linear(home, y, IRT_EQ, z);
  }
  void
  path(Home home, const IntArgs& c,
       const IntVarArgs& x, IntVar s, IntVar e,
       const IntVarArgs& y, IntVar z,
       IntConLevel icl) {
    path(home,c,0,x,s,e,y,z,icl);
  }
  void
  path(Home home, const IntArgs& c, int offset,
       const IntVarArgs& x, IntVar s, IntVar e, IntVar z, 
       IntConLevel icl) {
    Int::Limits::nonnegative(offset,"Int::path");
    if (home.failed()) return;
    IntVarArgs y(home, x.size(), Int::Limits::min, Int::Limits::max);
    path(home, c, offset, x, s, e, y, z, icl);
  }
  void
  path(Home home, const IntArgs& c,
       const IntVarArgs& x, IntVar s, IntVar e, IntVar z, 
       IntConLevel icl) {
    path(home,c,0,x,s,e,z,icl);
  }

}

// STATISTICS: int-post
