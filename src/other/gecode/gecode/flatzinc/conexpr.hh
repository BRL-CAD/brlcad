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

#ifndef __GECODE_FLATZINC_CONEXPR_HH__
#define __GECODE_FLATZINC_CONEXPR_HH__

#include <string>
#include <gecode/flatzinc/ast.hh>

namespace Gecode { namespace FlatZinc {

  /// Abstract representation of a constraint
  class ConExpr {
  public:
    /// Identifier for the constraint
    std::string id;
    /// Constraint arguments
    AST::Array* args;
    /// Constructor
    ConExpr(const std::string& id0, AST::Array* args0);
    /// Return argument \a i
    AST::Node* operator[](int i) const;
    /// Return number of arguments
    int size(void) const;
    /// Destructor
    ~ConExpr(void);
  };

  forceinline
  ConExpr::ConExpr(const std::string& id0, AST::Array* args0)
    : id(id0), args(args0) {}

  forceinline AST::Node*
  ConExpr::operator[](int i) const { return args->a[i]; }

  forceinline int
  ConExpr::size(void) const { return args->a.size(); }

  forceinline
  ConExpr::~ConExpr(void) {
    delete args;
  }

}}

#endif

// STATISTICS: flatzinc-any
