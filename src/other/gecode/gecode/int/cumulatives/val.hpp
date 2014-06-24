/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
 *     Mikael Lagerkvist, 2005
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

#include <algorithm>

/*
 * This is the propagation algorithm of the cumulatives constraint as
 * presented in:
 *   Nicolas Beldiceanu and Mats Carlsson, A New Multi-resource cumulatives
 *   Constraint with Negative Heights. CP 2002, pages 63-79, Springer-Verlag.
 */

namespace Gecode { namespace Int { namespace Cumulatives {

  template<class ViewM, class ViewP, class ViewU, class View>
  forceinline
  Val<ViewM,ViewP,ViewU,View>::Val(Home home,
                                   const ViewArray<ViewM>& _m,
                                   const ViewArray<View>& _s,
                                   const ViewArray<ViewP>& _p,
                                   const ViewArray<View>& _e,
                                   const ViewArray<ViewU>& _u,
                                   const SharedArray<int>& _c,
                                   bool _at_most) :
    Propagator(home),
    m(_m), s(_s), p(_p), e(_e), u(_u), c(_c), at_most(_at_most) {
    home.notice(*this,AP_DISPOSE);

    m.subscribe(home,*this,Int::PC_INT_DOM);
    s.subscribe(home,*this,Int::PC_INT_BND);
    p.subscribe(home,*this,Int::PC_INT_BND);
    e.subscribe(home,*this,Int::PC_INT_BND);
    u.subscribe(home,*this,Int::PC_INT_BND);
  }

  template<class ViewM, class ViewP, class ViewU, class View>
  ExecStatus
  Val<ViewM,ViewP,ViewU,View>
  ::post(Home home, const ViewArray<ViewM>& m,
         const ViewArray<View>& s, const ViewArray<ViewP>& p,
         const ViewArray<View>& e, const ViewArray<ViewU>& u,
         const SharedArray<int>& c, bool at_most) {
    (void) new (home) Val(home, m,s,p,e,u,c,at_most);
    return ES_OK;
  }

  template<class ViewM, class ViewP, class ViewU, class View>
  forceinline
  Val<ViewM,ViewP,ViewU,View>::Val(Space& home, bool share,
                                   Val<ViewM,ViewP,ViewU,View>& vp)
    : Propagator(home,share,vp), at_most(vp.at_most) {
    m.update(home,share,vp.m);
    s.update(home, share, vp.s);
    p.update(home, share, vp.p);
    e.update(home, share, vp.e);
    u.update(home, share, vp.u);
    c.update(home, share, vp.c);
  }

  template<class ViewM, class ViewP, class ViewU, class View>
  size_t
  Val<ViewM,ViewP,ViewU,View>::dispose(Space& home) {
    home.ignore(*this,AP_DISPOSE);
    if (!home.failed()) {
      m.cancel(home,*this,Int::PC_INT_DOM);
      s.cancel(home,*this,Int::PC_INT_BND);
      p.cancel(home,*this,Int::PC_INT_BND);
      e.cancel(home,*this,Int::PC_INT_BND);
      u.cancel(home,*this,Int::PC_INT_BND);
    }
    c.~SharedArray();
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class ViewM, class ViewP, class ViewU, class View>
  PropCost
  Val<ViewM,ViewP,ViewU,View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::LO, s.size());
  }

  template<class ViewM, class ViewP, class ViewU, class View>
  Actor*
  Val<ViewM,ViewP,ViewU,View>::copy(Space& home, bool share) {
    return new (home) Val<ViewM,ViewP,ViewU,View>(home,share,*this);
  }

  /// Types of events for the sweep-line
  typedef enum {EVENT_CHCK, EVENT_PROF, EVENT_PRUN} ev_t;
  /// An event collects the information for one evnet for the sweep-line
  class Event
  {
  public:
    /// The type of the event
    ev_t e;
    /// The task this event refers to
    int task;
    /// The date of this event
    int date;
    /// The quantity changed by this event (if any)
    int inc;
    /** If the type is EVENT_PROF and it is the first of the pair,
     * this value is true. Added to handle contribution-values
     * correctly for both at_most and at_least.
     */
    bool first_prof;

