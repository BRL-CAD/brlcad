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

#include <gecode/int.hh>

namespace Gecode { namespace Set { namespace Channel {

  template<class View>
  forceinline
  ChannelInt<View>::ChannelInt(Home home,
                               ViewArray<Gecode::Int::CachedView<
                                Gecode::Int::IntView> >& xs0,
                               ViewArray<CachedView<View> >& ys0)
    : Propagator(home), xs(xs0), ys(ys0) {
    for (int i=xs.size(); i--;)
      xs[i].initCache(home,IntSet(0,ys.size()-1));
    for (int i=ys.size(); i--;)
      ys[i].initCache(home,IntSet::empty,IntSet(0,xs.size()-1));
    xs.subscribe(home,*this, Gecode::Int::PC_INT_DOM);
    ys.subscribe(home,*this, PC_SET_ANY);
  }

  template<class View>
  forceinline
  ChannelInt<View>::ChannelInt(Space& home, bool share, ChannelInt& p)
    : Propagator(home,share,p) {
    xs.update(home,share,p.xs);
    ys.update(home,share,p.ys);
  }

  template<class View>
  forceinline ExecStatus
  ChannelInt<View>::post(Home home,
                         ViewArray<Gecode::Int::CachedView<
                          Gecode::Int::IntView> >& xs,
                         ViewArray<CachedView<View> >& ys) {
    // Sharing of ys is taken care of in the propagator:
    // The ys are propagated to be disjoint, so shared variables
    // result in failure.
    int xssize = xs.size();
    for (int i=ys.size(); i--;) {
      GECODE_ME_CHECK(ys[i].exclude(home, xssize, Limits::max));
      GECODE_ME_CHECK(ys[i].exclude(home, Limits::min, -1));
    }
    int yssize = ys.size();
    if (yssize > Gecode::Int::Limits::max)
      return ES_FAILED;
    for (int i=xs.size(); i--;) {
      GECODE_ME_CHECK(xs[i].gq(home, 0));
      GECODE_ME_CHECK(xs[i].le(home, static_cast<int>(yssize)));
    }

    (void) new (home) ChannelInt(home,xs,ys);
    return ES_OK;
  }

  template<class View>
  PropCost
  ChannelInt<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::LO, xs.size()+ys.size());
  }

  template<class View>
  forceinline size_t
  ChannelInt<View>::dispose(Space& home) {
    xs.cancel(home,*this, Gecode::Int::PC_INT_DOM);
    ys.cancel(home,*this, PC_SET_ANY);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  Actor*
  ChannelInt<View>::copy(Space& home, bool share) {
    return new (home) ChannelInt(home,share,*this);
  }

  template<class View>
  ExecStatus
  ChannelInt<View>::propagate(Space& home, const ModEventDelta&) {
    int assigned = 0;
    for (int v=xs.size(); v--;) {
      if (xs[v].assigned()) {
        assigned++;
        if (xs[v].modified())
          GECODE_ME_CHECK(ys[xs[v].val()].include(home,v));
      }
      if (xs[v].modified()) {
        Gecode::Int::ViewDiffRanges<Gecode::Int::IntView> d(xs[v]);
        Iter::Ranges::ToValues<Gecode::Int::ViewDiffRanges<
          Gecode::Int::IntView> > dv(d);
        for (; dv(); ++dv)
          GECODE_ME_CHECK(ys[dv.val()].exclude(home, v));
        xs[v].cache(home);
      }
    }

    for (int i=ys.size(); i--;) {
      if (ys[i].glbModified()) {
        GlbDiffRanges<View> yilb(ys[i]);
        Iter::Ranges::ToValues<GlbDiffRanges<View> > dv(yilb);
        for (;dv();++dv)
          GECODE_ME_CHECK(xs[dv.val()].eq(home,i));
        ys[i].cacheGlb(home);
      }
      if (ys[i].lubModified()) {
        LubDiffRanges<View> yiub(ys[i]);
        Iter::Ranges::ToValues<LubDiffRanges<View> > dv(yiub);
        for (;dv();++dv)
          GECODE_ME_CHECK(xs[dv.val()].nq(home,i));
        ys[i].cacheLub(home);
      }
    }

    return (assigned==xs.size()) ? home.ES_SUBSUMED(*this) : ES_NOFIX;
  }

}}}

// STATISTICS: set-prop
