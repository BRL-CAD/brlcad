/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
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

namespace Gecode { namespace Set { namespace Element {

  template<class SView, class RView>
  forceinline
  ElementDisjoint<SView,RView>::ElementDisjoint(Home home,
                                                IdxViewArray& iv0,
                                                RView y1)
    : Propagator(home), iv(iv0), x1(y1) {

    x1.subscribe(home,*this, PC_SET_ANY);
    iv.subscribe(home,*this, PC_SET_ANY);
  }

  template<class SView, class RView>
  forceinline
  ElementDisjoint<SView,RView>::ElementDisjoint(Space& home, bool share,  
                                                ElementDisjoint& p)
    : Propagator(home,share,p) {
    x1.update(home,share,p.x1);
    iv.update(home,share,p.iv);
  }

  template<class SView, class RView>
  forceinline ExecStatus
  ElementDisjoint<SView,RView>::post(Home home, IdxViewArray& xs,
                                     RView x1) {
    int n = xs.size();

    // s2 \subseteq {0,...,n-1}
    Iter::Ranges::Singleton s(0, n-1);
    GECODE_ME_CHECK(x1.intersectI(home,s));
    (void) new (home)
      ElementDisjoint(home,xs,x1);
    return ES_OK;
  }

  template<class SView, class RView>
  PropCost
  ElementDisjoint<SView,RView>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::LO, iv.size()+2);
  }

