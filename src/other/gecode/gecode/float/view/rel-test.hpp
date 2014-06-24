/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

namespace Gecode { namespace Float {

  template<class View>
  forceinline RelTest
  rtest_eq(View x, View y) {
    if ((x.min() > y.max()) || (x.max() < y.min())) return RT_FALSE;
    return (x.assigned() && y.assigned()) ? RT_TRUE : RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_eq(View x, FloatVal n) {
    if ((x.min() > n.max()) || (x.max() < n.min())) return RT_FALSE;
    return x.assigned() ? RT_TRUE : RT_MAYBE;
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
  rtest_lq(View x, FloatVal n) {
    if (x.max() <= n.min()) return RT_TRUE;
    if (x.min() > n.max())  return RT_FALSE;
    return RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_le(View x, View y) {
    if (x.max() < y.min()) return RT_TRUE;
    if (x.min() >= y.max())  return RT_FALSE;
    return RT_MAYBE;
  }

  template<class View>
  forceinline RelTest
  rtest_le(View x, FloatVal n) {
    if (x.max() < n.min()) return RT_TRUE;
    if (x.min() >= n.max())  return RT_FALSE;
    return RT_MAYBE;
  }

}}

// STATISTICS: float-var

