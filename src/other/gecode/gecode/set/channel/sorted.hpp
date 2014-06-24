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



#include <gecode/set.hh>
#include <gecode/int.hh>
#include <gecode/set/rel.hh>

namespace Gecode { namespace Set { namespace Channel {

  template<class View>
  forceinline
  ChannelSorted<View>::ChannelSorted(Home home, View y0,
                                     ViewArray< Gecode::Int::IntView >& ys)
    : Propagator(home), x0(y0), xs(ys) {
    x0.subscribe(home,*this, PC_SET_ANY);
    xs.subscribe(home,*this, Gecode::Int::PC_INT_BND);
  }

  template<class View>
  forceinline
  ChannelSorted<View>::ChannelSorted(Space& home, bool share, ChannelSorted& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    xs.update(home,share,p.xs);
  }

  template<class View>
  forceinline ExecStatus
  ChannelSorted<View>::post(Home home, View x0,
                            ViewArray<Gecode::Int::IntView>& xs) {
    unsigned int xs_size = static_cast<unsigned int>(xs.size());
    GECODE_ME_CHECK(x0.cardMin(home,xs_size));
    GECODE_ME_CHECK(x0.cardMax(home,xs_size));
    if (xs_size == 1) {
      SingletonView sv(xs[0]);
      GECODE_ES_CHECK((Rel::Eq<View,
                       SingletonView>::post(home,x0, sv)));
    } else {
      // Sharing in xs is handled correctly in the propagator:
      // if two views in xs are shared, this leads to failure.
      (void) new (home) ChannelSorted(home,x0,xs);
    }
    return ES_OK;
  }

  template<class View>
  PropCost
  ChannelSorted<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO, xs.size()+1);
  }

  template<class View>
  forceinline size_t
  ChannelSorted<View>::dispose(Space& home) {
    x0.cancel(home,*this, PC_SET_ANY);
    xs.cancel(home,*this, Gecode::Int::PC_INT_BND);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  Actor*
  ChannelSorted<View>::copy(Space& home, bool share) {
    return new (home) ChannelSorted(home,share,*this);
  }

  template<class View>
  ExecStatus
  ChannelSorted<View>::propagate(Space& home, const ModEventDelta&) {

    int xs_size = xs.size();

    bool loopFlag;

    do {
      loopFlag = false;

      // Order int vars in xs
      GECODE_ME_CHECK(xs[0].gq(home,x0.lubMin()));
      for (int i=xs_size-1; i--; ) {
        GECODE_ME_CHECK_MODIFIED(loopFlag, xs[i+1].gq(home,xs[i].min() + 1));
      }

      GECODE_ME_CHECK_MODIFIED(loopFlag, xs[xs_size-1].lq(home,x0.lubMax()));
      for (int i=xs_size-2; i--; ) {
        GECODE_ME_CHECK_MODIFIED(loopFlag, xs[i].lq(home,xs[i+1].max() - 1));
      }

      // if y from xs is assigned, add to glb(x0)
      for (int i=xs_size; i--; ) {
        if (xs[i].assigned()) {
          GECODE_ME_CHECK_MODIFIED(loopFlag, x0.include(home,xs[i].val()));
        }
      }

      // intersect every y in xs with lub(x0)
      for (int i=xs_size; i--; ) {
        LubRanges<View> ub(x0);
        GECODE_ME_CHECK_MODIFIED(loopFlag, xs[i].inter_r(home,ub,false));
      }

      // remove gaps between vars in xs from lub(x0)
      GECODE_ME_CHECK_MODIFIED(loopFlag,
                        x0.exclude(home,Limits::min,xs[0].min()-1));
      GECODE_ME_CHECK_MODIFIED(loopFlag,
                        x0.exclude(home,xs[xs_size-1].max()+1,
                                   Limits::max));

      for (int i=xs_size-1; i--; ) {
        int start = xs[i].max() + 1;
        int end   = xs[i+1].min() - 1;
        if (start<=end) {
          GECODE_ME_CHECK_MODIFIED(loopFlag, x0.exclude(home,start,end));
        }
      }

      // try to assign vars in xs from glb(x0)
      if (x0.glbSize()>0) {

        LubRanges<View> ub(x0);
        Iter::Ranges::ToValues<LubRanges<View> > ubv(ub);
        GlbRanges<View> lb(x0);
        Iter::Ranges::ToValues<GlbRanges<View> > lbv(lb);

        int i=0;
        for (; ubv() && lbv() && ubv.val()==lbv.val();
            ++ubv, ++lbv, i++) {
          GECODE_ME_CHECK_MODIFIED(loopFlag, xs[i].eq(home,lbv.val()));
        }

        if (i<xs_size-1 && x0.lubMax()==x0.glbMax()) {
          LubRanges<View> lbx0(x0);
          GlbRanges<View> ubx0(x0);
          Iter::Ranges::Inter<LubRanges<View>,GlbRanges<View> >
            inter(lbx0, ubx0);

          int to = x0.glbMax();
          int from = to;
          while (inter()) {
            from = inter.min();
            ++inter;
          }

          int i=xs_size-1;
          for (int j=to; j>=from;j--,i--) {
            GECODE_ME_CHECK_MODIFIED(loopFlag, xs[i].eq(home,j));
          }
        }
      }

    } while (loopFlag);

    for (int i=xs_size; i--; )
      if (!xs[i].assigned())
        return ES_FIX;
    return home.ES_SUBSUMED(*this);
  }

}}}

// STATISTICS: set-prop
