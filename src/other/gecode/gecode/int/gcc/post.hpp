/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Patrick Pekczynski <pekczynski@ps.uni-sb.de>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Patrick Pekczynski, 2004/2005
 *     Christian Schulte, 2009
 *     Guido Tack, 2009
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

#include <gecode/int/linear.hh>
#include <gecode/int/distinct.hh>

namespace Gecode { namespace Int { namespace GCC {

  template<class Card>
  /// %Sort by increasing cardinality
  class CardLess : public Support::Less<Card> {
  public:
    bool operator ()(const Card& x, const Card& y) {
      return x.card() < y.card();
    }
  };

  /**
   * \brief Post side constraints for the GCC
   *
   */
  template<class Card>
  ExecStatus
  postSideConstraints(Home home, ViewArray<IntView>& x, ViewArray<Card>& k) {
    CardLess<Card> cl;
    Support::quicksort(&k[0], k.size(), cl);
    Region r(home);

    {
      int smin = 0;
      int smax = 0;
      for (int i = k.size(); i--; ) {
        GECODE_ME_CHECK((k[i].gq(home, 0)));
        GECODE_ME_CHECK((k[i].lq(home, x.size())));
        smin += k[i].min();
        smax += k[i].max();
      }

      // not enough variables to satisfy min req
      if ((x.size() < smin) || (smax < x.size()))
        return ES_FAILED;
    }

    // Remove all values from the x that are not in v
    {
      int* v = r.alloc<int>(k.size());
      for (int i=k.size(); i--;)
        v[i]=k[i].card();
      Support::quicksort(v,k.size());
      for (int i=x.size(); i--; ) {
        Iter::Values::Array iv(v,k.size());
        GECODE_ME_CHECK(x[i].inter_v(home, iv, false));
      }
    }

    // Remove all values with 0 max occurrence
    // and remove corresponding occurrence variables from k
    {
      // The number of zeroes
      int n_z = 0;
      for (int i=k.size(); i--;)
        if (k[i].max() == 0)
          n_z++;

      if (n_z > 0) {
        int* z = r.alloc<int>(n_z);
        n_z = 0;
        int n_k = 0;
        for (int i=0; i<k.size(); i++)
          if (k[i].max() == 0) {
            z[n_z++] = k[i].card();            
          } else {
            k[n_k++] = k[i];
          }
        k.size(n_k);
        Support::quicksort(z,n_z);
        for (int i=x.size(); i--;) {
          Iter::Values::Array zi(z,n_z);
          GECODE_ME_CHECK(x[i].minus_v(home,zi,false));
        }
      }
    }

    if (Card::propagate) {
      Linear::Term<IntView>* t = r.alloc<Linear::Term<IntView> >(k.size());
      for (int i = k.size(); i--; ) {
        t[i].a=1; t[i].x=k[i].base();
      }
      Linear::post(home,t,k.size(),IRT_EQ,x.size(),ICL_BND);
    }

    return ES_OK;
  }


  /**
   * \brief Check if GCC is equivalent to distinct
   *
   */
  template<class Card>
  inline bool
  isDistinct(Home home, ViewArray<IntView>& x, ViewArray<Card>& k) {
    if (Card::propagate) {
      Region r(home);
      ViewRanges<IntView>* xrange = r.alloc<ViewRanges<IntView> >(x.size());
      for (int i = x.size(); i--; ){
        ViewRanges<IntView> iter(x[i]);
        xrange[i] = iter;
      }
      Iter::Ranges::NaryUnion drl(r, &xrange[0], x.size());
      if (static_cast<unsigned int>(k.size()) == Iter::Ranges::size(drl)) {
        for (int i=k.size(); i--;)
          if (k[i].min() != 1 || k[i].max() != 1)
            return false;
        return true;
      } else {
        return false;
      }
    } else {
      for (int i=k.size(); i--;)
        if (k[i].min() != 0 || k[i].max() != 1)
          return false;
      return true;
    }
  }

}}}

// STATISTICS: int-prop
