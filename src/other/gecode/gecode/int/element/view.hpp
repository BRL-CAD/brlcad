/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
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

#include <algorithm>

namespace Gecode { namespace Int { namespace Element {

  /// VarArg type for integer views
  template<>
  class ViewToVarArg<IntView> {
  public:
    typedef IntVarArgs argtype;
  };
  /// VarArg type for Boolean views
  template<>
  class ViewToVarArg<BoolView> {
  public:
    typedef BoolVarArgs argtype;
  };

  /**
   * \brief Class for pair of index and view
   *
   */
  template<class View>
  class IdxView {
  public:
    int idx; View view;

    static IdxView* allocate(Space&, int);
  };

  template<class View>
  forceinline IdxView<View>*
  IdxView<View>::allocate(Space& home, int n) {
    return home.alloc<IdxView<View> >(n);
  }

  template<class View>
  IdxViewArray<View>::IdxViewArray(void) : xs(NULL), n(0) {}

  template<class View>
  IdxViewArray<View>::IdxViewArray(const IdxViewArray<View>& a) {
    n = a.n; xs = a.xs;
  }

  template<class View>
  IdxViewArray<View>::IdxViewArray(Space& home,
    const typename ViewToVarArg<View>::argtype& xa) : xs(NULL) {
    n = xa.size();
    if (n>0) {
      xs = IdxView<View>::allocate(home, n);
      for (int i = n; i--; ) {
        xs[i].idx = i; xs[i].view = xa[i];
      }
    }
  }

  template<class View>
  IdxViewArray<View>::IdxViewArray(Space& home, int n0) : xs(NULL) {
    n = n0;
    if (n>0) {
      xs = IdxView<View>::allocate(home, n);
    }
  }

  template<class View>
  forceinline int
  IdxViewArray<View>::size(void) const {
    return n;
  }

  template<class View>
  forceinline void
  IdxViewArray<View>::size(int n0) {
    n = n0;
  }

  template<class View>
  forceinline IdxView<View>&
  IdxViewArray<View>::operator [](int i) {
    assert((i >= 0) && (i < size()));
    return xs[i];
  }

  template<class View>
  forceinline const IdxView<View>&
  IdxViewArray<View>::operator [](int i) const {
    assert((i >= 0) && (i < size()));
    return xs[i];
  }

  template<class View>
  void
  IdxViewArray<View>::subscribe(Space& home, Propagator& p, PropCond pc,
                                bool process) {
    for (int i = n; i--; )
      xs[i].view.subscribe(home,p,pc,process);
  }

  template<class View>
  void
  IdxViewArray<View>::cancel(Space& home, Propagator& p, PropCond pc) {
    for (int i = n; i--; )
      xs[i].view.cancel(home,p,pc);
  }

  template<class View>
  void
  IdxViewArray<View>::update(Space& home, bool share, IdxViewArray<View>& a) {
    n = a.size();
    if (n>0) {
      xs = IdxView<View>::allocate(home,n);
      for (int i=n; i--; ) {
        xs[i].idx = a[i].idx;
        xs[i].view.update(home,share,a[i].view);
      }
    }
  }


  /**
   * \brief Class for bounds-equality test
   *
   */
  template<class VA, class VC>
  class RelTestBnd {
  public:
    RelTest operator ()(VA,VC);
  };
  /**
   * \brief Class for bounds-equality test (specialized)
   *
   */
  template<class VA>
  class RelTestBnd<VA,ConstIntView> {
  public:
    RelTest operator ()(VA,ConstIntView);
  };

  /**
   * \brief Class for domain-equality test
   *
   */
  template<class VA, class VC>
  class RelTestDom {
  public:
    RelTest operator ()(VA,VC);
  };
  /**
   * \brief Class for domain-equality test (specialized)
   *
   */
  template<class VA>
  class RelTestDom<VA,ConstIntView> {
  public:
    RelTest operator ()(VA,ConstIntView);
  };


  template<class VA, class VC>
  forceinline RelTest
  RelTestBnd<VA,VC>::operator ()(VA x, VC y) {
    return rtest_eq_bnd(x,y);
  }
  template<class VA>
  forceinline RelTest
  RelTestBnd<VA,ConstIntView>::operator ()(VA x, ConstIntView y) {
    return rtest_eq_bnd(x,y.val());
  }

