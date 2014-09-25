/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

namespace Gecode { namespace Int { namespace Distinct {

  template<class View>
  forceinline
  Bnd<View>::Bnd(Home home, ViewArray<View>& x0)
    : Propagator(home), x(x0), y(home,x0) {
    // Both x and y initially contain the same variables
    //  - x is used for bounds propagation
    //  - y is used for performing singleton propagation
    // They can not be shared as singleton propagation removes
    // determined variables still required for bounds propagation.
    y.subscribe(home,*this,PC_INT_BND);
    int min = x[x.size()-1].min(), max = x[x.size()-1].max();
    for (int i=x.size()-1; i--; ) {
      min = std::min(min,x[i].min());
      max = std::max(max,x[i].max());
    }
    min_x = min; max_x = max;
  }

  template<class View>
  forceinline size_t
  Bnd<View>::dispose(Space& home) {
    y.cancel(home,*this,PC_INT_BND);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  forceinline
  Bnd<View>::Bnd(Space& home, bool share, Bnd<View>& p)
    : Propagator(home,share,p), min_x(p.min_x), max_x(p.max_x) {
    x.update(home,share,p.x);
    y.update(home,share,p.y);
  }

  template<class View>
  Actor*
  Bnd<View>::copy(Space& home, bool share) {
    return new (home) Bnd<View>(home,share,*this);
  }

  template<class View>
  PropCost
  Bnd<View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_VAL)
      return PropCost::linear(PropCost::LO, y.size());
    else
      return PropCost::quadratic(PropCost::LO, x.size());
  }


  /// %Rank information
  class Rank {
  public:
    int min, max;
  };

  /// Sort-order by increasing maximum (by index)
  template<class View>
  class MaxIncIdx {
  protected:
    ViewArray<View> x;
  public:
    forceinline
    MaxIncIdx(const ViewArray<View>& x0) : x(x0) {}
    forceinline bool
    operator ()(const int i, const int j) {
      return x[i].max() < x[j].max();
    }
  };

  /// Sort-order by increasing minimum (by index)
  template<class View>
  class MinIncIdx {
  protected:
    ViewArray<View> x;
  public:
    forceinline 
    MinIncIdx(const ViewArray<View>& x0) : x(x0) {}
    forceinline bool
    operator ()(const int i, const int j) {
      return x[i].min() < x[j].min();
    }
  };

  /// Sort-order by increasing minimum (direct)
  template<class View>
  class MinInc {
  public:
    forceinline bool
    operator ()(const View& x, const View& y) {
      return x.min() < y.min();
    }
  };

  /// Information on Hall intervals
  template<class IntType>
  class HallInfo {
  public:
    IntType bounds, t, d, h;
  };

  template<class IntType>
  forceinline void
  pathset_t(HallInfo<IntType> hall[], 
            IntType start, IntType end, IntType to) {
    IntType k, l;
    for (l=start; (k=l) != end; hall[k].t=to) {
      l = hall[k].t;
    }
  }

  template<class IntType>
  forceinline void
  pathset_h(HallInfo<IntType> hall[], 
            IntType start, IntType end, IntType to) {
    IntType k, l;
    for (l=start; (k=l) != end; hall[k].h=to) {
      l = hall[k].h;
    }
  }

  template<class IntType>
  forceinline IntType
  pathmin_h(const HallInfo<IntType> hall[], IntType i) {
    while (hall[i].h < i)
      i = hall[i].h;
    return i;
  }

  template<class IntType>
  forceinline IntType
  pathmin_t(const HallInfo<IntType> hall[], IntType i) {
    while (hall[i].t < i)
      i = hall[i].t;
    return i;
  }

  template<class IntType>
  forceinline IntType
  pathmax_h(const HallInfo<IntType> hall[], IntType i) {
    while (hall[i].h > i)
      i = hall[i].h;
    return i;
  }

  template<class IntType>
  forceinline IntType
  pathmax_t(const HallInfo<IntType> hall[], IntType i) {
    while (hall[i].t > i)
      i = hall[i].t;
    return i;
  }

