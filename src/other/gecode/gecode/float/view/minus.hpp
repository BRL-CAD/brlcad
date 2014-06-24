/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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
  MinusView::MinusView(void) {}
  forceinline
  MinusView::MinusView(const FloatView& y)
    : DerivedView<FloatView>(y) {}

  /*
   * Value access
   *
   */
  forceinline FloatVal
  MinusView::domain(void) const {
    return -x.domain();
  }
  forceinline FloatNum
  MinusView::min(void) const {
    return -x.max();
  }
  forceinline FloatNum
  MinusView::max(void) const {
    return -x.min();
  }
  forceinline FloatNum
  MinusView::med(void) const {
    return -x.med();
  }
  forceinline FloatVal
  MinusView::val(void) const {
    return -x.val();
  }

  forceinline FloatNum
  MinusView::size(void) const {
    return x.size();
  }


  /*
   * Domain tests
   *
   */
  forceinline bool
  MinusView::zero_in(void) const {
    return x.zero_in();
  }
  forceinline bool
  MinusView::in(FloatNum n) const {
    return x.in(-n);
  }
  forceinline bool
  MinusView::in(const FloatVal& n) const {
    return x.in(-n);
  }


  /*
   * Domain update by value
   *
   */
  forceinline ModEvent
  MinusView::lq(Space& home, int n) {
    return x.gq(home,-n);
  }
  forceinline ModEvent
  MinusView::lq(Space& home, FloatNum n) {
    return x.gq(home,-n);
  }
  forceinline ModEvent
  MinusView::lq(Space& home, FloatVal n) {
    return x.gq(home,-n);
  }

  forceinline ModEvent
  MinusView::gq(Space& home, int n) {
    return x.lq(home,-n);
  }
  forceinline ModEvent
  MinusView::gq(Space& home, FloatNum n) {
    return x.lq(home,-n);
  }
  forceinline ModEvent
  MinusView::gq(Space& home, FloatVal n) {
    return x.lq(home,-n);
  }

  forceinline ModEvent
  MinusView::eq(Space& home, int n) {
    return x.eq(home,-n);
  }
  forceinline ModEvent
  MinusView::eq(Space& home, FloatNum n) {
    return x.eq(home,-n);
  }
  forceinline ModEvent
  MinusView::eq(Space& home, const FloatVal& n) {
    return x.eq(home,-n);
  }


  /*
   * Delta information for advisors
   *
   */
  forceinline FloatNum
  MinusView::min(const Delta& d) const {
    return -x.max(d);
  }
  forceinline FloatNum
  MinusView::max(const Delta& d) const {
    return -x.min(d);
  }


  forceinline ModEventDelta
  MinusView::med(ModEvent me) {
    return VarImpView<FloatVar>::med(me);
  }

}}

// STATISTICS: float-var

