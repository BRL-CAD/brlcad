/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
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

namespace Gecode { namespace Set {

  /*
   * Creation of new variable implementations
   *
   */

  forceinline
  SetVarImp::SetVarImp(Space& home)
    : SetVarImpBase(home), lub(home), glb(home) {
    lub.card(Limits::card);
    assert(lub.card()==lub.size());
  }

  forceinline
  SetVarImp::SetVarImp(Space& home,int lbMin,int lbMax,int ubMin,int ubMax,
                       unsigned int cardMin, unsigned int cardMax)
    : SetVarImpBase(home),
      lub(home,ubMin,ubMax), glb(home,lbMin,lbMax) {
    glb.card(std::max(cardMin, glb.size() ));
    lub.card(std::min(cardMax, lub.size() ));
  }

  forceinline
  SetVarImp::SetVarImp(Space& home,
                       const IntSet& theGlb,int ubMin,int ubMax,
                       unsigned int cardMin, unsigned int cardMax)
    : SetVarImpBase(home),
      lub(home,ubMin,ubMax), glb(home,theGlb) {
    glb.card(std::max(cardMin, glb.size() ));
    lub.card(std::min(cardMax, lub.size() ));
  }

  forceinline
  SetVarImp::SetVarImp(Space& home,
                       int lbMin,int lbMax,const IntSet& theLub,
                       unsigned int cardMin, unsigned int cardMax)
    : SetVarImpBase(home),
      lub(home,theLub), glb(home,lbMin,lbMax) {
    glb.card(std::max(cardMin, glb.size() ));
    lub.card(std::min(cardMax, lub.size() ));
  }

  forceinline
  SetVarImp::SetVarImp(Space& home,
                       const IntSet& theGlb,const IntSet& theLub,
                       unsigned int cardMin, unsigned int cardMax)
    : SetVarImpBase(home), lub(home,theLub), glb(home,theGlb) {
    glb.card(std::max(cardMin, glb.size() ));
    lub.card(std::min(cardMax, lub.size() ));
  }


  forceinline bool
  SetVarImp::assigned(void) const {
    return glb.size() == lub.size();
  }

  forceinline unsigned int
  SetVarImp::cardMin(void) const { return glb.card(); }

  forceinline unsigned int
  SetVarImp::cardMax(void) const { return lub.card(); }

  forceinline bool
  SetVarImp::knownIn(int i) const { return (glb.in(i)); }

  forceinline bool
  SetVarImp::knownOut(int i) const { return !(lub.in(i)); }

  forceinline int
  SetVarImp::lubMin(void) const { return lub.min(); }

  forceinline int
  SetVarImp::lubMax(void) const { return lub.max(); }

  forceinline int
  SetVarImp::lubMinN(unsigned int n) const { return lub.minN(n); }

  forceinline int
  SetVarImp::glbMin(void) const { return glb.min(); }

  forceinline int
  SetVarImp::glbMax(void) const { return glb.max(); }

  forceinline unsigned int
  SetVarImp::glbSize() const { return glb.size(); }

  forceinline unsigned int
  SetVarImp::lubSize() const { return lub.size(); }

  /*
   * Support for delta information
   *
   */
  forceinline int
  SetVarImp::glbMin(const Delta& d) {
    return static_cast<const SetDelta&>(d).glbMin();
  }
  forceinline int
  SetVarImp::glbMax(const Delta& d) {
    return static_cast<const SetDelta&>(d).glbMax();
  }
  forceinline bool
  SetVarImp::glbAny(const Delta& d) {
    return static_cast<const SetDelta&>(d).glbAny();
  }
  forceinline int
  SetVarImp::lubMin(const Delta& d) {
    return static_cast<const SetDelta&>(d).lubMin();
  }
  forceinline int
  SetVarImp::lubMax(const Delta& d) {
    return static_cast<const SetDelta&>(d).lubMax();
  }
  forceinline bool
  SetVarImp::lubAny(const Delta& d) {
    return static_cast<const SetDelta&>(d).lubAny();
  }

  forceinline ModEvent
  SetVarImp::cardMin(Space& home,unsigned int newMin) {
    if (cardMin() >= newMin)
      return ME_SET_NONE;
    if (newMin > cardMax())
      return ME_SET_FAILED;
    glb.card(newMin);
    return cardMin_full(home);
  }

  forceinline ModEvent
  SetVarImp::cardMax(Space& home,unsigned int newMax) {
    if (cardMax() <= newMax)
      return ME_SET_NONE;
    if (cardMin() > newMax)
      return ME_SET_FAILED;
    lub.card(newMax);
    return cardMax_full(home);
  }

  forceinline ModEvent
  SetVarImp::intersect(Space& home,int i, int j) {
    BndSetRanges lb(glb);
    Iter::Ranges::Singleton s(i,j);
    Iter::Ranges::Diff<BndSetRanges, Iter::Ranges::Singleton> probe(lb, s);
    if (probe())
      return ME_SET_FAILED;
    if (assigned())
      return ME_SET_NONE;
    int oldMin = lub.min();
    int oldMax = lub.max();
    if (lub.intersect(home, i, j)) {
      SetDelta d;
      if (i == oldMin) {
        d._lubMin = lub.max()+1;
        d._lubMax = oldMax;
      } else if (j == oldMax) {
        d._lubMin = oldMin;
        d._lubMax = lub.min()-1;
      }
      return processLubChange(home, d);
    }
    return ME_SET_NONE;
  }

