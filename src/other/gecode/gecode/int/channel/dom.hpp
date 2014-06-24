/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
 *     Mikael Lagerkvist, 2006
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

namespace Gecode { namespace Int { namespace Channel {

  /**
   * \brief Combine view with information for domain propagation
   *
   */
  template<class View, class Offset>
  class DomInfo {
  public:
    /// The view
    View view;
    /// Last propagated size
    unsigned int size;
    /// Last propagated minimum
    int min;
    /// Last propagated maximum
    int max;
    /// Initialize
    void init(View x, int n);
    /// Update during cloning
    void update(Space& home, bool share, DomInfo<View,Offset>& vcb);
    /// Check whether propagation for assignment is to be done
    bool doval(void) const;
    /// Check whether propagation for domain is to be done
    bool dodom(void) const;
    /// Record that view got assigned
    void assigned(void);
    /// Record that one value got removed
    void removed(int i);
    /// Update the size and bounds information after pruning
    void done(Offset& o);
  };

  template<class View, class Offset>
  forceinline void
  DomInfo<View,Offset>::init(View x, int n) {
    view = x;
    size = static_cast<unsigned int>(n);
    min  = 0;
    max  = n-1;
  }

  template<class View, class Offset>
  forceinline void
  DomInfo<View,Offset>::update(Space& home, bool share,
                               DomInfo<View,Offset>& di) {
    view.update(home,share,di.view);
    size = di.size;
    min  = di.min;
    max  = di.max;
  }

  template<class View, class Offset>
  forceinline bool
  DomInfo<View,Offset>::doval(void) const {
    return (size != 1) && view.assigned();
  }

  template<class View, class Offset>
  forceinline bool
  DomInfo<View,Offset>::dodom(void) const {
    return size != view.size();
  }

  template<class View, class Offset>
  forceinline void
  DomInfo<View,Offset>::assigned(void) {
    size = 1;
  }

  template<class View, class Offset>
  forceinline void
  DomInfo<View,Offset>::removed(int i) {
    size--;
    if (i == min)
      min++;
    else if (i == max)
      max--;
  }

  template<class View, class Offset>
  forceinline void
  DomInfo<View,Offset>::done(Offset& o) {
    size = view.size();
    min  = o(view).min();
    max  = o(view).max();
  }

  // Propagate domain information from x to y
  template<class View, class Offset>
  ExecStatus
  prop_dom(Space& home, int n, DomInfo<View,Offset>* x, Offset& ox,
           DomInfo<View,Offset>* y, Offset& oy, ProcessStack& ya) {
    for (int i = n; i--; )
      // Only views with not yet propagated missing values
      if (x[i].dodom()) {
        // Iterate the values in the complement of x[i]
        ViewRanges<typename Offset::ViewType>
          xir(ox(x[i].view));
        Iter::Ranges::ComplVal<ViewRanges<typename Offset::ViewType> >
          xirc(x[i].min,x[i].max,xir);
        Iter::Ranges::ToValues<Iter::Ranges::ComplVal<
          ViewRanges<typename Offset::ViewType> > > jv(xirc);
        while (jv()) {
          // j is not in the domain of x[i], so prune i from y[j]
          int j = jv.val();
          ModEvent me = oy(y[j].view).nq(home,i);
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            if (me == ME_INT_VAL) {
              // Record that y[j] has been assigned and must be propagated
              ya.push(j);
            } else {
              // Obvious as x[i] is different from j
              y[j].removed(i);
            }
          }
          ++jv;
        }
        // Update which values have been propagated and what are the new bounds
        x[i].done(ox);
      }
    return ES_OK;
  }

  /*
   * The actual propagator
   *
   */
  template<class View, class Offset, bool shared>
  forceinline
  Dom<View,Offset,shared>::Dom(Home home, int n, DomInfo<View,Offset>* xy,
                               Offset& ox, Offset& oy)
    : Base<DomInfo<View,Offset>,Offset,PC_INT_DOM>(home,n,xy,ox,oy) {}

  template<class View, class Offset, bool shared>
  forceinline
  Dom<View,Offset,shared>::Dom(Space& home, bool share, 
                               Dom<View,Offset,shared>& p)
    : Base<DomInfo<View,Offset>,Offset,PC_INT_DOM>(home,share,p) {}

  template<class View, class Offset, bool shared>
  Actor*
  Dom<View,Offset,shared>::copy(Space& home, bool share) {
    return new (home) Dom<View,Offset,shared>(home,share,*this);
  }

  template<class View, class Offset, bool shared>
  PropCost
  Dom<View,Offset,shared>::cost(const Space&,
                                const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_VAL)
      return PropCost::quadratic(PropCost::LO, 2*n);
    else
      return PropCost::quadratic(PropCost::HI, 2*n);
  }

