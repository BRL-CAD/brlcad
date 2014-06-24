/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2010
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

namespace Gecode { namespace Int { namespace BinPacking {

  /*
   * Item
   *
   */
  forceinline
  Item::Item(void) 
    : s(0) {}
  forceinline
  Item::Item(IntView b, int s0)
    : DerivedView<IntView>(b), s(s0) {}

  forceinline IntView 
  Item::bin(void) const {
    return x;
  }
  forceinline
  void Item::bin(IntView b) {
    x = b;
  }
  forceinline int 
  Item::size(void) const {
    return s;
  }
  forceinline void 
  Item::size(int s0) {
    s = s0;
  }

  forceinline void
  Item::update(Space& home, bool share, Item& i) {
    x.update(home,share,i.x);
    s = i.s;
  }


  forceinline bool 
  same(const Item& i, const Item& j) {
    return same(i.bin(),j.bin()) && (i.size() == j.size());
  }
  forceinline bool
  before(const Item& i, const Item& j) {
    return before(i.bin(),j.bin())
      || (same(i.bin(),j.bin()) && (i.size() == j.size()));
  }

  /// For sorting according to size
  forceinline bool 
  operator <(const Item& i, const Item& j) {
    return i.size() > j.size();
  }


  /*
   * Size set
   *
   */
  forceinline
  SizeSet::SizeSet(void) {}
  forceinline
  SizeSet::SizeSet(Region& region, int n_max) 
    : n(0), t(0), s(region.alloc<int>(n_max)) {}
  forceinline void
  SizeSet::add(int s0) {
    t += s0; s[n++] = s0;
  }
  forceinline int
  SizeSet::card(void) const {
    return n;
  }
  forceinline int
  SizeSet::total(void) const {
    return t;
  }
  forceinline int
  SizeSet::operator [](int i) const {
    return s[i];
  }

  forceinline
  SizeSetMinusOne::SizeSetMinusOne(void) {}
  forceinline
  SizeSetMinusOne::SizeSetMinusOne(Region& region, int n_max) 
    : SizeSet(region,n_max), p(-1) {}
  forceinline void
  SizeSetMinusOne::minus(int s0) {
    // This rests on the fact that items are removed in order
    do
      p++;
    while (s[p] > s0);
    assert(p < n);
  }
  forceinline int
  SizeSetMinusOne::card(void) const {
    assert(p >= 0);
    return n - 1;
  }
  forceinline int
  SizeSetMinusOne::total(void) const {
    assert(p >= 0);
    return t - s[p];
  }
  forceinline int
  SizeSetMinusOne::operator [](int i) const {
    assert(p >= 0);
    return s[(i < p) ? i : i+1];
  }

  

  /*
   * Packing propagator
   *
   */

  forceinline
  Pack::Pack(Home home, ViewArray<OffsetView>& l0, ViewArray<Item>& bs0)
    : Propagator(home), l(l0), bs(bs0), t(0) {
    l.subscribe(home,*this,PC_INT_BND);
    bs.subscribe(home,*this,PC_INT_DOM);
    for (int i=bs.size(); i--; )
      t += bs[i].size();
  }

  forceinline
  Pack::Pack(Space& home, bool shared, Pack& p) 
    : Propagator(home,shared,p), t(p.t) {
    l.update(home,shared,p.l);
    bs.update(home,shared,p.bs);
  }

  forceinline size_t 
  Pack::dispose(Space& home) {
    l.cancel(home,*this,PC_INT_BND);
    bs.cancel(home,*this,PC_INT_DOM);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class SizeSet>
  forceinline bool
  Pack::nosum(const SizeSet& s, int a, int b, int& ap, int& bp) {
    if ((a <= 0) || (b >= s.total()))
      return false;
    int n=s.card()-1;
    int sc=0;
    int kp=0;
    while (sc + s[n-kp] < a) {
      sc += s[n-kp];
      kp++;
    }
    int k=0;
    int sa=0, sb = s[n-kp];
    while ((sa < a) && (sb <= b)) {
      sa += s[k++];
      if (sa < a) {
        kp--;
        sb += s[n-kp];
        sc -= s[n-kp];
        while (sa + sc >= a) {
          kp--;
          sc -= s[n-kp];
          sb += s[n-kp] - s[n-kp-k-1];
        }
      }
    }
    ap = sa + sc; bp = sb;
    return sa < a;
  }

  template<class SizeSet>
  forceinline bool
  Pack::nosum(const SizeSet& s, int a, int b) {
    int ap, bp;
    return nosum(s, a, b, ap, bp);
  }

}}}

// STATISTICS: int-prop