  forceinline ModEvent
  SetVarImp::intersect(Space& home,int i) {
    return intersect(home, i, i);
  }

  template<class I>
  inline ModEvent
  SetVarImp::intersectI(Space& home, I& iterator) {
    if (assigned()) {
      BndSetRanges lbi(glb);
      Iter::Ranges::Diff<BndSetRanges,I> probe(lbi,iterator);
      if (probe())
        return ME_SET_FAILED;
      return ME_SET_NONE;
    }
    if (!iterator()) {
      if (cardMin() > 0)
        return ME_SET_FAILED;
      lub.card(0);
      SetDelta d(1, 0, lub.min(), lub.max());
      lub.excludeAll(home);
      return notify(home, ME_SET_VAL, d);
    }
    int mi=iterator.min();
    int ma=iterator.max();
    ++iterator;
    if (iterator())
      return intersectI_full(home, mi, ma, iterator);
    else
      return intersect(home, mi, ma);
  }

  template<class I>
  ModEvent
  SetVarImp::intersectI_full(Space& home, int mi, int ma, I& iterator) {
    Iter::Ranges::SingletonAppend<I> si(mi,ma,iterator);
    if (lub.intersectI(home, si)) {
      BndSetRanges ub(lub);
      BndSetRanges lb(glb);
      if (!Iter::Ranges::subset(lb,ub)) {
        glb.become(home, lub);
        glb.card(glb.size());
        lub.card(glb.size());
        return ME_SET_FAILED;
      }
      ModEvent me = ME_SET_LUB;
      if (cardMax() > lub.size()) {
        lub.card(lub.size());
        if (cardMin() > cardMax()) {
          glb.become(home, lub);
          glb.card(glb.size());
          lub.card(glb.size());
          return ME_SET_FAILED;
        }
        me = ME_SET_CLUB;
      }
      if (cardMax() == lub.size() && cardMin() == cardMax()) {
        glb.become(home, lub);
        me = ME_SET_VAL;
      }
      SetDelta d;
      return notify(home, me, d);
    }
    return ME_SET_NONE;
  }

  forceinline ModEvent
  SetVarImp::include(Space& home, int i, int j) {
    if (j<i)
      return ME_SET_NONE;
    BndSetRanges ub(lub);
    Iter::Ranges::Singleton sij(i,j);
    if (!Iter::Ranges::subset(sij,ub)) {
      return ME_SET_FAILED;
    }
    SetDelta d;
    if (glb.include(home, i, j, d))
      return processGlbChange(home, d);
    return ME_SET_NONE;
  }

  forceinline ModEvent
  SetVarImp::include(Space& home, int i) {
    return include(home, i, i);
  }

  template<class I> forceinline ModEvent
  SetVarImp::includeI(Space& home, I& iterator) {
    if (!iterator()) {
      return ME_SET_NONE;
    }
    if (assigned()) {
      BndSetRanges lbi(glb);
      Iter::Ranges::Diff<I,BndSetRanges>
        probe(iterator,lbi);
      return probe() ? ME_SET_FAILED : ME_SET_NONE;
    }
    int mi=iterator.min();
    int ma=iterator.max();
    ++iterator;
    if (iterator())
      return includeI_full(home, mi, ma, iterator);
    else
      return include(home, mi, ma);
  }

  template<class I>
  ModEvent
  SetVarImp::includeI_full(Space& home, int mi, int ma, I& iterator) {
    Iter::Ranges::SingletonAppend<I> si(mi,ma,iterator);
    if (glb.includeI(home, si)) {
      BndSetRanges ub(lub);
      BndSetRanges lb(glb);
      if (!Iter::Ranges::subset(lb,ub)) {
        glb.become(home, lub);
        glb.card(glb.size());
        lub.card(glb.size());
        return ME_SET_FAILED;
      }
      ModEvent me = ME_SET_GLB;
      if (cardMin() < glb.size()) {
        glb.card(glb.size());
        if (cardMin() > cardMax()) {
          glb.become(home, lub);
          glb.card(glb.size());
          lub.card(glb.size());
          return ME_SET_FAILED;
        }
        me = ME_SET_CGLB;
      }
      if (cardMin() == glb.size() && cardMin() == cardMax()) {
        lub.become(home, glb);
        me = ME_SET_VAL;
      }
      SetDelta d;
      return notify(home, me, d);
    }
    return ME_SET_NONE;
  }

