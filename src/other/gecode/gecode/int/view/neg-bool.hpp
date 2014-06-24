/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
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
   * Negated Boolean views
   *
   */

  /*
   * Constructors and initialization
   *
   */
  forceinline
  NegBoolView::NegBoolView(void) {}
  forceinline
  NegBoolView::NegBoolView(const BoolView& y)
    : DerivedView<BoolView>(y) {}


  /*
   * Boolean domain tests
   *
   */
  forceinline BoolStatus
  NegBoolView::status(void) const {
    return x.status();
  }
  forceinline bool
  NegBoolView::zero(void) const {
    return x.one();
  }
  forceinline bool
  NegBoolView::one(void) const {
    return x.zero();
  }
  forceinline bool
  NegBoolView::none(void) const {
    return x.none();
  }


  /*
   * Boolean assignment operations
   *
   */
  forceinline ModEvent
  NegBoolView::zero_none(Space& home) {
    return x.one_none(home);
  }
  forceinline ModEvent
  NegBoolView::one_none(Space& home) {
    return x.zero_none(home);
  }

  forceinline ModEvent
  NegBoolView::zero(Space& home) {
    return x.one(home);
  }
  forceinline ModEvent
  NegBoolView::one(Space& home) {
    return x.zero(home);
  }


  /*
   * Value access
   *
   */
  forceinline int
  NegBoolView::min(void) const {
    return x.max();
  }
  forceinline int
  NegBoolView::max(void) const {
    return x.min();
  }
  forceinline int
  NegBoolView::val(void) const {
    return 1-x.val();
  }


  /*
   * Delta information for advisors
   *
   */
  forceinline int
  NegBoolView::min(const Delta& d) const {
    return x.max(d);
  }
  forceinline int
  NegBoolView::max(const Delta& d) const {
    return x.min(d);
  }
  forceinline bool
  NegBoolView::any(const Delta& d) const {
    return x.any(d);
  }
  forceinline bool
  NegBoolView::zero(const Delta& d) {
    return BoolView::one(d);
  }
  forceinline bool
  NegBoolView::one(const Delta& d) {
    return BoolView::zero(d);
  }


  /**
   * \brief %Range iterator for negated Boolean variable views
   * \ingroup TaskActorIntView
   */
  template<>
  class ViewRanges<NegBoolView> : public Iter::Ranges::Singleton {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const NegBoolView& x);
    /// Initialize with ranges for view \a x
    void init(const NegBoolView& x);
    //@}
  };

  forceinline
  ViewRanges<NegBoolView>::ViewRanges(void) {}

  forceinline
  ViewRanges<NegBoolView>::ViewRanges(const NegBoolView& x)
    : Iter::Ranges::Singleton(x.min(),x.max()) {}

  forceinline void
  ViewRanges<NegBoolView>::init(const NegBoolView& x) {
    Iter::Ranges::Singleton::init(x.min(),x.max());
  }

}}

// STATISTICS: int-var
