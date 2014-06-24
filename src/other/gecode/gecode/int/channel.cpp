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

#include <gecode/int/channel.hh>

namespace Gecode {

  void
  channel(Home home, const IntVarArgs& x, int xoff,
          const IntVarArgs& y, int yoff,
          IntConLevel icl) {
    using namespace Int;
    using namespace Channel;
    int n = x.size();
    if (n != y.size())
      throw ArgumentSizeMismatch("Int::channel");
    if (x.same(home) || y.same(home))
      throw ArgumentSame("Int::channel");
    Limits::check(xoff,"Int::channel");
    Limits::check(yoff,"Int::channel");
    if ((xoff < 0) || (yoff < 0))
      throw OutOfLimits("Int::channel");
    if (home.failed()) return;
    if (n == 0)
      return;

    if ((xoff < 2) && (yoff < 2) && (xoff == yoff)) {
      if (icl == ICL_DOM) {
        DomInfo<IntView,NoOffset<IntView> >* di = 
          static_cast<Space&>(home).
            alloc<DomInfo<IntView,NoOffset<IntView> > >(2*(n+xoff));
        for (int i=n; i--; ) {
          di[xoff+i    ].init(x[i],n+xoff);
          di[2*xoff+i+n].init(y[i],n+xoff);
        }
        if (xoff == 1) {
          IntVar x0(home,0,0);
          di[0].init(x0, n+xoff);
          IntVar y0(home,0,0);
          di[n+xoff].init(y0, n+xoff);
        }
        NoOffset<IntView> noff;
        if (x.same(home,y)) {
          GECODE_ES_FAIL((Dom<IntView,NoOffset<IntView>,true>
            ::post(home,n+xoff,di,noff,noff)));
        } else {
          GECODE_ES_FAIL((Dom<IntView,NoOffset<IntView>,false>
            ::post(home,n+xoff,di,noff,noff)));
        }
      } else {
        ValInfo<IntView>* vi = 
          static_cast<Space&>(home).alloc<ValInfo<IntView> >(2*(n+xoff));
        for (int i=n; i--; ) {
          vi[xoff+i    ].init(x[i],n+xoff);
          vi[2*xoff+i+n].init(y[i],n+xoff);
        }
        if (xoff == 1) {
          IntVar x0(home,0,0);
          vi[0].init(x0, n+xoff);
          IntVar y0(home,0,0);
          vi[n+xoff].init(y0, n+xoff);
        }
        NoOffset<IntView> noff;
        if (x.same(home,y)) {
          GECODE_ES_FAIL((Val<IntView,NoOffset<IntView>,true>
            ::post(home,n+xoff,vi,noff,noff)));
        } else {
          GECODE_ES_FAIL((Val<IntView,NoOffset<IntView>,false>
            ::post(home,n+xoff,vi,noff,noff)));
        }
      }
    } else {
      if (icl == ICL_DOM) {
        DomInfo<IntView,Offset>* di = 
          static_cast<Space&>(home).alloc<DomInfo<IntView,Offset> >(2*n);
        for (int i=n; i--; ) {
          di[i  ].init(x[i],n);
          di[i+n].init(y[i],n);
        }
        Offset ox(-xoff);
        Offset oy(-yoff);
        if (x.same(home,y)) {
          GECODE_ES_FAIL((Dom<IntView,Offset,true>
                          ::post(home,n,di,ox,oy)));
        } else {
          GECODE_ES_FAIL((Dom<IntView,Offset,false>
                          ::post(home,n,di,ox,oy)));
        }
      } else {
        ValInfo<IntView>* vi = 
          static_cast<Space&>(home).alloc<ValInfo<IntView> >(2*n);
        for (int i=n; i--; ) {
          vi[i  ].init(x[i],n);
          vi[i+n].init(y[i],n);
        }
        Offset ox(-xoff);
        Offset oy(-yoff);
        if (x.same(home,y)) {
          GECODE_ES_FAIL((Val<IntView,Offset,true>
                          ::post(home,n,vi,ox,oy)));
        } else {
          GECODE_ES_FAIL((Val<IntView,Offset,false>
                          ::post(home,n,vi,ox,oy)));
        }
      }
    }

  }

  void
  channel(Home home, const IntVarArgs& x, const IntVarArgs& y,
          IntConLevel icl) {
    channel(home, x, 0, y, 0, icl);
  }
  void
  channel(Home home, BoolVar x0, IntVar x1, IntConLevel) {
    using namespace Int;
    if (home.failed()) return;
    GECODE_ES_FAIL(Channel::LinkSingle::post(home,x0,x1));
  }

  void
  channel(Home home, const BoolVarArgs& x, IntVar y, int o,
          IntConLevel) {
    using namespace Int;
    if (x.same(home))
      throw ArgumentSame("Int::channel");
    Limits::check(o,"Int::channel");
    if (home.failed()) return;
    ViewArray<BoolView> xv(home,x);
    GECODE_ES_FAIL(Channel::LinkMulti::post(home,xv,y,o));
  }

}

// STATISTICS: int-post
