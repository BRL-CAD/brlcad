/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christopher Mears <Chris.Mears@monash.edu>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christopher Mears, 2011
 *     Christian Schulte, 2011
 *     Guido Tack, 2011
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

#include <gecode/set/precede.hh>

namespace Gecode {

  void
  precede(Home home, const SetVarArgs& x, int s, int t) {
    using namespace Set;
    Limits::check(s,"Set::precede");
    Limits::check(t,"Set::precede");
    if (home.failed()) return;

    ViewArray<SetView> y(home, x);
    GECODE_ES_FAIL(Precede::Single<SetView>::post(home, y, s, t));
  }

  void
  precede(Home home, const SetVarArgs& x, const IntArgs& c) {
    using namespace Set;
    if (c.size() < 2)
      return;
    for (int i=c.size(); i--; )
      Limits::check(c[i],"Set::precede");
    if (home.failed()) return;

    for (int i=c.size()-1; i--; ) {
      ViewArray<SetView> y(home, x);
      GECODE_ES_FAIL(Precede::Single<SetView>::post(home, y, c[i], c[i+1]));
    }
  }

}

// STATISTICS: set-post
