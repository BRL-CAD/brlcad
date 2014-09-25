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

#ifndef __GECODE_FLATZINC_REGISTRY_HH__
#define __GECODE_FLATZINC_REGISTRY_HH__

#include <gecode/flatzinc.hh>
#include <string>
#include <map>

namespace Gecode { namespace FlatZinc {

  /// Map from constraint identifier to constraint posting functions
  class GECODE_FLATZINC_EXPORT Registry {
  public:
    /// Type of constraint posting function
    typedef void (*poster) (FlatZincSpace&,
                            const ConExpr&,
                            AST::Node*);
    /// Add posting function \a p with identifier \a id
    void add(const std::string& id, poster p);
    /// Post constraint specified by \a ce
    void post(FlatZincSpace& s, const ConExpr& ce);

  private:
    /// The actual registry
    std::map<std::string,poster> r;
  };
  
  /// Return global registry object
  GECODE_FLATZINC_EXPORT Registry& registry(void);

}}

#endif

// STATISTICS: flatzinc-any
