/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

namespace Gecode { namespace Int { namespace Distinct {

  /*
   * Eliminating singleton variables
   *
   */
  template<class View, bool complete>
  ExecStatus
  prop_val(Space& home, ViewArray<View>& x) {
    assert(x.size() > 1);
    int n = x.size();

    Region r(home);
    int* stack = r.alloc<int>(n);
    int* c_v = &stack[0];
    // c_n is the current number of values on stack
    int c_n = 0;

    // Collect all assigned variables on stack
    for (int i = n; i--; )
      if (x[i].assigned()) {
        c_v[c_n++]=x[i].val(); x[i]=x[--n];
      }

    // The number of trips
    int t = 0;
    do {
      t++;
      if (!complete && (t > 16)) {
        // Give up after sixteen iterations, but the values must be
        // propagated first
        // Maybe we are lucky in that this iteration does the trick...
        ExecStatus es = ES_FIX;
        while (c_n > 0) {
          int v = c_v[--c_n];
          // Check whether value is on stack only once
          for (int i = c_n; i--; )
            if (c_v[i] == v)
              goto failed;
          // Tell and do not collect new values
          for (int i = n; i--; ) {
            ModEvent me = x[i].nq(home,v);
            if (me_failed(me))
              goto failed;
            if (me == ME_INT_VAL)
              es = ES_NOFIX;
          }
        }
        x.size(n);
        return es;
      }
      if (c_n > 31) {
        // Many values, use full domain operation
        IntSet d(&c_v[0],c_n);
        // If the size s of d is different from the number of values,
        // a value must have appeared multiply: failure
        if (d.size() != static_cast<unsigned int>(c_n))
          goto failed;
        // We do not need the values on the stack any longer, reset
        c_n = 0;
        // Tell and collect new values
        for (int i = n; i--; )
          if ((d.min() <= x[i].max()) && (d.max() >= x[i].min())) {
            IntSetRanges dr(d);
            ModEvent me = x[i].minus_r(home,dr,false);
            if (me_failed(me))
              goto failed;
            if (me == ME_INT_VAL) {
              c_v[c_n++]=x[i].val(); x[i]=x[--n];
            }
          }
      } else {
        // Values for next iteration
        int* n_v = &c_v[c_n];
        // Stack top for the next iteration
        int n_n = 0;
        while (c_n > 0) {
          int v = c_v[--c_n];
          // Check whether value is not on current stack
          for (int i = c_n; i--; )
            if (c_v[i] == v)
              goto failed;
          // Check whether value is not on next stack
          for (int i = n_n; i--; )
            if (n_v[i] == v)
              goto failed;
          // Tell and collect new values
          for (int i = n; i--; ) {
            ModEvent me = x[i].nq(home,v);
            if (me_failed(me))
              goto failed;
            if (me == ME_INT_VAL) {
              n_v[n_n++]=x[i].val(); x[i]=x[--n];
            }
          }
        }
        c_v = n_v; c_n = n_n;
      }
    } while (c_n > 0);
    x.size(n);
    return ES_FIX;
  failed:
    x.size(0);
    return ES_FAILED;
  }


  /*
   * The propagator proper
   *
   */
  template<class View>
  forceinline
  Val<View>::Val(Home home, ViewArray<View>& x)
  : NaryPropagator<View,PC_INT_VAL>(home,x) {}

  template<class View>
  forceinline
  Val<View>::Val(Space& home, bool share, Val<View>& p)
    : NaryPropagator<View,PC_INT_VAL>(home,share,p) {}

  template<class View>
  Actor*
  Val<View>::copy(Space& home, bool share) {
    return new (home) Val<View>(home,share,*this);
  }

  template<class View>
  ExecStatus
  Val<View>::propagate(Space& home, const ModEventDelta&) {
    GECODE_ES_CHECK((prop_val<View,true>(home,x)));
    return (x.size() < 2) ? home.ES_SUBSUMED(*this) : ES_FIX;
  }

  template<class View>
  ExecStatus
  Val<View>::post(Home home, ViewArray<View>& x) {
    if (x.size() == 2)
      return Rel::Nq<View>::post(home,x[0],x[1]);
    if (x.size() > 2)
      (void) new (home) Val<View>(home,x);
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