    /// Simple constructor
    Event(ev_t _e, int _task, int _date, int _inc = 0, bool _first_prof = false)
      : e(_e), task(_task), date(_date), inc(_inc), first_prof(_first_prof)
    {}

    // Default constructor for region-allocated memory
    Event(void) {}

    /// Order events based on date.
    bool operator <(const Event& ev) const {
      if (date == ev.date) {
        if (e == EVENT_PROF && ev.e != EVENT_PROF) return true;
        if (e == EVENT_CHCK && ev.e == EVENT_PRUN) return true;
        return false;
      }
      return date < ev.date;
    }
  };

  template<class ViewM, class ViewP, class ViewU, class View>
  ExecStatus
  Val<ViewM,ViewP,ViewU,View>::prune(Space& home, int low, int up, int r,
                                     int ntask, int su,
                                     int* contribution,
                                     int* prune_tasks, int& prune_tasks_size) {

    if (ntask > 0 && (at_most ? su > c[r] : su < c[r])) {
      return ES_FAILED;
    }

    int pti = 0;
    while (pti != prune_tasks_size) {
      int t = prune_tasks[pti];

      // Algorithm 2.
      // Prune the machine, start, and end for required
      // tasks for machine r that have heights possibly greater than 0.
      if (ntask != 0 &&
          (at_most ? u[t].min() < 0
           : u[t].max() > 0) &&
          (at_most ? su-contribution[t] > c[r]
           : su-contribution[t] < c[r])) {
        if (me_failed(m[t].eq(home, r))||
            me_failed(s[t].gq(home, up-p[t].max()+1))||
            me_failed(s[t].lq(home, low))||
            me_failed(e[t].lq(home,low+p[t].max()))||
            me_failed(e[t].gq(home, up+1))||
            me_failed(p[t].gq(home,std::min(up-s[t].max()+1,e[t].min()-low)))
            ) {
          return ES_FAILED;
        }
      }

      // Algorithm 3.
      // Remove values that prevent us from reaching the limit
      if (at_most ? u[t].min() > std::min(0, c[r])
          : u[t].max() < std::max(0, c[r])) {
        if (at_most ? su-contribution[t]+u[t].min() > c[r]
            : su-contribution[t]+u[t].max() < c[r]) {
          if (e[t].min() > low  &&
              s[t].max() <= up &&
              p[t].min() > 0) {
            if (me_failed(m[t].nq(home, r))) {
              return ES_FAILED;
            }
          } else if (m[t].assigned()) {
            int ptmin = p[t].min();
            if (ptmin > 0) {
              Iter::Ranges::Singleton
                a(low-ptmin+1, up), b(low+1, up+ptmin);
              if (me_failed(s[t].minus_r(home,a,false)) ||
                  me_failed(e[t].minus_r(home, b,false)))
                return ES_FAILED;
            }
            if (me_failed(p[t].lq(home,std::max(std::max(low-s[t].min(),
                                                         e[t].max()-up-1),
                                                0)))) {
              return ES_FAILED;
            }
          }
        }
      }

      // Algorithm 4.
      // Remove bad negative values from for-sure heights.
      /* \note "It is not sufficient to only test for assignment of machine[t]
       *         since Algorithm 3 can remove the value." Check this against
       *         the conditions for Alg3.
       */
      if (m[t].assigned() &&
          m[t].val() == r &&
          e[t].min() > low    &&
          s[t].max() <= up  &&
          p[t].min() > 0 ) {
        if (me_failed(at_most
                      ? u[t].lq(home, c[r]-su+contribution[t])
                      : u[t].gq(home, c[r]-su+contribution[t]))) {
          return ES_FAILED;
        }
      }

      // Remove tasks that are no longer relevant.
      if (!m[t].in(r) || (e[t].max() <= up+1)) {
        prune_tasks[pti] = prune_tasks[--prune_tasks_size];
      } else {
        ++pti;
      }
    }

    return ES_OK;
  }

  namespace {
    template<class C>
    class Less {
    public:
      bool operator ()(const C& lhs, const C& rhs) {
        return lhs < rhs;
      }
    };
  }

