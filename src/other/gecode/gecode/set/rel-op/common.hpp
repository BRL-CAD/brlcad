/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
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

#ifndef __GECODE_SET_RELOP_COMM_ICC__
#define __GECODE_SET_RELOP_COMM_ICC__

namespace Gecode {

  template<class View0, class View1>
  forceinline bool
  viewarrayshared(const Space& home,
                  const ViewArray<View0>& va, const View1& y) {
    return va.shared(home,y);
  }

  template<>
  forceinline bool
  viewarrayshared<Set::SingletonView,Set::SetView>
  (const Space&, const ViewArray<Set::SingletonView>&, const Set::SetView&) {
    return false;
  }

  template<>
  forceinline bool
  viewarrayshared<Set::ComplementView<Set::SingletonView>,Set::SetView>
  (const Space&, const ViewArray<Set::ComplementView<Set::SingletonView> >&,
   const Set::SetView&) {
    return false;
  }

  template<>
  forceinline bool
  viewarrayshared<Set::ComplementView<Set::SingletonView>,
                       Set::ComplementView<Set::SetView> >
  (const Space&, const ViewArray<Set::ComplementView<Set::SingletonView> >&,
   const Set::ComplementView<Set::SetView>&) {
    return false;
  }


namespace Set { namespace RelOp {

  /*
   * Detect sharing between 3 variables
   *
   */
  template<class View0, class View1, class View2>
  forceinline bool
  shared(View0 v0, View1 v1, View2 v2) {
    return shared(v0,v1) || shared(v0,v2) || shared(v1,v2);
  }

  template<class View0, class View1, class View2>
  ExecStatus interCard(Space& home,
                       bool& retmodified, View0& x0, View1& x1, View2& x2) {
    bool modified = false;
    do {
      retmodified |= modified;
      modified = false;

      {
        LubRanges<View0> x0ub(x0);
        LubRanges<View1> x1ub(x1);
        Iter::Ranges::Union<LubRanges<View0>, LubRanges<View1> >
          u1(x0ub,x1ub);
        unsigned int s1 = Iter::Ranges::size(u1);

        if (x0.cardMin() + x1.cardMin() > s1) {
          GECODE_ME_CHECK_MODIFIED(modified,
            x2.cardMin(home, x0.cardMin()+x1.cardMin()-s1));
        }

        // unsigned int res = std::max(x0.cardMin()+
        //                             (x1.cardMin()<s1 ?
        //                              0 : x1.cardMin()-s1),
        //                             std::max(x0.cardMin(),
        //                                      x1.cardMin()));
        // GECODE_ME_CHECK_MODIFIED(modified, x2.cardMin(home,res));
      }

      {
        GlbRanges<View0> x0lb(x0);
        GlbRanges<View1> x1lb(x1);
        Iter::Ranges::Union<GlbRanges<View0>, GlbRanges<View1> >
          u1(x0lb,x1lb);
        unsigned int s1 = Iter::Ranges::size(u1);
        GECODE_ME_CHECK_MODIFIED(modified,
                          x2.cardMax(home,
                                     x0.cardMax()+x1.cardMax()-s1));
      }

      if (x2.cardMax() < x1.cardMin())
        GECODE_ME_CHECK_MODIFIED(modified,
                          x0.cardMax(home,
                            Set::Limits::card+x2.cardMax()-x1.cardMin()));

      if (x2.cardMax() < x0.cardMin())
        GECODE_ME_CHECK_MODIFIED(modified,
                          x1.cardMax(home,
                            Set::Limits::card+x2.cardMax()-x0.cardMin()));

      GECODE_ME_CHECK_MODIFIED(modified,
                        x0.cardMin(home,x2.cardMin()));
      GECODE_ME_CHECK_MODIFIED(modified,
                        x1.cardMin(home,x2.cardMin()));
    } while(modified);
    return ES_FIX;
  }
  template<class View0, class View1, class View2>
  ExecStatus unionCard(Space& home,
                       bool& retmodified, View0& x0, View1& x1, View2& x2) {
    bool modified = false;
    do {
      retmodified |= modified;
      modified = false;

      {
        LubRanges<View0> x0ub(x0);
        LubRanges<View1> x1ub(x1);
        Iter::Ranges::Inter<LubRanges<View0>, LubRanges<View1> > i1(x0ub,x1ub);
        unsigned int s1 = Iter::Ranges::size(i1);
        unsigned int res = std::max(x0.cardMin()+
                                    (x1.cardMin()<s1 ?
                                     0 : x1.cardMin()-s1),
                                    std::max(x0.cardMin(),
                                             x1.cardMin()));
        GECODE_ME_CHECK_MODIFIED(modified, x2.cardMin(home,res));
      }

      {
        LubRanges<View0> x0ub(x0);
        LubRanges<View1> x1ub(x1);
        Iter::Ranges::Union<LubRanges<View0>, LubRanges<View1> > u1(x0ub,x1ub);
        unsigned int s1 = Iter::Ranges::size(u1);
        GECODE_ME_CHECK_MODIFIED(modified,
                          x2.cardMax(home,
                                     std::min(x0.cardMax()+x1.cardMax(),s1)));
      }

      if (x2.cardMin() > x1.cardMax())
        GECODE_ME_CHECK_MODIFIED(modified,
                          x0.cardMin(home,x2.cardMin() - x1.cardMax()));

      if (x2.cardMin() > x0.cardMax())
        GECODE_ME_CHECK_MODIFIED(modified,
                          x1.cardMin(home,x2.cardMin() - x0.cardMax()));

      GECODE_ME_CHECK_MODIFIED(modified,
                        x0.cardMax(home,x2.cardMax()));
      GECODE_ME_CHECK_MODIFIED(modified,
                        x1.cardMax(home,x2.cardMax()));
    } while(modified);
    return ES_FIX;
  }

