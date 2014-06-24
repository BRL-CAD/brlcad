/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2007
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

#ifndef __GECODE_FLATZINC_VARSPEC__HH__
#define __GECODE_FLATZINC_VARSPEC__HH__

#include <gecode/flatzinc/option.hh>
#include <gecode/flatzinc/ast.hh>
#include <vector>
#include <iostream>

namespace Gecode { namespace FlatZinc {

  /// %Alias for a variable specification
  class Alias {
  public:
    const int v;
    Alias(int v0) : v(v0) {}
  };

  /// Base class for variable specifications
  class VarSpec {
  public:
    /// Destructor
    virtual ~VarSpec(void) {}
    /// Variable index
    int i;
    /// Whether the variable aliases another variable
    bool alias;
    /// Whether the variable is assigned
    bool assigned;
    /// Whether the variable was introduced in the mzn2fzn translation
    bool introduced;
    /// Whether the variable functionally depends on another variable
    bool funcDep;
    /// Constructor
    VarSpec(bool introduced0, bool funcDep0)
    : introduced(introduced0), funcDep(funcDep0) {}
  };

  /// Specification for integer variables
  class IntVarSpec : public VarSpec {
  public:
    Option<AST::SetLit* > domain;
    IntVarSpec(const Option<AST::SetLit* >& d,
               bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = false;
      assigned = false;
      domain = d;
    }
    IntVarSpec(int i0, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = false; assigned = true; i = i0;
    }
    IntVarSpec(const Alias& eq, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = true; i = eq.v;
    }
    ~IntVarSpec(void) {
      if (!alias && !assigned && domain())
        delete domain.some();
    }
  };

  /// Specification for Boolean variables
  class BoolVarSpec : public VarSpec {
  public:
    Option<AST::SetLit* > domain;
    BoolVarSpec(Option<AST::SetLit* >& d, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = false; assigned = false; domain = d;
    }
    BoolVarSpec(bool b, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = false; assigned = true; i = b;
    }
    BoolVarSpec(const Alias& eq, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = true; i = eq.v;
    }
    ~BoolVarSpec(void) {
      if (!alias && !assigned && domain())
        delete domain.some();
    }
  };

  /// Specification for floating point variables
  class FloatVarSpec : public VarSpec {
  public:
    Option<std::pair<double,double> > domain;
    FloatVarSpec(Option<std::pair<double,double> >& d,
                 bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep), domain(d) {
      alias = false; assigned = false;
    }
    FloatVarSpec(double d, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = false; assigned = true;
      domain = Option<std::pair<double,double> >::some(std::pair<double,double>(d,d));
    }
    FloatVarSpec(const Alias& eq, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = true; i = eq.v;
    }
  };

  /// Specification for set variables
  class SetVarSpec : public VarSpec {
  public:
    Option<AST::SetLit*> upperBound;
    SetVarSpec(bool introduced, bool funcDep) : VarSpec(introduced,funcDep) {
      alias = false; assigned = false;
      upperBound = Option<AST::SetLit* >::none();
    }
    SetVarSpec(const Option<AST::SetLit* >& v, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = false; assigned = false; upperBound = v;
    }
    SetVarSpec(AST::SetLit* v, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = false; assigned = true;
      upperBound = Option<AST::SetLit*>::some(v);
    }
    SetVarSpec(const Alias& eq, bool introduced, bool funcDep)
    : VarSpec(introduced,funcDep) {
      alias = true; i = eq.v;
    }
    ~SetVarSpec(void) {
      if (!alias && upperBound())
        delete upperBound.some();
    }
  };

}}

#endif

// STATISTICS: flatzinc-any