  template<class ViewM, class ViewP, class ViewU, class View>
  ExecStatus
  Val<ViewM,ViewP,ViewU,View>::propagate(Space& home, const ModEventDelta&) {
    // Check for subsumption
    bool subsumed = true;
    for (int t = s.size(); t--; )
      if (!(p[t].assigned() && e[t].assigned()   &&
            m[t].assigned()  && s[t].assigned() &&
            u[t].assigned())) {
        subsumed = false;
        break;
      }
    // Propagate information for machine r
    Region region(home);
    Event *events = region.alloc<Event>(s.size()*8);
    int  events_size;
    int *prune_tasks = region.alloc<int>(s.size());
    int  prune_tasks_size;
    int *contribution = region.alloc<int>(s.size());
    for (int r = c.size(); r--; ) {
      events_size = 0;
#define GECODE_PUSH_EVENTS(E) assert(events_size < s.size()*8);     \
        events[events_size++] = E

      // Find events for sweep-line
      for (int t = s.size(); t--; ) {
        if (m[t].assigned() &&
            m[t].val() == r &&
            s[t].max() < e[t].min()) {
          if (at_most
              ? u[t].min() > std::min(0, c[r])
              : u[t].max() < std::max(0, c[r])) {
            GECODE_PUSH_EVENTS(Event(EVENT_CHCK, t, s[t].max(), 1));
            GECODE_PUSH_EVENTS(Event(EVENT_CHCK, t, e[t].min(), -1));
          }
          if (at_most
              ? u[t].min() > 0
              : u[t].max() < 0) {
            GECODE_PUSH_EVENTS(Event(EVENT_PROF, t, s[t].max(),
                                     at_most ? u[t].min() : u[t].max(), true));
            GECODE_PUSH_EVENTS(Event(EVENT_PROF, t, e[t].min(),
                                     at_most ? -u[t].min() : -u[t].max()));
          }
        }

        if (m[t].in(r)) {
          if (at_most
              ? u[t].min() < 0
              : u[t].max() > 0) {
            GECODE_PUSH_EVENTS(Event(EVENT_PROF, t, s[t].min(),
                                     at_most ? u[t].min() : u[t].max(), true));
            GECODE_PUSH_EVENTS(Event(EVENT_PROF, t, e[t].max(),
                                     at_most ? -u[t].min() : -u[t].max()));
          }
          if (!(m[t].assigned() &&
                u[t].assigned() &&
                s[t].assigned() &&
                e[t].assigned())) {
            GECODE_PUSH_EVENTS(Event(EVENT_PRUN, t, s[t].min()));
          }
        }
      }
#undef GECODE_PUSH_EVENTS

      // If there are no events, continue with next machine
      if (events_size == 0) {
        continue;
      }

      // Sort the events according to date
      Less<Event> less;
      Support::insertion(&events[0], events_size, less);

      // Sweep line along d, starting at 0
      int d        = 0;
      int ntask    = 0;
      int su  = 0;
      int ei = 0;

      prune_tasks_size = 0;
      for (int i = s.size(); i--; ) contribution[i] = 0;

      d = events[ei].date;
      while (ei < events_size) {
        if (events[ei].e != EVENT_PRUN) {
          if (d != events[ei].date) {
            GECODE_ES_CHECK(prune(home, d, events[ei].date-1, r,
                                  ntask, su,
                                  contribution, prune_tasks, prune_tasks_size));
            d = events[ei].date;
          }
          if (events[ei].e == EVENT_CHCK) {
            ntask += events[ei].inc;
          } else /* if (events[ei].e == EVENT_PROF) */ {
            su += events[ei].inc;
            if(events[ei].first_prof)
              contribution[events[ei].task] = at_most
                ? std::max(contribution[events[ei].task], events[ei].inc)
                : std::min(contribution[events[ei].task], events[ei].inc);
          }
        } else /* if (events[ei].e == EVENT_PRUN) */ {
          assert(prune_tasks_size<s.size());
          prune_tasks[prune_tasks_size++] = events[ei].task;
        }
        ei++;
      }

      GECODE_ES_CHECK(prune(home, d, d, r,
                            ntask, su,
                            contribution, prune_tasks, prune_tasks_size));
    }
    return subsumed ? home.ES_SUBSUMED(*this): ES_NOFIX;
  }

}}}

// STATISTICS: int-prop
