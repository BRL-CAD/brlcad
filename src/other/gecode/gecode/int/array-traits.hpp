/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2005
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

  class IntVarArgs;
  class IntVarArray;
  class BoolVarArgs;
  class BoolVarArray;
  class IntArgs;
  
  /// Traits of %IntVarArgs
  template<>
  class ArrayTraits<VarArgArray<IntVar> > {
  public:
    typedef IntVarArgs StorageType;
    typedef IntVar     ValueType;
    typedef IntVarArgs ArgsType;
  };

  /// Traits of %IntVarArray
  template<>
  class ArrayTraits<VarArray<IntVar> > {
  public:
    typedef IntVarArray  StorageType;
    typedef IntVar       ValueType;
    typedef IntVarArgs   ArgsType;
  };

  /// Traits of %BoolVarArgs
  template<>
  class ArrayTraits<VarArgArray<BoolVar> > {
  public:
    typedef BoolVarArgs StorageType;
    typedef BoolVar     ValueType;
    typedef BoolVarArgs ArgsType;
  };

  /// Traits of %BoolVarArray
  template<>
  class ArrayTraits<VarArray<BoolVar> > {
  public:
    typedef BoolVarArray  StorageType;
    typedef BoolVar       ValueType;
    typedef BoolVarArgs   ArgsType;
  };

  /// Traits of %IntArgs
  template<>
  class ArrayTraits<PrimArgArray<int> > {
  public:
    typedef IntArgs StorageType;
    typedef int     ValueType;
    typedef IntArgs ArgsType;
  };

  /// Traits of %IntSetArgs
  template<>
  class ArrayTraits<ArgArray<IntSet> > {
  public:
    typedef IntSetArgs StorageType;
    typedef IntSet     ValueType;
    typedef IntSetArgs ArgsType;
  };

  /// Traits of %IntVarArray
  template<>
  class ArrayTraits<IntVarArray> {
  public:
    typedef IntVarArray  StorageType;
    typedef IntVar       ValueType;
    typedef IntVarArgs   ArgsType;
  };
  
  /// Traits of %IntVarArgs
  template<>
  class ArrayTraits<IntVarArgs> {
  public:
    typedef IntVarArgs StorageType;
    typedef IntVar     ValueType;
    typedef IntVarArgs ArgsType;
  };

  /// Traits of %IntArgs
  template<>
  class ArrayTraits<IntArgs> {
  public:
    typedef IntArgs StorageType;
    typedef int     ValueType;
    typedef IntArgs ArgsType;
  };
  
  /// Traits of %BoolVarArray
  template<>
  class ArrayTraits<BoolVarArray> {
  public:
    typedef BoolVarArray  StorageType;
    typedef BoolVar       ValueType;
    typedef BoolVarArgs   ArgsType;
  };
  
  /// Traits of %BoolVarArgs
  template<>
  class ArrayTraits<BoolVarArgs> {
  public:
    typedef BoolVarArgs StorageType;
    typedef BoolVar     ValueType;
    typedef BoolVarArgs ArgsType;
  };

}

// STATISTICS: int-other
