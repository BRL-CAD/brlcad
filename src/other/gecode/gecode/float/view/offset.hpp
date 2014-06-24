/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Vincent Barichard, 2012
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

namespace Gecode { namespace Float {

  /*
   * Constructors and initialization
   *
   */
  forceinline
  OffsetView::OffsetView(void) {}
  forceinline
  OffsetView::OffsetView(const FloatView& y, FloatNum d)
    : DerivedView<FloatView>(y), c(d) {}


  /*
   * Value access
   *
   */
  forceinline FloatNum
  OffsetView::offset(void) const {
    return c;
  }
  forceinline void
  OffsetView::offset(FloatNum n) {
    c = n;
  }
  forceinline FloatVal
  OffsetView::domain(void) const {
    return x.domain()+c;
  }
  forceinline FloatNum
  OffsetView::min(void) const {
    return x.min()+c;
  }
  forceinline FloatNum
  OffsetView::max(void) const {
    return x.max()+c;
  }
  forceinline FloatNum
  OffsetView::med(void) const {
    return x.med()+c;
  }
  forceinline FloatVal
  OffsetView::val(void) const {
    return x.val()+c;
  }

  forceinline FloatNum
  OffsetView::size(void) const {
    return x.size();
  }


  /*
   * Domain tests
   *
   */
  forceinline bool
  OffsetView::zero_in(void) const {
    return x.in(-c);
  }
  forceinline bool
  OffsetView::in(FloatNum n) const {
    return x.in(n-c);
  }
  forceinline bool
  OffsetView::in(const FloatVal& n) const {
    return x.in(n-c);
  }


  /*
   * Domain update by value
   *
   */
  forceinline ModEvent
  OffsetView::lq(Space& home, int n) {
    return x.lq(home,n-c);
  }
  forceinline ModEvent
  OffsetView::lq(Space& home, FloatNum n) {
    return x.lq(home,n-c);
  }
  forceinline ModEvent
  OffsetView::lq(Space& home, FloatVal n) {
    return x.lq(home,n-c);
  }

  forceinline ModEvent
  OffsetView::gq(Space& home, int n) {
    return x.gq(home,n-c);
  }
  forceinline ModEvent
  OffsetView::gq(Space& home, FloatNum n) {
    return x.gq(home,n-c);
  }
  forceinline ModEvent
  OffsetView::gq(Space& home, FloatVal n) {
    return x.gq(home,n-c);
  }

  forceinline ModEvent
  OffsetView::eq(Space& home, int n) {
    return x.eq(home,n-c);
  }
  forceinline ModEvent
  OffsetView::eq(Space& home, FloatNum n) {
    return x.eq(home,n-c);
  }
  forceinline ModEvent
  OffsetView::eq(Space& home, const FloatVal& n) {
    return x.eq(home,n-c);
  }


  /*
   * Delta information for advisors
   *
   */
  forceinline FloatNum
  OffsetView::min(const Delta& d) const {
    return x.min(d)+c;
  }
  forceinline FloatNum
  OffsetView::max(const Delta& d) const {
    return x.max(d)+c;
  }

  forceinline ModEventDelta
  OffsetView::med(ModEvent me) {
    return VarImpView<FloatVar>::med(me);
  }


  /*
   * Cloning
   *
   */
  forceinline void
  OffsetView::update(Space& home, bool share, OffsetView& y) {
    DerivedView<FloatView>::update(home,share,y);
    c=y.c;
  }

}}

// STATISTICS: float-var

