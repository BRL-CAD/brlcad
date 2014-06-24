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

namespace Gecode { namespace Set { namespace Precede {
    
  template<class View>
  forceinline
  Single<View>::Index::Index(Space& home, Propagator& p,
                             Council<Index>& c, int i0)
    : Advisor(home,p,c), i(i0) {}

  template<class View>
  forceinline
  Single<View>::Index::Index(Space& home, bool share, Index& a)
    : Advisor(home,share,a), i(a.i) {}

  
  template<class View>
  forceinline ExecStatus 
  Single<View>::updateAlpha(Space& home) {
    int n = x.size();
    while (alpha < n) {
      if (x[alpha].notContains(s)) {
        GECODE_ME_CHECK(x[alpha].exclude(home, t));
      } else if (x[alpha].contains(t)) {
        GECODE_ME_CHECK(x[alpha].include(home, s));
      } else {
        break;
      }
      alpha++;
    }
    return ES_OK;
  }

  template<class View>
  forceinline ExecStatus
  Single<View>::updateBeta(Space& home) {
    int n = x.size();
    do {
      beta++;
    } while ((beta < n) &&
             (x[beta].notContains(s) || x[beta].contains(t)));
    if (beta > gamma) {
      GECODE_ME_CHECK(x[alpha].exclude(home, t));
      GECODE_ME_CHECK(x[alpha].include(home, s));
    }
    return ES_OK;
  }
  
  template<class View>
  forceinline
  Single<View>::Single(Home home, ViewArray<View>& x0, 
                       int s0, int t0, int b, int g)
    : NaryPropagator<View, PC_SET_NONE>(home,x0), 
      c(home), s(s0), t(t0), alpha(0), beta(b), gamma(g) {
    for (int i=x.size(); i--; )
      if (!x[i].assigned())
        x[i].subscribe(home,*new (home) Index(home,*this,c,i));
    View::schedule(home, *this, ME_SET_BB);
  }

  template<class View>
  inline ExecStatus
  Single<View>::post(Home home, ViewArray<View>& x, int s, int t) {
    {
      int alpha = 0;
      while (alpha < x.size()) {
        if (x[alpha].notContains(s)) {
          GECODE_ME_CHECK(x[alpha].exclude(home, t));
        } else if (x[alpha].contains(t)) {
          GECODE_ME_CHECK(x[alpha].include(home, s));
        } else {
          break;
        }
        alpha++;
      }
      x.drop_fst(alpha);
      if (x.size() == 0)
        return ES_OK;
    }
    // alpha has been normalized to 0
    int beta = 0, gamma = 0;
    do {
      gamma++;
    } while ((gamma < x.size()) &&
             (!x[gamma].notContains(s) || !x[gamma].contains(t)));
    do {
      beta++;
    } while ((beta < x.size()) &&
             (x[beta].notContains(s) || x[beta].contains(t)));
    if (beta > gamma) {
      GECODE_ME_CHECK(x[0].exclude(home, t));
      GECODE_ME_CHECK(x[0].include(home, s));
      return ES_OK;
    }
    if (gamma < x.size())
      x.drop_lst(gamma);
    (void) new (home) Single<View>(home, x, s, t, beta, gamma);
    return ES_OK;
  }
    


  template<class View>
  forceinline
  Single<View>::Single(Space& home, bool share, Single& p)
    : NaryPropagator<View,PC_SET_NONE>(home, share, p),
      s(p.s), t(p.t),
      alpha(p.alpha), beta(p.beta), gamma(p.gamma) {
    c.update(home, share, p.c);
  }
  template<class View>
  Propagator*
  Single<View>::copy(Space& home, bool share) {
    // Try to eliminate assigned views at the beginning
    if (alpha > 0) {
      int i = 0;
      while ((i < alpha) && x[i].assigned())
        i++;
      x.drop_fst(i);
      for (Advisors<Index> as(c); as(); ++as)
        as.advisor().i -= i;
      alpha -= i; beta -= i; gamma -= i;
    }
    // Try to eliminate assigned views at the end
    if (gamma < x.size()) {
      int i = x.size()-1;
      while ((i > gamma) && x[i].assigned())
        i--;
      x.drop_lst(i);
    }
    return new (home) Single<View>(home, share, *this);
  }


  template<class View>
  inline size_t
  Single<View>::dispose(Space& home) {
    // Cancel remaining advisors
    for (Advisors<Index> as(c); as(); ++as)
      x[as.advisor().i].cancel(home,as.advisor());
    c.dispose(home);
    (void) NaryPropagator<View,PC_SET_NONE>::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  PropCost 
  Single<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO, x.size());
  }

  template<class View>
  ExecStatus
  Single<View>::advise(Space& home, Advisor& a0, const Delta&) {
    Index& a(static_cast<Index&>(a0));
    int i = a.i;
    // Check for gamma
    if ((beta <= gamma) && (i < gamma) && 
        x[i].notContains(s) && x[i].contains(t))
      gamma = i;
    if (x[i].assigned()) {
      a.dispose(home,c);
      if (c.empty())
        return ES_NOFIX;
    } else if ((i < alpha) || (i > gamma)) {
      x[i].cancel(home,a);
      a.dispose(home,c);
      return (c.empty()) ? ES_NOFIX : ES_FIX;
    }
    if (beta > gamma)
      return ES_NOFIX;
    if ((alpha == i) || (beta == i))
      return ES_NOFIX;
    return ES_FIX;
  }
  
  template<class View>
  ExecStatus
  Single<View>::propagate(Space& home, const ModEventDelta&) {
    int n = x.size();
    if (beta > gamma) {
      GECODE_ME_CHECK(x[alpha].exclude(home, t));
      GECODE_ME_CHECK(x[alpha].include(home, s));
      return home.ES_SUBSUMED(*this);
    }
    if (alpha < n && (x[alpha].notContains(s) || x[alpha].contains(t))) {
      if (x[alpha].notContains(s)) {
        GECODE_ME_CHECK(x[alpha].exclude(home, t));
      } else {
        GECODE_ME_CHECK(x[alpha].include(home, s));
      }
      alpha++;
      while (alpha < beta) {
        if (x[alpha].notContains(s)) {
          GECODE_ME_CHECK(x[alpha].exclude(home, t));
        } else {
          GECODE_ME_CHECK(x[alpha].include(home, s));
        }
        alpha++;
      }
      GECODE_ES_CHECK(updateAlpha(home));
      beta = alpha;
      if (alpha < n)
        GECODE_ES_CHECK(updateBeta(home));
    } else if ((beta < n) && (x[beta].notContains(s) || x[beta].contains(t))) {
      GECODE_ES_CHECK(updateBeta(home));
    }
    
    return (c.empty()) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }
  
}}}

// STATISTICS: set-prop
