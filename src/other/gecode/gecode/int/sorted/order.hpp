/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Patrick Pekczynski <pekczynski@ps.uni-sb.de>
 *
 *  Copyright:
 *     Patrick Pekczynski, 2004
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

namespace Gecode { namespace Int { namespace Sorted {

  /**
   *  \brief Build \f$\sigma\f$
   *
   *  Creates a sorting permutation \f$\sigma\f$ by sorting the
   *  views in \a x according to their lower bounds
   */

  template<class View, bool Perm>
  inline void
  sort_sigma(Space& home, ViewArray<View>& x, ViewArray<View>& z) {
    if (Perm) {
      Region r(home);
      ViewPair<View>* xz = r.alloc<ViewPair<View> >(x.size());
      for (int i=x.size(); i--; ) {
        xz[i].x=x[i]; xz[i].z=z[i];
      }
      TupleMinIncExt<View> min_inc;
      Support::quicksort<ViewPair<View>,TupleMinIncExt<View> >
        (&xz[0], x.size(), min_inc);
      for (int i=x.size(); i--; ) {
        x[i]=xz[i].x; z[i]=xz[i].z;
      }
    } else {
      TupleMinInc<View> min_inc;
      Support::quicksort<View,TupleMinInc<View> >(&x[0], x.size(), min_inc);
    }
  }

  /**
   *  \brief Build \f$\tau\f$
   *
   *  Creates a sorting permutation \f$\tau\f$ by sorting
   *  a given array of indices in \a tau according to the upper bounds
   *  of the views in \a x
   */

  template<class View, bool Perm>
  inline void
  sort_tau(ViewArray<View>& x, ViewArray<View>& z, int tau[]) {
    if (Perm) {
      TupleMaxIncExt<View> ltmax(x,z);
      Support::quicksort(&(*tau), x.size(), ltmax);
    } else {
      TupleMaxInc<View> ltmax(x);
      Support::quicksort(&(*tau), x.size(), ltmax);
    }
  }

  /**
   * \brief Performing normalization on the views in \a y
   *
   * The views in \a y are called normalized if
   * \f$\forall i\in\{0,\dots, n-1\}: min(y_0) \leq \dots \leq min(y_{n-1}) \wedge
   *  max(y_0) \leq \dots \leq max(y_{n-1})\f$ holds.
   */
  template<class View>
  inline bool
  normalize(Space& home,
            ViewArray<View>& y,
            ViewArray<View>& x,
            bool& nofix) {

    int ys = y.size();
    for (int i = 1; i < ys; i++) {
      ModEvent me_lb = y[i].gq(home, y[i - 1].min());
      if (me_failed(me_lb))
        return false;
      nofix |= (me_modified(me_lb) && y[i - 1].min() != y[i].min());
    }

    for (int i = ys - 1; i--; ) {
      ModEvent me_ub = y[i].lq(home, y[i + 1].max());
      if (me_failed(me_ub))
        return false;
      nofix |= (me_modified(me_ub) && y[i + 1].max() != y[i].max());
    }

    int xs = x.size();
    for (int i = xs; i--; ) {
      ModEvent me = x[i].gq(home, y[0].min());
      if (me_failed(me))
        return false;
      nofix |= (me_modified(me) && x[i].min() != y[0].min());

      me = x[i].lq(home, y[xs - 1].max());
      if (me_failed(me))
        return false;
      nofix |= (me_modified(me) && x[i].max() != y[xs - 1].max());
    }
    return true;
  }

  /**
   *  \brief Bounds consistency on the permutation views
   *
   *  Check, whether the permutation view are bounds consistent.
   *  This function tests, whether there are "crossing edges", i.e.
   *  whether the current domains permit matchings between unsorted views
   *  \a x and the sorted variables \a y violating the property
   *  that \a y is sorted.
   */

  template<class View>
  inline bool
  perm_bc(Space& home, int tau[],
          SccComponent sinfo[],
          int scclist[],
          ViewArray<View>& x,
          ViewArray<View>& z,
          bool& crossingedge,
          bool& nofix) {

    int ps = x.size();

    for (int i = 1; i < ps; i++) {
      // if there are "crossed edges"
      if (x[i - 1].min() < x[i].min()) {
        if (z[i - 1].min() > z[i].min()) {
          if (z[i].min() != sinfo[scclist[i]].leftmost) {
            // edge does not take part in solution
            if (z[i].assigned()) { // vital edge do not remove it
              if (x[i - 1].max() < x[i].min()) {
                // invalid permutation
                // the upper bound sorting cannot fix this
                return false;
              }
            } else {
              crossingedge = true;
              // and the permutation can still be changed
              // fix the permutation, i.e. modify z
              ModEvent me_z = z[i].gq(home, z[i - 1].min());
              if (me_failed(me_z)) {
                return false;
              }
              nofix |= ( me_modified(me_z) &&
                         z[i - 1].min() != z[i].min());
            }
          }
        }
      }
    }

    // the same check as above for the upper bounds
    for (int i = ps - 1; i--; ) {
      if (x[tau[i]].max() < x[tau[i + 1]].max()) {
        if (z[tau[i]].max() > z[tau[i + 1]].max()) {
          if (z[tau[i]].max() != sinfo[scclist[tau[i]]].rightmost) {
            // edge does not take part in solution
            if (z[tau[i]].assigned()) {
              if (x[tau[i + 1]].min() > x[tau[i]].max()) {
                // invalid permutation
                return false;
              }
            } else {
              crossingedge = true;
              ModEvent me_z = z[tau[i]].lq(home, z[tau[i + 1]].max());
              if (me_failed(me_z)) {
                return false;
              }
              nofix |= (me_modified(me_z) &&
                        z[tau[i + 1]].max() != z[tau[i]].max());
            }
          }
        }
      }
    }

    return true;
  }

}}}

// STATISTICS: int-prop

