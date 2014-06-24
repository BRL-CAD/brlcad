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

#include <gecode/float.hh>

namespace Gecode {

  FloatValArgs::FloatValArgs(int n, int e0, ...) : PrimArgArray<FloatVal>(n) {
    va_list args;
    va_start(args, e0);
    a[0] = e0;
    for (int i = 1; i < n; i++)
      a[i] = va_arg(args,FloatNum);
    va_end(args);
  }
  
  FloatVarArray::FloatVarArray(Space& home, int n, FloatNum min, FloatNum max)
    : VarArray<FloatVar>(home,n) {
    Float::Limits::check(min,"FloatVarArray::FloatVarArray");
    Float::Limits::check(max,"FloatVarArray::FloatVarArray");
    if (min > max)
      throw Float::VariableEmptyDomain("FloatVarArray::FloatVarArray");
    for (int i = size(); i--; )
      x[i]._init(home,min,max);
  }

  FloatVarArgs::FloatVarArgs(Space& home, int n, FloatNum min, FloatNum max)
    : VarArgArray<FloatVar>(n) {
    Float::Limits::check(min,"FloatVarArgs::FloatVarArgs");
    Float::Limits::check(max,"FloatVarArgs::FloatVarArgs");
    if (min > max)
      throw Float::VariableEmptyDomain("FloatVarArgs::FloatVarArgs");
    for (int i = size(); i--; )
      a[i]._init(home,min,max);
  }

}

// STATISTICS: float-post
