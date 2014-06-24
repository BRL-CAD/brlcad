/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
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

namespace Gecode { namespace Float { namespace Branch {

  forceinline
  ValCommitLqGq::ValCommitLqGq(Space& home, const ValBranch& vb)
    : ValCommit<FloatView,FloatVal>(home,vb) {}
  forceinline
  ValCommitLqGq::ValCommitLqGq(Space& home, bool shared, ValCommitLqGq& vc)
    : ValCommit<FloatView,FloatVal>(home,shared,vc) {}
  forceinline ModEvent
  ValCommitLqGq::commit(Space& home, unsigned int a, FloatView x, int, 
                        FloatNumBranch nl) {
    // Should we try the smaller half first?
    if ((a == 0) == nl.l) {
      if ((x.min() == nl.n) || (x.max() == nl.n)) 
        return x.eq(home,x.min());
      else 
        return x.lq(home,nl.n);
    } else {
      if ((x.min() == nl.n) || (x.max() == nl.n))
        return x.eq(home,x.max());
      else 
        return x.gq(home,nl.n);
    }
  }
  forceinline NGL* 
  ValCommitLqGq::ngl(Space&, unsigned int, FloatView, FloatNumBranch) const {
    return NULL;
  }
  forceinline void
  ValCommitLqGq::print(const Space&, unsigned int a, FloatView, int i, 
                       FloatNumBranch nl,
                       std::ostream& o) const {
    o << "var[" << i << "] " 
      << (((a == 0) == nl.l) ? "<=" : ">=") << "(" << nl.n << ")";
  }

}}}

// STATISTICS: float-branch

