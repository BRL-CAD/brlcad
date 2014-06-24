/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
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

namespace Gecode { namespace Int {

  template<class View, class A>
  forceinline void
  SupportValues<View,A>::reset(void) {
    rp = rp_fst; v = rp->min;
    max = rp->min + static_cast<int>((rp+1)->pos - rp->pos) - 1;
  }

  template<class View, class A>
  inline
  SupportValues<View,A>::SupportValues(A& a0, View x0)
    : a(a0), x(x0), bs(a,x.size(),true) {
    unsigned int n = 0;
    for (ViewRanges<View> r(x); r(); ++r)
      n++;
    rp_fst = a.template alloc<RangePos>(n+1);
    rp_lst = rp_fst + n;
    unsigned int p = 0;
    int i = 0;
    for (ViewRanges<View> r(x); r(); ++r) {
      rp_fst[i].min = r.min();
      rp_fst[i].pos = p;
      p += r.width(); i++;
    }
    rp_fst[i].pos=p;
    reset();
  }

  template<class View, class A>
  forceinline
  SupportValues<View,A>::~SupportValues(void) {
    bs.dispose(a);
    a.free(rp_fst,static_cast<unsigned long int>(rp_lst-rp_fst+1));
  }

  template<class View, class A>
  forceinline void
  SupportValues<View,A>::operator ++(void) {
    if (++v > max)
      if (++rp < rp_lst) {
        v = rp->min;
        max = rp->min + static_cast<int>((rp+1)->pos - rp->pos) - 1;
      }
  }

  template<class View, class A>
  forceinline bool
  SupportValues<View,A>::operator ()(void) const {
    return rp < rp_lst;
  }

  template<class View, class A>
  forceinline int
  SupportValues<View,A>::val(void) const {
    return v;
  }

  template<class View, class A>
  forceinline void
  SupportValues<View,A>::support(void) {
    bs.clear(rp->pos + static_cast<unsigned int>(v-rp->min));
  }

  template<class View, class A>
  forceinline bool
  SupportValues<View,A>::_support(int n) {
    RangePos* l = rp_fst;
    RangePos* r = rp_lst-1;
    while (true) {
      if (l > r) return false;
      RangePos* m = l + (r-l)/2;
      int max = m->min + static_cast<int>((m+1)->pos - m->pos) - 1;
      if ((n >= m->min) && (n <= max)) {
        bs.clear(m->pos + static_cast<unsigned int>(n-m->min));
        return true;
      }
      if (l == r) return false;
      if (n < m->min)
        r=m-1;
      else
        l=m+1;
    }
    GECODE_NEVER;
    return false;
  }

  template<class View, class A>
  forceinline bool
  SupportValues<View,A>::support(int n) {
    if ((n < x.min()) || (n > x.max()))
      return false;
    return _support(n);
  }

  template<class View, class A>
  forceinline bool
  SupportValues<View,A>::support(long long int n) {
    if ((n < x.min()) || (n > x.max()))
      return false;
    return _support(static_cast<int>(n));
  }

  template<class View, class A>
  forceinline void
  SupportValues<View,A>::Unsupported::find(void) {
    // Skip all supported positions
    while ((p < sv.x.size()) && !sv.bs.get(p))
      p = sv.bs.next(p);
    // Move to matching range
    while ((rp < sv.rp_lst) && (p >= (rp+1)->pos))
      rp++;
  }

  template<class View, class A>
  forceinline
  SupportValues<View,A>::Unsupported::Unsupported(SupportValues& sv0)
    : rp(sv0.rp_fst), p(0), sv(sv0) {
    find();
  }

  template<class View, class A>
  forceinline void
  SupportValues<View,A>::Unsupported::operator ++(void) {
    p++; find();
  }

  template<class View, class A>
  forceinline bool
  SupportValues<View,A>::Unsupported::operator ()(void) const {
    return rp < sv.rp_lst;
  }

  template<class View, class A>
  forceinline int
  SupportValues<View,A>::Unsupported::val(void) const {
    return static_cast<int>(rp->min+(p-rp->pos));
  }

  template<class View, class A>
  inline ModEvent
  SupportValues<View,A>::tell(Space& home) {
    Unsupported u(*this);
    return x.minus_v(home,u,false);
  }

}}

// STATISTICS: int-prop

