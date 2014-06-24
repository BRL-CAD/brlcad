/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     David Rijsman <David.Rijsman@quintiq.com>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     David Rijsman, 2009
 *     Christian Schulte, 2009
 *
 *  Last modified:
 *     $Date$
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

namespace Gecode { namespace Int { namespace Sequence {

  template<class View, class Val>
  forceinline
  Sequence<View,Val>::Sequence(Home home, ViewArray<View>& x0, Val s0, 
                               int q0, int l0, int u0)    
    : Propagator(home), x(x0), s(s0), q(q0), l(l0), u(u0), 
      vvsamax(home,x,s0,q0), vvsamin(home,x,s0,q0), ac(home) {
    home.notice(*this,AP_DISPOSE);
    for (int i=x.size(); i--; ) {
      if (undecided(x[i],s)) {
        x[i].subscribe(home,*new (home) SupportAdvisor<View>(home,*this,ac,i));   
      } else {
        x[i].schedule(home,*this,x[i].assigned() ? ME_INT_VAL : ME_INT_BND);
      }
    }
  }

  namespace {
    /// Helper class for updating ints or IntSets
    template<class Val>
    class UpdateVal {
    public:
      static void update(Val& n, Space& home, bool share, Val& old);
    };
    /// Helper class for updating ints or IntSets, specialised for int
    template<>
    class UpdateVal<int> {
    public:
      static void update(int& n, Space&, bool, int& old) {
        n = old;
      }
    };
    /// Helper class for updating ints or IntSets, specialised for IntSet
    template<>
    class UpdateVal<IntSet> {
    public:
      static void update(IntSet& n, Space& home, bool share,
                         IntSet& old) {
        n.update(home,share,old);
      }
    };
  }

  template<class View, class Val>
  forceinline
  Sequence<View,Val>::Sequence(Space& home, bool share, Sequence& p)
    : Propagator(home,share,p), q(p.q), l(p.l), u(p.u),
      vvsamax(), vvsamin() {
    UpdateVal<Val>::update(s,home,share,p.s);
    x.update(home,share,p.x);
    ac.update(home,share,p.ac);
    vvsamax.update(home,share,p.vvsamax);
    vvsamin.update(home,share,p.vvsamin);
  }

  template<class View,class Val>
  ExecStatus
  Sequence<View,Val>::advise(Space& home, Advisor& _a, const Delta& d) {
    SupportAdvisor<View>& a = static_cast<SupportAdvisor<View>&>(_a);
    ExecStatus status = vvsamax.advise(home,x,s,q,a.i,d);
    if ( ES_NOFIX == vvsamin.advise(home,x,s,q,a.i,d) ) {
      status = ES_NOFIX;
    }

    if (!undecided(x[a.i],s)) {
      if (!x[a.i].assigned())
        x[a.i].cancel(home,a);

      if ( ES_NOFIX == status ) {
        return home.ES_NOFIX_DISPOSE(ac,a);
      } else {
        return home.ES_FIX_DISPOSE(ac,a);
      }
    }

    return status;
  }

  template<class View, class Val>
  forceinline size_t
  Sequence<View,Val>::dispose(Space& home) {
    home.ignore(*this,AP_DISPOSE);
    ac.dispose(home);
    s.~Val();
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View, class Val>
  forceinline ExecStatus
  Sequence<View,Val>::check(Space& home, ViewArray<View>& x, Val s, int q, int l, int u) {
    Region r(home);
    // could do this with an array of length q...
    int* upper = r.alloc<int>(x.size()+1);
    int* lower = r.alloc<int>(x.size()+1);
    upper[0] = 0;
    lower[0] = 0;
    for ( int j=0; j<x.size(); j++ ) {
      upper[j+1] = upper[j];
      lower[j+1] = lower[j];
      if (includes(x[j],s)) {
        upper[j+1] += 1;
      } else if (excludes(x[j],s)) {
        lower[j+1] += 1;
      }
      if ( j+1 >= q && (q - l < lower[j+1] - lower[j+1-q] || upper[j+1] - upper[j+1-q] > u) ) {
        return ES_FAILED;
      }
    }
    return ES_OK;
  }

  template<class View, class Val>
  ExecStatus
  Sequence<View,Val>::post(Home home, ViewArray<View>& x, Val s, int q, int l, int u) {
    GECODE_ES_CHECK(check(home,x,s,q,l,u));
    Sequence<View,Val>* p = new (home) Sequence<View,Val>(home,x,s,q,l,u);

    GECODE_ES_CHECK(p->vvsamax.propagate(home,x,s,q,l,u));
    GECODE_ES_CHECK(p->vvsamin.propagate(home,x,s,q,l,u));

   return ES_OK;
  }

  template<class View, class Val>
  Actor*
  Sequence<View,Val>::copy(Space& home, bool share) {
    return new (home) Sequence<View,Val>(home,share,*this);
  }

  template<class View, class Val>
  PropCost
  Sequence<View,Val>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::cubic(PropCost::HI,x.size());
  }

  template<class View, class Val>
  ExecStatus 
  Sequence<View,Val>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ES_CHECK(vvsamax.propagate(home,x,s,q,l,u));
    GECODE_ES_CHECK(vvsamin.propagate(home,x,s,q,l,u));

    for (int i=x.size(); i--; )
      if (undecided(x[i],s))
        return ES_FIX;

    return home.ES_SUBSUMED(*this);
  }

}}}

// STATISTICS: int-prop

