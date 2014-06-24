/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
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

#include <gecode/set.hh>
#include <gecode/int.hh>

namespace Gecode { namespace Set { namespace Int {

  /// Value Iterator for values above a certain weight
  template<class I>
  class OverweightValues {
  private:
    /// The threshold above which values should be iterated
    int threshold;
    /// The value iterator
    I iter;
    /// A superset of the elements found in the iterator
    const SharedArray<int> elements;
    /// Weights for all the possible elements
    const SharedArray<int> weights;
    /// The current index into the elements and weights
    int index;
    /// Move to the next element
    void next(void);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    OverweightValues(void);
    /// Initialize with elements/weights pairs, threshold \a t and iterator \a i
    OverweightValues(int t,
                     SharedArray<int>& elements0,
                     SharedArray<int>& weights0,
                     I& i);
    /// Initialize with elements/weights pairs, threshold \a t and iterator \a i
    void init(int t,
              SharedArray<int>& elements0,
              SharedArray<int>& weights0,
              I& i);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a value or done
    bool operator ()(void) const;
    /// Move iterator to next value (if possible)
    void operator ++(void);
    //@}
    /// \name Value access
    //@{
    /// Return current value
    int  val(void) const;
    //@}
  };

  template<class I>
  forceinline void
  OverweightValues<I>::next(void) {
    while (iter()) {
      while (elements[index]<iter.val()) index++;
      assert(elements[index]==iter.val());
      if (weights[index] > threshold) {
        return;
      }
      ++iter;
    }
  }

  template<class I>
  forceinline
  OverweightValues<I>::OverweightValues(void) {}

  template<class I>
  forceinline
  OverweightValues<I>::OverweightValues(int t,
                                        SharedArray<int>& elements0,
                                        SharedArray<int>& weights0,
                                        I& i) : threshold(t),
                                                iter(i),
                                                elements(elements0),
                                                weights(weights0),
                                                index(0) {
    next();
  }

  template<class I>
  forceinline void
  OverweightValues<I>::init(int t,
                            SharedArray<int>& elements0,
                            SharedArray<int>& weights0,
                            I& i) {
    threshold = t; iter = i;
    elements = elements0; weights = weights0;
    index = 0;
    next();
  }

  template<class I>
  forceinline bool
  OverweightValues<I>::operator ()(void) const { return iter(); }

  template<class I>
  forceinline void
  OverweightValues<I>::operator ++(void) { ++iter; next(); }

  template<class I>
  forceinline int
  OverweightValues<I>::val(void) const { return elements[index]; }

  template<class View>
  forceinline
  Weights<View>::Weights(Home home,
                   const SharedArray<int>& elements0,
                   const SharedArray<int>& weights0,
                   View x0, Gecode::Int::IntView y0)
    : Propagator(home), elements(elements0), weights(weights0),
      x(x0), y(y0) {
    home.notice(*this,AP_DISPOSE);
    x.subscribe(home,*this, PC_SET_ANY);
    y.subscribe(home,*this, Gecode::Int::PC_INT_BND);
  }

  template<class View>
  forceinline
  Weights<View>::Weights(Space& home, bool share, Weights& p)
    : Propagator(home,share,p) {
    x.update(home,share,p.x);
    y.update(home,share,p.y);
    elements.update(home,share,p.elements);
    weights.update(home,share,p.weights);
  }

  template<class View>
  inline ExecStatus
  Weights<View>::post(Home home, const SharedArray<int>& elements,
                      const SharedArray<int>& weights,
                      View x, Gecode::Int::IntView y) {
    if (elements.size() != weights.size())
      throw ArgumentSizeMismatch("Weights");
    Region r(home);
    int* els_arr = r.alloc<int>(elements.size());
    for (int i=elements.size(); i--;)
      els_arr[i] = elements[i];
    IntSet els(els_arr, elements.size());
    IntSetRanges er(els);
    GECODE_ME_CHECK(x.intersectI(home, er));
    (void) new (home) Weights(home,elements,weights,x,y);
    return ES_OK;
  }

  template<class View>
  PropCost
  Weights<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO, y.size()+1);
  }