  template<class VA, class VC>
  forceinline RelTest
  RelTestDom<VA,VC>::operator ()(VA x, VC y) {
    return rtest_eq_dom(x,y);
  }
  template<class VA>
  forceinline RelTest
  RelTestDom<VA,ConstIntView>::operator ()(VA x, ConstIntView y) {
    return rtest_eq_dom(x,y.val());
  }




  /*
   * Base class
   *
   */

  template<class VA, class VB, class VC, PropCond pc_ac>
  View<VA,VB,VC,pc_ac>::View(Home home, IdxViewArray<VA>& iv0,
                             VB y0, VC y1)
    : Propagator(home), iv(iv0), x0(y0), x1(y1) {
    x0.subscribe(home,*this,PC_INT_DOM);
    x1.subscribe(home,*this,pc_ac);
    iv.subscribe(home,*this,pc_ac);
  }

  template<class VA, class VB, class VC, PropCond pc_ac>
  forceinline
  View<VA,VB,VC,pc_ac>::View(Space& home, bool share, View& p)
    : Propagator(home,share,p) {
    x0.update(home,share,p.x0);
    x1.update(home,share,p.x1);
    iv.update(home,share,p.iv);
  }

  template<class VA, class VB, class VC, PropCond pc_ac>
  PropCost
  View<VA,VB,VC,pc_ac>::cost(const Space&, const ModEventDelta&) const {
    // This is required for subscribing to variables in the
    // above constructor, but this is then the only time this
    // virtual function is ever used!
    return PropCost::linear(PropCost::LO,iv.size()+2);
  }

  template<class VA, class VB, class VC, PropCond pc_ac>
  forceinline size_t
  View<VA,VB,VC,pc_ac>::dispose(Space& home) {
    x0.cancel(home,*this,PC_INT_DOM);
    x1.cancel(home,*this,pc_ac);
    iv.cancel(home,*this,pc_ac);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }




  /**
   * \brief Value iterator for indices in index-view map
   *
   */
  template<class View>
  class IterIdxView {
  private:
    const IdxView<View> *cur, *end;
  public:
    IterIdxView(void);
    IterIdxView(const IdxView<View>*, const IdxView<View>*);
    void init(const IdxView<View>*, const IdxView<View>*);
    bool operator ()(void) const;
    void operator ++(void);
    int  val(void) const;
  };

  template<class View>
  forceinline
  IterIdxView<View>::IterIdxView(void) {}
  template<class View>
  forceinline
  IterIdxView<View>::IterIdxView(const IdxView<View>* b,
                                 const IdxView<View>* e)
    : cur(b), end(e) {}
  template<class View>
  forceinline void
  IterIdxView<View>::init(const IdxView<View>* b,
                          const IdxView<View>* e) {
    cur=b; end=e;
  }
  template<class View>
  forceinline bool
  IterIdxView<View>::operator ()(void) const {
    return cur < end;
  }
  template<class View>
  forceinline void
  IterIdxView<View>::operator ++(void) {
    cur++;
  }
  template<class View>
  forceinline int
  IterIdxView<View>::val(void) const {
    return cur->idx;
  }




  /*
   * Generic scanning: does all but computing new domain for result
   * (which is specific to bounds/domain version)
   *
   */

