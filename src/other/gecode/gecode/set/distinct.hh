/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
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

#ifndef __GECODE_SET_ATMOSTONE_HH__
#define __GECODE_SET_ATMOSTONE_HH__

#include <gecode/set.hh>

namespace Gecode { namespace Set { namespace Distinct {

  /**
   * \namespace Gecode::Set::Distinct
   * \brief Propagators for global distinctness constraints
   */

  /**
   * \brief %Propagator for the AtMostOneIntersection constraint
   *
   * Requires \code #include <gecode/set/distinct.hh> \endcode
   * \ingroup FuncSetProp
   */
  class AtmostOne : public NaryPropagator<SetView, PC_SET_ANY> {
  protected:
    /// Cardinality of the sets
    unsigned int c;
    /// Constructor for cloning \a p
    AtmostOne(Space& home, bool share,AtmostOne& p);
    /// Constructor for posting
    AtmostOne(Home home,ViewArray<SetView>&,unsigned int);
  public:
    /// Copy propagator during cloning
    GECODE_SET_EXPORT virtual Actor*      copy(Space& home, bool);
    /// Perform propagation
    GECODE_SET_EXPORT virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for \f$\forall 0\leq i\leq |x| : |x_i|=c\f$ and \f$\forall 0\leq i<j\leq |x| : |x_i\cap x_j|\leq 1\f$
    static ExecStatus post(Home home,ViewArray<SetView> x,unsigned int c);
  };

}}}

#include <gecode/set/distinct/atmostOne.hpp>

#endif

// STATISTICS: set-prop
