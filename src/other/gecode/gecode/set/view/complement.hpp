/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
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

#include <sstream>

namespace Gecode { namespace Set {

  template<class View>
  forceinline
  ComplementView<View>::ComplementView(void) {}
  
  template<class View>
  forceinline
  ComplementView<View>::ComplementView(View& y)
    : DerivedView<View>(y) {}
  
  template<class View>
  forceinline ModEvent
  ComplementView<View>::me_negateset(ModEvent me) {
    switch(me) {
    case ME_SET_LUB : return ME_SET_GLB;
    case ME_SET_GLB : return ME_SET_LUB;
    case ME_SET_CLUB : return ME_SET_CGLB;
    case ME_SET_CGLB : return ME_SET_CLUB;
    default: return me;
    }
  }
  
  template<class View>
  forceinline PropCond
  ComplementView<View>::pc_negateset(PropCond pc) {
    switch(pc) {
    case PC_SET_CLUB  : return PC_SET_CGLB;
    case PC_SET_CGLB  : return PC_SET_CLUB;
    default: return pc;
    }
  }
  
  template<class View>
  forceinline unsigned int
  ComplementView<View>::glbSize(void) const {
    return Limits::card - x.lubSize();
  }
  
  template<class View>
  forceinline unsigned int
  ComplementView<View>::lubSize(void) const {
    return Limits::card - x.glbSize();
  }
  
  template<class View>
  forceinline unsigned int
  ComplementView<View>::unknownSize(void) const {
    return lubSize() - glbSize();
  }
  
  template<class View>
  forceinline bool
  ComplementView<View>::contains(int n) const { return x.notContains(n); }
  
  template<class View>
  forceinline bool
  ComplementView<View>::notContains(int n) const { return x.contains(n); }
  
  template<class View>
  forceinline unsigned int
  ComplementView<View>::cardMin(void) const {
    return Limits::card - x.cardMax();
  }
  
  template<class View>
  forceinline unsigned int
  ComplementView<View>::cardMax(void) const {
    return Limits::card - x.cardMin();
  }
  
  template<class View>
  forceinline int
  ComplementView<View>::lubMin(void) const {
    GlbRanges<View> lb(x);
    RangesCompl<GlbRanges<View> > lbc(lb);
    if (lbc()) {
      return lbc.min();
    } else {
      return BndSet::MIN_OF_EMPTY;
    }
  }
  
  template<class View>
  forceinline int
  ComplementView<View>::lubMax(void) const {
    GlbRanges<View> lb(x);
    RangesCompl<GlbRanges<View> > lbc(lb);
    if (lbc()) {
      while(lbc()) ++lbc;
      return lbc.max();
    } else {
      return BndSet::MAX_OF_EMPTY;
    }
  }
  
  template<class View>
  forceinline int
  ComplementView<View>::glbMin(void) const {
    LubRanges<View> ub(x);
    RangesCompl<LubRanges<View> > ubc(ub);
    if (ubc()) {
      return ubc.min();
    } else {
      return BndSet::MIN_OF_EMPTY;
    }
  }
  
  template<class View>
  forceinline int
  ComplementView<View>::glbMax(void) const {
    LubRanges<View> ub(x);
    RangesCompl<LubRanges<View> > ubc(ub);
    if (ubc()) {
      while (ubc()) ++ubc;
      return ubc.max();
    } else {
      return BndSet::MAX_OF_EMPTY;
    }
  }
  
  template<class View>
  forceinline ModEvent
  ComplementView<View>::cardMin(Space& home, unsigned int c) {
    if (c < Limits::card)
      return me_negateset(x.cardMax(home, Limits::card - c));
    return ME_SET_NONE;
  }
  
  template<class View>
  forceinline ModEvent
  ComplementView<View>::cardMax(Space& home, unsigned int c) {
    if (c < Limits::card)
      return me_negateset(x.cardMin(home, Limits::card - c));
    return ME_SET_NONE;
  }

  template<class View>
  forceinline ModEvent
  ComplementView<View>::include(Space& home, int c) {
    return me_negateset((x.exclude(home, c)));
  }
  
  template<class View>
  forceinline ModEvent
  ComplementView<View>::exclude(Space& home, int c) {
    return me_negateset((x.include(home, c)));
  }
  
  template<class View>
  forceinline ModEvent
  ComplementView<View>::intersect(Space& home, int c) {
    Iter::Ranges::Singleton si(c,c);
    RangesCompl<Iter::Ranges::Singleton> csi(si);
    return me_negateset((x.includeI(home, csi)));
  }
  