  template<class VA, class VB, class VC, PropCond pc_ac, class RelTest>
  ExecStatus
  scan(Space& home, IdxViewArray<VA>& iv,
       VB x0, VC x1, Propagator& p, RelTest rt) {
    assert(iv.size() > 1);
    /*
     * Prunes pairs of index, variable
     *  - checks for idx value removed
     *  - checks for disequal variables
     *
     */
    ViewValues<VB> vx0(x0);
    int i = 0;
    int j = 0;
    while (vx0() && (i < iv.size())) {
      if (iv[i].idx < vx0.val()) {
        iv[i].view.cancel(home,p,pc_ac);
        ++i;
      } else if (iv[i].idx > vx0.val()) {
        ++vx0;
      } else {
        assert(iv[i].idx == vx0.val());
        switch (rt(iv[i].view,x1)) {
        case RT_FALSE:
          iv[i].view.cancel(home,p,pc_ac);
          break;
        case RT_TRUE:
        case RT_MAYBE:
          iv[j++] = iv[i];
          break;
        default: GECODE_NEVER;
        }
        ++vx0; ++i;
      }
    }
    while (i < iv.size())
      iv[i++].view.cancel(home,p,pc_ac);
    bool adjust = (j<iv.size());
    iv.size(j);

    if (iv.size() == 0)
      return ES_FAILED;

    if (iv.size() == 1) {
      GECODE_ME_CHECK(x0.eq(home,iv[0].idx));
    } else if (adjust) {
      IterIdxView<VA> v(&iv[0],&iv[0]+iv.size());
      GECODE_ME_CHECK(x0.narrow_v(home,v,false));
      assert(x0.size() == static_cast<unsigned int>(iv.size()));
    }
    return ES_OK;
  }




  /*
   * Bounds consistent propagator
   *
   */

  template<class VA, class VB, class VC>
  forceinline
  ViewBnd<VA,VB,VC>::ViewBnd(Home home,
                             IdxViewArray<VA>& iv, VB x0, VC x1)
    : View<VA,VB,VC,PC_INT_BND>(home,iv,x0,x1) {}

  template<class VA, class VB, class VC>
  ExecStatus
  ViewBnd<VA,VB,VC>::post(Home home,
                          IdxViewArray<VA>& iv, VB x0, VC x1) {
    GECODE_ME_CHECK(x0.gq(home,0));
    GECODE_ME_CHECK(x0.le(home,iv.size()));
    if (x0.assigned()) {
      (void) new (home) Rel::EqBnd<VA,VC>(home,iv[x0.val()].view,x1);
      return ES_OK;
    } else {
      assert(iv.size()>1);
      (void) new (home) ViewBnd<VA,VB,VC>(home,iv,x0,x1);
    }
    return ES_OK;
  }


  template<class VA, class VB, class VC>
  forceinline
  ViewBnd<VA,VB,VC>::ViewBnd(Space& home, bool share, ViewBnd& p)
    : View<VA,VB,VC,PC_INT_BND>(home,share,p) {}

  template<class VA, class VB, class VC>
  Actor*
  ViewBnd<VA,VB,VC>::copy(Space& home, bool share) {
    return new (home) ViewBnd<VA,VB,VC>(home,share,*this);
  }

  template<class VA, class VB, class VC>
  ExecStatus
  ViewBnd<VA,VB,VC>::propagate(Space& home, const ModEventDelta&) {
    assert(iv.size() > 1);
    RelTestBnd<VA,VC> rt;
    GECODE_ES_CHECK((scan<VA,VB,VC,PC_INT_BND,RelTestBnd<VA,VC> >
                     (home,iv,x0,x1,*this,rt)));
    if (iv.size() == 1) {
      ExecStatus es = home.ES_SUBSUMED(*this);
      (void) new (home) Rel::EqBnd<VA,VC>(home,iv[0].view,x1);
      return es;
    }
    assert(iv.size() > 1);
    // Compute new result
    int min = iv[iv.size()-1].view.min();
    int max = iv[iv.size()-1].view.max();
    for (int i=iv.size()-1; i--; ) {
      min = std::min(iv[i].view.min(),min);
      max = std::max(iv[i].view.max(),max);
    }
    ExecStatus es = shared(x0,x1) ? ES_NOFIX : ES_FIX;
    {
     ModEvent me = x1.lq(home,max);
     if (me_failed(me))
       return ES_FAILED;
     if (me_modified(me) && (x1.max() != max))
       es = ES_NOFIX;
    }
    {
     ModEvent me = x1.gq(home,min);
     if (me_failed(me))
       return ES_FAILED;
     if (me_modified(me) && (x1.min() != min))
       es = ES_NOFIX;
    }
    return (x1.assigned() && (min == max)) ?
      home.ES_SUBSUMED(*this) : es;
  }





  /*
   * Domain consistent propagator
   *
   */

  template<class VA, class VB, class VC>
  forceinline
  ViewDom<VA,VB,VC>::ViewDom(Home home,
                             IdxViewArray<VA>& iv, VB x0, VC x1)
    : View<VA,VB,VC,PC_INT_DOM>(home,iv,x0,x1) {}

