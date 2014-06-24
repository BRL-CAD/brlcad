/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
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

#include <gecode/int/element.hh>

namespace Gecode { namespace Int { namespace Element {

  Actor*
  Pair::copy(Space& home, bool share) {
    return new (home) Pair(home,share,*this);
  }

  /// Value iterator for pair of iterators
  class PairValues {
  private:
    /// View for x-values
    IntView x;
    /// Iterator for x-values
    ViewValues<IntView> xv;
    /// Iterator for y-values
    ViewValues<IntView> yv;
    /// Width
    int w;
  public:
    /// \name Constructors and initialization
    //@{
    /// Initialize with views \a x and \a y and width \a w
    PairValues(IntView x, IntView y, int w);
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
    int val(void) const;
    //@}
  };

  forceinline
  PairValues::PairValues(IntView x0, IntView y0, int w0)
    : x(x0), xv(x0), yv(y0), w(w0) {}
  forceinline void
  PairValues::operator ++(void) {
    ++xv;
    if (!xv()) {
      xv.init(x); ++yv;
    }
  }
  forceinline bool
  PairValues::operator ()(void) const {
    return yv();
  }
  forceinline int
  PairValues::val(void) const {
    return xv.val()+w*yv.val();
  }


  ExecStatus
  Pair::propagate(Space& home, const ModEventDelta&) {
    Region r(home);
    
    if (x0.assigned()) {
      // Bitset for supported div and mod values
      Support::BitSet<Region> d(r,static_cast<unsigned int>((x2.max() / w)+1));
      for (ViewValues<IntView> i(x2); i(); ++i) {
        d.set(static_cast<unsigned int>(i.val() / w));
      }
      Iter::Values::BitSet<Support::BitSet<Region> > id(d,x1.min(),x1.max());
      GECODE_ME_CHECK(x1.inter_v(home,id,false));
    } else {
      // Bitset for supported div and mod values
      Support::BitSet<Region> 
        d(r,static_cast<unsigned int>((x2.max() / w)+1)), 
        m(r,static_cast<unsigned int>(w));
      for (ViewValues<IntView> i(x2); i(); ++i) {
        d.set(static_cast<unsigned int>(i.val() / w)); 
        m.set(static_cast<unsigned int>(i.val() % w));
      }
      Iter::Values::BitSet<Support::BitSet<Region> > im(m,x0.min(),x0.max());
      GECODE_ME_CHECK(x0.inter_v(home,im,false));
      Iter::Values::BitSet<Support::BitSet<Region> > id(d,x1.min(),x1.max());
      GECODE_ME_CHECK(x1.inter_v(home,id,false));
    }

    if (x0.assigned() && x1.assigned()) {
      GECODE_ME_CHECK(x2.eq(home,x0.val()+w*x1.val()));
      return home.ES_SUBSUMED(*this);
    } else if (x1.assigned()) {
      OffsetView x0x1w(x0,x1.val()*w);
      GECODE_REWRITE(*this,(Rel::EqDom<OffsetView,IntView>
                            ::post(home(*this),x0x1w,x2)));
    }

    PairValues xy(x0,x1,w);
    GECODE_ME_CHECK(x2.inter_v(home,xy,false));

    if (x2.assigned()) {
      GECODE_ME_CHECK(x0.eq(home,x2.val() % w));
      GECODE_ME_CHECK(x1.eq(home,static_cast<int>(x2.val() / w)));
      return home.ES_SUBSUMED(*this);
    }

    return ES_NOFIX;
  }

}}}

// STATISTICS: int-prop

