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
  ValCommitInc::ValCommitInc(Space& home, const ValBranch& vb)
    : ValCommit<SetView,int>(home,vb) {}
  forceinline
  ValCommitInc::ValCommitInc(Space& home, bool shared, ValCommitInc& vc)
    : ValCommit<SetView,int>(home,shared,vc) {}
  forceinline ModEvent
  ValCommitInc::commit(Space& home, unsigned int a, SetView x, int, int n) {
    return (a == 0) ? x.include(home,n) : x.exclude(home,n);
  }
  forceinline NGL*
  ValCommitInc::ngl(Space& home, unsigned int a, SetView x, int n) const {
    if (a == 0)
      return new (home) IncNGL(home,x,n);
    else
      return NULL;
  }
  forceinline void
  ValCommitInc::print(const Space&, unsigned int a, SetView, int i, int n,
                      std::ostream& o) const {
    o << "var[" << i << "]." 
      << ((a == 0) ? "include" : "exclude") << "(" << n << ")";
  }

  forceinline
  ValCommitExc::ValCommitExc(Space& home, const ValBranch& vb)
    : ValCommit<SetView,int>(home,vb) {}
  forceinline
  ValCommitExc::ValCommitExc(Space& home, bool shared, ValCommitExc& vc)
    : ValCommit<SetView,int>(home,shared,vc) {}
  forceinline ModEvent
  ValCommitExc::commit(Space& home, unsigned int a, SetView x, int, int n) {
    return (a == 0) ? x.exclude(home,n) : x.include(home,n);
  }
  forceinline NGL*
  ValCommitExc::ngl(Space& home, unsigned int a, SetView x, int n) const {
    if (a == 0)
      return new (home) ExcNGL(home,x,n);
    else
      return NULL;
  }
  forceinline void
  ValCommitExc::print(const Space&, unsigned int a, SetView, int i, int n,
                      std::ostream& o) const {
    o << "var[" << i << "]." 
      << ((a == 0) ? "exclude" : "include") << "(" << n << ")";
  }

}}}

// STATISTICS: set-branch

