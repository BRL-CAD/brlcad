/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
 *     Denys Duchier <denys.duchier@univ-orleans.fr>
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

namespace Gecode { namespace Set { namespace Int {

  template<class View>
  forceinline
  MinElement<View>::MinElement(Home home, View y0, Gecode::Int::IntView y1)
    : MixBinaryPropagator<View,PC_SET_ANY,Gecode::Int::IntView,Gecode::Int::PC_INT_BND> (home, y0, y1) {}

  template<class View>
  forceinline ExecStatus
  MinElement<View>::post(Home home, View x0, Gecode::Int::IntView x1) {
    GECODE_ME_CHECK(x0.cardMin(home,1));
    (void) new (home) MinElement(home,x0,x1);
    return ES_OK;
  }

  template<class View>
  forceinline
  MinElement<View>::MinElement(Space& home, bool share, MinElement& p)
    : MixBinaryPropagator<View,PC_SET_ANY,Gecode::Int::IntView,Gecode::Int::PC_INT_BND> (home, share, p) {}

  template<class View>
  Actor*
  MinElement<View>::copy(Space& home, bool share) {
   return new (home) MinElement(home,share,*this);
  }

  template<class View>
  ExecStatus
  MinElement<View>::propagate(Space& home, const ModEventDelta&) {
    //x1 is an element of x0.ub
    //x1 =< smallest element of x0.lb
    //x1 =< x0.cardinialityMin-est largest element of x0.ub
    //(these 2 take care of determined x0)
    //No element in x0 is smaller than x1
    //if x1 is determined, it is part of the ub.

    //Consequently:
    //The domain of x1 is a subset of x0.ub up to the first element in x0.lb.
    //x0 lacks everything smaller than smallest possible x1.

    {
      LubRanges<View> ub(x0);
      GECODE_ME_CHECK(x1.inter_r(home,ub,false));
    }
    GECODE_ME_CHECK(x1.lq(home,x0.glbMin()));
    //if cardMin>lbSize?
    assert(x0.cardMin()>=1);

    {
      /// Compute n-th largest element in x0.lub for n = x0.cardMin()-1
      unsigned int size = 0;
      int maxN = BndSet::MAX_OF_EMPTY;
      for (LubRanges<View> ubr(x0); ubr(); ++ubr, ++size) {}
      Region r(home);
      int* ub = r.alloc<int>(size*2);
      int i=0;
      for (LubRanges<View> ubr(x0); ubr(); ++ubr, ++i) {
        ub[2*i]   = ubr.min();
        ub[2*i+1] = ubr.max();
      }
      unsigned int x0cm = x0.cardMin()-1;
      for (unsigned int i=size; i--;) {
        unsigned int width = static_cast<unsigned int>(ub[2*i+1]-ub[2*i]+1);
        if (width > x0cm) {
          maxN = static_cast<int>(ub[2*i+1]-x0cm);
          break;
        }
        x0cm -= width;
      }
      GECODE_ME_CHECK(x1.lq(home,maxN));
    }

    GECODE_ME_CHECK( x0.exclude(home,
                                Limits::min, x1.min()-1) );

    if (x1.assigned()) {
      GECODE_ME_CHECK(x0.include(home,x1.val()));
      GECODE_ME_CHECK(x0.exclude(home,
                                 Limits::min, x1.val()-1));
      return home.ES_SUBSUMED(*this);
    }

    return ES_FIX;
  }


  template<class View>
  forceinline
  NotMinElement<View>::NotMinElement(Home home, View y0,
                                     Gecode::Int::IntView y1)
    : MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM> (home, y0, y1) {}

  template<class View>
  forceinline ExecStatus
  NotMinElement<View>::post(Home home, View x0, Gecode::Int::IntView x1) {
    (void) new (home) NotMinElement(home,x0,x1);
    return ES_OK;
  }

  template<class View>
  forceinline
  NotMinElement<View>::NotMinElement(Space& home, bool share,
                                     NotMinElement& p)
    : MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM> (home, share, p) {}

