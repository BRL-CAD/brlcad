/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Mikael Lagerkvist, 2007
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
namespace Gecode { namespace Int { namespace Extensional {
  /*
   * The propagator proper
   *
   */

  template<class View, bool subscribe>
  forceinline
  Base<View,subscribe>::Base(Home home, ViewArray<View>& x0,
                             const TupleSet& t)
    : Propagator(home), x(x0), tupleSet(t), last_data(NULL) {
    if (subscribe)
      x.subscribe(home, *this, PC_INT_DOM);

    assert(ts()->finalized());

    init_last(home, ts()->last, ts()->tuple_data);

    home.notice(*this,AP_DISPOSE);
  }

  template<class View, bool subscribe>
  forceinline
  Base<View,subscribe>::Base(Space& home, bool share, Base<View,subscribe>& p)
    : Propagator(home,share,p), last_data(NULL) {
    x.update(home, share, p.x);
    tupleSet.update(home, share, p.tupleSet);

    init_last(home, p.last_data, p.ts()->tuple_data);
  }

  template<class View, bool subscribe>
  forceinline void
  Base<View,subscribe>::init_last(Space& home, Tuple** source, Tuple* base) {
    if (last_data == NULL) {
      int literals = static_cast<int>(ts()->domsize*x.size());
      last_data = home.alloc<Tuple*>(literals);
      for (int i = literals; i--; )
        last_data[i] = ts()->tuple_data+(source[i]-base);
    }
  }

  template<class View, bool subscribe>
  forceinline TupleSet::TupleSetI*
  Base<View,subscribe>::ts(void) {
    return tupleSet.implementation();
  }

  template<class View, bool subscribe>
  PropCost
  Base<View,subscribe>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::quadratic(PropCost::HI,x.size());
  }

#define GECODE_LAST_TUPLE(l) (*(l))

  template<class View, bool subscribe>
  forceinline Tuple
  Base<View,subscribe>::last(int i, int n) {
    return GECODE_LAST_TUPLE(last_data[(i*ts()->domsize) + n]);
  }

  template<class View, bool subscribe>
  forceinline Tuple
  Base<View,subscribe>::last_next(int i, int n) {
    assert(last(i,n) != NULL);
    assert(last(i,n)[i] == n+ts()->min);
    int pos = (i*static_cast<int>(ts()->domsize)) + n;
    ++(last_data[pos]);
    if (last(i,n)[i] != (n+ts()->min))
      last_data[pos] = ts()->nullpointer;
    return last(i,n);
  }


  template<class View, bool subscribe>
  forceinline void
  Base<View,subscribe>::init_dom(Space& home, Domain dom) {
    unsigned int domsize = ts()->domsize;
    for (int i = x.size(); i--; ) {
      dom[i].init(home, domsize);
      for (ViewValues<View> vv(x[i]); vv(); ++vv)
        dom[i].set(static_cast<unsigned int>(vv.val()-ts()->min));
    }
  }

  template<class View, bool subscribe>
  forceinline bool
  Base<View,subscribe>::valid(Tuple t, Domain dom) {
    for (int i = x.size(); i--; )
      if (!dom[i].get(static_cast<unsigned int>(t[i]-ts()->min)))
        return false;
    return true;
  }
#undef GECODE_LAST_TUPLE
  template<class View, bool subscribe>
  forceinline Tuple
  Base<View,subscribe>::find_support(Domain dom, int i, int n) {
    Tuple l = last(i,n);
    while ((l != NULL) && !valid(l, dom))
      l = last_next(i,n);
    return l;
  }


  template<class View, bool subscribe>
  forceinline size_t
  Base<View,subscribe>::dispose(Space& home) {
    home.ignore(*this,AP_DISPOSE);
    (void) Propagator::dispose(home);
    if (subscribe)
      x.cancel(home,*this,PC_INT_DOM);
    // take care of last_data
    unsigned int literals = ts()->domsize*x.size();
    home.rfree(last_data, sizeof(Tuple*)*literals);
    (void) tupleSet.~TupleSet();
    return sizeof(*this);
  }

}}}

// STATISTICS: int-prop

