/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

namespace Gecode { namespace Int {

  /*
   * Testing equality
   *
   */

  template<class View>
  forceinline RelTest
  rtest_eq_bnd(View x, View y) {
    if ((x.min() > y.max()) || (x.max() < y.min())) return RT_FALSE;
    return (x.assigned() && y.assigned()) ? RT_TRUE : RT_MAYBE;
  }

  template<class View>
  RelTest
  rtest_eq_dom_check(View x, View y) {
    ViewRanges<View> rx(x), ry(y);
    while (rx() && ry()) {
      if (rx.max() < ry.min()) {
        ++rx;
      } else if (ry.max() < rx.min()) {
        ++ry;
      } else return RT_MAYBE;
    }
    return RT_FALSE;
  }

  template<class View>
  forceinline RelTest
  rtest_eq_dom(View x, View y) {
    RelTest rt = rtest_eq_bnd(x,y);
    if (rt != RT_MAYBE) return rt;
    return (x.range() && y.range()) ? RT_MAYBE : rtest_eq_dom_check(x,y);
  }


  template<class View>
  forceinline RelTest
  rtest_eq_bnd(View x, int n) {
    if ((n > x.max()) || (n < x.min())) return RT_FALSE;
    return x.assigned() ? RT_TRUE : RT_MAYBE;
  }

  template<class View>
  RelTest
  rtest_eq_dom_check(View x, int n) {
    ViewRanges<View> rx(x);
    while (n > rx.max()) ++rx;
    return (n >= rx.min()) ? RT_MAYBE : RT_FALSE;
  }

  template<class View>
  forceinline RelTest
  rtest_eq_dom(View x, int n) {
    RelTest rt = rtest_eq_bnd(x,n);
    if (rt != RT_MAYBE) return rt;
    return x.range() ? RT_MAYBE : rtest_eq_dom_check(x,n);
  }



  /*
   * Testing disequality
   *
   */

  template<class View>
  forceinline RelTest
  rtest_nq_bnd(View x, View y) {
    if ((x.min() > y.max()) || (x.max() < y.min())) return RT_TRUE;
    return (x.assigned() && y.assigned()) ? RT_FALSE : RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_nq_dom_check(View x, View y) {
    ViewRanges<View> rx(x), ry(y);
    while (rx() && ry()) {
      if (rx.max() < ry.min()) {
        ++rx;
      } else if (ry.max() < rx.min()) {
        ++ry;
      } else return RT_MAYBE;
    }
    return RT_TRUE;
  }

  template<class View>
  forceinline RelTest
  rtest_nq_dom(View x, View y) {
    RelTest rt = rtest_nq_bnd(x,y);
    if (rt != RT_MAYBE) return rt;
    return (x.range() && y.range()) ? RT_MAYBE : rtest_nq_dom_check(x,y);
  }


  template<class View>
  forceinline RelTest
  rtest_nq_bnd(View x, int n) {
    if ((n > x.max()) || (n < x.min())) return RT_TRUE;
    return (x.assigned()) ? RT_FALSE : RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_nq_dom_check(View x, int n) {
    ViewRanges<View> rx(x);
    while (n > rx.max()) ++rx;
    return (n >= rx.min()) ? RT_MAYBE : RT_TRUE;
  }

  template<class View>
  forceinline RelTest
  rtest_nq_dom(View x, int n) {
    RelTest rt = rtest_nq_bnd(x,n);
    if (rt != RT_MAYBE) return rt;
    return x.range() ? RT_MAYBE : rtest_nq_dom_check(x,n);
  }


  /*
   * Testing inequalities
   *
   */

  template<class View>
  forceinline RelTest
  rtest_lq(View x, int n) {
    if (x.max() <= n) return RT_TRUE;
    if (x.min() > n)  return RT_FALSE;
    return RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_lq(View x, View y) {
    if (x.max() <= y.min()) return RT_TRUE;
    if (x.min() > y.max())  return RT_FALSE;
    return RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_le(View x, int n) {
    if (x.max() <  n) return RT_TRUE;
    if (x.min() >= n) return RT_FALSE;
    return RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_le(View x, View y) {
    if (x.max() <  y.min()) return RT_TRUE;
    if (x.min() >= y.max()) return RT_FALSE;
    return RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_gq(View x, int n) {
    if (x.max() <  n) return RT_FALSE;
    if (x.min() >= n) return RT_TRUE;
    return RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_gq(View x, View y) {
    if (x.max() <  y.min()) return RT_FALSE;
    if (x.min() >= y.max()) return RT_TRUE;
    return RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_gr(View x, int n) {
    if (x.max() <= n) return RT_FALSE;
    if (x.min() >  n) return RT_TRUE;
    return RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_gr(View x, View y) {
    if (x.max() <= y.min()) return RT_FALSE;
    if (x.min() >  y.max()) return RT_TRUE;
    return RT_MAYBE;
  }

}}

// STATISTICS: int-var