  template<class View>
  Actor*
  NotMinElement<View>::copy(Space& home, bool share) {
    return new (home) NotMinElement(home,share,*this);
  }

  template<class View>
  ExecStatus
  NotMinElement<View>::propagate(Space& home, const ModEventDelta&) {
    // cheap tests for entailment:
    // if x0 is empty, then entailed
    // if max(x1) < min(x0.lub) or min(x1) > max(x0.lub), then entailed
    // if min(x0.glb) < min(x1), then entailed
    if ((x0.cardMax() == 0) ||
        ((x1.max() < x0.lubMin()) || (x1.min() > x0.lubMax())) ||
        ((x0.glbSize() > 0) && (x0.glbMin() < x1.min())))
      return home.ES_SUBSUMED(*this);
    // if x1 is determined and = x0.lub.min: remove it from x0,
    // then entailed
    if (x1.assigned() && x1.val()==x0.lubMin()) {
      GECODE_ME_CHECK(x0.exclude(home,x1.val()));
      return home.ES_SUBSUMED(*this);
    }
    // if min(x0) is decided: remove min(x0) from the domain of x1
    // then entailed
    if (x0.glbMin() == x0.lubMin()) {
      GECODE_ME_CHECK(x1.nq(home,x0.glbMin()));
      return home.ES_SUBSUMED(*this);
    }
    // if x1 is determined and = x0.glb.min, then we need at least
    // one more element; if there is only one below, then we must
    // take it.
    if (x1.assigned() && x0.glbSize() > 0 && x1.val()==x0.glbMin()) {
      unsigned int oldGlbSize = x0.glbSize();
      // if there is only 1 unknown below x1, then we must take it
      UnknownRanges<View> ur(x0);
      assert(ur());
      // the iterator is not empty: otherwise x0 would be determined
      // and min(x0) would have been decided and the preceding if
      // would have caught it.  Also, the first range is not above
      // x1 otherwise the very first if would have caught it.
      // so let's check if the very first range of unknowns is of
      // size 1 and there is no second one or it starts above x1.
      if (ur.width()==1) {
        int i=ur.min();
        ++ur;
        if (!ur() || ur.min()>x1.val()) {
          GECODE_ME_CHECK(x0.include(home,i));
          return home.ES_SUBSUMED(*this);
        }
      }
      GECODE_ME_CHECK(x0.cardMin(home, oldGlbSize+1));
    }
    // if dom(x1) and lub(x0) are disjoint, then entailed;
    {
      LubRanges<View> ub(x0);
      Gecode::Int::ViewRanges<Gecode::Int::IntView> d(x1);
      Gecode::Iter::Ranges::Inter<LubRanges<View>,
        Gecode::Int::ViewRanges<Gecode::Int::IntView> > ir(ub,d);
      if (!ir()) return home.ES_SUBSUMED(*this);
    }
    // x0 is fated to eventually contain at least x0.cardMin elements.
    // therefore min(x0) <= x0.cardMin-th largest element of x0.lub
    // if x1 > than that, then entailed.
    {
      // to find the x0.cardMin-th largest element of x0.lub, we need
      // some sort of reverse range iterator. we will need to fake one
      // by storing the ranges of the forward iterator in an array.
      // first we need to know how large the array needs to be. so, let's
      // count the ranges:
      int num_ranges = 0;
      for (LubRanges<View> ur(x0); ur(); ++ur, ++num_ranges) {}
      // create an array for storing min and max of each range
      Region r(home);
      int* _ur = r.alloc<int>(num_ranges*2);
      // now, we fill the array:
      int i = 0;
      for (LubRanges<View> ur(x0); ur(); ++ur, ++i) {
        _ur[2*i  ] = ur.min();
        _ur[2*i+1] = ur.max();
      }
      // now we search from the top the range that contains the
      // nth largest value.
      unsigned int n = x0.cardMin();
      int nth_largest = BndSet::MAX_OF_EMPTY;
      for (int i=num_ranges; i--; ) {
        // number of values in range
        unsigned int num_values = static_cast<unsigned int>(_ur[2*i+1]-_ur[2*i]+1);
        // does the range contain the value?
        if (num_values >= n) {
          // record it and exit the loop
          nth_largest = static_cast<int>(_ur[2*i+1]-n+1);
          break;
        }
        // otherwise, we skipped num_values
        n -= num_values;
      }
      // if x1.min > nth_largest, then entailed
      if (x1.min() > nth_largest)
        return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }

  template<class View, ReifyMode rm>
  forceinline
  ReMinElement<View,rm>::ReMinElement(Home home, View y0,
                                      Gecode::Int::IntView y1,
                                      Gecode::Int::BoolView b2)
    : Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,
      Gecode::Int::BoolView> (home, y0, y1, b2) {}

  template<class View, ReifyMode rm>
  forceinline ExecStatus
  ReMinElement<View,rm>::post(Home home, View x0, Gecode::Int::IntView x1,
                              Gecode::Int::BoolView b2) {
    (void) new (home) ReMinElement(home,x0,x1,b2);
    return ES_OK;
  }

  template<class View, ReifyMode rm>
  forceinline
  ReMinElement<View,rm>::ReMinElement(Space& home, bool share,
                                      ReMinElement& p)
    : Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,
      Gecode::Int::BoolView> (home, share, p) {}

  template<class View, ReifyMode rm>
  Actor*
  ReMinElement<View,rm>::copy(Space& home, bool share) {
   return new (home) ReMinElement(home,share,*this);
  }

  template<class View, ReifyMode rm>
  ExecStatus
  ReMinElement<View,rm>::propagate(Space& home, const ModEventDelta&) {
    // check if b is determined
    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this, (MinElement<View>::post(home(*this),x0,x1)));
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return home.ES_SUBSUMED(*this);        
      GECODE_REWRITE(*this, (NotMinElement<View>::post(home(*this),x0,x1)));
    }
    // cheap tests for => b=0
    // if x0 is empty, then b=0 and entailed
    // if max(x1) < min(x0.lub) or min(x1) > max(x0.lub), then b=0 and entailed
    // if min(x0.glb) < min(x1), then b=0 and entailed
    if ((x0.cardMax() == 0) ||
        ((x1.max() < x0.lubMin()) || (x1.min() > x0.lubMax())) ||
        ((x0.glbSize() > 0) && (x0.glbMin() < x1.min())))
      {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        return home.ES_SUBSUMED(*this);
      }
    // if min(x0) is decided
    if (x0.glbMin() == x0.lubMin()) {
      // if x1 is det: check if = min(x0), assign b, entailed
      if (x1.assigned()) {
        if (x1.val() == x0.glbMin()) {
          if (rm != RM_IMP)
            GECODE_ME_CHECK(b.one(home));
        } else {
          if (rm != RM_PMI)
            GECODE_ME_CHECK(b.zero(home));
        }
        return home.ES_SUBSUMED(*this);
      }
      // if min(x0) not in dom(x1): b=0, entailed
      else if ((x0.glbMin() < x1.min()) ||
               (x0.glbMin() > x1.max()) ||
               !x1.in(x0.glbMin()))
        {
          if (rm != RM_PMI)
            GECODE_ME_CHECK(b.zero(home));
          return home.ES_SUBSUMED(*this);
        }
    }
    // // if dom(x1) and lub(x0) are disjoint, then b=0, entailed;
    // {
    //   LubRanges<View> ub(x0);
    //   Gecode::Int::ViewRanges<Gecode::Int::IntView> d(x1);
    //   Gecode::Iter::Ranges::Inter<LubRanges<View>,
    //     Gecode::Int::ViewRanges<Gecode::Int::IntView> > ir(ub,d);
    //   if (!ir()) {
    //     GECODE_ME_CHECK(b.zero(home));
    //     return home.ES_SUBSUMED(*this);
    //   }
    // }
    // // x0 is fated to eventually contain at least x0.cardMin elements.
    // // therefore min(x0) <= x0.cardMin-th largest element of x0.lub
    // // if x1 > than that, then b=0 and entailed.
    // {
    //   // to find the x0.cardMin-th largest element of x0.lub, we need
    //   // some sort of reverse range iterator. we will need to fake one
    //   // by storing the ranges of the forward iterator in an array.
    //   // first we need to know how large the array needs to be. so, let's
    //   // count the ranges:
    //   int num_ranges = 0;
    //   for (LubRanges<View> ur(x0); ur(); ++ur, ++num_ranges) {}
    //   // create an array for storing min and max of each range
    //   Region re(home);
    //   int* _ur = re.alloc<int>(num_ranges*2);
    //   // now, we fill the array:
    //   int i = 0;
    //   for (LubRanges<View> ur(x0); ur(); ++ur, ++i) {
    //     _ur[2*i  ] = ur.min();
    //     _ur[2*i+1] = ur.max();
    //   }
    //   // now we search from the top the range that contains the
    //   // nth largest value.
    //   int n = x0.cardMin();
    //   int nth_largest = BndSet::MAX_OF_EMPTY;
    //   for (int i=num_ranges; i--; ) {
    //     // number of values in range
    //     int num_values = _ur[2*i+1]-_ur[2*i]+1;
    //     // does the range contain the value?
    //     if (num_values >= n)
    //       {
    //         // record it and exit the loop
    //         nth_largest = _ur[2*i+1]-n+1;
    //         break;
    //       }
    //     // otherwise, we skipped num_values
    //     n -= num_values;
    //   }
    //   // if x1.min > nth_largest, then entailed
    //   if (x1.min() > nth_largest) {
    //     GECODE_ME_CHECK(b.zero(home));
    //     return home.ES_SUBSUMED(*this);
    //   }
    // }
    return ES_FIX;
  }

