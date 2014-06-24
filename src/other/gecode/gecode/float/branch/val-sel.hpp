/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2012
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

namespace Gecode {

  forceinline Archive&
  operator <<(Archive& e, FloatNumBranch nl) {
    return e << nl.n << nl.l;
  }

  forceinline Archive&
  operator >>(Archive& e, FloatNumBranch& nl) {
    return e >> nl.n >> nl.l;
  }

}

namespace Gecode { namespace Float { namespace Branch {

  forceinline
  ValSelLq::ValSelLq(Space& home, const ValBranch& vb) 
    : ValSel<FloatView,FloatNumBranch>(home,vb) {}
  forceinline
  ValSelLq::ValSelLq(Space& home, bool shared, ValSelLq& vs)
    : ValSel<FloatView,FloatNumBranch>(home,shared,vs) {}
  forceinline FloatNumBranch
  ValSelLq::val(const Space&, FloatView x, int) {
    FloatNumBranch nl;
    nl.n = x.med(); nl.l = true;
    return nl;
  }

  forceinline
  ValSelGq::ValSelGq(Space& home, const ValBranch& vb) 
    : ValSel<FloatView,FloatNumBranch>(home,vb) {}
  forceinline
  ValSelGq::ValSelGq(Space& home, bool shared, ValSelGq& vs)
    : ValSel<FloatView,FloatNumBranch>(home,shared,vs) {}
  forceinline FloatNumBranch
  ValSelGq::val(const Space&, FloatView x, int) {
    FloatNumBranch nl;
    nl.n = x.med(); nl.l = false;
    return nl;
  }

  forceinline
  ValSelRnd::ValSelRnd(Space& home, const ValBranch& vb) 
    : ValSel<FloatView,FloatNumBranch>(home,vb), r(vb.rnd()) {}
  forceinline
  ValSelRnd::ValSelRnd(Space& home, bool shared, ValSelRnd& vs)
    : ValSel<FloatView,FloatNumBranch>(home,shared,vs) {
    r.update(home,shared,vs.r);
  }
  forceinline FloatNumBranch
  ValSelRnd::val(const Space&, FloatView x, int) {
    FloatNumBranch nl;
    nl.n = x.med(); nl.l = (r(2U) == 0U);
    return nl;
  }
  forceinline bool
  ValSelRnd::notice(void) const {
    return true;
  }
  forceinline void
  ValSelRnd::dispose(Space&) {
    r.~Rnd();
  }

}}}

// STATISTICS: float-branch