  template<class View>
  forceinline size_t
  Weights<View>::dispose(Space& home) {
    home.ignore(*this,AP_DISPOSE);
    x.cancel(home,*this, PC_SET_ANY);
    y.cancel(home,*this, Gecode::Int::PC_INT_BND);
    elements.~SharedArray();
    weights.~SharedArray();
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  Actor*
  Weights<View>::copy(Space& home, bool share) {
    return new (home) Weights(home,share,*this);
  }

  /// Compute the weight of the elements in the iterator \a I
  template<class I>
  forceinline
  int weightI(SharedArray<int>& elements,
              SharedArray<int>& weights,
              I& iter) {
    int sum = 0;
    int i = 0;
    Iter::Ranges::ToValues<I> v(iter);
    for (; v(); ++v) {
      // Skip all elements below the current
      while (elements[i]<v.val()) i++;
      assert(elements[i] == v.val());
      sum += weights[i];
    }
    assert(!v());
    return sum;
  }


  /// Sort order for integers
  class IntLess {
  public:
    bool operator ()(int x, int y);
  };

  forceinline bool
  IntLess::operator ()(int x, int y) {
    return x < y;
  }

  template<class View>
  ExecStatus
  Weights<View>::propagate(Space& home, const ModEventDelta&) {

    ModEvent me = ME_SET_NONE;

    if (!x.assigned()) {
      // Collect the weights of the elements in the unknown set in an array
      int size = elements.size();
      Region r(home);
      int* currentWeights = r.alloc<int>(size);

      UnknownRanges<View> ur(x);
      Iter::Ranges::ToValues<UnknownRanges<View> > urv(ur);
      for (int i=0; i<size; i++) {
        if (!urv() || elements[i]<urv.val()) {
          currentWeights[i] = 0;
        } else {
          assert(elements[i] == urv.val());
          currentWeights[i] = weights[i];
          ++urv;
        }
      }

      // Sort the weights of the unknown elements
      IntLess il;
      Support::quicksort<int>(currentWeights, size, il);

      // The maximum number of elements that can still be added to x
      int delta = static_cast<int>(std::min(x.unknownSize(), x.cardMax() - x.glbSize()));

      // The weight of the elements already in x
      GlbRanges<View> glb(x);
      int glbWeight = weightI<GlbRanges<View> >(elements, weights, glb);

      // Compute the weight of the current lower bound of x, plus at most
      // delta-1 further elements with smallest negative weights. This weight
      // determines which elements in the upper bound cannot possibly be
      // added to x (those whose weight would exceed the capacity even if
      // all other elements are minimal)
      int lowWeight = glbWeight;
      for (int i=0; i<delta-1; i++) {
        if (currentWeights[i] >= 0)
          break;
        lowWeight+=currentWeights[i];
      }

      // Compute the lowest possible weight of x. If there is another element
      // with negative weight left, then add its weight to lowWeight.
      // Otherwise lowWeight is already the lowest possible weight.
      int lowestWeight = lowWeight;
      if (delta>0 && currentWeights[delta-1]<0)
        lowestWeight+=currentWeights[delta-1];

      // If after including the minimal number of required elements,
      // no more element with negative weight is available, then
      // a tighter lower bound can be computed.
      if ( (x.cardMin() - x.glbSize() > 0 &&
            currentWeights[x.cardMin() - x.glbSize() - 1] >= 0) ||
           currentWeights[0] >= 0 ) {
        int lowestPosWeight = glbWeight;
        for (unsigned int i=0; i<x.cardMin() - x.glbSize(); i++) {
          lowestPosWeight += currentWeights[i];
        }
        lowestWeight = std::max(lowestWeight, lowestPosWeight);        
      }

      // Compute the highest possible weight of x as the weight of the lower
      // bound plus the weight of the delta heaviest elements still in the
      // upper bound.
      int highestWeight = glbWeight;
      for (int i=0; i<delta; i++) {
        if (currentWeights[size-i-1]<=0)
          break;
        highestWeight += currentWeights[size-i-1];
      }

      // Prune the weight using the computed bounds
      GECODE_ME_CHECK(y.gq(home, lowestWeight));
      GECODE_ME_CHECK(y.lq(home, highestWeight));

      // Exclude all elements that are too heavy from the set x.
      // Elements are too heavy if their weight alone already
      // exceeds the remaining capacity
      int remainingCapacity = y.max()-lowWeight;

      UnknownRanges<View> ur2(x);
      Iter::Ranges::ToValues<UnknownRanges<View> > urv2(ur2);
      OverweightValues<Iter::Ranges::ToValues<UnknownRanges<View> > >
        ov(remainingCapacity, elements, weights, urv2);
      Iter::Values::ToRanges<OverweightValues<
        Iter::Ranges::ToValues<UnknownRanges<View> > > > ovr(ov);
      me = x.excludeI(home, ovr);
      GECODE_ME_CHECK(me);
    }
    if (x.assigned()) {
      // If x is assigned, just compute its weight and assign y.
      GlbRanges<View> glb(x);
      int w =
        weightI<GlbRanges<View> >(elements, weights, glb);
      GECODE_ME_CHECK(y.eq(home, w));
      return home.ES_SUBSUMED(*this);
    }

    return me_modified(me) ? ES_NOFIX : ES_FIX;
  }

}}}

// STATISTICS: set-prop