  template<class View0, class View1>
  ExecStatus
  unionNCard(Space& home, bool& modified, ViewArray<View0>& x,
             View1& y, GLBndSet& unionOfDets) {
    int xsize = x.size();
    // Max(Xi.cardMin) <= y.card <= Sum(Xi.cardMax)
    // Xi.card <=y.cardMax
    unsigned int cardMaxSum=unionOfDets.size();
    bool maxValid = true;
    for (int i=xsize; i--; ){
      cardMaxSum+=x[i].cardMax();
      if (cardMaxSum < x[i].cardMax()) { maxValid = false; } //overflow
      GECODE_ME_CHECK_MODIFIED(modified, y.cardMin(home,x[i].cardMin()) );
      GECODE_ME_CHECK_MODIFIED(modified, x[i].cardMax(home,y.cardMax()) );
    }
    if (maxValid) {
      GECODE_ME_CHECK_MODIFIED(modified, y.cardMax(home,cardMaxSum));
    }
    //y.cardMin - Sum(Xj.cardMax) <= Xi.card

    if (x.size() == 0)
      return ES_NOFIX;

    Region r(home);
    //TODO: overflow management is a waste now.
    {
      unsigned int* rightSum = r.alloc<unsigned int>(xsize);
      rightSum[xsize-1]=0;

      for (int i=x.size()-1;i--;) {
        rightSum[i] = rightSum[i+1] + x[i+1].cardMax();
        if (rightSum[i] < rightSum[i+1]) {
          //overflow, fill the rest of the array.
          for (int j=i; j>0;j--) {
            rightSum[j]=Limits::card;
          }
          break;
        }
      }

      //Size of union of determied vars missing from x sneaked in here:
      unsigned int leftAcc=unionOfDets.size();

      for (int i=0; i<xsize;i++) {
        unsigned int jsum = leftAcc+rightSum[i];
        //If jsum did not overflow and is less than y.cardMin:
        if (jsum >= leftAcc &&  jsum < y.cardMin()) {
          GECODE_ME_CHECK_MODIFIED(modified, x[i].cardMin(home,y.cardMin()-jsum));
        }
        leftAcc += x[i].cardMax();
        if (leftAcc < x[i].cardMax()) {leftAcc = Limits::card;}
      }
    }

    //y.cardMin - |U(Xj.ub)| <= Xi.card

    {
      GLBndSet* rightUnion =
        static_cast<GLBndSet*>(r.ralloc(sizeof(GLBndSet)*xsize));
      new (&rightUnion[xsize-1]) GLBndSet(home);
      for (int i=xsize-1;i--;){
        BndSetRanges prev(rightUnion[i+1]);
        LubRanges<View0> prevX(x[i+1]);
        Iter::Ranges::Union< BndSetRanges,LubRanges<View0> >
          iter(prev,prevX);
        new (&rightUnion[i]) GLBndSet(home);
        rightUnion[i].includeI(home, iter);
      }

      //union of determied vars missing from x sneaked in here:
      GLBndSet leftAcc;
      leftAcc.update(home,unionOfDets);
      for (int i=0; i<xsize; i++) {
        BndSetRanges left(leftAcc);
        BndSetRanges right(rightUnion[i]);
        Iter::Ranges::Union<BndSetRanges,
          BndSetRanges> iter(left, right);
        unsigned int unionSize = Iter::Ranges::size(iter);
        if (y.cardMin() > unionSize) {
          GECODE_ME_CHECK_MODIFIED(modified,
                            x[i].cardMin(home, y.cardMin() - unionSize) );
        }
        LubRanges<View0> xiub(x[i]);
        leftAcc.includeI(home, xiub);
      }

      for (int i=xsize; i--;)
        rightUnion[i].dispose(home);
      leftAcc.dispose(home);
    }

    //no need for this: |y.lb - U(Xj.cardMax)| <= S.card

    return ES_NOFIX;

  }

