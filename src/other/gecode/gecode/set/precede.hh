/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christopher Mears <Chris.Mears@monash.edu>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christopher Mears, 2011
 *     Christian Schulte, 2011
 *     Guido Tack, 2011
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

#ifndef __GECODE_SET_PRECEDE_HH__
#define __GECODE_SET_PRECEDE_HH__

#include <gecode/set.hh>

/**
 * \namespace Gecode::Set::Precede
 * \brief Value precedence propagators
 */

namespace Gecode { namespace Set { namespace Precede {
    
  /**
   * \brief Single value precedence propagator
   *
   * The propagator is based on: Yat Chiu Law and Jimmy H.M. Lee,
   * Global Constraints for Integer and Set Value Precedence,
   * CP 2004, 362--376.
   *
   * Requires \code #include <gecode/set/precede.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View>
  class Single : public NaryPropagator<View,PC_SET_NONE> {
  protected:
    using NaryPropagator<View,PC_SET_NONE>::x;
    /// %Advisors for views (by position in array)
    class Index : public Advisor {
    public:
      /// The position of the view in the view array
      int i;
      /// Create index advisor
      Index(Space& home, Propagator& p, Council<Index>& c, int i);
      /// Clone index advisor \a a
      Index(Space& home, bool share, Index& a);
    };
    /// The advisor council
    Council<Index> c;
    /// The value \a s must precede \a t
    int s, t;
    /// Pointers updated during propagation
    int alpha, beta, gamma;
    /// Update the alpha pointer
    ExecStatus updateAlpha(Space& home);
    /// Update the beta pointer
    ExecStatus updateBeta(Space& home);
    /// Constructor for posting
    Single(Home home, ViewArray<View>& x, int s, int t, int beta, int gamma);
    /// Constructor for cloning \a p
    Single(Space& home, bool share, Single<View>& p);
  public:
    /// Copy propagator during cloning
    virtual Propagator* copy(Space& home, bool share);
    /// Cost function
    virtual PropCost cost(const Space&, const ModEventDelta&) const;
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Give advice to propagator
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator that \a s precedes \a t in \a x
    static ExecStatus post(Home home, ViewArray<View>& x, int s, int t);
  };

}}}

#include <gecode/set/precede/single.hpp>

#endif

// STATISTICS: set-prop
