/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
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

#include <gecode/int.hh>

namespace Gecode { namespace Set { namespace Channel {

  template<class View>
  template<class A>
  forceinline
  ChannelBool<View>::IndexAdvisor::IndexAdvisor(Space& home,
                                                ChannelBool<View>& p,
                                                Council<A>& c, int index)
    : Advisor(home,p,c), idx(index) {
    if (idx == -1)
      p.y.subscribe(home,*this);
    else {
      p.x[idx].subscribe(home,*this);
    }
  }

  template<class View>
  forceinline
  ChannelBool<View>::IndexAdvisor::IndexAdvisor(Space& home, bool share,
                                                IndexAdvisor& a)
    : Advisor(home,share,a), idx(a.idx) {}

  template<class View>
  forceinline int
  ChannelBool<View>::IndexAdvisor::index(void) const {
    return idx;
  }

  template<class View>
  template<class A>
  forceinline void
  ChannelBool<View>::IndexAdvisor::dispose(Space& home, Council<A>& c) {
    ChannelBool<View>& p = static_cast<ChannelBool<View>&>(propagator());
    if (idx == -1)
      p.y.cancel(home,*this);
    else {
      p.x[idx].cancel(home,*this);
    }
    Advisor::dispose(home,c);
  }

  template<class View>
  forceinline
  ChannelBool<View>::ChannelBool(Home home,
                                 ViewArray<Gecode::Int::BoolView>& x0,
                                 View y0)
    : Super(home,x0,y0), co(home), running(false) {
      bool assigned = false;
      for (int i=x.size(); i--;) {
        if (x[i].zero()) {
          assigned = true;
          SetDelta d;
          zeros.include(home, i, i, d);
        } else if (x[i].one()) {
          assigned = true;
          SetDelta d;
          ones.include(home, i, i, d);
        } else {
          (void) new (home) IndexAdvisor(home,*this,co,i);
        }
      }
      if (assigned)
        Gecode::Int::BoolView::schedule(home, *this, Gecode::Int::ME_BOOL_VAL);
      View::schedule(home, *this, y.assigned() ? ME_SET_VAL : ME_SET_BB);
      (void) new (home) IndexAdvisor(home,*this,co,-1);
    }

  template<class View>
  forceinline
  ChannelBool<View>::ChannelBool(Space& home, bool share, ChannelBool& p)
    : Super(home,share,p), running(false) {
    co.update(home, share, p.co);
  }

  template<class View>
  forceinline ExecStatus
  ChannelBool<View>::post(Home home, ViewArray<Gecode::Int::BoolView>& x,
                          View y) {
    GECODE_ME_CHECK(y.intersect(home, 0, x.size()-1));
    (void) new (home) ChannelBool(home,x,y);
    return ES_OK;
  }

  template<class View>
  PropCost
  ChannelBool<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::LO, x.size()+1);
  }