  forceinline ModEvent
  SetVarImp::exclude(Space& home, int i, int j) {
    if (j<i)
      return ME_SET_NONE;
    Iter::Ranges::Singleton sij(i,j);
    BndSetRanges lb(glb);
    Iter::Ranges::Inter<Iter::Ranges::Singleton,BndSetRanges> probe(sij,lb);
    if (probe())
      return ME_SET_FAILED;
    SetDelta d;
    if (lub.exclude(home, i, j, d))
      return processLubChange(home, d);
    return ME_SET_NONE;
  }

  forceinline ModEvent
  SetVarImp::exclude(Space& home, int i) {
    return exclude(home, i, i);
  }

  template<class I>
  inline ModEvent
  SetVarImp::excludeI(Space& home, I& iterator) {
    if (!iterator())
      return ME_SET_NONE;
    if (assigned()) {
      BndSetRanges ubi(lub);
      Iter::Ranges::Inter<BndSetRanges,I> probe(ubi,iterator);
      return probe() ? ME_SET_FAILED : ME_SET_NONE;
    }
    int mi=iterator.min();
    int ma=iterator.max();
    ++iterator;
    if (iterator())
      return excludeI_full(home, mi, ma, iterator);
    else
      return exclude(home, mi, ma);
  }

  template<class I>
  ModEvent
  SetVarImp::excludeI_full(Space& home, int mi, int ma, I& iterator) {
    Iter::Ranges::SingletonAppend<I> si(mi,ma,iterator);
    if (lub.excludeI(home, si)) {
      BndSetRanges ub(lub);
      BndSetRanges lb(glb);
      if (!Iter::Ranges::subset(lb,ub)) {
        glb.become(home, lub);
        glb.card(glb.size());
        lub.card(glb.size());
        return ME_SET_FAILED;
      }
      ModEvent me = ME_SET_LUB;
      if (cardMax() > lub.size()) {
        lub.card(lub.size());
        if (cardMin() > cardMax()) {
          glb.become(home, lub);
          glb.card(glb.size());
          lub.card(glb.size());
          return ME_SET_FAILED;
        }
        me = ME_SET_CLUB;
      }
      if (cardMax() == lub.size() && cardMin() == cardMax()) {
        glb.become(home, lub);
        me = ME_SET_VAL;
      }
      SetDelta d;
      return notify(home, me, d);
    }
    return ME_SET_NONE;
  }

  /*
   * Copying a variable
   *
   */

  forceinline SetVarImp*
  SetVarImp::copy(Space& home, bool share) {
    return copied() ?
      static_cast<SetVarImp*>(forward()) :
      perform_copy(home,share);
  }

  /*
   * Iterator specializations
   *
   */

  /**
   * \brief Range iterator for the least upper bound of a set variable implementation
   *
   * This class provides (by specialization) a range iterator
   * for the least upper bounds of set variable implementations.
   *
   * \ingroup TaskActorSet
   */
  template<>
  class LubRanges<SetVarImp*> : public BndSetRanges {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    LubRanges(void);
    /// Initialize with ranges for variable implementation \a x
    LubRanges(const SetVarImp*);
    /// Initialize with ranges for variable implementation \a x
    void init(const SetVarImp*);
    //@}
  };

  forceinline
  LubRanges<SetVarImp*>::LubRanges(void) {}

  forceinline
  LubRanges<SetVarImp*>::LubRanges(const SetVarImp* x)
    : BndSetRanges(x->lub) {}

  forceinline void
  LubRanges<SetVarImp*>::init(const SetVarImp* x) {
    BndSetRanges::init(x->lub);
  }

  /**
   * \brief Range iterator for the greatest lower bound of a set variable implementation
   *
   * This class provides (by specialization) a range iterator
   * for the greatest lower bounds of set variable implementations.
   *
   * \ingroup TaskActorSet
   */
  template<>
  class GlbRanges<SetVarImp*> : public BndSetRanges {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    GlbRanges(void);
    /// Initialize with ranges for variable implementation \a x
    GlbRanges(const SetVarImp* x);
    /// Initialize with ranges for variable implementation \a x
    void init(const SetVarImp*);
    //@}
  };

  forceinline
  GlbRanges<SetVarImp*>::GlbRanges(void) {}

  forceinline
  GlbRanges<SetVarImp*>::GlbRanges(const SetVarImp* x)
    : BndSetRanges(x->glb) {}

  forceinline void
  GlbRanges<SetVarImp*>::init(const SetVarImp* x) {
    BndSetRanges::init(x->glb);
  }


  /*
   * Dependencies
   *
   */
  forceinline void
  SetVarImp::subscribe(Space& home, Propagator& p, PropCond pc, bool schedule) {
    SetVarImpBase::subscribe(home,p,pc,assigned(),schedule);
  }
  forceinline void
  SetVarImp::cancel(Space& home, Propagator& p, PropCond pc) {
    SetVarImpBase::cancel(home,p,pc,assigned());
  }
  forceinline void
  SetVarImp::subscribe(Space& home, Advisor& a) {
    SetVarImpBase::subscribe(home,a,assigned());
  }
  forceinline void
  SetVarImp::cancel(Space& home, Advisor& a) {
    SetVarImpBase::cancel(home,a,assigned());
  }

}}

// STATISTICS: set-var
