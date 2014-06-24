/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Denys Duchier <denys.duchier@univ-orleans.fr>
 *
 *  Copyright:
 *     Denys Duchier, 2011
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

namespace Gecode { namespace Set { namespace Channel {

  template <typename View>
  forceinline
  ChannelSet<View>::ChannelSet(Home home,
                               ViewArray<CachedView<View> >& xs0,
                               ViewArray<CachedView<View> >& ys0)
    : Propagator(home), xs(xs0), ys(ys0)
  {
    for (int i=xs.size(); i--;)
      xs[i].initCache(home,IntSet::empty,IntSet(0,ys.size()-1));
    for (int i=ys.size(); i--;)
      ys[i].initCache(home,IntSet::empty,IntSet(0,xs.size()-1));
    xs.subscribe(home,*this, PC_SET_ANY);
    ys.subscribe(home,*this, PC_SET_ANY);
  }

  template <typename View>
  forceinline
  ChannelSet<View>::ChannelSet(Space& home, bool share, ChannelSet& p)
    : Propagator(home, share, p)
  {
    xs.update(home, share, p.xs);
    ys.update(home, share, p.ys);
  }

  template <typename View>
  forceinline ExecStatus
  ChannelSet<View>::post(Home home,
                         ViewArray<CachedView<View> >& xs,
                         ViewArray<CachedView<View> >& ys)
  {
    int xssize = xs.size();
    for (int i=ys.size(); i--;)
      {
        GECODE_ME_CHECK(ys[i].exclude(home, xssize, Limits::max));
        GECODE_ME_CHECK(ys[i].exclude(home, Limits::min, -1));
      }
    int yssize = ys.size();
    for (int i=xs.size(); i--;)
      {
        GECODE_ME_CHECK(xs[i].exclude(home, yssize, Limits::max));
        GECODE_ME_CHECK(xs[i].exclude(home, Limits::min, -1));
      }
    (void) new (home) ChannelSet(home,xs,ys);
    return ES_OK;
  }

  template <typename View>
  PropCost
  ChannelSet<View>::cost(const Space&, const ModEventDelta&) const
  {
    return PropCost::quadratic(PropCost::HI, xs.size()+ys.size());
  }

  template <typename View>
  forceinline size_t
  ChannelSet<View>::dispose(Space& home)
  {
    xs.cancel(home, *this, PC_SET_ANY);
    ys.cancel(home, *this, PC_SET_ANY);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template <typename View>
  Actor*
  ChannelSet<View>::copy(Space& home, bool share)
  {
    return new (home) ChannelSet(home,share,*this);
  }

  template <typename View>
  ExecStatus
  ChannelSet<View>::propagate(Space& home, const ModEventDelta&)
  {
    int assigned = 0;
    bool again = true;
    while (again)
      {
        assigned = 0;
        again = false;
        for (int i=xs.size(); i--;)
          {
            if (xs[i].assigned())
              ++assigned;
            if (xs[i].glbModified())
              {
                GlbDiffRanges<View> xilb(xs[i]);
                Iter::Ranges::ToValues<GlbDiffRanges<View> > dv(xilb);
                for (;dv();++dv)
                  GECODE_ME_CHECK(ys[dv.val()].include(home,i));
                xs[i].cacheGlb(home);
                again = true;
              }
            if (xs[i].lubModified())
              {
                LubDiffRanges<View> xiub(xs[i]);
                Iter::Ranges::ToValues<LubDiffRanges<View> > dv(xiub);
                for (;dv();++dv)
                  GECODE_ME_CHECK(ys[dv.val()].exclude(home,i));
                xs[i].cacheLub(home);
                again = true;
              }
          }
        for (int i=ys.size(); i--;)
          {
            if (ys[i].assigned())
              ++assigned;
            if (ys[i].glbModified())
              {
                GlbDiffRanges<View> yilb(ys[i]);
                Iter::Ranges::ToValues<GlbDiffRanges<View> > dv(yilb);
                for (;dv();++dv)
                  GECODE_ME_CHECK(xs[dv.val()].include(home,i));
                ys[i].cacheGlb(home);
                again = true;
              }
            if (ys[i].lubModified())
              {
                LubDiffRanges<View> yiub(ys[i]);
                Iter::Ranges::ToValues<LubDiffRanges<View> > dv(yiub);
                for (;dv();++dv)
                  GECODE_ME_CHECK(xs[dv.val()].exclude(home,i));
                ys[i].cacheLub(home);
                again = true;
              }
          }
      }

    return (assigned == xs.size()+ys.size()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }
}}}

// STATISTICS: set-prop
