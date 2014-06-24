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

  template<class View, bool shared>
  forceinline
  Basic<View,shared>::Basic(Home home, ViewArray<View>& x,
                            const TupleSet& t)
    : Base<View>(home,x,t) {
  }

  template<class View, bool shared>
  forceinline ExecStatus
  Basic<View,shared>::post(Home home, ViewArray<View>& x,
                           const TupleSet& t) {
    // All variables in the correct domain
    for (int i = x.size(); i--; ) {
      GECODE_ME_CHECK(x[i].gq(home, t.min()));
      GECODE_ME_CHECK(x[i].lq(home, t.max()));
    }
    (void) new (home) Basic<View,shared>(home,x,t);
    return ES_OK;
  }

  template<class View, bool shared>
  forceinline
  Basic<View,shared>::Basic(Space& home, bool share, Basic<View,shared>& p)
    : Base<View>(home,share,p) {
  }

  template<class View, bool shared>
  PropCost
  Basic<View,shared>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) == ME_INT_VAL)
      return PropCost::quadratic(PropCost::HI,x.size());
    else
      return PropCost::cubic(PropCost::HI,x.size());
  }

  template<class View, bool shared>
  Actor*
  Basic<View,shared>::copy(Space& home, bool share) {
    return new (home) Basic<View,shared>(home,share,*this);
  }

  template<class View, bool shared>
  ExecStatus
  Basic<View,shared>::propagate(Space& home, const ModEventDelta&) {
    // Set up datastructures

    // Bit-sets for amortized O(1) access to domains
    Region r(home);
    BitSet* dom = r.alloc<BitSet>(x.size());
    init_dom(home, dom);

    // Bit-sets for processed values.
    BitSet* has_support = r.alloc<BitSet>(x.size());
    for (int i = x.size(); i--; )
      has_support[i].init(home, ts()->domsize);


    // Values to prune
    Support::StaticStack<int,Region> nq(r,static_cast<int>(ts()->domsize));

    // Run algorithm

    // Check consistency for each view-value pair
    for (int i = x.size(); i--; ) {
      for (ViewValues<View> vv(x[i]); vv(); ++vv) {
        // Value offset for indexing
        int val = vv.val() - ts()->min;
        if (!has_support[i].get(static_cast<unsigned int>(val))) {
          // Find support for value vv.val() in view
          Tuple l = find_support(dom, i, val);
          if (l == NULL) {
            // No possible supports left
            nq.push(vv.val());
          } else {
            // Mark values as supported
            // Only forward direction marking is needed since all
            // previous values have been checked
            for (int j = i; j--; ) {
              has_support[j].set(static_cast<unsigned int>(l[j]- ts()->min));
              assert(has_support[j].get(l[j] - ts()->min));
            }
          }
        }
      }

      // Prune values for x[i] which do not have support anymore
      while (!nq.empty())
        GECODE_ME_CHECK(x[i].nq(home,nq.pop()));
    }

    for (int i = x.size(); i--; )
      if (!x[i].assigned())
        return shared ? ES_NOFIX : ES_FIX;
    return home.ES_SUBSUMED(*this);
  }

}}}

// STATISTICS: int-prop

