/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Denys Duchier <denys.duchier@univ-orleans.fr>
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Denys Duchier, 2011
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
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

#include <gecode/set.hh>

#include <gecode/set/channel.hh>

namespace Gecode {

  void
  channelSorted(Home home, const IntVarArgs& x, SetVar y) {
    if (home.failed()) return;
    ViewArray<Int::IntView> xa(home,x);
    GECODE_ES_FAIL(Set::Channel::ChannelSorted<Set::SetView>::post(home,y,xa));
  }

  void
  channel(Home home, const IntVarArgs& x, const SetVarArgs& y) {
    if (home.failed()) return;
    ViewArray<Int::CachedView<Int::IntView> > xa(home,x.size());
    for (int i=x.size(); i--;)
      new (&xa[i]) Int::CachedView<Int::IntView>(x[i]);
    ViewArray<Set::CachedView<Set::SetView> > ya(home,y.size());
    for (int i=y.size(); i--;)
      new (&ya[i]) Set::CachedView<Set::SetView>(y[i]);
    GECODE_ES_FAIL((Set::Channel::ChannelInt<Set::SetView>::post(home,xa,ya)));
  }

  void
  channel(Home home, const BoolVarArgs& x, SetVar y) {
    if (home.failed()) return;
    ViewArray<Int::BoolView> xv(home,x);
    GECODE_ES_FAIL((Set::Channel::ChannelBool<Set::SetView>
                         ::post(home,xv,y)));
  }

  void
  channel(Home home, const SetVarArgs& x, const SetVarArgs& y)
  {
    if (home.failed()) return;
    ViewArray<Set::CachedView<Set::SetView> > xa(home, x.size());
    for (int i=x.size(); i--;)
      new (&xa[i]) Int::CachedView<Set::SetView>(x[i]);
    ViewArray<Set::CachedView<Set::SetView> > ya(home, y.size());
    for (int i=y.size(); i--;)
      new (&ya[i]) Set::CachedView<Set::SetView>(y[i]);
    GECODE_ES_FAIL((Set::Channel::ChannelSet<Set::SetView>::post(home,xa,ya)));
  }

}

// STATISTICS: set-post
