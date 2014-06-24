/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2013
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

#include <gecode/set/branch.hh>

namespace Gecode { namespace Set { namespace Branch {

  NGL*
  IncNGL::copy(Space& home, bool share) {
    return new (home) IncNGL(home,share,*this);
  }
  NGL::Status
  IncNGL::status(const Space&) const {
    // Is n in the glb(x)?
    if (x.contains(n))
      return NGL::SUBSUMED;
    else
      // Is n not in the lub(x)?
      return x.notContains(n) ? NGL::FAILED : NGL::NONE;
  }
  ExecStatus
  IncNGL::prune(Space& home) {
    return me_failed(x.exclude(home,n)) ? ES_FAILED : ES_OK;
  }


  NGL*
  ExcNGL::copy(Space& home, bool share) {
    return new (home) ExcNGL(home,share,*this);
  }
  NGL::Status
  ExcNGL::status(const Space&) const {
    // Is n not in the lub(x)?
    if (x.notContains(n))
      return NGL::SUBSUMED;
    else
      // Is n in the lub(x)?
      return x.contains(n) ? NGL::FAILED : NGL::NONE;
  }
  ExecStatus
  ExcNGL::prune(Space& home) {
    return me_failed(x.include(home,n)) ? ES_FAILED : ES_OK;
  }

}}}

// STATISTICS: set-branch
