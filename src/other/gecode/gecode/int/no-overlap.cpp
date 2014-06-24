/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

#include <gecode/int/no-overlap.hh>

namespace Gecode {

  namespace Int { namespace NoOverlap {

    bool
    optional(const BoolVarArgs& m) {
      for (int i=m.size(); i--; )
        if (m[i].none())
          return true;
      return false;
    }

  }}

  void
  nooverlap(Home home, 
            const IntVarArgs& x, const IntArgs& w, 
            const IntVarArgs& y, const IntArgs& h,
            IntConLevel) {
    using namespace Int;
    using namespace NoOverlap;
    if ((x.size() != w.size()) || (x.size() != y.size()) || 
        (x.size() != h.size()))
      throw ArgumentSizeMismatch("Int::nooverlap");      
    for (int i=x.size(); i--; ) {
      Limits::nonnegative(w[i],"Int::nooverlap");
      Limits::nonnegative(h[i],"Int::nooverlap");
      Limits::check(static_cast<long long int>(x[i].max()) + w[i],
                    "Int::nooverlap");
      Limits::check(static_cast<long long int>(y[i].max()) + h[i],
                    "Int::nooverlap");
    }
    if (home.failed()) return;

    ManBox<FixDim,2>* b 
      = static_cast<Space&>(home).alloc<ManBox<FixDim,2> >(x.size());
    for (int i=x.size(); i--; ) {
      b[i][0] = FixDim(x[i],w[i]);
      b[i][1] = FixDim(y[i],h[i]);
    }

    GECODE_ES_FAIL((
      NoOverlap::ManProp<ManBox<FixDim,2> >::post(home,b,x.size())));
  }

  void
  nooverlap(Home home, 
            const IntVarArgs& x, const IntArgs& w, 
            const IntVarArgs& y, const IntArgs& h,
            const BoolVarArgs& m,
            IntConLevel) {
    using namespace Int;
    using namespace NoOverlap;
    if ((x.size() != w.size()) || (x.size() != y.size()) ||
        (x.size() != h.size()) || (x.size() != m.size()))
      throw ArgumentSizeMismatch("Int::nooverlap");      
    for (int i=x.size(); i--; ) {
      Limits::nonnegative(w[i],"Int::nooverlap");
      Limits::nonnegative(h[i],"Int::nooverlap");
      Limits::check(static_cast<long long int>(x[i].max()) + w[i],
                    "Int::nooverlap");
      Limits::check(static_cast<long long int>(y[i].max()) + h[i],
                    "Int::nooverlap");
    }
    if (home.failed()) return;
    
    if (optional(m)) {
      OptBox<FixDim,2>* b 
        = static_cast<Space&>(home).alloc<OptBox<FixDim,2> >(x.size());
      for (int i=x.size(); i--; ) {
        b[i][0] = FixDim(x[i],w[i]);
        b[i][1] = FixDim(y[i],h[i]);
        b[i].optional(m[i]);
      }
      GECODE_ES_FAIL((
        NoOverlap::OptProp<OptBox<FixDim,2> >::post(home,b,x.size())));
    } else {
      ManBox<FixDim,2>* b 
        = static_cast<Space&>(home).alloc<ManBox<FixDim,2> >(x.size());
      int n = 0;
      for (int i=0; i<x.size(); i++)
        if (m[i].one()) {
          b[n][0] = FixDim(x[i],w[i]);
          b[n][1] = FixDim(y[i],h[i]);
          n++;
        }
      GECODE_ES_FAIL((NoOverlap::ManProp<ManBox<FixDim,2> >::post(home,b,n)));
    }
  }

  void
  nooverlap(Home home, 
            const IntVarArgs& x0, const IntVarArgs& w, const IntVarArgs& x1,
            const IntVarArgs& y0, const IntVarArgs& h, const IntVarArgs& y1,
            IntConLevel) {
    using namespace Int;
    using namespace NoOverlap;
    if ((x0.size() != w.size())  || (x0.size() != x1.size()) || 
        (x0.size() != y0.size()) || (x0.size() != h.size()) || 
        (x0.size() != y1.size()))
      throw ArgumentSizeMismatch("Int::nooverlap");
    if (home.failed()) return;

    for (int i=x0.size(); i--; ) {
      GECODE_ME_FAIL(IntView(w[i]).gq(home,0));
      GECODE_ME_FAIL(IntView(h[i]).gq(home,0));
    }

    if (w.assigned() && h.assigned()) {
      IntArgs wc(x0.size()), hc(x0.size());
      for (int i=x0.size(); i--; ) {
        wc[i] = w[i].val();
        hc[i] = h[i].val();
      }
      nooverlap(home, x0, wc, y0, hc);
    } else {
      ManBox<FlexDim,2>* b 
        = static_cast<Space&>(home).alloc<ManBox<FlexDim,2> >(x0.size());
      for (int i=x0.size(); i--; ) {
        b[i][0] = FlexDim(x0[i],w[i],x1[i]);
        b[i][1] = FlexDim(y0[i],h[i],y1[i]);
      }
      GECODE_ES_FAIL((
        NoOverlap::ManProp<ManBox<FlexDim,2> >::post(home,b,x0.size())));
    }
  }

  void
  nooverlap(Home home, 
            const IntVarArgs& x0, const IntVarArgs& w, const IntVarArgs& x1,
            const IntVarArgs& y0, const IntVarArgs& h, const IntVarArgs& y1,
            const BoolVarArgs& m,
            IntConLevel) {
    using namespace Int;
    using namespace NoOverlap;
    if ((x0.size() != w.size())  || (x0.size() != x1.size()) || 
        (x0.size() != y0.size()) || (x0.size() != h.size()) || 
        (x0.size() != y1.size()) || (x0.size() != m.size()))
      throw ArgumentSizeMismatch("Int::nooverlap");
    if (home.failed()) return;

    for (int i=x0.size(); i--; ) {
      GECODE_ME_FAIL(IntView(w[i]).gq(home,0));
      GECODE_ME_FAIL(IntView(h[i]).gq(home,0));
    }

    if (w.assigned() && h.assigned()) {
      IntArgs wc(x0.size()), hc(x0.size());
      for (int i=x0.size(); i--; ) {
        wc[i] = w[i].val();
        hc[i] = h[i].val();
      }
      nooverlap(home, x0, wc, y0, hc, m);
    } else if (optional(m)) {
      OptBox<FlexDim,2>* b 
        = static_cast<Space&>(home).alloc<OptBox<FlexDim,2> >(x0.size());
      for (int i=x0.size(); i--; ) {
        b[i][0] = FlexDim(x0[i],w[i],x1[i]);
        b[i][1] = FlexDim(y0[i],h[i],y1[i]);
        b[i].optional(m[i]);
      }
      GECODE_ES_FAIL((
        NoOverlap::OptProp<OptBox<FlexDim,2> >::post(home,b,x0.size())));
    } else {
      ManBox<FlexDim,2>* b 
        = static_cast<Space&>(home).alloc<ManBox<FlexDim,2> >(x0.size());
      int n = 0;
      for (int i=0; i<x0.size(); i++)
        if (m[i].one()) {
          b[n][0] = FlexDim(x0[i],w[i],x1[i]);
          b[n][1] = FlexDim(y0[i],h[i],y1[i]);
          n++;
        }
      GECODE_ES_FAIL((NoOverlap::ManProp<ManBox<FlexDim,2> >::post(home,b,n)));
    }
  }

}

// STATISTICS: int-post