  template<class View>
  forceinline size_t
  ChannelBool<View>::dispose(Space& home) {
    co.dispose(home);
    (void) Super::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  Actor*
  ChannelBool<View>::copy(Space& home, bool share) {
    return new (home) ChannelBool(home,share,*this);
  }

  template<class View>
  ExecStatus
  ChannelBool<View>::propagate(Space& home, const ModEventDelta&) {
    running = true;
    if (zeros.size() > 0) {
      BndSetRanges zi(zeros);
      GECODE_ME_CHECK(y.excludeI(home, zi));
      zeros.init(home);
    }
    if (ones.size() > 0) {
      BndSetRanges oi(ones);
      GECODE_ME_CHECK(y.includeI(home, oi));
      ones.init(home);
    }
    running = false;

    if (delta.glbMin() != 1 || delta.glbMax() != 0) {
      if (!delta.glbAny()) {
        for (int i=delta.glbMin(); i<=delta.glbMax(); i++)
          GECODE_ME_CHECK(x[i].one(home));
      } else {
        GlbRanges<View> glb(y);
        for (Iter::Ranges::ToValues<GlbRanges<View> > gv(glb); gv(); ++gv) {
          GECODE_ME_CHECK(x[gv.val()].one(home));
        }
      }
    }
    if (delta.lubMin() != 1 || delta.lubMax() != 0) {
      if (!delta.lubAny()) {
        for (int i=delta.lubMin(); i<=delta.lubMax(); i++)
          GECODE_ME_CHECK(x[i].zero(home));
      } else {
        int cur = 0;
        for (LubRanges<View> lub(y); lub(); ++lub) {
          for (; cur < lub.min(); cur++) {
            GECODE_ME_CHECK(x[cur].zero(home));
          }
          cur = lub.max() + 1;
        }
        for (; cur < x.size(); cur++) {
          GECODE_ME_CHECK(x[cur].zero(home));
        }
      }
    }

    new (&delta) SetDelta();

    return y.assigned() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

  template<class View>
  ExecStatus
  ChannelBool<View>::advise(Space& home, Advisor& _a, const Delta& _d) {
    IndexAdvisor& a = static_cast<IndexAdvisor&>(_a);
    const SetDelta& d = static_cast<const SetDelta&>(_d);

    ModEvent me = View::modevent(d);
    int index = a.index();
    if ( (running && index == -1 && me != ME_SET_VAL)
         || (index == -1 && me == ME_SET_CARD) ) {
      return ES_OK;
    }

    if (index >= 0) {
      if (x[index].zero()) {
        SetDelta dummy;
        zeros.include(home, index, index, dummy);
      } else {
        assert(x[index].one());
        SetDelta dummy;
        ones.include(home, index, index, dummy);
      }
      return home.ES_NOFIX_DISPOSE( co, a);
    }

    if ((a.index() == -1) && Rel::testSetEventLB(me)) {
      if (d.glbAny()) {
        new (&delta)
          SetDelta(2,0, delta.lubMin(), delta.lubMax());
      } else {
        if (delta.glbMin() == 1 && delta.glbMax() == 0) {
          new (&delta)
            SetDelta(d.glbMin(), d.glbMax(),
                     delta.lubMin(), delta.lubMax());
        } else {
          if (delta.glbMin() != 2 || delta.glbMax() != 0) {
            if ((delta.glbMin() <= d.glbMin() && delta.glbMax() >= d.glbMin())
                ||
                (delta.glbMin() <= d.glbMax() && delta.glbMax() >= d.glbMax())
               ) {
                 new (&delta)
                   SetDelta(std::min(delta.glbMin(), d.glbMin()),
                            std::max(delta.glbMax(), d.glbMax()),
                            delta.lubMin(), delta.lubMax());
            } else {
              new (&delta)
                SetDelta(2, 0, delta.lubMin(), delta.lubMax());
            }
          }
        }
      }
    }
    if ((a.index() == -1) && Rel::testSetEventUB(me)) {
      if (d.lubAny()) {
        new (&delta)
          SetDelta(delta.glbMin(), delta.glbMax(), 2,0);
      } else {
        if (delta.lubMin() == 1 && delta.lubMax() == 0) {
          new (&delta)
            SetDelta(delta.glbMin(), delta.glbMax(),
                     d.lubMin(), d.lubMax());
        } else {
          if (delta.lubMin() != 2 || delta.lubMax() != 0) {
            if ((delta.lubMin() <= d.lubMin() && delta.lubMax() >= d.lubMin())
                ||
                (delta.lubMin() <= d.lubMax() && delta.lubMax() >= d.lubMax())
               ) {
                 new (&delta)
                   SetDelta(delta.lubMin(), delta.lubMax(),
                            std::min(delta.lubMin(), d.lubMin()),
                            std::max(delta.lubMax(), d.lubMax())
                            );
            } else {
              new (&delta)
                SetDelta(delta.glbMin(), delta.glbMax(), 2, 0);
            }
          }
        }
      }
    }

    if (y.assigned())
      return home.ES_NOFIX_DISPOSE( co, a);
    else
      return ES_NOFIX;
  }

}}}

// STATISTICS: set-prop