  template<class View>
  forceinline ModEvent
  ComplementView<View>::intersect(Space& home, int i, int j) {
    Iter::Ranges::Singleton si(i,j);
    RangesCompl<Iter::Ranges::Singleton> csi(si);
    return me_negateset((x.includeI(home, csi)));
  }
  
  template<class View>
  forceinline ModEvent
  ComplementView<View>::include(Space& home, int j, int k) {
    return me_negateset(x.exclude(home,j,k));
  }
  
  template<class View>
  forceinline ModEvent
  ComplementView<View>::exclude(Space& home, int j, int k) {
    return me_negateset(x.include(home,j,k));
  }
  
  template<class View>
  template<class I> ModEvent
  ComplementView<View>::excludeI(Space& home,I& iter) {
    return me_negateset(x.includeI(home,iter));
  }

  template<class View>
  template<class I> ModEvent
  ComplementView<View>::includeI(Space& home,I& iter) {
    return me_negateset(x.excludeI(home,iter));
  }
  
  template<class View>
  template<class I> ModEvent
  ComplementView<View>::intersectI(Space& home,I& iter) {
    RangesCompl<I> c(iter);
    return me_negateset(x.includeI(home,c));
  }
  
  template<class View>
  forceinline void
  ComplementView<View>::subscribe(Space& home, Propagator& p, PropCond pc,
                                  bool schedule) {
    x.subscribe(home,p, pc_negateset(pc),schedule);
  }
  
  template<class View>
  forceinline void
  ComplementView<View>::cancel(Space& home, Propagator& p, PropCond pc) {
    x.cancel(home,p, pc_negateset(pc));
  }
  
  template<class View>
  forceinline void
  ComplementView<View>::subscribe(Space& home, Advisor& a) {
    x.subscribe(home,a);
  }
  
  template<class View>
  forceinline void
  ComplementView<View>::cancel(Space& home, Advisor& a) {
    x.cancel(home,a);
  }
  
  template<class View>
  forceinline void
  ComplementView<View>::schedule(Space& home, Propagator& p, ModEvent me) {
    return View::schedule(home,p,me_negateset(me));
  }
  template<class View>
  forceinline ModEvent
  ComplementView<View>::me(const ModEventDelta& med) {
    return me_negateset(View::me(med));
  }
  
  template<class View>
  forceinline ModEventDelta
  ComplementView<View>::med(ModEvent me) {
    return me_negateset(View::med(me));
  }
  
  /*
   * Delta information for advisors
   *
   */
  
  template<class View>
  forceinline ModEvent
  ComplementView<View>::modevent(const Delta& d) {
    return me_negateset(View::modevent(d));
  }
  
  template<class View>
  forceinline int
  ComplementView<View>::glbMin(const Delta& d) const {
    return x.lubMin(d);
  }
  
  template<class View>
  forceinline int
  ComplementView<View>::glbMax(const Delta& d) const {
    return x.lubMax(d);
  }
  
  template<class View>
  forceinline bool
  ComplementView<View>::glbAny(const Delta& d) const {
    return x.lubAny(d);
  }
  
  template<class View>
  forceinline int
  ComplementView<View>::lubMin(const Delta& d) const {
    return x.glbMin(d);
  }
  
  template<class View>
  forceinline int
  ComplementView<View>::lubMax(const Delta& d) const {
    return x.glbMax(d);
  }

  template<class View>
  forceinline bool
  ComplementView<View>::lubAny(const Delta& d) const {
    return x.glbAny(d);
  }


  /**
   * \brief %Range iterator for least upper bound of complement set views
   * \ingroup TaskActorSetView
   */
  template<class View>
  class LubRanges<ComplementView<View> > {
  private:
    GlbRanges<View> lb;
    RangesCompl<GlbRanges<View> > lbc;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    LubRanges(void) {}
    /// Initialize with ranges for view \a x
    LubRanges(const ComplementView<View>& x);
    /// Initialize with ranges for view \a x
    void init(const ComplementView<View>& x);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}

    /// \name Range access
    //@{
    /// Return smallest value of range
    int min(void) const;
    /// Return largest value of range
    int max(void) const;
    /// Return width of ranges (distance between minimum and maximum)
    unsigned int width(void) const;
    //@}
  };

  template<class View>
  forceinline
  LubRanges<ComplementView<View> >::LubRanges(const ComplementView<View>& s)
    : lb(s.base()), lbc(lb) {}

  template<class View>
  forceinline void
  LubRanges<ComplementView<View> >::init(const ComplementView<View>& s) {
    lb.init(s.base());
    lbc.init(lb);
  }

