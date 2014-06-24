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

#include <algorithm>
#include <climits>

#include <gecode/float/linear.hh>
#include <gecode/float/rel.hh>

namespace Gecode { namespace Float { namespace Linear {

  void
  estimate(Term* t, int n, FloatVal c, FloatNum& l, FloatNum &u) {
    FloatVal est = c;
    for (int i=n; i--; ) 
      est += t[i].a * t[i].x.domain();
    FloatNum min = est.min();
    FloatNum max = est.max();
    if (min < Limits::min)
      min = Limits::min;
    if (min > Limits::max)
      min = Limits::max;
    l = min;
    if (max < Limits::min)
      max = Limits::min;
    if (max > Limits::max)
      max = Limits::max;
    u = max;
  }

  /// Sort linear terms by view
  class TermLess {
  public:
    forceinline bool
    operator ()(const Term& a, const Term& b) {
      return before(a.x,b.x);
    }
  };

  /// Extend terms by adding view for result
  FloatView
  extend(Home home, Region& r, Term*& t, int& n) {
    FloatNum min, max;
    estimate(t,n,0.0,min,max);
    FloatVar x(home,min,max);
    Term* et = r.alloc<Term>(n+1);
    for (int i=n; i--; )
      et[i] = t[i];
    et[n].a=-1.0; et[n].x=x;
    t=et; n++;
    return x;
  }


  /**
   * \brief Posting n-ary propagators
   *
   */
  template<class View>
  forceinline void
  post_nary(Home home,
            ViewArray<View>& x, ViewArray<View>& y, FloatRelType frt, 
            FloatVal c) {
    switch (frt) {
    case FRT_EQ:
      GECODE_ES_FAIL((Eq<View,View >::post(home,x,y,c)));
      break;
    case FRT_LQ:
      GECODE_ES_FAIL((Lq<View,View >::post(home,x,y,c)));
      break;
    default: GECODE_NEVER;
    }
  }

  void
  dopost(Home home, Term* t, int n, FloatRelType frt, FloatVal c) {
    Limits::check(c,"Float::linear");

    for (int i=n; i--; )
      if (t[i].x.assigned()) {
        c -= t[i].a * t[i].x.val();
        t[i]=t[--n];
      }

    if ((c < Limits::min) || (c > Limits::max))
      throw OutOfLimits("Float::linear");

    /*
     * Join coefficients for aliased variables:
     *
     */
    {
      // Group same variables
      TermLess tl;
      Support::quicksort<Term,TermLess>(t,n,tl);

      // Join adjacent variables
      int i = 0;
      int j = 0;
      while (i < n) {
        Limits::check(t[i].a,"Float::linear");
        FloatVal a = t[i].a;
        FloatView x = t[i].x;
        while ((++i < n) && same(t[i].x,x)) {
          a += t[i].a;
          Limits::check(a,"Float::linear");
        }
        if (a != 0.0) {
          t[j].a = a; t[j].x = x; j++;
        }
      }
      n = j;
    }

    Term *t_p, *t_n;
    int n_p, n_n;

    /*
     * Partition into positive/negative coefficents
     *
     */
    if (n > 0) {
      int i = 0;
      int j = n-1;
      while (true) {
        while ((t[j].a < 0) && (--j >= 0)) ;
        while ((t[i].a > 0) && (++i <  n)) ;
        if (j <= i) break;
        std::swap(t[i],t[j]);
      }
      t_p = t;     n_p = i;
      t_n = t+n_p; n_n = n-n_p;
    } else {
      t_p = t; n_p = 0;
      t_n = t; n_n = 0;
    }

    /*
     * Make all coefficients positive
     *
     */
    for (int i=n_n; i--; )
      t_n[i].a = -t_n[i].a;

    if (frt == FRT_GQ) {
      frt = FRT_LQ;
      std::swap(n_p,n_n); std::swap(t_p,t_n); c = -c;
    }

    if (n == 0) {
      switch (frt) {
      case FRT_EQ: if (!c.in(0.0)) home.fail(); break;
      case FRT_LQ: if (c.max() < 0.0) home.fail(); break;
      default: GECODE_NEVER;
      }
      return;
    }

    /*
     * Test for unit coefficients only
     *
     */
    bool is_unit = true;
    for (int i=n; i--; )
      if (t[i].a != 1.0) {
        is_unit = false;
        break;
      }
    
    if (is_unit) {
      // Unit coefficients
      ViewArray<FloatView> x(home,n_p);
      for (int i = n_p; i--; )
        x[i] = t_p[i].x;
      ViewArray<FloatView> y(home,n_n);
      for (int i = n_n; i--; )
        y[i] = t_n[i].x;
      post_nary<FloatView>(home,x,y,frt,c);
    } else {
      // Arbitrary coefficients 
      ViewArray<ScaleView> x(home,n_p);
      for (int i = n_p; i--; )
        x[i] = ScaleView(t_p[i].a,t_p[i].x);
      ViewArray<ScaleView> y(home,n_n);
      for (int i = n_n; i--; )
        y[i] = ScaleView(t_n[i].a,t_n[i].x);
      post_nary<ScaleView>(home,x,y,frt,c);
    }
  }

  void
  post(Home home, Term* t, int n, FloatRelType frt, FloatVal c) {
    Region re(home);
    switch (frt) {
    case FRT_EQ: case FRT_LQ: case FRT_GQ:
      break;
    case FRT_NQ: case FRT_LE: case FRT_GR:
      rel(home, extend(home,re,t,n), frt, c); 
      frt=FRT_EQ; c=0.0;
      break;
    default:
      throw UnknownRelation("Float::linear");
    }
    dopost(home, t, n, frt, c);
  }

  void
  post(Home home, Term* t, int n, FloatRelType frt, FloatVal c, Reify r) {
    Region re(home);
    rel(home, extend(home,re,t,n), frt, c, r); 
    dopost(home, t, n, FRT_EQ, 0.0);
  }

}}}

// STATISTICS: float-post