  template<class VA, class VB, class VC>
  ExecStatus
  ViewDom<VA,VB,VC>::post(Home home,
                          IdxViewArray<VA>& iv, VB x0, VC x1){
    GECODE_ME_CHECK(x0.gq(home,0));
    GECODE_ME_CHECK(x0.le(home,iv.size()));
    if (x0.assigned()) {
      (void) new (home) Rel::EqDom<VA,VC>(home,iv[x0.val()].view,x1);
      return ES_OK;
    } else {
      assert(iv.size()>1);
      (void) new (home) ViewDom<VA,VB,VC>(home,iv,x0,x1);
    }
    return ES_OK;
  }


  template<class VA, class VB, class VC>
  forceinline
  ViewDom<VA,VB,VC>::ViewDom(Space& home, bool share, ViewDom& p)
    : View<VA,VB,VC,PC_INT_DOM>(home,share,p) {}

  template<class VA, class VB, class VC>
  Actor*
  ViewDom<VA,VB,VC>::copy(Space& home, bool share) {
    return new (home) ViewDom<VA,VB,VC>(home,share,*this);
  }


  template<class VA, class VB, class VC>
  PropCost
  ViewDom<VA,VB,VC>::cost(const Space&, const ModEventDelta& med) const {
    return PropCost::linear((VA::me(med) != ME_INT_DOM) ?
                            PropCost::LO : PropCost::HI, iv.size()+2);
  }

  template<class VA, class VB, class VC>
  ExecStatus
  ViewDom<VA,VB,VC>::propagate(Space& home, const ModEventDelta& med) {
    assert(iv.size() > 1);
    if (VA::me(med) != ME_INT_DOM) {
      RelTestBnd<VA,VC> rt;
      GECODE_ES_CHECK((scan<VA,VB,VC,PC_INT_DOM,RelTestBnd<VA,VC> >
                       (home,iv,x0,x1,*this,rt)));
      if (iv.size() == 1) {
        ExecStatus es = home.ES_SUBSUMED(*this);
        (void) new (home) Rel::EqDom<VA,VC>(home,iv[0].view,x1);
        return es;
      }
      // Compute new result
      int min = iv[iv.size()-1].view.min();
      int max = iv[iv.size()-1].view.max();
      for (int i=iv.size()-1; i--; ) {
        min = std::min(iv[i].view.min(),min);
        max = std::max(iv[i].view.max(),max);
      }
      GECODE_ME_CHECK(x1.lq(home,max));
      GECODE_ME_CHECK(x1.gq(home,min));
      return (x1.assigned() && (min == max)) ?
        home.ES_SUBSUMED(*this) :
        home.ES_NOFIX_PARTIAL(*this,VA::med(ME_INT_DOM));
    }
    RelTestDom<VA,VC> rt;
    GECODE_ES_CHECK((scan<VA,VB,VC,PC_INT_DOM,RelTestDom<VA,VC> >
                     (home,iv,x0,x1,*this,rt)));
    if (iv.size() == 1) {
      ExecStatus es = home.ES_SUBSUMED(*this);
      (void) new (home) Rel::EqDom<VA,VC>(home,iv[0].view,x1);
      return es;
    }
    assert(iv.size() > 1);
    
    if (x1.assigned()) {
      for (int i = iv.size(); i--; )
        if (iv[i].view.in(x1.val()))
          return shared(x0,x1) ? ES_NOFIX : ES_FIX;
      return ES_FAILED;
    } else {
      Region r(home);
      ViewRanges<VA>* i_view = r.alloc<ViewRanges<VA> >(iv.size());
      for (int i = iv.size(); i--; )
        i_view[i].init(iv[i].view);
      Iter::Ranges::NaryUnion i_val(r, i_view, iv.size());
      ModEvent me = x1.inter_r(home,i_val);
      r.free<ViewRanges<VA> >(i_view,iv.size());
      GECODE_ME_CHECK(me);
      return (shared(x0,x1) || me_modified(me)) ? ES_NOFIX : ES_FIX;
    }
  }

}}}

// STATISTICS: int-prop

