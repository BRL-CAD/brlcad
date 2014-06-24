/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2012
 *     Gabor Szokoli, 2004
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

namespace Gecode { namespace Set { namespace Branch {

  forceinline
  ValSelMin::ValSelMin(Space& home, const ValBranch& vb) 
    : ValSel<SetView,int>(home,vb) {}
  forceinline
  ValSelMin::ValSelMin(Space& home, bool shared, ValSelMin& vs)
    : ValSel<SetView,int>(home,shared,vs) {}
  forceinline int 
  ValSelMin::val(const Space&, SetView x, int) {
    UnknownRanges<SetView> u(x);
    return u.min();
  }

  forceinline
  ValSelMax::ValSelMax(Space& home, const ValBranch& vb) 
    : ValSel<SetView,int>(home,vb) {}
  forceinline
  ValSelMax::ValSelMax(Space& home, bool shared, ValSelMax& vs)
    : ValSel<SetView,int>(home,shared,vs) {}
  forceinline int 
  ValSelMax::val(const Space&, SetView x, int) {
    int max = 0;
    for (UnknownRanges<SetView> u(x); u(); ++u)
      max = u.max();
    return max;
  }

  forceinline
  ValSelMed::ValSelMed(Space& home, const ValBranch& vb) 
    : ValSel<SetView,int>(home,vb) {}
  forceinline
  ValSelMed::ValSelMed(Space& home, bool shared, ValSelMed& vs)
    : ValSel<SetView,int>(home,shared,vs) {}
  forceinline int 
  ValSelMed::val(const Space&, SetView x, int) {
    UnknownRanges<SetView> u1(x);
    unsigned int i = Iter::Ranges::size(u1) / 2;
    UnknownRanges<SetView> u2(x);
    int med = (u2.min()+u2.max()) / 2;
    ++u2;
    if (!u2()) {
      return med;
    }
    UnknownRanges<SetView> u3(x);
    while (i >= u3.width()) {
      i -= u3.width();
      ++u3;
    }
    return u3.min() + static_cast<int>(i);
  }

  forceinline
  ValSelRnd::ValSelRnd(Space& home, const ValBranch& vb) 
    : ValSel<SetView,int>(home,vb), r(vb.rnd()) {}
  forceinline
  ValSelRnd::ValSelRnd(Space& home, bool shared, ValSelRnd& vs)
    : ValSel<SetView,int>(home,shared,vs) {
    r.update(home,shared,vs.r);
  }
  forceinline int
  ValSelRnd::val(const Space&, SetView x, int) {
    UnknownRanges<SetView> u(x);
    unsigned int p = r(Iter::Ranges::size(u));
    for (UnknownRanges<SetView> i(x); i(); ++i) {
      if (i.width() > p)
        return i.min() + static_cast<int>(p);
      p -= i.width();
    }
    GECODE_NEVER;
    return 0;
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

// STATISTICS: set-branch