  template<class View>
  forceinline
  MaxElement<View>::MaxElement(Home home, View y0, Gecode::Int::IntView y1)
    : MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND> (home, y0, y1) {}

  template<class View>
  forceinline
  MaxElement<View>::MaxElement(Space& home, bool share, MaxElement& p)
    : MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_BND> (home, share, p) {}

  template<class View>
  ExecStatus
  MaxElement<View>::post(Home home, View x0,
                              Gecode::Int::IntView x1) {
    GECODE_ME_CHECK(x0.cardMin(home,1));
    (void) new (home) MaxElement(home,x0,x1);
    return ES_OK;
  }

  template<class View>
  Actor*
  MaxElement<View>::copy(Space& home, bool share) {
    return new (home) MaxElement(home,share,*this);
  }

  template<class View>
  ExecStatus
  MaxElement<View>::propagate(Space& home, const ModEventDelta&) {
    LubRanges<View> ub(x0);
    GECODE_ME_CHECK(x1.inter_r(home,ub,false));
    GECODE_ME_CHECK(x1.gq(home,x0.glbMax()));
    assert(x0.cardMin()>=1);
    GECODE_ME_CHECK(x1.gq(home,x0.lubMinN(x0.cardMin()-1)));
    GECODE_ME_CHECK(x0.exclude(home,
                               x1.max()+1,Limits::max) );

    if (x1.assigned()) {
      GECODE_ME_CHECK(x0.include(home,x1.val()));
      GECODE_ME_CHECK( x0.exclude(home,
                                  x1.val()+1,Limits::max) );
      return home.ES_SUBSUMED(*this);
    }

    return ES_FIX;
  }