  template<class View, class IntType>
  forceinline ExecStatus
  prop_bnd(Space& home, ViewArray<View>& x, 
            int* minsorted, int* maxsorted) {
    const int n = x.size();

    Region r(home);

    // Setup rank and bounds info
    HallInfo<IntType>* hall = r.alloc<HallInfo<IntType> >(2*n+2);
    Rank* rank = r.alloc<Rank>(n);

    int nb = 0;
    {
      IntType min  = x[minsorted[0]].min();
      IntType max  = x[maxsorted[0]].max() + 1;
      IntType last = min - 2;

      hall[0].bounds = last;

      int i = 0;
      int j = 0;
      while (true) {
        if ((i < n) && (min < max)) {
          if (min != last)
            hall[++nb].bounds = last = min;
          rank[minsorted[i]].min = nb;
          if (++i < n)
            min = x[minsorted[i]].min();
        } else {
          if (max != last)
            hall[++nb].bounds = last = max;
          rank[maxsorted[j]].max = nb;
          if (++j == n)
            break;
          max = x[maxsorted[j]].max() + 1;
        }
      }
      hall[nb+1].bounds = hall[nb].bounds + 2;
    }

    // If tells cross holes, we do not compute a fixpoint
    ExecStatus es = ES_FIX;

    // Propagate lower bounds
    for (int i=nb+2; --i;) {
      hall[i].t = hall[i].h = i-1;
      hall[i].d = hall[i].bounds - hall[i-1].bounds;
    }
    for (int i=0; i<n; i++) { // visit intervals in increasing max order
      IntType x0 = rank[maxsorted[i]].min;
      IntType z = pathmax_t(hall, x0+1);
      IntType j = hall[z].t;
      if (--hall[z].d == 0)
        hall[z = pathmax_t(hall, hall[z].t=z+1)].t = j;
      pathset_t(hall, x0+1, z, z); // path compression
      IntType y = rank[maxsorted[i]].max;
      if (hall[z].d < hall[z].bounds-hall[y].bounds)
        return ES_FAILED;
      if (hall[x0].h > x0) {
        IntType w = pathmax_h(hall, hall[x0].h);
        IntType m = hall[w].bounds;
        ModEvent me = x[maxsorted[i]].gq(home,m);
        if (me_failed(me))
          return ES_FAILED;
        if ((me == ME_INT_VAL) ||
            ((me == ME_INT_BND) && (m != x[maxsorted[i]].min())))
          es = ES_NOFIX;
        pathset_h(hall, x0, w, w); // path compression
      }
      if (hall[z].d == hall[z].bounds-hall[y].bounds) {
        pathset_h(hall, hall[y].h, j-1, y); // mark hall interval
        hall[y].h = j-1;
      }
    }

    // Propagate upper bounds
    for (int i=nb+1; i--;) {
      hall[i].t = hall[i].h = i+1;
      hall[i].d = hall[i+1].bounds - hall[i].bounds;
    }
    for (int i=n; --i>=0; ) { // visit intervals in decreasing min order
      IntType x0 = rank[minsorted[i]].max;
      IntType z = pathmin_t(hall, x0-1);
      IntType j = hall[z].t;
      if (--hall[z].d == 0)
        hall[z = pathmin_t(hall, hall[z].t=z-1)].t = j;
      pathset_t(hall, x0-1, z, z);
      IntType y = rank[minsorted[i]].min;
      if (hall[z].d < hall[y].bounds-hall[z].bounds)
        return ES_FAILED;
      if (hall[x0].h < x0) {
        IntType w = pathmin_h(hall, hall[x0].h);
        IntType m = hall[w].bounds - 1;
        ModEvent me = x[minsorted[i]].lq(home,m);
        if (me_failed(me))
          return ES_FAILED;
        if ((me == ME_INT_VAL) ||
            ((me == ME_INT_BND) && (m != x[maxsorted[i]].min())))
          es = ES_NOFIX;
        pathset_h(hall, x0, w, w);
      }
      if (hall[z].d == hall[y].bounds-hall[z].bounds) {
        pathset_h(hall, hall[y].h, j+1, y);
        hall[y].h = j+1;
      }
    }

    return es;
  }

