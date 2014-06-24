/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     David Rijsman <David.Rijsman@quintiq.com>
 *
 *  Copyright:
 *     David Rijsman, 2009
 *
 *  Last modified:
 *     $Date$
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

#ifndef __GECODE_INT_SEQUENCE_HH__
#define __GECODE_INT_SEQUENCE_HH__

#include <gecode/int.hh>
#include <gecode/int/rel.hh>

namespace Gecode { namespace Int { namespace Sequence {

  /**
   * \namespace Gecode::Int::Sequence
   * \brief %Sequence propagators
   *
   * This namespace contains a propagator for the
   * cumulatives constraint as presented in
   * Willem Jan van Hoeve, Gilles Pesant, Louis-Martin Rousseau, and
   * Ashish Sabharwal, New filtering algorithms for combinations of 
   * among constraints. Constraints, 14(2), 273-292, 2009.
   *
   */

  template<class View> 
  class SupportAdvisor;

  template<class View,class Val,bool iss> 
  class ViewValSupport;

  /**
   * \brief An array of %ViewValSupport data structures
   *
   */
  template<class View, class Val,bool iss>
  class ViewValSupportArray {
  private:
    /// The actual array
    ViewValSupport<View,Val,iss>* xs;
    /// The size of the array
    int n;
  public:
    /// Default constructor
    ViewValSupportArray(void);
    /// Copy constructor
    ViewValSupportArray(const ViewValSupportArray<View,Val,iss>&);
    /// Construct an ViewValSupportArray from \a x
    ViewValSupportArray(Space& home, ViewArray<View>&, Val s, int q);
    /// Construct an ViewValSupportArray of size \a n
    ViewValSupportArray(Space& home, int n);
    /// Return the current size
    int size(void) const;
    /// Access element \a n
    ViewValSupport<View,Val,iss>& operator [](int n);
    /// Access element \a n
    const ViewValSupport<View,Val,iss>& operator [](int) const;
    /// Cloning
    void update(Space& home, bool share, ViewValSupportArray<View,Val,iss>& x);
    /// Propagate
    ExecStatus propagate(Space& home,ViewArray<View>& a,Val s,int q,int l,int u);
    /// Advise
    ExecStatus advise(Space& home,ViewArray<View>& a,Val s,int q,int j,const Delta& d);
  };

  /**
   * \brief %Sequence propagator for array of integers
   *
   * Requires \code #include <gecode/int/sequence.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, class Val>
  class Sequence : public Propagator {
  protected:
    /// Constructor for cloning \a p
    Sequence(Space& home, bool shared, Sequence& p);
    /// Constructor for creation
    Sequence(Home home, ViewArray<View>& x, Val s, int q, int l, int u);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Advise function
    ExecStatus advise(Space& home, Advisor& _a, const Delta& d);
    /// Cost function 
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for 
    static  ExecStatus post(Home home, ViewArray<View>& x, Val s, int q, int l, int u);
    /// Check for consistency 
    static  ExecStatus check(Space& home, ViewArray<View>& x, Val s, int q, int l, int u);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  private:
    /// Views to sequence
    ViewArray<View> x;
    /// Value counted in the sequence
    Val  s;
    /// Length of each sequence
    int  q;
    /// Lower bound
    int  l;
    /// Upper bound
    int  u;
    /// Array containing supports for s
    ViewValSupportArray<View,Val,true>  vvsamax;
    /// Array containing supports for not s
    ViewValSupportArray<View,Val,false> vvsamin;
    /// Council for advisors
    Council<SupportAdvisor<View> > ac;
  };

}}}

#include <gecode/int/sequence/set-op.hpp>
#include <gecode/int/sequence/violations.hpp>
#include <gecode/int/sequence/int.hpp>
#include <gecode/int/sequence/view.hpp>

#endif

// STATISTICS: int-prop