  template<class View>
  forceinline
  NotMaxElement<View>::NotMaxElement(Home home, View y0,
                                     Gecode::Int::IntView y1)
    : MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM> (home, y0, y1) {}

  template<class View>
  forceinline
  NotMaxElement<View>::NotMaxElement(Space& home, bool share,
                                     NotMaxElement& p)
    : MixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM> (home, share, p) {}

  template<class View>
  ExecStatus
  NotMaxElement<View>::post(Home home, View x0, Gecode::Int::IntView x1) {
    (void) new (home) NotMaxElement(home,x0,x1);
    return ES_OK;
  }

  template<class View>
  Actor*
  NotMaxElement<View>::copy(Space& home, bool share) {
    return new (home) NotMaxElement(home,share,*this);
  }

  template<class View>
  ExecStatus
  NotMaxElement<View>::propagate(Space& home, const ModEventDelta&) {
    // cheap tests for entailment:
    // if x0 is empty, then entailed
    // if max(x1) < min(x0.lub) or min(x1) > max(x0.lub), then entailed
    // if max(x0.glb) > max(x1), then entailed
    if ((x0.cardMax() == 0) ||
        ((x1.max() < x0.lubMin()) || (x1.min() > x0.lubMax())) ||
        ((x0.glbSize() > 0) && (x0.glbMax() > x1.max())))
      return home.ES_SUBSUMED(*this);
    // if x1 is determined and = max(x0.lub): remove it from x0,
    // then entailed
    if (x1.assigned() && x1.val()==x0.lubMax()) {
      GECODE_ME_CHECK(x0.exclude(home,x1.val()));
      return home.ES_SUBSUMED(*this);
    }
    // if max(x0) is decided: remove max(x0) from the domain of x1
    // then entailed
    if (x0.glbMax() == x0.lubMax()) {
      GECODE_ME_CHECK(x1.nq(home,x0.glbMax()));
      return home.ES_SUBSUMED(*this);
    }
    // if x1 is determined and = max(x0.glb), then we need at least
    // one more element; if there is only one above, then we must
    // take it.
    if (x1.assigned() && x0.glbSize() > 0 && x1.val()==x0.glbMax()) {
      unsigned int oldGlbSize = x0.glbSize();
      // if there is only 1 unknown above x1, then we must take it
      UnknownRanges<View> ur(x0);
      // there is at least one unknown above x1 otherwise it would
      // have been caught by the if for x1=max(x0.lub)
      while (ur.max() < x1.val()) {
        assert(ur());
        ++ur;
      };
      // if the first range above x1 contains just 1 element,
      // and is the last range, then take that element
      if (ur.width() == 1) {
        int i = ur.min();
        ++ur;
        if (!ur()) {
          // last range
          GECODE_ME_CHECK(x0.include(home,i));
          return home.ES_SUBSUMED(*this);
        }
      }
      GECODE_ME_CHECK(x0.cardMin(home, oldGlbSize+1));
    }
    // if dom(x1) and lub(x0) are disjoint, then entailed
    {
      LubRanges<View> ub(x0);
      Gecode::Int::ViewRanges<Gecode::Int::IntView> d(x1);
      Gecode::Iter::Ranges::Inter<LubRanges<View>,
        Gecode::Int::ViewRanges<Gecode::Int::IntView> > ir(ub,d);
      if (!ir()) return home.ES_SUBSUMED(*this);
    }
    // x0 is fated to eventually contain at least x0.cardMin elements.
    // therefore max(x0) >= x0.cardMin-th smallest element of x0.lub.
    // if x1 < than that, then entailed.
    {
      unsigned int n = x0.cardMin();
      int nth_smallest = BndSet::MIN_OF_EMPTY;
      for (LubRanges<View> ur(x0); ur(); ++ur) {
        if (ur.width() >= n) {
          // record it and exit the loop
          nth_smallest = static_cast<int>(ur.min() + n - 1);
          break;
        }
        // otherwise, we skipped ur.width() values
        n -= ur.width();
      }
      // if x1.max < nth_smallest, then entailed
      if (x1.max() < nth_smallest)
        return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }

  template<class View, ReifyMode rm>
  forceinline
  ReMaxElement<View,rm>::ReMaxElement(Home home, View y0,
                                      Gecode::Int::IntView y1,
                                      Gecode::Int::BoolView b2)
    : Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,
      Gecode::Int::BoolView> (home, y0, y1, b2) {}

  template<class View, ReifyMode rm>
  forceinline
  ReMaxElement<View,rm>::ReMaxElement(Space& home, bool share,
                                      ReMaxElement& p)
    : Gecode::Int::ReMixBinaryPropagator<View,PC_SET_ANY,
      Gecode::Int::IntView,Gecode::Int::PC_INT_DOM,
      Gecode::Int::BoolView> (home, share, p) {}

  template<class View, ReifyMode rm>
  ExecStatus
  ReMaxElement<View,rm>::post(Home home, View x0,
                              Gecode::Int::IntView x1,
                              Gecode::Int::BoolView b2) {
    (void) new (home) ReMaxElement(home,x0,x1,b2);
    return ES_OK;
  }

  template<class View, ReifyMode rm>
  Actor*
  ReMaxElement<View,rm>::copy(Space& home, bool share) {
    return new (home) ReMaxElement(home,share,*this);
  }

  template<class View, ReifyMode rm>
  ExecStatus
  ReMaxElement<View,rm>::propagate(Space& home, const ModEventDelta&) {
    // check if b is determined
    if (b.one()) {
      if (rm == RM_PMI)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this, (MaxElement<View>::post(home(*this),x0,x1)));
    }
    if (b.zero()) {
      if (rm == RM_IMP)
        return home.ES_SUBSUMED(*this);
      GECODE_REWRITE(*this, (NotMaxElement<View>::post(home(*this),x0,x1)));
    }
    // cheap tests for => b=0
    // if x0 is empty, then b=0 and entailed
    // if max(x1) < min(x0.lub) or min(x1) > max(x0.lub), then b=0 and entailed
    // if max(x0.glb) > max(x1), then b=0 and entailed
    if ((x0.cardMax() == 0) ||
        ((x1.max() < x0.lubMin()) || (x1.min() > x0.lubMax())) ||
        ((x0.glbSize() > 0) && (x0.glbMax() > x1.max())))
      {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        return home.ES_SUBSUMED(*this);
      }
    // if max(x0) is decided
    if (x0.glbMax() == x0.lubMax()) {
      // if x1 is det: check if = max(x0), assign b, entailed
      if (x1.assigned()) {
        if (x1.val() == x0.glbMax()) {
          if (rm != RM_IMP)
            GECODE_ME_CHECK(b.one(home));
        } else {
          if (rm != RM_PMI)
            GECODE_ME_CHECK(b.zero(home));
        }
        return home.ES_SUBSUMED(*this);
      }
      // if max(x0) not in dom(x1): b=0, entailed
      else if ((x0.glbMax() < x1.min()) ||
               (x0.glbMax() > x1.max()) ||
               !x1.in(x0.glbMax()))
        {
          if (rm != RM_PMI)
            GECODE_ME_CHECK(b.zero(home));
          return home.ES_SUBSUMED(*this);
        }
    }
    // if dom(x1) and lub(x0) are disjoint, then b=0, entailed
    {
      LubRanges<View> ub(x0);
      Gecode::Int::ViewRanges<Gecode::Int::IntView> d(x1);
      Gecode::Iter::Ranges::Inter<LubRanges<View>,
        Gecode::Int::ViewRanges<Gecode::Int::IntView> > ir(ub,d);
      if (!ir()) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        return home.ES_SUBSUMED(*this);
      }
    }
    // x0 is fated to eventually contain at least x0.cardMin elements.
    // therefore max(x0) >= x0.cardMin-th smallest element of x0.lub.
    // if x1 < than that, then b=0, entailed.
    {
      unsigned int n = x0.cardMin();
      int nth_smallest = BndSet::MIN_OF_EMPTY;
      for (LubRanges<View> ur(x0); ur(); ++ur) {
        if (ur.width() >= n)
          {
            // record it and exit the loop
            nth_smallest = static_cast<int>(ur.min() + n - 1);
            break;
          }
        // otherwise, we skipped ur.width() values
        n -= ur.width();
      }
      // if x1.max < nth_smallest, then entailed
      if (x1.max() < nth_smallest) {
        if (rm != RM_PMI)
          GECODE_ME_CHECK(b.zero(home));
        return home.ES_SUBSUMED(*this);
      }
    }
    return ES_FIX;
  }

}}}

// STATISTICS: set-prop
