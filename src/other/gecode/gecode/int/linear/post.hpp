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

#include <algorithm>
#include <climits>

namespace Gecode { namespace Int { namespace Linear {

  template<class View>
  inline void
  estimate(Term<View>* t, int n, int c, int& l, int &u) {
    long long int min = c;
    long long int max = c;
    for (int i=n; i--; ) {
      long long int a = t[i].a;
      if (a > 0) {
        min += a*t[i].x.min();
        max += a*t[i].x.max();
      } else {
        max += a*t[i].x.min();
        min += a*t[i].x.max();
      }
    }
    if (min < Limits::min)
      min = Limits::min;
    if (min > Limits::max)
      min = Limits::max;
    l = static_cast<int>(min);
    if (max < Limits::min)
      max = Limits::min;
    if (max > Limits::max)
      max = Limits::max;
    u = static_cast<int>(max);
  }

  /// Sort linear terms by view
  template<class View>
  class TermLess {
  public:
    forceinline bool
    operator ()(const Term<View>& a, const Term<View>& b) {
      return before(a.x,b.x);
    }
  };

  /// Compute the greatest common divisor of \a a and \a b
  inline int 
  gcd(int a, int b) {
    if (b > a) 
      std::swap(a,b);
    while (b > 0) {
      int t = b; b = a % b; a = t;
    }
    return a;
  }



  /** \brief Normalize linear integer constraints
   *
   * \param t array of linear terms
   * \param n size of array
   * \param t_p array of linear terms over integers with positive coefficients
   * \param n_p number of postive terms
   * \param t_n array of linear terms over integers with negative coefficients
   * \param n_n number of negative terms
   * \param gcd greatest common divisor of all coefficients
   *
   * Replaces all negative coefficients by positive coefficients.
   *
   *  - Variables occuring multiply in the term array are replaced
   *    by a single occurence: for example, \f$ax+bx\f$ becomes
   *    \f$(a+b)x\f$.
   *  - If in the above simplification the value for \f$(a+b)\f$ (or for
   *    \f$a\f$ and \f$b\f$) exceeds the limits for integers as
   *    defined in Limits::Int, an exception of type
   *    Int::NumericalOverflow is thrown.
   *  - Divides all coefficients by their greatest common divisor and
   *    returns the gcd \a g
   *
   * Returns true, if all coefficients are unit coefficients
   */
  template<class View>
  inline bool
  normalize(Term<View>* t, int &n,
            Term<View>* &t_p, int &n_p,
            Term<View>* &t_n, int &n_n, 
            int& g) {
    /*
     * Join coefficients for aliased variables:
     *
     */
    {
      // Group same variables
      TermLess<View> tl;
      Support::quicksort<Term<View>,TermLess<View> >(t,n,tl);

      // Join adjacent variables
      int i = 0;
      int j = 0;
      while (i < n) {
        Limits::check(t[i].a,"Int::linear");
        long long int a = t[i].a;
        View x = t[i].x;
        while ((++i < n) && same(t[i].x,x)) {
          a += t[i].a;
          Limits::check(a,"Int::linear");
        }
        if (a != 0) {
          t[j].a = static_cast<int>(a); t[j].x = x; j++;
        }
      }
      n = j;
    }

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

    /*
     * Compute greatest common divisor
     *
     */
    if ((n > 0) && (g > 0)) {
      g = t[0].a;
      for (int i=1; (g > 1) && (i < n); i++)
        g = gcd(t[i].a,g);
      if (g > 1)
        for (int i=n; i--; )
          t[i].a /= g;
    } else {
      g = 1;
    }

    /*
     * Test for unit coefficients only
     *
     */
    for (int i=n; i--; )
      if (t[i].a != 1)
        return false;
    return true;
  }

}}}

// STATISTICS: int-post