  template<class SView, class RView>
  forceinline size_t
  ElementDisjoint<SView,RView>::dispose(Space& home) {
    x1.cancel(home,*this, PC_SET_ANY);
    iv.cancel(home,*this,PC_SET_ANY);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class SView, class RView>
  Actor*
  ElementDisjoint<SView,RView>::copy(Space& home, bool share) {
    return new (home) ElementDisjoint(home,share,*this);
  }

  template<class SView, class RView>
  ExecStatus
  ElementDisjoint<SView,RView>::propagate(Space& home, const ModEventDelta&) {
    int n = iv.size();

    Region r(home);

    bool fix_flag = false;
    do {
      fix_flag = false;
      // Compute union of all selected elements' lower bounds
      GlbRanges<RView> x1lb(x1);
      Iter::Ranges::ToValues<GlbRanges<RView> > vx1lb(x1lb);
      GLBndSet unionOfSelected(home);
      for(int i=0; vx1lb(); ++vx1lb) {
        while (iv[i].idx < vx1lb.val()) i++;
        GlbRanges<SView> clb(iv[i].view);
        unionOfSelected.includeI(home,clb);
      }

      {
        LubRanges<RView> x1ub(x1);
        Iter::Ranges::ToValues<LubRanges<RView> > vx1ub(x1ub);
        int i=0;
        int j=0;
        // Cancel all elements that are no longer in the upper bound
        while (vx1ub()) {
          if (iv[i].idx < vx1ub.val()) {
            iv[i].view.cancel(home,*this, PC_SET_ANY);
            i++;
            continue;
          }
          iv[j] = iv[i];
          ++vx1ub;
          ++i; ++j;
        }

        // cancel the variables with index greater than
        // max of lub(x1)
        for (int k=i; k<n; k++) {
          iv[k].view.cancel(home,*this, PC_SET_ANY);
        }
        n = j;
        iv.size(n);
      }

      {
      UnknownRanges<RView> x1u(x1);
      Iter::Ranges::Cache x1uc(r,x1u);
      Iter::Ranges::ToValues<Iter::Ranges::Cache>
        vx1u(x1uc);
      int i=0;
      int j=0;
      while (vx1u()) {
        while (iv[i].idx < vx1u.val()) {
          iv[j] = iv[i];
          i++; j++;
        }
        assert(iv[i].idx == vx1u.val());

        SView candidate = iv[i].view;
        int candidateInd = iv[i].idx;

        GlbRanges<SView> clb(candidate);
        BndSetRanges uos(unionOfSelected);
        Iter::Ranges::Inter<GlbRanges<SView>, BndSetRanges>
          inter(clb, uos);
        if (inter()) {
          ModEvent me = x1.exclude(home,candidateInd);
          fix_flag |= me_modified(me);
          GECODE_ME_CHECK(me);

          candidate.cancel(home,*this, PC_SET_ANY);
          ++i;
          ++vx1u;
          continue;
        }
        iv[j] = iv[i];
        ++vx1u;
        ++i; ++j;
      }

      unionOfSelected.dispose(home);

      // copy remaining variables
      for (int k=i; k<n; k++) {
        iv[j] = iv[k];
        j++;
      }
      n = j;
      iv.size(n);
      }

      if (x1.cardMax()==0) {
        // Selector is empty, we're done
        return home.ES_SUBSUMED(*this);
      }

      {
        // remove all elements in a selected variable from
        // all other selected variables
        GlbRanges<RView> x1lb(x1);
        Iter::Ranges::ToValues<GlbRanges<RView> > vx1lb(x1lb);
        int i=0;
        for (; vx1lb(); ++vx1lb) {
          while (iv[i].idx < vx1lb.val()) i++;
          assert(iv[i].idx==vx1lb.val());
          GlbRanges<RView> x1lb2(x1);
          Iter::Ranges::ToValues<GlbRanges<RView> > vx1lb2(x1lb2);
          for (int j=0; vx1lb2(); ++vx1lb2) {
            while (iv[j].idx < vx1lb2.val()) j++;
            assert(iv[j].idx==vx1lb2.val());
            if (iv[i].idx!=iv[j].idx) {
              GlbRanges<SView> xilb(iv[i].view);
              ModEvent me = iv[j].view.excludeI(home,xilb);
              fix_flag |= me_modified(me);
              GECODE_ME_CHECK(me);
            }
          }
        }
      }

      // remove all elements from the selector that overlap
      // with all other possibly selected elements, if
      // at least two more elements need to be selected
      if (x1.cardMin()-x1.glbSize() > 1) {
        UnknownRanges<RView> x1u(x1);
        Iter::Ranges::Cache x1uc(r,x1u);
        Iter::Ranges::ToValues<Iter::Ranges::Cache>
          vx1u(x1uc);

        for (; vx1u() && x1.cardMin()-x1.glbSize() > 1; ++vx1u) {
          int i = 0;
          while (iv[i].idx < vx1u.val()) i++;
          assert(iv[i].idx == vx1u.val());
          bool flag=true;

          UnknownRanges<RView> x1u2(x1);
          Iter::Ranges::ToValues<UnknownRanges<RView> > vx1u2(x1u2);
          for (; vx1u2(); ++vx1u2) {
            int j = 0;
            while (iv[j].idx < vx1u2.val()) j++;
            assert(iv[j].idx == vx1u2.val());
            if (iv[i].idx!=iv[j].idx) {
              GlbRanges<SView> xjlb(iv[j].view);
              GlbRanges<SView> xilb(iv[i].view);
              Iter::Ranges::Inter<GlbRanges<SView>, GlbRanges<SView> >
                inter(xjlb, xilb);
              if (!inter()) {
                flag = false;
                goto here;
              }
            }
          }
        here:
          if (flag) {
            ModEvent me = x1.exclude(home,iv[i].idx);
            fix_flag |= me_modified(me);
            GECODE_ME_CHECK(me);
          }
        }
      }

      // if exactly two more elements need to be selected
      // and there is a possible element i such that all other pairs of
      // elements overlap, select i
      UnknownRanges<RView> x1u(x1);
      Iter::Ranges::Cache x1uc(r,x1u);
      Iter::Ranges::ToValues<Iter::Ranges::Cache>
        vx1u(x1uc);

      for (; x1.cardMin()-x1.glbSize() == 2 && vx1u(); ++vx1u) {
        int i = 0;
        while (iv[i].idx < vx1u.val()) i++;
        assert (iv[i].idx == vx1u.val());
        bool flag=true;

        UnknownRanges<RView> x1u2(x1);
        Iter::Ranges::ToValues<UnknownRanges<RView> > vx1u2(x1u2);
        for (; vx1u2(); ++vx1u2) {
          int j = 0;
          while (iv[j].idx < vx1u2.val()) j++;
          assert (iv[j].idx == vx1u2.val());
          if (iv[i].idx!=iv[j].idx) {
            UnknownRanges<RView> x1u3(x1);
            Iter::Ranges::ToValues<UnknownRanges<RView> > vx1u3(x1u3);
            for (; vx1u3(); ++vx1u3) {
              int k = 0;
              while (iv[k].idx < vx1u3.val()) k++;
              assert (iv[k].idx == vx1u3.val());
              if (iv[j].idx!=iv[k].idx && iv[i].idx!=iv[k].idx) {
                GlbRanges<SView> xjlb(iv[j].view);
                GlbRanges<SView> xilb(iv[k].view);
                Iter::Ranges::Inter<GlbRanges<SView>, GlbRanges<SView> >
                  inter(xjlb, xilb);
                if (!inter()) {
                  flag = false;
                  goto here2;
                }
              }
            }
          }
        }
      here2:
        if (flag) {
          ModEvent me = x1.include(home,iv[i].idx);
          fix_flag |= me_modified(me);
          GECODE_ME_CHECK(me);
        }
      }
    } while (fix_flag);

    bool allAssigned = true;
    for (int i=iv.size(); i--;)
      if (!iv[i].view.assigned()) {
        allAssigned = false;
        break;
      }
    if (!x1.assigned())
      allAssigned = false;

    return allAssigned ? home.ES_SUBSUMED(*this) : ES_FIX;
  }


}}}

// STATISTICS: set-prop
