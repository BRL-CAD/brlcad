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

#include <gecode/int.hh>

namespace Gecode {

  IntArgs::IntArgs(int n, int e0, ...) : PrimArgArray<int>(n) {
    va_list args;
    va_start(args, e0);
    a[0] = e0;
    for (int i = 1; i < n; i++)
      a[i] = va_arg(args,int);
    va_end(args);
  }
  
  IntVarArray::IntVarArray(Space& home, int n, int min, int max)
    : VarArray<IntVar>(home,n) {
    Int::Limits::check(min,"IntVarArray::IntVarArray");
    Int::Limits::check(max,"IntVarArray::IntVarArray");
    if (min > max)
      throw Int::VariableEmptyDomain("IntVarArray::IntVarArray");
    for (int i = size(); i--; )
      x[i]._init(home,min,max);
  }

  IntVarArray::IntVarArray(Space& home, int n, const IntSet& s)
    : VarArray<IntVar>(home,n) {
    Int::Limits::check(s.min(),"IntVarArray::IntVarArray");
    Int::Limits::check(s.max(),"IntVarArray::IntVarArray");
    if (s.size() == 0)
      throw Int::VariableEmptyDomain("IntVarArray::IntVarArray");
    for (int i = size(); i--; )
      x[i]._init(home,s);
  }

  BoolVarArray::BoolVarArray(Space& home, int n, int min, int max)
    : VarArray<BoolVar>(home, n) {
    if ((min < 0) || (max > 1))
      throw Int::NotZeroOne("BoolVarArray::BoolVarArray");
    if (min > max)
      throw Int::VariableEmptyDomain("BoolVarArray::BoolVarArray");
    for (int i = size(); i--; )
      x[i]._init(home,min,max);
  }

  IntVarArgs::IntVarArgs(Space& home, int n, int min, int max)
    : VarArgArray<IntVar>(n) {
    Int::Limits::check(min,"IntVarArgs::IntVarArgs");
    Int::Limits::check(max,"IntVarArgs::IntVarArgs");
    if (min > max)
      throw Int::VariableEmptyDomain("IntVarArgs::IntVarArgs");
    for (int i = size(); i--; )
      a[i]._init(home,min,max);
  }

  IntVarArgs::IntVarArgs(Space& home, int n, const IntSet& s)
    : VarArgArray<IntVar>(n) {
    Int::Limits::check(s.min(),"IntVarArgs::IntVarArgs");
    Int::Limits::check(s.max(),"IntVarArgs::IntVarArgs");
    if (s.size() == 0)
      throw Int::VariableEmptyDomain("IntVarArgs::IntVarArgs");
    for (int i = size(); i--; )
      a[i]._init(home,s);
  }

  BoolVarArgs::BoolVarArgs(Space& home, int n, int min, int max)
    : VarArgArray<BoolVar>(n) {
    if ((min < 0) || (max > 1))
      throw Int::NotZeroOne("BoolVarArgs::BoolVarArgs");
    if (min > max)
      throw Int::VariableEmptyDomain("BoolVarArgs::BoolVarArgs");
    for (int i = size(); i--; )
      a[i]._init(home,min,max);
  }

}

// STATISTICS: int-post