  /*
   * Xi UB is subset of YUB
   * Subscribes to Y UB
   */
  template<class View0, class View1>
  ExecStatus
  unionNXiUB(Space& home,
             bool& modified, ViewArray<View0>& x, View1& y,
             GLBndSet &) {
    int xsize = x.size();
    for (int i=xsize; i--; ) {
      LubRanges<View1> yub(y);
      GECODE_ME_CHECK_MODIFIED(modified, x[i].intersectI(home, yub));
    }
    return ES_FIX;
  }

  // cardinality rules for PartitionN constraint
  template<class View0, class View1>
  ExecStatus
  partitionNCard(Space& home,
                 bool& modified, ViewArray<View0>& x, View1& y,
                 GLBndSet& unionOfDets) {
    unsigned int cardMinSum=unionOfDets.size();
    unsigned int cardMaxSum=unionOfDets.size();
    int xsize = x.size();
    for (int i=xsize; i--; ) {
      cardMinSum+=x[i].cardMin();
      if (cardMinSum < x[i].cardMin()) {
        //sum of mins overflows: fail the space.
        GECODE_ME_CHECK(ME_SET_FAILED);
      }
    }
    GECODE_ME_CHECK_MODIFIED(modified, y.cardMin(home,cardMinSum));
    for (int i=xsize; i--; ) {
      cardMaxSum+=x[i].cardMax();
      if (cardMaxSum < x[i].cardMax()) {
        //sum of maxes overflows: no useful information to tell.
        goto overflow;
      }
    }
    GECODE_ME_CHECK_MODIFIED(modified, y.cardMax(home,cardMaxSum));

    if (x.size() == 0)
      return ES_NOFIX;

  overflow:

    //Cardinality of each x[i] limited by cardinality of y minus all x[j]s:

    {
      Region r(home);
      unsigned int* rightMinSum = r.alloc<unsigned int>(xsize);
      unsigned int* rightMaxSum = r.alloc<unsigned int>(xsize);
      rightMinSum[xsize-1]=0;
      rightMaxSum[xsize-1]=0;

      for (int i=x.size()-1;i--;) {
        rightMaxSum[i] = rightMaxSum[i+1] + x[i+1].cardMax();
        if (rightMaxSum[i] < rightMaxSum[i+1]) {
          //overflow, fill the rest of the array.
          for (int j=i; j>0;j--) {
            rightMaxSum[j]=Limits::card;
          }
          break;
        }
      }
      for (int i=x.size()-1;i--;) {
        rightMinSum[i] = rightMinSum[i+1] + x[i+1].cardMin();
        if (rightMinSum[i] < rightMinSum[i+1]) {
          //overflow, fail the space
          GECODE_ME_CHECK(ME_SET_FAILED);
        }
      }
      unsigned int leftMinAcc=unionOfDets.size();
      unsigned int leftMaxAcc=unionOfDets.size();

      for (int i=0; i<xsize;i++) {
        unsigned int maxSum = leftMaxAcc+rightMaxSum[i];
        unsigned int minSum = leftMinAcc+rightMinSum[i];
        //If maxSum did not overflow and is less than y.cardMin:
        if (maxSum >= leftMaxAcc &&  maxSum < y.cardMin()) {
          GECODE_ME_CHECK_MODIFIED(modified, x[i].cardMin(home,y.cardMin()-maxSum));
        }

        //Overflow, fail.
        if (minSum < leftMinAcc || y.cardMax() < minSum) {
          GECODE_ME_CHECK(ME_SET_FAILED);
        }
        else {
          GECODE_ME_CHECK_MODIFIED(modified, x[i].cardMax(home,y.cardMax()-minSum));
        }

        leftMaxAcc += x[i].cardMax();
        if (leftMaxAcc < x[i].cardMax())
          leftMaxAcc = Limits::card;
        leftMinAcc += x[i].cardMin();
        if (leftMinAcc < x[i].cardMin())
          GECODE_ME_CHECK(ME_SET_FAILED);
      }
    }

    return ES_NOFIX;
  }

