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

namespace Gecode { namespace Int { namespace Count {

  template<class VX, class VY>
  forceinline
  LqInt<VX,VY>::LqInt(Home home, ViewArray<VX>& x, int n_s, VY y, int c)
    : IntBase<VX,VY>(home,x,n_s,y,c) {}

  template<class VX, class VY>
  ExecStatus
  LqInt<VX,VY>::post(Home home, ViewArray<VX>& x, VY y, int c) {
    // Eliminate decided views
    int n_x = x.size();
    for (int i=n_x; i--; )
      switch (holds(x[i],y)) {
      case RT_FALSE:
        x[i] = x[--n_x]; break;
      case RT_TRUE:
        x[i] = x[--n_x]; c--; break;
      case RT_MAYBE:
        break;
      default:
        GECODE_NEVER;
      }
    x.size(n_x);
    if (c < 0)
      return ES_FAILED;
    if (c >= n_x)
      return ES_OK;
    // All views must be different
    if (c == 0)
      return post_false(home,x,y);
    (void) new (home) LqInt<VX,VY>(home,x,n_x-c+1,y,c);
    return ES_OK;
  }

  template<class VX, class VY>
  forceinline
  LqInt<VX,VY>::LqInt(Space& home, bool share, LqInt<VX,VY>& p)
    : IntBase<VX,VY>(home,share,p) {}

  template<class VX, class VY>
  Actor*
  LqInt<VX,VY>::copy(Space& home, bool share) {
    return new (home) LqInt<VX,VY>(home,share,*this);
  }

  template<class VX, class VY>
  ExecStatus
  LqInt<VX,VY>::propagate(Space& home, const ModEventDelta&) {
    // Eliminate decided views from subscribed views
    int n_x = x.size();
    for (int i=n_s; i--; )
      switch (holds(x[i],y)) {
      case RT_FALSE:
        x[i].cancel(home,*this,PC_INT_DOM);
        x[i]=x[--n_s]; x[n_s]=x[--n_x];
        break;
      case RT_TRUE:
        x[i].cancel(home,*this,PC_INT_DOM);
        x[i]=x[--n_s]; x[n_s]=x[--n_x]; c--;
        break;
      case RT_MAYBE:
        break;
      default:
        GECODE_NEVER;
      }
    x.size(n_x);
    if (c < 0)
      return ES_FAILED;
    if (c >= n_x)
      return home.ES_SUBSUMED(*this);
    // Eliminate decided views from unsubscribed views
    for (int i=n_x; i-- > n_s; )
      switch (holds(x[i],y)) {
      case RT_FALSE: x[i]=x[--n_x]; break;
      case RT_TRUE:  x[i]=x[--n_x]; c--; break;
      case RT_MAYBE: break;
      default:       GECODE_NEVER;
      }
    x.size(n_x);
    if (c < 0)
      return ES_FAILED;
    if (c >= n_x)
      return home.ES_SUBSUMED(*this);
    if (c == 0) {
      // All views must be different
      GECODE_ES_CHECK(post_false(home,x,y));
      return home.ES_SUBSUMED(*this);
    }
    // Now, there must be new subscriptions from x[n_s] up to x[n_x-c+1]
    int m = n_x-c;
    while (n_s <= m)
      x[n_s++].subscribe(home,*this,PC_INT_DOM,false);
    return ES_FIX;
  }

}}}

// STATISTICS: int-prop
