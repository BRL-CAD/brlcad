/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
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
   * BndSet
   *
   */

  forceinline
  BndSet::BndSet(void) :
    first(NULL), last(NULL), _size(0), _card(0) {}

  forceinline RangeList*
  BndSet::fst(void) const {
    return first;
  }

  forceinline RangeList*
  BndSet::lst(void) const {
    return last;
  }

  forceinline void
  BndSet::dispose(Space& home) {
    if (fst()!=NULL)
      fst()->dispose(home,lst());
  }

  forceinline void
  BndSet::fst(RangeList* f) {
    first = f;
  }

  forceinline void
  BndSet::lst(RangeList* l) {
    last = l;
  }

  forceinline
  BndSet::BndSet(Space& home, int mn, int mx) {
    if (mn>mx) {
      fst(NULL); lst(NULL); _size = 0;
    } else {
      RangeList* p =
        new (home) RangeList(mn,mx,NULL);
      fst(p); lst(p);
      _size = static_cast<unsigned int>(mx-mn+1);
    }
  }

  forceinline RangeList*
  BndSet::ranges(void) const {
    return fst();
  }

  forceinline unsigned int
  BndSet::size(void) const {
    return _size;
  }

  forceinline bool
  BndSet::empty(void) const {
    return (_size==0);
  }

  forceinline int
  BndSet::min(void) const {
    if (fst()==NULL)
      return MIN_OF_EMPTY;
    else
      return fst()->min();
  }

  forceinline int
  BndSet::max(void) const {
    if (lst()==NULL) 
      return MAX_OF_EMPTY;
    else 
      return lst()->max();
  }

  // nth smallest element
  forceinline int
  BndSet::minN(unsigned int n) const {
    for (RangeList* c = fst(); c != NULL; c = c->next()) {
      if (c->width() > n)
        return static_cast<int>(c->min() + n);
      n -= c->width();
    }
    return MIN_OF_EMPTY;
  }

  forceinline unsigned int
  BndSet::card(void) const {
    return _card;
  }

  forceinline void
  BndSet::card(unsigned int c) {
    _card = c;
  }

  forceinline void
  BndSet::update(Space& home, BndSet& d) {
    if (d.fst() == fst())
      return;
    if (fst() != NULL)
      fst()->dispose(home,lst());
    _size = d.size();
    if (_size == 0) {
      fst(NULL); lst(NULL);
      return;
    }

    int n=0;
    for (RangeList* c = d.fst(); c != NULL; c = c->next())
      n++;

    RangeList* r = home.alloc<RangeList>(n);
    fst(r); lst(r+n-1);

    {
      RangeList* c = d.fst();
      for (int i=0; i<n; i++) {
        r[i].min(c->min());
        r[i].max(c->max());
        r[i].next(&r[i+1]);
        c = c->next();
      }
    }
    r[n-1].next(NULL);
  }

  template<class I> forceinline bool
  BndSet::overwrite(Space& home, I& ri) {
    // Is new domain empty?
    if (!ri()) {
      //Was it empty?
      if (fst()==NULL)
        return false;
      fst()->dispose(home,lst());
      _size=0; fst(NULL); lst(NULL);
      return true;
    }

    RangeList* f =
      new (home) RangeList(ri.min(),ri.max(),NULL);
    RangeList* l = f;
    unsigned int s = ri.width();

    ++ri;

    while (ri()){
      RangeList *n = new (home) RangeList(ri.min(),ri.max(),NULL);
      l->next(n);
      l=n;
      s += ri.width();
      ++ri;
    }

    if (fst() != NULL)
      fst()->dispose(home,lst());
    fst(f); lst(l);

    // If the size did not change, nothing changed, as overwriting
    // must not at the same time include and exclude elements.
    if (size() == s)
      return false;

    _size = s;
    return true;
  }

  forceinline void
  BndSet::become(Space& home, const BndSet& that){
    if (fst()!=NULL){
      assert(lst()!=NULL);
      assert(fst()!= that.fst());
      fst()->dispose(home,lst());
    }
    fst(that.fst());
    lst(that.lst());
    _size = that.size();
    assert(isConsistent());
  }

  forceinline bool
  BndSet::in(int i) const {
    for (RangeList* c = fst(); c != NULL; c = c->next()) {
      if (c->min() <= i && c->max() >= i)
        return true;
      if (c->min() > i)
        return false;
    }
    return false;
  }

  /*
   * Range iterator for BndSets
   *
   */

  forceinline
  BndSetRanges::BndSetRanges(void) {}

  forceinline
  BndSetRanges::BndSetRanges(const BndSet& s)
    : Iter::Ranges::RangeList(s.ranges()) {}

  forceinline void
  BndSetRanges::init(const BndSet& s) {
    Iter::Ranges::RangeList::init(s.ranges());
  }

  /*
   * GLBndSet
   *
   */

  forceinline
  GLBndSet::GLBndSet(void) {}

  forceinline
  GLBndSet::GLBndSet(Space&) {}

  forceinline
  GLBndSet::GLBndSet(Space& home, int mi, int ma)
    : BndSet(home,mi,ma) {}

  forceinline
  GLBndSet::GLBndSet(Space& home, const IntSet& s)
    : BndSet(home,s) {}

  forceinline void
  GLBndSet::init(Space& home) {
    dispose(home);
    fst(NULL);
    lst(NULL);
    _size = 0;
  }

  forceinline bool
  GLBndSet::include(Space& home, int mi, int ma, SetDelta& d) {
    assert(ma >= mi);
    if (fst()==NULL) {
      RangeList* p = new (home) RangeList(mi,ma,NULL);
      fst(p);
      lst(p);
      _size=static_cast<unsigned int>(ma-mi+1);
      d._glbMin = mi;
      d._glbMax = ma;
      return true;
    }
    bool ret = include_full(home, mi, ma, d);
    assert(isConsistent());
    return ret;
  }

  template<class I> bool
  GLBndSet::includeI(Space& home, I& i) {
    if (!i())
      return false;
    BndSetRanges j(*this);
    Iter::Ranges::Union<BndSetRanges,I> ij(j,i);
    bool me = overwrite(home, ij);
    assert(isConsistent());
    return me;
  }


  /*
   * LUBndSet
   *
   */

  forceinline
  LUBndSet::LUBndSet(void) {}

  forceinline
  LUBndSet::LUBndSet(Space& home)
    : BndSet(home,Limits::min,Limits::max) {}

  forceinline
  LUBndSet::LUBndSet(Space& home, int mi, int ma)
    : BndSet(home,mi,ma) {}

  forceinline
  LUBndSet::LUBndSet(Space& home, const IntSet& s)
    : BndSet(home,s) {}

  forceinline void
  LUBndSet::init(Space& home) {
    RangeList *p =
      new (home) RangeList(Limits::min,
                           Limits::max,
                           NULL);
    fst(p);
    lst(p);
    _size = Limits::card;
  }

  forceinline bool
  LUBndSet::exclude(Space& home, int mi, int ma, SetDelta& d) {
    assert(ma >= mi);
    if ((mi > max()) || (ma < min())) { return false; }
    if (mi <= min() && ma >= max() ) { //the range covers the whole set
      d._lubMin = min();
      d._lubMax = max();
      fst()->dispose(home,lst()); fst(NULL); lst(NULL);
      _size=0;
      return true;
    }
    bool ret =  exclude_full(home, mi, ma, d);
    assert(isConsistent());
    return ret;
  }

  forceinline bool
  LUBndSet::intersect(Space& home, int mi, int ma) {
    assert(ma >= mi);
    if ((mi <= min()) && (ma >= max())) { return false; }
    if (_size == 0) return false;
    if (ma < min() || mi > max() ) { // empty the whole set
     fst()->dispose(home,lst()); fst(NULL); lst(NULL);
     _size=0;
     return true;
    }
    bool ret = intersect_full(home, mi, ma);
    assert(isConsistent());
    return ret;
  }

  template<class I> bool
  LUBndSet::intersectI(Space& home, I& i) {
    if (fst()==NULL) { return false; }
    if (!i()) {
      fst()->dispose(home,lst()); fst(NULL); lst(NULL);
      _size=0;
      return true;
    }
    BndSetRanges j(*this);
    Iter::Ranges::Inter<BndSetRanges,I> ij(j,i);
    bool ret = overwrite(home, ij);
    assert(isConsistent());
    return ret;
  }

  template<class I> bool
  LUBndSet::excludeI(Space& home, I& i) {
    if (!i()) { return false; }
    BndSetRanges j(*this);
    Iter::Ranges::Diff<BndSetRanges,I> ij(j,i);
    bool ret = overwrite(home, ij);
    assert(isConsistent());
    return ret;
  }

  forceinline void
  LUBndSet::excludeAll(Space& home) {
    fst()->dispose(home,lst()); fst(NULL); lst(NULL);
    _size=0;
  }

  /*
   * A complement iterator spezialized for the BndSet limits
   *
   */
  template<class I>
  RangesCompl<I>::RangesCompl(void) {}

  template<class I>
  RangesCompl<I>::RangesCompl(I& i)
    : Iter::Ranges::Compl<Limits::min,
                          Limits::max,
                          I>(i) {}

  template<class I> void
  RangesCompl<I>::init(I& i) {
    Iter::Ranges::Compl<Limits::min,
      Limits::max,I>::init(i);
  }

}}

// STATISTICS: set-var

