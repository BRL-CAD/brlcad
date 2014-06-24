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

  /**
   * \brief Class for advising the propagator
   */
  template<class View>
  class SupportAdvisor : public Advisor {
  public:
    /// Index position
    int i;
    /// Initialize
    SupportAdvisor(Space& home, Propagator& p, Council<SupportAdvisor>& c,
                   int i0);
    /// Copy during cloning
    SupportAdvisor(Space& home, bool share, SupportAdvisor& a);
    /// Dispose advisor
    void dispose(Space& home, Council<SupportAdvisor>& c);
  };

  template<class View>
  forceinline
  SupportAdvisor<View>::SupportAdvisor(Space& home, Propagator& p, 
                                       Council<SupportAdvisor>& c,int i0)
    : Advisor(home,p,c), i(i0) {
  }

  template<class View>
  forceinline
  SupportAdvisor<View>::SupportAdvisor(Space& home, bool share, 
                                       SupportAdvisor& a)
    : Advisor(home,share,a), i(a.i) {
  }
  
  template<class View>
  forceinline void
  SupportAdvisor<View>::dispose(Space& home, Council<SupportAdvisor>& c) {
    Advisor::dispose(home,c);
  }

  /**
   * \brief Class for view value support structure
   */
  template<class View, class Val,bool iss>
  class ViewValSupport {
  public:
    /// Initialize
    void init(Space& home, ViewArray<View>& x,Val s, int i, int q);
    /// Update
    void update(Space& home, bool share, ViewValSupport<View,Val,iss>& vvs, int n0);
    /// Advise
    ExecStatus advise(Space& home,ViewArray<View>& a,Val s,int i,int q, int j,const Delta& d);
    /// Propagate 
    ExecStatus propagate(Space& home,ViewArray<View>& a,Val s,int i,int q,int l,int u);
    /// Return true if sequence j has been violated
    bool violated(int j, int q, int l, int u) const;
    /// Check if retired 
    bool retired(void) const;
    /// Allocate an instance
    static ViewValSupport* allocate(Space&,int);
  private:
    /// Schedules the invoking support to be concluded (to be shaved)
    ExecStatus schedule_conclusion(ViewArray<View>& a,Val s,int i);
    /// Returns true if a conclusion to be made has been scheduled
    bool conlusion_scheduled(void) const;
    /// Retires invoking instance
    void retire(void);
    /// Return number of forced to s elements in sequence j
    int values(int j, int q) const;
    /// Checks if x[i] has been shaved
    bool shaved(const View& x, Val s, int i) const;
    /// Push up the cumulative array
    bool pushup(ViewArray<View>& a,Val s,int i,int q,int idx, int v);
    /// Make conclusion
    ExecStatus conclude(Space& home,ViewArray<View>& a,Val s,int i);
    /// Returns true if a[idx]=s not possible taking into account a[i]=s if iss or a[i]!=s if ! iss
    bool s_not_possible(ViewArray<View>& a, Val s, int i, int idx) const;
    /// Returns true if a[idx] can be not s taking into account a[i]=s if iss or a[i]!=s if ! iss
    bool alternative_not_possible(ViewArray<View>& a,Val s,int i, int idx) const;
    /// Returns true if there are potential violations marked
    bool has_potential_violation(void) const;
    /// Returns next potential violation and removes it
    int next_potential_violation(void);
    /// Adds potential violation i
    void potential_violation(int i);
    // Sets element idx of cumulative array to v
    void set(int idx, int v, int q, int n);
    /// Adds potential violations for location idx in cumulative array
    void potential_violation(int idx, int q, int n);
    /// Cumulative array
    int* y;
    /// Potential violation set
    Violations v;
  };

    
  template<class View, class Val,bool iss>
  forceinline ViewValSupport<View,Val,iss>*
  ViewValSupport<View,Val,iss>::allocate(Space& home, int n) {
    return home.alloc<ViewValSupport<View,Val,iss> >(n);
  }

  template<class View, class Val,bool iss>
  forceinline bool 
  ViewValSupport<View,Val,iss>::has_potential_violation(void) const {
    return !v.empty();
  }
    
  template<class View, class Val,bool iss>
  forceinline int 
  ViewValSupport<View,Val,iss>::next_potential_violation(void) {
    return static_cast<int>(v.get());
  }

  template<class View, class Val,bool iss>
  forceinline void 
  ViewValSupport<View,Val,iss>::potential_violation(int k) {
    v.add(static_cast<unsigned int>(k));
  }
  

  template<class View, class Val,bool iss>
  forceinline bool 
  ViewValSupport<View,Val,iss>::retired(void) const {
    return NULL == y;
  }

  template<class View, class Val,bool iss>
  forceinline void
  ViewValSupport<View,Val,iss>::retire(void) {
    y = NULL;
  }

  template<class View,class Val,bool iss>
  forceinline bool
  ViewValSupport<View,Val,iss>::alternative_not_possible
  (ViewArray<View>& a, Val s, int i, int idx) const {
    (void) i;
    return includes(a[idx-1],s) || (iss && (idx-1 == i));
  }

  template<class View, class Val,bool iss>
  forceinline bool
  ViewValSupport<View,Val,iss>::s_not_possible
  (ViewArray<View>& a, Val s, int i, int idx) const {
    (void) i;
    return excludes(a[idx-1],s) || (!iss && (i == idx-1));
  }
 

  template<class View, class Val,bool iss>
  forceinline void
  ViewValSupport<View,Val,iss>::init(Space& home, ViewArray<View>& a, Val s,
                                     int i, int q) {
    y = home.alloc<int>(a.size()+1);
    v.init(home,static_cast<unsigned int>(a.size()));
    y[0] = 0;
    for ( int l=0; l<a.size(); l++ ) {
      if ( alternative_not_possible(a,s,i,l+1) ) {
        y[l+1] = y[l] + 1;
      } else {
        y[l+1] = y[l];
      }
      if ( l+1 >= q ) {
        potential_violation(l+1-q);
      }
      if ( l <= a.size() - q ) {
        potential_violation(l);
      }

    }
  }

  template<class View, class Val,bool iss>
  forceinline void
  ViewValSupport<View,Val,iss>::update(Space& home, bool share,
                                       ViewValSupport<View,Val,iss>& vvs, 
                                       int n0) {
    y = NULL;
    if ( !vvs.retired() ) {
      y = home.alloc<int>(n0);
      for ( int l=0; l<n0; l++ ) {
        y[l] = vvs.y[l];
      }
      v.update(home,share,vvs.v);
      // = &home.construct<S>(S::key_compare(),S::allocator_type(home));
    }
  }

  template<class View,class Val,bool iss>
  forceinline bool
  ViewValSupport<View,Val,iss>::shaved(const View& x, Val s, int) const {
    if (iss)
      return excludes(x,s);
    else
      return includes(x,s);
  }

  template<class View,class Val,bool iss>
  forceinline ExecStatus 
  ViewValSupport<View,Val,iss>::schedule_conclusion(ViewArray<View>& a, Val s,
                                                    int i) {
    if (!retired()) {
      if ((iss && includes(a[i],s)) || (!iss && excludes(a[i],s)))
        return ES_FAILED;
      y[0] = 1;
      potential_violation(0);
    }

    return ES_OK;
  }

  template<class View,class Val,bool iss>
  forceinline bool
  ViewValSupport<View,Val,iss>::conlusion_scheduled(void) const {
    return !retired() && y[0] > 0;
  }

  template<class View,class Val,bool iss>
  forceinline int
  ViewValSupport<View,Val,iss>::values(int j, int q) const {
    return y[j+q]-y[j];
  }

  template<class View,class Val,bool iss>
  forceinline bool
  ViewValSupport<View,Val,iss>::violated(int j, int q, int l, int u) const {
    return values(j,q) < l || values(j,q) > u;
  }

  template<class View,class Val,bool iss>
  forceinline ExecStatus
  ViewValSupport<View,Val,iss>::conclude(Space& home,ViewArray<View>& a,
                                         Val s, int i) {
    if ( iss ) {
      GECODE_ME_CHECK(exclude(home,a[i],s)); 
    } else {
      GECODE_ME_CHECK(include(home,a[i],s)); 
    }

    retire();

    return ES_OK;
  }

  template<class View,class Val,bool iss>
  forceinline void
  ViewValSupport<View,Val,iss>::potential_violation(int idx, int q, int n) {
    if ( idx >= q ) {
      potential_violation(idx-q);
    }
    if ( idx <= n - q - 1) {
      potential_violation(idx);
    }
  }

  template<class View,class Val,bool iss>
  forceinline void
  ViewValSupport<View,Val,iss>::set(int idx, int v, int q, int n) {
    assert(y[idx]<=v);
    if ( y[idx] < v ) {
      y[idx] = v;
      potential_violation(idx,q,n);   
    }
  }

  template<class View,class Val,bool iss>
  forceinline bool 
  ViewValSupport<View,Val,iss>::pushup(ViewArray<View>& a,Val s,int i,int q,int idx, int v) {
    if ( !retired() ) {
      int n = a.size() + 1;

      set(idx,y[idx]+v,q,n);

      if ( y[idx] > idx ) {
        return false;
      }
      
      int t = idx;

      // repair y on the left
      while ( idx > 0 && ((y[idx]-y[idx-1]>1) || ((y[idx]-y[idx-1]==1 && s_not_possible(a,s,i,idx)))) )  {
        if ( s_not_possible(a,s,i,idx) ) {
          set(idx-1,y[idx],q,n);
        } else {
          set(idx-1,y[idx]-1,q,n);
        }
        if ( y[idx-1]>idx-1 ) {
          return false;
        }
        idx -= 1;
      }

      idx = t;

      // repair y on the right
      while ( idx < a.size() && ((y[idx]-y[idx+1]>0) || ((y[idx]-y[idx+1]==0) && alternative_not_possible(a,s,i,idx+1))) ) {
        if ( alternative_not_possible(a,s,i,idx+1) ) {
          set(idx+1,y[idx]+1,q,n);
        } else {
          set(idx+1,y[idx],q,n);
        }
        idx += 1;
      }
    }

    return true;
  }

  template<class View,class Val,bool iss>
  forceinline ExecStatus
  ViewValSupport<View,Val,iss>::advise(Space&, ViewArray<View>& a,
                                       Val s,int i,int q, int j,
                                       const Delta&) {
    ExecStatus status = ES_FIX; 
    if (!retired()) {
      if ((j == i) && shaved(a[j],s,j)) {
        retire();
      } else {
        switch (takes(a[j],s)) {
        case TS_YES:
          if (y[j+1]-y[j] == 0) {
            if (!pushup(a,s,i,q,j+1,1)) {
              GECODE_ES_CHECK(schedule_conclusion(a,s,i));
            }
          }
          break;
        case TS_NO:
          if (y[j+1]-y[j] > 0) {
            if (!pushup(a,s,i,q,j,y[j+1]-y[j])) {
              GECODE_ES_CHECK(schedule_conclusion(a,s,i));
            }
          }
          break;
        case TS_MAYBE:
          break;
        default:
          GECODE_NEVER;
        }

        if ( has_potential_violation() )
          status = ES_NOFIX;
      }
    }

    return status;
  }

  template<class View,class Val,bool iss>
  forceinline ExecStatus 
  ViewValSupport<View,Val,iss>::propagate(Space& home,ViewArray<View>& a,Val s,int i,int q,int l,int u) {
    if ( !retired() ) {
      if ( conlusion_scheduled() ) {
        return conclude(home,a,s,i);
      }

      while (has_potential_violation()) {
        int j = next_potential_violation();
        if (violated(j,q,l,u)) {
          int forced_to_s = values(j,q);
          if (forced_to_s < l) {
            if (!pushup(a,s,i,q,j+q,l-forced_to_s))
              return conclude(home,a,s,i);
          } else {
            if (!pushup(a,s,i,q,j,forced_to_s-u))
              return conclude(home,a,s,i);
          }
          if (violated(j,q,l,u))
            return conclude(home,a,s,i);
        }
      }
    }
    return ES_OK;
  }

  template<class View,class Val,bool iss>
  ViewValSupportArray<View,Val,iss>::ViewValSupportArray(void) : xs(NULL), n(0) {
  }

  template<class View,class Val,bool iss>
  ViewValSupportArray<View,Val,iss>::ViewValSupportArray(const ViewValSupportArray<View,Val,iss>& a) {
    n = a.n; xs = a.xs;
  }

  template<class View,class Val,bool iss>
  ViewValSupportArray<View,Val,iss>::ViewValSupportArray(Space& home,ViewArray<View>& x, Val s, int q) : xs(NULL) {
    n = x.size();
    if ( n > 0 ) {
      xs = ViewValSupport<View,Val,iss>::allocate(home,n);
      for (int i = n; i--; ) {
        xs[i].init(home,x,s,i,q);
      }
    }
  }

  template<class View,class Val,bool iss>
  ViewValSupportArray<View,Val,iss>::ViewValSupportArray(Space& home, int n0) : xs(NULL) {
    n = n0;
    if (n>0) {
      xs = ViewValSupport<View,Val,iss>::allocate(home,n);
    }
  }

  template<class View,class Val,bool iss>
  forceinline int
    ViewValSupportArray<View,Val,iss>::size(void) const {
      return n;
  }

  template<class View,class Val,bool iss>
  forceinline ViewValSupport<View,Val,iss>&
    ViewValSupportArray<View,Val,iss>::operator [](int i) {
      assert((i >= 0) && (i < size()));
      return xs[i];
  }

  template<class View,class Val,bool iss>
  forceinline const ViewValSupport<View,Val,iss>&
  ViewValSupportArray<View,Val,iss>::operator [](int i) const {
    assert((i >= 0) && (i < size()));
    return xs[i];
  }

  template<class View,class Val,bool iss>
  void
  ViewValSupportArray<View,Val,iss>::update(Space& home, bool share, ViewValSupportArray<View,Val,iss>& a) {
    n = a.size();
    if (n>0) {
      xs = ViewValSupport<View,Val,iss>::allocate(home,n);
      for (int i=n; i--; ) {
        xs[i].update(home,share,a[i],n+1);
      }
    }
  }

  template<class View,class Val,bool iss>
  ExecStatus
  ViewValSupportArray<View,Val,iss>::propagate(Space& home,ViewArray<View>& a,Val s,int q,int l,int u) {
    for (int i=n; i--; ) {
      GECODE_ES_CHECK(xs[i].propagate(home,a,s,i,q,l,u));
    }
    return ES_OK;
  }

  template<class View,class Val,bool iss>
  ExecStatus
  ViewValSupportArray<View,Val,iss>::advise(Space& home,ViewArray<View>& a,Val s,int q,int j,const Delta& d) {
    ExecStatus status = ES_FIX;
    for (int i=n; i--; ) {
      if ( ES_NOFIX == xs[i].advise(home,a,s,i,q,j,d) )
        status = ES_NOFIX;
    }
    return status;
  }
}}}


// STATISTICS: int-prop

