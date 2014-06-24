/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
 *     Gabor Szokoli, 2004
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

namespace Gecode {

  /*
   * Constructors and access
   *
   */

  forceinline
  SetVar::SetVar(void) {}

  forceinline
  SetVar::SetVar(const SetVar& y)
    : VarImpVar<Set::SetVarImp>(y.varimp()) {}

  forceinline
  SetVar::SetVar(const Set::SetView& y)
    : VarImpVar<Set::SetVarImp>(y.varimp()) {}


  /*
   * Variable information
   *
   */

  forceinline unsigned int
  SetVar::glbSize(void) const { return x->glbSize(); }

  forceinline unsigned int
  SetVar::lubSize(void) const { return x->lubSize(); }

  forceinline unsigned int
  SetVar::unknownSize(void) const { return x->lubSize()-x->glbSize(); }

  forceinline bool
  SetVar::contains(int i) const { return x->knownIn(i); }

  forceinline bool
  SetVar::notContains(int i) const { return x->knownOut(i); }

  forceinline unsigned int
  SetVar::cardMin(void) const { return x->cardMin(); }

  forceinline unsigned int
  SetVar::cardMax(void) const { return x->cardMax(); }

  forceinline int
  SetVar::lubMin(void) const { return x->lubMin(); }

  forceinline int
  SetVar::lubMax(void) const { return x->lubMax(); }

  forceinline int
  SetVar::glbMin(void) const { return x->glbMin(); }

  forceinline int
  SetVar::glbMax(void) const { return x->glbMax(); }



  /*
   * Range and value iterators for set variables
   *
   */

  forceinline
  SetVarGlbRanges::SetVarGlbRanges(void) {}

  forceinline
  SetVarGlbRanges::SetVarGlbRanges(const SetVar& s)
    : iter(s.varimp()) {}

  forceinline
  bool
  SetVarGlbRanges::operator ()(void) const { return iter(); }

  forceinline
  void
  SetVarGlbRanges::operator ++(void) { ++iter; }

  forceinline
  int
  SetVarGlbRanges::min(void) const { return iter.min(); }

  forceinline
  int
  SetVarGlbRanges::max(void) const { return iter.max(); }

  forceinline
  unsigned int
  SetVarGlbRanges::width(void) const { return iter.width(); }

  forceinline
  SetVarLubRanges::SetVarLubRanges(void) {}

  forceinline
  SetVarLubRanges::SetVarLubRanges(const SetVar& s)
    : iter(s.varimp()) {}

  forceinline
  bool
  SetVarLubRanges::operator ()(void) const { return iter(); }

  forceinline
  void
  SetVarLubRanges::operator ++(void) { ++iter; }

  forceinline
  int
  SetVarLubRanges::min(void) const { return iter.min(); }

  forceinline
  int
  SetVarLubRanges::max(void) const { return iter.max(); }

  forceinline
  unsigned int
  SetVarLubRanges::width(void) const { return iter.width(); }

  forceinline
  SetVarUnknownRanges::SetVarUnknownRanges(void) {}

  forceinline
  SetVarUnknownRanges::SetVarUnknownRanges(const SetVar& s) {
    iter.init(s.varimp());
  }

  forceinline
  bool
  SetVarUnknownRanges::operator ()(void) const { return iter(); }

  forceinline
  void
  SetVarUnknownRanges::operator ++(void) { ++iter; }

  forceinline
  int
  SetVarUnknownRanges::min(void) const { return iter.min(); }

  forceinline
  int
  SetVarUnknownRanges::max(void) const { return iter.max(); }

  forceinline
  unsigned int
  SetVarUnknownRanges::width(void) const { return iter.width(); }

  forceinline
  SetVarGlbValues::SetVarGlbValues(const SetVar& x) {
    SetVarGlbRanges ivr(x);
    iter.init(ivr);
  }

  forceinline bool
  SetVarGlbValues::operator ()(void) const {
    return iter();
  }

  forceinline void
  SetVarGlbValues::operator ++(void) {
    ++iter;
  }

  forceinline int
  SetVarGlbValues::val(void) const {
    return iter.val();
  }

  forceinline
  SetVarLubValues::SetVarLubValues(const SetVar& x) {
    SetVarLubRanges ivr(x);
    iter.init(ivr);
  }

  forceinline bool
  SetVarLubValues::operator ()(void) const {
    return iter();
  }

  forceinline void
  SetVarLubValues::operator ++(void) {
    ++iter;
  }

  forceinline int
  SetVarLubValues::val(void) const {
    return iter.val();
  }

  forceinline
  SetVarUnknownValues::SetVarUnknownValues(const SetVar& x) {
    SetVarUnknownRanges ivr(x);
    iter.init(ivr);
  }

  forceinline bool
  SetVarUnknownValues::operator ()(void) const {
    return iter();
  }

  forceinline void
  SetVarUnknownValues::operator ++(void) {
    ++iter;
  }

  forceinline int
  SetVarUnknownValues::val(void) const {
    return iter.val();
  }

}

// STATISTICS: set-var

