/*
 *  Copyright (c) Mikael Lagerkvist, 2007
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

#include "gecode/kernel.hh"
#include "gecode/int.hh"
#include "gecode/iter.hh"
#include "gecode/int/rel.hh"

namespace Extra {
  using namespace ::Gecode;

  /**
   * \brief Domain-consistent propagator for \f$ x_1=x_2\lor x_1=x_3\f$.
   */
  template <class View>
  class DisEq : public TernaryPropagator<View,Int::PC_INT_DOM> {
  protected:
    using TernaryPropagator<View,Int::PC_INT_DOM>::x0;
    using TernaryPropagator<View,Int::PC_INT_DOM>::x1;
    using TernaryPropagator<View,Int::PC_INT_DOM>::x2;

    /// Constructor for cloning \a p
    DisEq(Space* home, bool share, DisEq& p);
    /// Constructor for posting
    DisEq(Space* home, View x0, View x1, View x2);
  public:
    /// Constructor for rewriting \a p during cloning
    DisEq(Space* home, bool share, Propagator& p, View x0, View x1, View x2);
    /// Copy propagator during cloning
    virtual Actor* copy(Space* home, bool share);
    /// Perform propagation
    virtual ExecStatus propagate(Space* home, ModEventDelta);
    /// Post propagator
    static  ExecStatus post(Space* home, View x0, View x1, View x2);
  };

  template <class View>
  inline
  DisEq<View>::DisEq(Space* home, View dex0, View dex1, View dex2)
    : TernaryPropagator<View,Int::PC_INT_DOM>(home,dex0,dex1,dex2) {}

  template <class View>
  ExecStatus
  DisEq<View>::post(Space* home, View dex0, View dex1, View dex2) {
    (void) new (home) DisEq<View>(home,dex0,dex1,dex2);
    return ES_OK;
  }

  template <class View>
  inline
  DisEq<View>::DisEq(Space* home, bool share, DisEq<View>& p)
    : TernaryPropagator<View,Int::PC_INT_DOM>(home,share,p) {}

  template <class View>
  inline
  DisEq<View>::DisEq(Space* home, bool share, Propagator& p,
                     View dex0, View dex1, View dex2)
    : TernaryPropagator<View,Int::PC_INT_DOM>(home,share,p,dex0,dex1,dex2) {}

  template <class View>
  Actor*
  DisEq<View>::copy(Space* home, bool share) {
    return new (home) DisEq<View>(home,share,*this);
  }

  template <class View>
  ExecStatus
  DisEq<View>::propagate(Space* home, ModEventDelta) {
    typedef Int::ViewRanges<View> Ranges;

    Ranges vr1(x1), vr2(x2);
    Iter::Ranges::Union<Ranges, Ranges> u12(vr1, vr2);
    GECODE_ME_CHECK(x0.inter_r(home, u12, false));

    if (rtest_nq_dom(x0, x1) == Int::RT_TRUE)
      GECODE_REWRITE(this, (Int::Rel::EqDom<View, View>::post(home, x0, x2)));
    if (rtest_nq_dom(x0, x2) == Int::RT_TRUE)
      GECODE_REWRITE(this, (Int::Rel::EqDom<View, View>::post(home, x0, x1)));

    return x0.assigned() && (x1.assigned() || x2.assigned()) ? ES_SUBSUMED(this,home) : ES_FIX;
  }

  /** \brief Post propagator for \f$ x_1=x_2\lor x_1=x_3\f$
   */
  void diseq(Space* home, IntVar x1, IntVar x2, IntVar x3) {
    Int::IntView v1(x1), v2(x2), v3(x3);
    GECODE_ES_FAIL(DisEq<Int::IntView>::post(home, v1, v2, v3));
  }
}
