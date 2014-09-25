/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Stefano Gualandi <stefano.gualandi@gmail.com>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Stefano Gualandi, 2013
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

#include <gecode/int/bin-packing.hh>

namespace Gecode {

  void
  binpacking(Home home, 
             const IntVarArgs& l, 
             const IntVarArgs& b, const IntArgs& s,
             IntConLevel) {
    using namespace Int;
    if (l.same(home,b))
      throw ArgumentSame("Int::binpacking");
    if (b.size() != s.size())
      throw ArgumentSizeMismatch("Int::binpacking");      
    for (int i=s.size(); i--; )
      Limits::nonnegative(s[i],"Int::binpacking");
    if (home.failed()) return;

    ViewArray<OffsetView> lv(home,l.size());
    for (int i=l.size(); i--; )
      lv[i] = OffsetView(l[i],0);

    ViewArray<BinPacking::Item> bs(home,b.size());
    for (int i=bs.size(); i--; )
      bs[i] = BinPacking::Item(b[i],s[i]);

    GECODE_ES_FAIL(Int::BinPacking::Pack::post(home,lv,bs));
  }

  IntSet
  binpacking(Home home, int d,
             const IntVarArgs& l, const IntVarArgs& b, 
             const IntArgs& s, const IntArgs& c,
             IntConLevel) {
    using namespace Int;

    if (l.same(home,b))
      throw ArgumentSame("Int::binpacking");

    // The number of items
    int n = b.size();
    // The number of bins
    int m = l.size() / d;

    // Check input sizes
    if ((n*d != s.size()) || (m*d != l.size()) || (d != c.size()))
      throw ArgumentSizeMismatch("Int::binpacking");      
    for (int i=s.size(); i--; )
      Limits::nonnegative(s[i],"Int::binpacking");
    for (int i=c.size(); i--; )
      Limits::nonnegative(c[i],"Int::binpacking");

    if (home.failed()) 
      return IntSet::empty;

    // Capacity constraint for each dimension
    for (int k=d; k--; )
      for (int j=m; j--; ) {
        IntView li(l[j*d+k]);
        if (me_failed(li.lq(home,c[k]))) {
          home.fail();
          return IntSet::empty;
        }
      }

    // Post a binpacking constraint for each dimension
    for (int k=d; k--; ) {
      ViewArray<OffsetView> lv(home,m);
      for (int j=m; j--; )
        lv[j] = OffsetView(l[j*d+k],0);
      
      ViewArray<BinPacking::Item> bv(home,n);
      for (int i=n; i--; )
        bv[i] = BinPacking::Item(b[i],s[i*d+k]);
      
      if (Int::BinPacking::Pack::post(home,lv,bv) == ES_FAILED) {
        home.fail();
        return IntSet::empty;
      }
    }


    // Clique Finding and distinct posting
    {
      // First construct the conflict graph
      Region r(home);
      BinPacking::ConflictGraph cg(home,r,b,m);

      for (int i=0; i<n-1; i++) {
        for (int j=i+1; j<n; j++) {
          unsigned int nl = 0;
          unsigned int ds = 0;
          IntVarValues ii(b[i]), jj(b[j]);
          while (ii() && jj()) {
            if (ii.val() < jj.val()) {
              ++ii;
            } else if (ii.val() > jj.val()) {
              ++jj;
            } else {
              ds++;
              for (int k=d; k--; )
                if (s[i*d+k] + s[j*d+k] > c[k]) {
                  nl++;
                  break;
                }
              ++ii; ++jj;
            }
          }
          if (nl >= ds)
            cg.edge(i,j);
        }
      }

      if (cg.post() == ES_FAILED)
        home.fail();

      // The clique can be computed even if home is failed
      return cg.maxclique();
    }
  }
  
}

// STATISTICS: int-post