  template<class View, class Offset, bool shared>
  ExecStatus
  Dom<View,Offset,shared>::propagate(Space& home, const ModEventDelta& med) {
    Region r(home);
    ProcessStack xa(r,n);
    ProcessStack ya(r,n);

    DomInfo<View,Offset>* x = xy;
    DomInfo<View,Offset>* y = xy+n;

    if (View::me(med) == ME_INT_VAL) {
      // Scan x and y for assigned but not yet propagated views
      for (int i = n; i--; ) {
        if (x[i].doval()) xa.push(i);
        if (y[i].doval()) ya.push(i);
      }

      if (shared) {
        do {
          // Propagate assigned views for x
          GECODE_ES_CHECK((prop_val<View,Offset,DomInfo<View,Offset> >
                           (home,n,x,ox,y,oy,n_na,xa,ya)));
          // Propagate assigned views for y
          GECODE_ES_CHECK((prop_val<View,Offset,DomInfo<View,Offset> >
                           (home,n,y,oy,x,ox,n_na,ya,xa)));
          assert(ya.empty());
        } while (!xa.empty() || !ya.empty());
        return home.ES_NOFIX_PARTIAL(*this,View::med(ME_INT_DOM));
      } else {
        do {
          // Propagate assigned views for x
          GECODE_ES_CHECK((prop_val<View,Offset,DomInfo<View,Offset> >
                           (home,n,x,ox,y,oy,n_na,xa,ya)));
          // Propagate assigned views for y
          GECODE_ES_CHECK((prop_val<View,Offset,DomInfo<View,Offset> >
                           (home,n,y,oy,x,ox,n_na,ya,xa)));
          assert(ya.empty());
        } while (!xa.empty());
        return home.ES_FIX_PARTIAL(*this,View::med(ME_INT_DOM));
      }
    }

    // Process changed views for y
    // This propagates from y to x and possibly records xs that
    // got assigned
    GECODE_ES_CHECK(prop_dom(home,n,y,oy,x,ox,xa));

    // Process assigned views for x
    GECODE_ES_CHECK((prop_val<View,Offset,DomInfo<View,Offset> >
                     (home,n,x,ox,y,oy,n_na,xa,ya)));

    // Perform domain consistent propagation for distinct on the x views
    if (dc.available()) {
      GECODE_ES_CHECK(dc.sync(home));
    } else {
      ViewArray<View> xv(r,n);
      for (int i=n; i--; )
        xv[i] = x[i].view;
      GECODE_ES_CHECK(dc.init(home,xv));
    }
    bool assigned;
    GECODE_ES_CHECK(dc.propagate(home,assigned));
    if (assigned) {
      // This has assigned some more views in x which goes unnoticed
      // (that is, not recorded in xa). This must be checked and propagated
      // to the y views, however the distinctness on x is already
      // propagated.
      for (int i=n; i--; )
        if (x[i].doval()) {
          int j = ox(x[i].view).val();
          // Assign the y variable to i (or test if already assigned!)
          ModEvent me = oy(y[j].view).eq(home,i);
          if (me_failed(me))
            return ES_FAILED;
          if (me_modified(me)) {
            // Record that y[j] has been assigned and must be propagated
            assert(me == ME_INT_VAL);
            // Otherwise the modification event would not be ME_INT_VAL
            ya.push(j);
          }
          x[i].assigned(); n_na--;
        }
    }

    // Process changed views for x
    // This propagates from x to y and possibly records ys that
    // got assigned
    GECODE_ES_CHECK(prop_dom(home,n,x,ox,y,oy,ya));

    // Process assigned view again (some might have been found above)
    while (!xa.empty() || !ya.empty()) {
      // Process assigned views for x
      GECODE_ES_CHECK((prop_val<View,Offset,DomInfo<View,Offset> >
                       (home,n,x,ox,y,oy,n_na,xa,ya)));
      // Process assigned views for y
      GECODE_ES_CHECK((prop_val<View,Offset,DomInfo<View,Offset> >
                       (home,n,y,oy,x,ox,n_na,ya,xa)));
    };

    if (shared) {
      for (int i=2*n; i--; )
        if (!xy[i].view.assigned())
          return ES_NOFIX;
      return home.ES_SUBSUMED(*this);
    } else {
      return (n_na == 0) ? home.ES_SUBSUMED(*this) : ES_FIX;
    }
  }

  template<class View, class Offset, bool shared>
  ExecStatus
  Dom<View,Offset,shared>::post(Home home, int n, DomInfo<View,Offset>* xy,
                                Offset& ox, Offset& oy) {
    assert(n > 0);
    if (n == 1) {
      GECODE_ME_CHECK(ox(xy[0].view).eq(home,0));
      GECODE_ME_CHECK(oy(xy[1].view).eq(home,0));
      return ES_OK;
    }
    for (int i=n; i--; ) {
      GECODE_ME_CHECK(ox(xy[i  ].view).gq(home,0));
      GECODE_ME_CHECK(ox(xy[i  ].view).le(home,n));
      GECODE_ME_CHECK(oy(xy[i+n].view).gq(home,0));
      GECODE_ME_CHECK(oy(xy[i+n].view).le(home,n));
    }
    (void) new (home) Dom<View,Offset,shared>(home,n,xy,ox,oy);
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

