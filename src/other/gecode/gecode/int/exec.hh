/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
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

#ifndef __GECODE_INT_EXEC_HH__
#define __GECODE_INT_EXEC_HH__

#include <gecode/int.hh>

/**
 * \namespace Gecode::Int::Exec
 * \brief Synchronized execution
 */

namespace Gecode { namespace Int { namespace Exec {

  /**
   * \brief Conditional propagator
   *
   * Requires \code #include <gecode/int/exec.hh> \endcode
   * \ingroup FuncIntProp
   */
  class When : public UnaryPropagator<BoolView,PC_BOOL_VAL> {
  protected:
    using UnaryPropagator<BoolView,PC_BOOL_VAL>::x0;
    /// Then function pointer
    void (*t)(Space&);
    /// Else function pointer
    void (*e)(Space&);
    /// Constructor for cloning \a p
    When(Space& home, bool share, When& p);
    /// Constructor for creation
    When(Home home, BoolView x, void (*t0)(Space&), void (*e0)(Space&));
  public:
    /// Copy propagator during cloning
    GECODE_INT_EXPORT 
    virtual Actor* copy(Space& home, bool share);
    /// Perform propagation
    GECODE_INT_EXPORT 
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator
    GECODE_INT_EXPORT 
    static ExecStatus post(Home home, BoolView x,
                           void (*t)(Space&), void (*e)(Space&));
  };

}}}

#include <gecode/int/exec/when.hpp>

#endif

// STATISTICS: int-prop