  // Xi LB includes YLB minus union Xj UB
  // Xi UB is subset of YUB minus union of Xj LBs
  template<class View0, class View1>
  ExecStatus
  partitionNXi(Space& home,
               bool& modified, ViewArray<View0>& x, View1& y) {
    int xsize = x.size();
    Region r(home);
    GLBndSet* afterUB =
      static_cast<GLBndSet*>(r.ralloc(sizeof(GLBndSet)*xsize));
    GLBndSet* afterLB =
      static_cast<GLBndSet*>(r.ralloc(sizeof(GLBndSet)*xsize));

    {
      GLBndSet sofarAfterUB;
      GLBndSet sofarAfterLB;
      for (int i=xsize; i--;) {
        new (&afterUB[i]) GLBndSet(home);
        new (&afterLB[i]) GLBndSet(home);
        afterUB[i].update(home,sofarAfterUB);
        afterLB[i].update(home,sofarAfterLB);
        LubRanges<View0> xiub(x[i]);
        GlbRanges<View0> xilb(x[i]);
        sofarAfterUB.includeI(home,xiub);
        sofarAfterLB.includeI(home,xilb);
      }
      sofarAfterUB.dispose(home);
      sofarAfterLB.dispose(home);
    }

    {
      GLBndSet sofarBeforeUB;
      GLBndSet sofarBeforeLB;
      for (int i=0; i<xsize; i++) {
        LubRanges<View1> yub(y);
        BndSetRanges slb(sofarBeforeLB);
        BndSetRanges afterlb(afterLB[i]);
        Iter::Ranges::Union<BndSetRanges,
          BndSetRanges> xjlb(slb, afterlb);
        Iter::Ranges::Diff<LubRanges<View1>,
          Iter::Ranges::Union<BndSetRanges,
          BndSetRanges> > diff1(yub, xjlb);
        GECODE_ME_CHECK_MODIFIED(modified, x[i].intersectI(home,diff1));

        GlbRanges<View1> ylb(y);
        BndSetRanges sub(sofarBeforeUB);
        BndSetRanges afterub(afterUB[i]);
        Iter::Ranges::Union<BndSetRanges,
          BndSetRanges> xjub(sub, afterub);
        Iter::Ranges::Diff<GlbRanges<View1>,
          Iter::Ranges::Union<BndSetRanges,
          BndSetRanges> > diff2(ylb, xjub);
        GECODE_ME_CHECK_MODIFIED(modified, x[i].includeI(home,diff2));

        LubRanges<View0> xiub(x[i]);
        GlbRanges<View0> xilb(x[i]);
        sofarBeforeUB.includeI(home,xiub);
        sofarBeforeLB.includeI(home,xilb);
      }
      sofarBeforeLB.dispose(home);
      sofarBeforeUB.dispose(home);
    }

    for (int i=xsize;i--;) {
      afterUB[i].dispose(home);
      afterLB[i].dispose(home);
    }

    return ES_NOFIX;
  }

  // Xi UB is subset of YUB minus union of Xj LBs
  template<class View0, class View1>
  ExecStatus
  partitionNXiUB(Space& home,
                 bool& modified, ViewArray<View0>& x, View1& y,
                 GLBndSet& unionOfDets) {
    int xsize = x.size();
    Region r(home);
    GLBndSet* afterLB =
      static_cast<GLBndSet*>(r.ralloc(sizeof(GLBndSet)*xsize));

    {
      GLBndSet sofarAfterLB;
      for (int i=xsize; i--;) {
        new (&afterLB[i]) GLBndSet(home);
        afterLB[i].update(home,sofarAfterLB);
        GlbRanges<View0> xilb(x[i]);
        sofarAfterLB.includeI(home,xilb);
      }
      sofarAfterLB.dispose(home);
    }

    {
      GLBndSet sofarBeforeLB;
      sofarBeforeLB.update(home,unionOfDets);
      for (int i=0; i<xsize; i++) {
        LubRanges<View1> yub(y);
        BndSetRanges slb(sofarBeforeLB);
        BndSetRanges afterlb(afterLB[i]);
        Iter::Ranges::Union<BndSetRanges,
          BndSetRanges> xjlb(slb, afterlb);
        Iter::Ranges::Diff<LubRanges<View1>,
          Iter::Ranges::Union<BndSetRanges,
          BndSetRanges> > diff1(yub, xjlb);
        GECODE_ME_CHECK_MODIFIED(modified, x[i].intersectI(home,diff1));

        GlbRanges<View0> xilb(x[i]);
        sofarBeforeLB.includeI(home,xilb);
      }
      sofarBeforeLB.dispose(home);
    }
    for (int i=xsize; i--;)
      afterLB[i].dispose(home);
    return ES_NOFIX;
  }