  template<class View>
  forceinline bool
  LubRanges<ComplementView<View> >::operator ()(void) const { return lbc(); }

  template<class View>
  forceinline void
  LubRanges<ComplementView<View> >::operator ++(void) { return ++lbc; }

  template<class View>
  forceinline int
  LubRanges<ComplementView<View> >::min(void) const { return lbc.min(); }

  template<class View>
  forceinline int
  LubRanges<ComplementView<View> >::max(void) const { return lbc.max(); }

  template<class View>
  forceinline unsigned int
  LubRanges<ComplementView<View> >::width(void) const { return lbc.width(); }

  /**
   * \brief Range iterator for the least upper bound of double-complement-views
   *
   * This class provides (by specialization) a range iterator
   * for the least upper bounds of complements of complement set views.
   *
   * \ingroup TaskActorSet
   */
  template<class View>
  class LubRanges<ComplementView<ComplementView<View> > > :
    public LubRanges<View> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    LubRanges(void) {}
    /// Initialize with ranges for view \a x
    LubRanges(const ComplementView<ComplementView<View> >& x);
    /// Initialize with ranges for view \a x
    void init(const ComplementView<ComplementView<View> >& x);
    //@}
  };

  template<class View>
  forceinline
  LubRanges<ComplementView<ComplementView<View> > >::
  LubRanges(const ComplementView<ComplementView<View> >& x) :
    LubRanges<View>(x) {}

  template<class View>
  forceinline void
  LubRanges<ComplementView<ComplementView<View> > >::
  init(const ComplementView<ComplementView<View> >& x) {
    LubRanges<View>::init(x);
  }

  /**
   * \brief %Range iterator for greatest lower bound of complement set views
   * \ingroup TaskActorSetView
   */
  template<class View>
  class GlbRanges<ComplementView<View> > {
  private:
    LubRanges<View> ub;
    RangesCompl<LubRanges<View> > ubc;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    GlbRanges(void) {}
    /// Initialize with ranges for view \a x
    GlbRanges(const ComplementView<View> & x);
    /// Initialize with ranges for view \a x
    void init(const ComplementView<View> & x);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}

    /// \name Range access
    //@{
    /// Return smallest value of range
    int min(void) const;
    /// Return largest value of range
    int max(void) const;
    /// Return width of ranges (distance between minimum and maximum)
    unsigned int width(void) const;
    //@}
  };

  template<class View>
  forceinline
  GlbRanges<ComplementView<View> >::GlbRanges(const ComplementView<View> & s)
    : ub(s.base()), ubc(ub) {}

  template<class View>
  forceinline void
  GlbRanges<ComplementView<View> >::init(const ComplementView<View> & s) {
    ub.init(s.base());
    ubc.init(ub);
  }

  template<class View>
  forceinline bool
  GlbRanges<ComplementView<View> >::operator ()(void) const { return ubc(); }

  template<class View>
  forceinline void
  GlbRanges<ComplementView<View> >::operator ++(void) { return ++ubc; }

  template<class View>
  forceinline int
  GlbRanges<ComplementView<View> >::min(void) const { return ubc.min(); }

  template<class View>
  forceinline int
  GlbRanges<ComplementView<View> >::max(void) const { return ubc.max(); }

  template<class View>
  forceinline unsigned int
  GlbRanges<ComplementView<View> >::width(void) const { return ubc.width(); }

  /**
   * \brief Range iterator for the greatest lower bound of double-complement-views
   *
   * This class provides (by specialization) a range iterator
   * for the greatest lower bounds of complements of complement set views.
   *
   * \ingroup TaskActorSet
   */
  template<class View>
  class GlbRanges<ComplementView<ComplementView<View> > > :
    public GlbRanges<View> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    GlbRanges(void) {}
    /// Initialize with ranges for view \a x
    GlbRanges(const ComplementView<ComplementView<View> >& x);
    /// Initialize with ranges for view \a x
    void init(const ComplementView<ComplementView<View> >& x);
    //@}
  };

  template<class View>
  forceinline
  GlbRanges<ComplementView<ComplementView<View> > >::
  GlbRanges(const ComplementView<ComplementView<View> >& x) :
    GlbRanges<View>(x) {}

  template<class View>
  forceinline void
  GlbRanges<ComplementView<ComplementView<View> > >::
  init(const ComplementView<ComplementView<View> >& x) {
    GlbRanges<View>::init(x);
  }

  template<class Char, class Traits, class View>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
              const ComplementView<View>& x) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << "(" << x.base() << ")^C";
    return os << s.str();
  }

}}

// STATISTICS: set-var
