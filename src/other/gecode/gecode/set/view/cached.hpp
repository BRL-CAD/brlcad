/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2011
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
  CachedView<View>::CachedView(void) {}
  
  template<class View>
  forceinline
  CachedView<View>::CachedView(const View& y)
    : DerivedView<View>(y) {}

  template<class View>
  forceinline unsigned int
  CachedView<View>::glbSize(void) const {
    return x.glbSize();
  }
  
  template<class View>
  forceinline unsigned int
  CachedView<View>::lubSize(void) const {
    return x.lubSize();
  }
  
  template<class View>
  forceinline unsigned int
  CachedView<View>::unknownSize(void) const {
    return x.unknownSize();
  }
  
  template<class View>
  forceinline bool
  CachedView<View>::contains(int n) const { return x.contains(n); }
  
  template<class View>
  forceinline bool
  CachedView<View>::notContains(int n) const { return x.notContains(n); }
  
  template<class View>
  forceinline unsigned int
  CachedView<View>::cardMin(void) const {
    return x.cardMin();
  }
  
  template<class View>
  forceinline unsigned int
  CachedView<View>::cardMax(void) const {
    return x.cardMax();
  }
  
  template<class View>
  forceinline int
  CachedView<View>::lubMin(void) const {
    return x.lubMin();
  }
  
  template<class View>
  forceinline int
  CachedView<View>::lubMax(void) const {
    return x.lubMax();
  }
  
  template<class View>
  forceinline int
  CachedView<View>::glbMin(void) const {
    return x.glbMin();
  }
  
  template<class View>
  forceinline int
  CachedView<View>::glbMax(void) const {
    return x.glbMax();
  }
  
  template<class View>
  forceinline ModEvent
  CachedView<View>::cardMin(Space& home, unsigned int m) {
    return x.cardMin(home,m);
  }
  
  template<class View>
  forceinline ModEvent
  CachedView<View>::cardMax(Space& home, unsigned int m) {
    return x.cardMax(home,m);
  }

  template<class View>
  forceinline ModEvent
  CachedView<View>::include(Space& home, int i) {
    return x.include(home,i);
  }
  
  template<class View>
  forceinline ModEvent
  CachedView<View>::exclude(Space& home, int i) {
    return x.exclude(home,i);
  }
  
  template<class View>
  forceinline ModEvent
  CachedView<View>::intersect(Space& home, int i) {
    return x.intersect(home,i);
  }
  
  template<class View>
  forceinline ModEvent
  CachedView<View>::intersect(Space& home, int i, int j) {
    return x.intersect(home,i,j);
  }
  
  template<class View>
  forceinline ModEvent
  CachedView<View>::include(Space& home, int i, int j) {
    return x.include(home,i,j);
  }
  
  template<class View>
  forceinline ModEvent
  CachedView<View>::exclude(Space& home, int i, int j) {
    return x.exclude(home,i,j);
  }
  
  template<class View>
  template<class I> ModEvent
  CachedView<View>::excludeI(Space& home,I& iter) {
    return x.excludeI(home,iter);
  }

  template<class View>
  template<class I> ModEvent
  CachedView<View>::includeI(Space& home,I& iter) {
    return x.includeI(home,iter);
  }
  
  template<class View>
  template<class I> ModEvent
  CachedView<View>::intersectI(Space& home,I& iter) {
    return x.intersectI(home,iter);
  }
  
  template<class View>
  forceinline void
  CachedView<View>::subscribe(Space& home, Propagator& p, PropCond pc,
                                  bool schedule) {
    x.subscribe(home,p,pc,schedule);
  }
  
  template<class View>
  forceinline void
  CachedView<View>::cancel(Space& home, Propagator& p, PropCond pc) {
    x.cancel(home,p,pc);
  }
  
  template<class View>
  forceinline void
  CachedView<View>::subscribe(Space& home, Advisor& a) {
    x.subscribe(home,a);
  }
  
  template<class View>
  forceinline void
  CachedView<View>::cancel(Space& home, Advisor& a) {
    x.cancel(home,a);
  }
  
  template<class View>
  forceinline void
  CachedView<View>::schedule(Space& home, Propagator& p, ModEvent me) {
    return View::schedule(home,p,me);
  }
  template<class View>
  forceinline ModEvent
  CachedView<View>::me(const ModEventDelta& med) {
    return View::me(med);
  }
  
  template<class View>
  forceinline ModEventDelta
  CachedView<View>::med(ModEvent me) {
    return View::med(me);
  }
  
  /*
   * Delta information for advisors
   *
   */
  
  template<class View>
  forceinline ModEvent
  CachedView<View>::modevent(const Delta& d) {
    return View::modevent(d);
  }
  
  template<class View>
  forceinline int
  CachedView<View>::glbMin(const Delta& d) const {
    return x.glbMin(d);
  }
  
  template<class View>
  forceinline int
  CachedView<View>::glbMax(const Delta& d) const {
    return x.glbMax(d);
  }
  
  template<class View>
  forceinline bool
  CachedView<View>::glbAny(const Delta& d) const {
    return x.glbAny(d);
  }
  
  template<class View>
  forceinline int
  CachedView<View>::lubMin(const Delta& d) const {
    return x.lubMin(d);
  }
  
  template<class View>
  forceinline int
  CachedView<View>::lubMax(const Delta& d) const {
    return x.lubMax(d);
  }

  template<class View>
  forceinline bool
  CachedView<View>::lubAny(const Delta& d) const {
    return x.lubAny(d);
  }

  template<class View>
  forceinline void
  CachedView<View>::update(Space& home, bool share, CachedView<View>& y) {
    lubCache.update(home,y.lubCache);
    glbCache.update(home,y.glbCache);
    DerivedView<View>::update(home,share,y);
  }

  /*
   * Cache operations
   *
   */
  template<class View>
  void
  CachedView<View>::initCache(Space& home,
                              const IntSet& glb, const IntSet& lub) {
     glbCache.init(home);
     IntSetRanges gr(glb);
     glbCache.includeI(home,gr);
     lubCache.init(home);
     IntSetRanges lr(lub);
     lubCache.intersectI(home,lr);
   }

  template<class View>
  forceinline void
  CachedView<View>::cacheGlb(Space& home) {
    GlbRanges<View> gr(DerivedView<View>::base());
    glbCache.includeI(home,gr);
  }

  template<class View>
  forceinline void
  CachedView<View>::cacheLub(Space& home) {
    LubRanges<View> lr(DerivedView<View>::base());
    lubCache.intersectI(home,lr);
  }

  template<class View>
  forceinline bool
  CachedView<View>::glbModified(void) const {
    return glbCache.size() != glbSize();
  }

  template<class View>
  forceinline bool
  CachedView<View>::lubModified(void) const {
    return lubCache.size() != lubSize();
  }

  /**
   * \brief %Range iterator for least upper bound of cached set views
   * \ingroup TaskActorSetView
   */
  template<class View>
  class LubRanges<CachedView<View> > : public LubRanges<View> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    LubRanges(void) {}
    /// Initialize with ranges for view \a x
    LubRanges(const CachedView<View>& x);
    /// Initialize with ranges for view \a x
    void init(const CachedView<View>& x);
    //@}
  };

  template<class View>
  forceinline
  LubRanges<CachedView<View> >::LubRanges(const CachedView<View>& s)
    : LubRanges<View>(s.base()) {}

  template<class View>
  forceinline void
  LubRanges<CachedView<View> >::init(const CachedView<View>& s) {
    LubRanges<View>::init(s.base());
  }

  /**
   * \brief %Range iterator for greatest lower bound of cached set views
   * \ingroup TaskActorSetView
   */
  template<class View>
  class GlbRanges<CachedView<View> > : public GlbRanges<View> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    GlbRanges(void) {}
    /// Initialize with ranges for view \a x
    GlbRanges(const CachedView<View> & x);
    /// Initialize with ranges for view \a x
    void init(const CachedView<View> & x);
    //@}
  };

  template<class View>
  forceinline
  GlbRanges<CachedView<View> >::GlbRanges(const CachedView<View> & s)
    : GlbRanges<View>(s.base()) {}

  template<class View>
  forceinline void
  GlbRanges<CachedView<View> >::init(const CachedView<View> & s) {
    GlbRanges<View>::init(s.base());
  }

  template<class Char, class Traits, class View>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
              const CachedView<View>& x) {
    return os << x.base();
  }

  template<class View>
  forceinline
  GlbDiffRanges<View>::GlbDiffRanges(const CachedView<View>& x)
    : gr(x.base()), cr(x.glbCache) {
    Iter::Ranges::Diff<GlbRanges<View>,BndSetRanges>::init(gr,cr);
  }

  template<class View>
  forceinline
  LubDiffRanges<View>::LubDiffRanges(const CachedView<View>& x)
    : cr(x.lubCache), lr(x.base()) {
    Iter::Ranges::Diff<BndSetRanges,LubRanges<View> >::init(cr,lr);
  }

}}

// STATISTICS: set-var