  // Xi LB includes YLB minus union Xj UB
  template<class View0, class View1>
  ExecStatus
  partitionNXiLB(Space& home,
                 bool& modified, ViewArray<View0>& x, View1& y,
                 GLBndSet& unionOfDets) {
    int xsize = x.size();
    Region r(home);
    GLBndSet* afterUB =
      static_cast<GLBndSet*>(r.ralloc(sizeof(GLBndSet)*xsize));

    {
      GLBndSet sofarAfterUB;
      for (int i=xsize; i--;) {
        new (&afterUB[i]) GLBndSet(home);
        afterUB[i].update(home,sofarAfterUB);
        LubRanges<View0> xiub(x[i]);
        sofarAfterUB.includeI(home,xiub);
      }
      sofarAfterUB.dispose(home);
    }

    {
      //The union of previously determined x[j]-s is added to the mix here:
      GLBndSet sofarBeforeUB;
      sofarBeforeUB.update(home,unionOfDets);
      for (int i=0; i<xsize; i++) {
        GlbRanges<View1> ylb(y);
        BndSetRanges sub(sofarBeforeUB);
        BndSetRanges afterub(afterUB[i]);
        Iter::Ranges::Union<BndSetRanges,
          BndSetRanges> xjub(sub, afterub);
        Iter::Ranges::Diff<GlbRanges<View1>,
          Iter::Ranges::Union<BndSetRanges,
          BndSetRanges> > diff2(ylb, xjub);
        GECODE_ME_CHECK_MODIFIED(modified, x[i].includeI(home,diff2));

        LubRanges<View0> xiub(x[i]);
        sofarBeforeUB.includeI(home,xiub);
      }
      sofarBeforeUB.dispose(home);
    }
    for (int i=xsize;i--;)
      afterUB[i].dispose(home);
    return ES_NOFIX;
  }

  // Y LB contains union of X LBs
  template<class View0, class View1>
  ExecStatus
  partitionNYLB(Space& home,
                bool& modified, ViewArray<View0>& x, View1& y,
                GLBndSet& unionOfDets) {
    assert(unionOfDets.isConsistent());
    int xsize = x.size();
    Region r(home);
    GlbRanges<View0>* xLBs = r.alloc<GlbRanges<View0> >(xsize);
    int nonEmptyCounter=0;
    for (int i = xsize; i--; ) {
      GlbRanges<View0> r(x[i]);
      if (r()) {
        xLBs[nonEmptyCounter] = r;
        nonEmptyCounter++;
      }
    }
    if (nonEmptyCounter !=0) {
      Iter::Ranges::NaryUnion xLBUnion(r,xLBs,nonEmptyCounter);
      BndSetRanges dets(unionOfDets);
      xLBUnion |= dets;
      GECODE_ME_CHECK_MODIFIED(modified, y.includeI(home,xLBUnion));
    }
    return ES_FIX;
  }

  // Y UB is subset of union of X UBs
  template<class View0, class View1>
  ExecStatus
  partitionNYUB(Space& home,
                bool& modified, ViewArray<View0>& x, View1& y,
                GLBndSet& unionOfDets) {
    int xsize = x.size();
    Region r(home);
    LubRanges<View0>* xUBs = r.alloc<LubRanges<View0> >(xsize);
    int nonEmptyCounter=0;
    for (int i = xsize; i--; ) {
      LubRanges<View0> r(x[i]);
      if (r()) {
        xUBs[nonEmptyCounter] = r;
        nonEmptyCounter++;
      }
    }
    if (nonEmptyCounter != 0) {
      Iter::Ranges::NaryUnion xUBUnion(r,xUBs,nonEmptyCounter);
      BndSetRanges dets(unionOfDets);
      xUBUnion |= dets;
      GECODE_ME_CHECK_MODIFIED(modified, y.intersectI(home,xUBUnion));
    }
    return ES_FIX;
  }

}}}

#endif

// STATISTICS: set-prop