  template<class View>
  forceinline ExecStatus
  prop_bnd(Space& home, ViewArray<View>& x, int& min_x, int& max_x) {

    const int n = x.size();

    Region r(home);

    int* minsorted = r.alloc<int>(n);
    int* maxsorted = r.alloc<int>(n);

    unsigned int d = static_cast<unsigned int>(max_x - min_x) + 1;

    if (d < static_cast<unsigned int>(n))
      return ES_FAILED;

    if (d > 2*static_cast<unsigned int>(n)) {
      for (int i = n; i--; )
        minsorted[i]=maxsorted[i]=i;

      MinIncIdx<View> min_inc(x);
      Support::quicksort<int,MinIncIdx<View> >(minsorted, n, min_inc);
      MaxIncIdx<View> max_inc(x);
      Support::quicksort<int,MaxIncIdx<View> >(maxsorted, n, max_inc);
    } else {

      int* minbucket = r.alloc<int>(d);
      int* maxbucket = r.alloc<int>(d);
      for (unsigned int i=d; i--; )
        minbucket[i]=maxbucket[i]=0;

      for (int i=n; i--; ) {
        minbucket[x[i].min() - min_x]++;
        maxbucket[x[i].max() - min_x]++;
      }

      int c_min = 0, c_max = 0;
      for (unsigned int i=0; i<d; i++) {
        int t_min = minbucket[i];
        int t_max = maxbucket[i];
        minbucket[i] = c_min; c_min += t_min;
        maxbucket[i] = c_max; c_max += t_max;
      }
      assert((c_min == n) && (c_max == n));

      for (int i=n; i--; ) {
        minsorted[minbucket[x[i].min() - min_x]++] = i;
        maxsorted[maxbucket[x[i].max() - min_x]++] = i;
      }
    }

    // Update minimum and maximum information
    min_x = x[minsorted[0]].min();
    max_x = x[maxsorted[n-1]].max();


    if (d > (static_cast<unsigned int>(Int::Limits::max) / 2U))
      return prop_bnd<View,long long int>(home,x,minsorted,maxsorted);
    else
      return prop_bnd<View,int>(home,x,minsorted,maxsorted);
  }

  template<class View>
  ExecStatus
  prop_bnd(Space& home, ViewArray<View>& x) {
    int min = x[x.size()-1].min(), max = x[x.size()-1].max();
    for (int i=x.size()-1; i--; ) {
      min = std::min(min,x[i].min());
      max = std::max(max,x[i].max());
    }
    return prop_bnd(home, x, min, max);
  }

  template<class View>
  ExecStatus
  Bnd<View>::propagate(Space& home, const ModEventDelta& med) {
    assert(x.size() > 1);

    if (View::me(med) == ME_INT_VAL) {
      ExecStatus es = prop_val<View,false>(home,y);
      GECODE_ES_CHECK(es);
      if (y.size() < 2)
        return home.ES_SUBSUMED(*this);
      if (es == ES_FIX)
        return home.ES_FIX_PARTIAL(*this,View::med(ME_INT_BND));
    }

    if (y.size() == 2)
      GECODE_REWRITE(*this,Rel::Nq<View>::post(home(*this),y[0],y[1]));

    ExecStatus es = prop_bnd<View>(home,x,min_x,max_x);

    GECODE_ES_CHECK(es);
    if (es == ES_NOFIX && View::me(modeventdelta()) ==  ME_INT_VAL)
      return es;

    const int n = x.size();

    if ((n > 2*y.size()) && (n > 6)) {
      // If there are many assigned views, try to eliminate them
      unsigned int d = static_cast<unsigned int>(max_x - min_x) + 1;
      if (d > 2*static_cast<unsigned int>(n)) {
        MinInc<View> min_inc;
        Support::quicksort<View,MinInc<View> >(&x[0], n, min_inc);
      } else {
        Region r(home);
        int* minbucket = r.alloc<int>(d);
        View* minsorted = r.alloc<View>(n);

        for (unsigned int i=d; i--; )
          minbucket[i]=0;
        for (int i=n; i--; )
          minbucket[x[i].min() - min_x]++;

        int c_min = 0;
        for (unsigned int i=0; i<d; i++) {
          int t_min = minbucket[i];
          minbucket[i] = c_min; c_min += t_min;
        }
        assert(c_min == n);

        for (int i=n; i--;)
          minsorted[minbucket[x[i].min() - min_x]++] = x[i];
        for (int i=n; i--;)
          x[i] = minsorted[i];
      }

      int i   = 0;
      int j   = 0;
      int max = x[0].max()-1;
      while (i < n-1) {
        if (!x[i].assigned() ||
            (x[i].val() <= max) || (x[i].val() > x[i+1].min())) {
          // Keep view x[i]
          max = std::max(max,x[i].max());
          x[j++]=x[i];
        }
        i++;
      }
      if (!x[i].assigned() || (x[i].val() <= max))
        x[j++]=x[i];
      x.size(j);
    }

    if (x.size() < 2)
      return home.ES_SUBSUMED(*this);

    if (x.size() == 2)
      GECODE_REWRITE(*this,Rel::Nq<View>::post(home(*this),x[0],x[1]));

    return es;
  }

  template<class View>
  ExecStatus
  Bnd<View>::post(Home home, ViewArray<View>& x){
    if (x.size() == 2)
      return Rel::Nq<View>::post(home,x[0],x[1]);
    if (x.size() > 2)
      (void) new (home) Bnd<View>(home,x);
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

