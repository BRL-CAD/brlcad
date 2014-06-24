/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2005
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

namespace Gecode {

  /*
   * Implementation
   *
   */

  forceinline
  FloatValArgs::FloatValArgs(void) : PrimArgArray<FloatVal>(0) {}

  forceinline
  FloatValArgs::FloatValArgs(int n) : PrimArgArray<FloatVal>(n) {}
  
  forceinline
  FloatValArgs::FloatValArgs(const SharedArray<FloatVal>& x)
    : PrimArgArray<FloatVal>(x.size()) {
    for (int i=x.size(); i--;)
      a[i] = x[i];
  }
  forceinline
  FloatValArgs::FloatValArgs(const std::vector<FloatVal>& x)
    : PrimArgArray<FloatVal>(x) {}
  template<class InputIterator>
  forceinline
  FloatValArgs::FloatValArgs(InputIterator first, InputIterator last)
    : PrimArgArray<FloatVal>(first,last) {}
  
  forceinline
  FloatValArgs::FloatValArgs(int n, const FloatVal* e)
  : PrimArgArray<FloatVal>(n, e) {}
  
  forceinline
  FloatValArgs::FloatValArgs(const PrimArgArray<FloatVal>& a) : PrimArgArray<FloatVal>(a) {}

  forceinline FloatValArgs
  FloatValArgs::create(int n, FloatVal start, int inc) {
    FloatValArgs r(n);
    for (int i=0; i<n; i++, start+=inc)
      r[i] = start;
    return r;
  }

  forceinline
  FloatVarArray::FloatVarArray(void) {}

  forceinline
  FloatVarArray::FloatVarArray(Space& home, int n)
    : VarArray<FloatVar>(home,n) {}

  forceinline
  FloatVarArray::FloatVarArray(const FloatVarArray& a)
    : VarArray<FloatVar>(a) {}

  forceinline
  FloatVarArray::FloatVarArray(Space& home, const FloatVarArgs& a)
    : VarArray<FloatVar>(home,a) {}

}

// STATISTICS: float-other
