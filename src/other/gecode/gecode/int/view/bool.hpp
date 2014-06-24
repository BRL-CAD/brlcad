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
  BoolView::BoolView(void) {}
  forceinline
  BoolView::BoolView(const BoolVar& y)
    : VarImpView<BoolVar>(y.varimp()) {}
  forceinline
  BoolView::BoolView(BoolVarImp* y)
    : VarImpView<BoolVar>(y) {}

  /*
   * Value access
   *
   */
  forceinline BoolStatus
  BoolView::status(void) const {
    return x->status();
  }
  forceinline int
  BoolView::min(void) const {
    return x->min();
  }
  forceinline int
  BoolView::max(void) const {
    return x->max();
  }
  forceinline int
  BoolView::med(void) const {
    return x->med();
  }
  forceinline int
  BoolView::val(void) const {
    return x->val();
  }

  forceinline unsigned int
  BoolView::size(void) const {
    return x->size();
  }
  forceinline unsigned int
  BoolView::width(void) const {
    return x->width();
  }
  forceinline unsigned int
  BoolView::regret_min(void) const {
    return x->regret_min();
  }
  forceinline unsigned int
  BoolView::regret_max(void) const {
    return x->regret_max();
  }


  /*
   * Domain tests
   *
   */
  forceinline bool
  BoolView::range(void) const {
    return x->range();
  }
  forceinline bool
  BoolView::in(int n) const {
    return x->in(n);
  }
  forceinline bool
  BoolView::in(long long int n) const {
    return x->in(n);
  }


  /*
   * Domain update by value
   *
   */
  forceinline ModEvent
  BoolView::lq(Space& home, int n) {
    return x->lq(home,n);
  }
  forceinline ModEvent
  BoolView::lq(Space& home, long long int n) {
    return x->lq(home,n);
  }

  forceinline ModEvent
  BoolView::le(Space& home, int n) {
    return x->lq(home,n-1);
  }
  forceinline ModEvent
  BoolView::le(Space& home, long long int n) {
    return x->lq(home,n-1);
  }

  forceinline ModEvent
  BoolView::gq(Space& home, int n) {
    return x->gq(home,n);
  }
  forceinline ModEvent
  BoolView::gq(Space& home, long long int n) {
    return x->gq(home,n);
  }

  forceinline ModEvent
  BoolView::gr(Space& home, int n) {
    return x->gq(home,n+1);
  }
  forceinline ModEvent
  BoolView::gr(Space& home, long long int n) {
    return x->gq(home,n+1);
  }

  forceinline ModEvent
  BoolView::nq(Space& home, int n) {
    return x->nq(home,n);
  }
  forceinline ModEvent
  BoolView::nq(Space& home, long long int n) {
    return x->nq(home,n);
  }

  forceinline ModEvent
  BoolView::eq(Space& home, int n) {
    return x->eq(home,n);
  }
  forceinline ModEvent
  BoolView::eq(Space& home, long long int n) {
    return x->eq(home,n);
  }


  /*
   * Iterator-based domain update
   *
   */
  template<class I>
  forceinline ModEvent
  BoolView::narrow_r(Space& home, I& i, bool depend) {
    return x->narrow_r(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  BoolView::inter_r(Space& home, I& i, bool depend) {
    return x->inter_r(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  BoolView::minus_r(Space& home, I& i, bool depend) {
    return x->minus_r(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  BoolView::narrow_v(Space& home, I& i, bool depend) {
    return x->narrow_v(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  BoolView::inter_v(Space& home, I& i, bool depend) {
    return x->inter_v(home,i,depend);
  }
  template<class I>
  forceinline ModEvent
  BoolView::minus_v(Space& home, I& i, bool depend) {
    return x->minus_v(home,i,depend);
  }


  /*
   * Boolean domain tests
   *
   */
  forceinline bool
  BoolView::zero(void) const {
    return x->zero();
  }
  forceinline bool
  BoolView::one(void) const {
    return x->one();
  }
  forceinline bool
  BoolView::none(void) const {
    return x->none();
  }


  /*
   * Boolean assignment operations
   *
   */
  forceinline ModEvent
  BoolView::zero_none(Space& home) {
    return x->zero_none(home);
  }
  forceinline ModEvent
  BoolView::one_none(Space& home) {
    return x->one_none(home);
  }

  forceinline ModEvent
  BoolView::zero(Space& home) {
    return x->zero(home);
  }
  forceinline ModEvent
  BoolView::one(Space& home) {
    return x->one(home);
  }


  /*
   * Delta information for advisors
   *
   */
  forceinline int
  BoolView::min(const Delta& d) const {
    return BoolVarImp::min(d);
  }
  forceinline int
  BoolView::max(const Delta& d) const {
    return BoolVarImp::max(d);
  }
  forceinline bool
  BoolView::any(const Delta& d) const {
    return BoolVarImp::any(d);
  }
  forceinline bool
  BoolView::zero(const Delta& d) {
    return BoolVarImp::zero(d);
  }
  forceinline bool
  BoolView::one(const Delta& d) {
    return BoolVarImp::one(d);
  }



  forceinline ModEventDelta
  BoolView::med(ModEvent me) {
    return VarImpView<BoolVar>::med(me);
  }

  /**
   * \brief %Range iterator for Boolean variable views
   * \ingroup TaskActorIntView
   */
  template<>
  class ViewRanges<BoolView> : public Iter::Ranges::Singleton {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const BoolView& x);
    /// Initialize with ranges for view \a x
    void init(const BoolView& x);
    //@}
  };

  forceinline
  ViewRanges<BoolView>::ViewRanges(void) {}

  forceinline
  ViewRanges<BoolView>::ViewRanges(const BoolView& x)
    : Iter::Ranges::Singleton(x.min(),x.max()) {}

  forceinline void
  ViewRanges<BoolView>::init(const BoolView& x) {
    Iter::Ranges::Singleton::init(x.min(),x.max());
  }

}}

// STATISTICS: int-var
