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

namespace Gecode { namespace Int {

  /*
   * Constructors and initialization
   *
   */
  forceinline
  IntView::IntView(void) {}
  forceinline
  IntView::IntView(const IntVar& y)
    : VarImpView<IntVar>(y.varimp()) {}
  forceinline
  IntView::IntView(IntVarImp* y)
    : VarImpView<IntVar>(y) {}

  /*
   * Value access
   *
   */
  forceinline int
  IntView::min(void) const {
    return x->min();
  }
  forceinline int
  IntView::max(void) const {
    return x->max();
  }
  forceinline int
  IntView::med(void) const {
    return x->med();
  }
  forceinline int
  IntView::val(void) const {
    return x->val();
  }

  forceinline unsigned int
  IntView::size(void) const {
    return x->size();
  }
  forceinline unsigned int
  IntView::width(void) const {
    return x->width();
  }
  forceinline unsigned int
  IntView::regret_min(void) const {
    return x->regret_min();
  }
  forceinline unsigned int
  IntView::regret_max(void) const {
    return x->regret_max();
  }


  /*
   * Domain tests
   *
   */
  forceinline bool
  IntView::range(void) const {
    return x->range();
  }
  forceinline bool
  IntView::in(int n) const {
    return x->in(n);
  }
  forceinline bool
  IntView::in(long long int n) const {
    return x->in(n);
  }


  /*
   * Domain update by value
   *
   */
  forceinline ModEvent
  IntView::lq(Space& home, int n) {
    return x->lq(home,n);
  }
  forceinline ModEvent
  IntView::lq(Space& home, long long int n) {
    return x->lq(home,n);
  }

  forceinline ModEvent
  IntView::le(Space& home, int n) {
    return x->lq(home,n-1);
  }
  forceinline ModEvent
  IntView::le(Space& home, long long int n) {
    return x->lq(home,n-1);
  }

  forceinline ModEvent
  IntView::gq(Space& home, int n) {
    return x->gq(home,n);
  }
  forceinline ModEvent
  IntView::gq(Space& home, long long int n) {
    return x->gq(home,n);
  }

  forceinline ModEvent
  IntView::gr(Space& home, int n) {
    return x->gq(home,n+1);
  }
  forceinline ModEvent
  IntView::gr(Space& home, long long int n) {
    return x->gq(home,n+1);
  }

  forceinline ModEvent
  IntView::nq(Space& home, int n) {
    return x->nq(home,n);
  }
  forceinline ModEvent
  IntView::nq(Space& home, long long int n) {
    return x->nq(home,n);
  }

  forceinline ModEvent
  IntView::eq(Space& home, int n) {
    return x->eq(home,n);
  }
  forceinline ModEvent
  IntView::eq(Space& home, long long int n) {
    return x->eq(home,n);
  }


  /*
   * Iterator-based domain update
   *
   */
  template<class I>
  forceinline ModEvent
  IntView::narrow_r(Space& home, I& i, bool depend) {
    return x->narrow_r(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  IntView::inter_r(Space& home, I& i, bool depend) {
    return x->inter_r(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  IntView::minus_r(Space& home, I& i, bool depend) {
    return x->minus_r(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  IntView::narrow_v(Space& home, I& i, bool depend) {
    return x->narrow_v(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  IntView::inter_v(Space& home, I& i, bool depend) {
    return x->inter_v(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  IntView::minus_v(Space& home, I& i, bool depend) {
    return x->minus_v(home,i,depend);
  }




  /*
   * Delta information for advisors
   *
   */
  forceinline int
  IntView::min(const Delta& d) const {
    return IntVarImp::min(d);
  }
  forceinline int
  IntView::max(const Delta& d) const {
    return IntVarImp::max(d);
  }
  forceinline bool
  IntView::any(const Delta& d) const {
    return IntVarImp::any(d);
  }


  forceinline ModEventDelta
  IntView::med(ModEvent me) {
    return VarImpView<IntVar>::med(me);
  }


  /**
   * \brief %Range iterator for integer variable views
   * \ingroup TaskActorIntView
   */
  template<>
  class ViewRanges<IntView> : public IntVarImpFwd {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const IntView& x);
    /// Initialize with ranges for view \a x
    void init(const IntView& x);
    //@}
  };

  forceinline
  ViewRanges<IntView>::ViewRanges(void) {}

  forceinline
  ViewRanges<IntView>::ViewRanges(const IntView& x)
    : IntVarImpFwd(x.varimp()) {}

  forceinline void
  ViewRanges<IntView>::init(const IntView& x) {
    IntVarImpFwd::init(x.varimp());
  }

}}

// STATISTICS: int-var

