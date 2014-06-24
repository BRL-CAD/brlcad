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
 *     Patrick Pekczynski, 2004
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

namespace Gecode { namespace Int { namespace GCC {

  /**
   * \defgroup GCCBndSup Support for GCC bounds propagation
   * 
   * \ingroup FuncIntProp
   */

  /**
   * \brief Class for computing unreachable values in the value %GCC propagator
   *
   * \ingroup GCCBndSup
   */
  class UnReachable {
  public:
    /// Number of variables with lower bound
    int minb;
    /// Number of variables with upper bound
    int maxb;
    /// Number of equal variables
    int eq;
    /// Number of smaller variables
    int le;
    /// Number of greater variables
    int gr;
  };

  /**
   * \brief Bounds consistency check for cardinality variables.
   * \ingroup GCCBndSup
   */

  template<class Card>
  ExecStatus
  prop_card(Space& home, 
            ViewArray<IntView>& x, ViewArray<Card>& k) {
    int n = x.size();
    int m = k.size();
    Region r(home);
    UnReachable* rv = r.alloc<UnReachable>(m);
    for(int i = m; i--; )
      rv[i].minb=rv[i].maxb=rv[i].le=rv[i].gr=rv[i].eq=0;

    for (int i = n; i--; ) {
      int min_idx;
      if (!lookupValue(k,x[i].min(),min_idx))
        return ES_FAILED;
      if (x[i].assigned()) {
        rv[min_idx].minb++;
        rv[min_idx].maxb++;
        rv[min_idx].eq++;
      } else {
        // count the number of variables
        // with lower bound k[min_idx].card()
        rv[min_idx].minb++;
        int max_idx;
        if (!lookupValue(k,x[i].max(),max_idx))
          return ES_FAILED;
        // count the number of variables
        // with upper bound k[max_idx].card()
        rv[max_idx].maxb++;
      }
    }

    rv[0].le = 0;
    int c_min = 0;
    for (int i = 1; i < m; i++) {
      rv[i].le = c_min + rv[i - 1].maxb;
      c_min += rv[i - 1].maxb;
    }

    rv[m-1].gr = 0;
    int c_max = 0;
    for (int i = m-1; i--; ) {
      rv[i].gr = c_max + rv[i + 1].minb;
      c_max += rv[i + 1].minb;
    }

    for (int i = m; i--; ) {
      int reachable = x.size() - rv[i].le - rv[i].gr;
      if (!k[i].assigned()) {
        GECODE_ME_CHECK(k[i].lq(home, reachable));
        GECODE_ME_CHECK(k[i].gq(home, rv[i].eq));
      } else {
        // check validity of the cardinality value
        if ((rv[i].eq > k[i].max()) || (k[i].max() > reachable))
          return ES_FAILED;
      }
    }

    return ES_OK;
  }


  /** \brief Consistency check, whether the cardinality values are feasible.
   * \ingroup GCCBndSup
   */
  template<class Card>
  forceinline bool
  card_consistent(ViewArray<IntView>& x, ViewArray<Card>& k) {
    int smin = 0;
    int smax = 0;
    for (int i = k.size(); i--; ) {
      smax += k[i].max();
      smin += k[i].min();
    }
    // Consistent if number of variables within cardinality bounds
    return (smin <= x.size()) && (x.size() <= smax);
  }

  /**
   * \brief Maps domain bounds to their position in hall[].bounds.
   * \ingroup GCCBndSup
   */
  class Rank {
  public:
    /**
     * \brief \f$ rank[i].min = z
     * \Leftrightarrow min(x_i) = hall[z].bounds \f$
     */
    int min;
    /**
     * \brief \f$ rank[i].max = z
     * \Leftrightarrow max(x_i) = hall[z].bounds \f$
     */
    int max;
  };

  /**
   * \brief Compares two indices \a i, \a j of two views
   * \a \f$ x_i \f$ \f$ x_j\f$ according to the
   * ascending order of the views upper bounds
   *
   * \ingroup GCCBndSup
   */
  template<class View>
  class MaxInc {
  protected:
    /// View array for comparison
    ViewArray<View> x;
  public:
    /// Constructor
    MaxInc(const ViewArray<View>& x0) : x(x0) {}
    /// Order
    forceinline bool
    operator ()(const int i, const int j) {
      return x[i].max() < x[j].max();
    }
  };


  /**
   * \brief Compares two indices \a i, \a j of two views
   * \a \f$ x_i \f$ \f$ x_j\f$ according to the
   * ascending order of the views lower bounds
   *
   * \ingroup GCCBndSup
   */
  template<class View>
  class MinInc {
  protected:
    /// View array for comparison
    ViewArray<View> x;
  public:
    /// Constructor
    MinInc(const ViewArray<View>& x0) : x(x0) {}
    /// Comparison
    forceinline bool
    operator ()(const int i, const int j) {
      return x[i].min() < x[j].min();
    }
  };


  /**
   * \brief Compares two cardinality views
   * \a \f$ x_i \f$ \f$ x_j\f$ according to the index
   *
   * \ingroup GCCBndSup
   */
  template<class Card>
  class MinIdx {
  public:
    /// Comparison
    forceinline bool
    operator ()(const Card& x, const Card& y) {
      return x.card() < y.card();
    }
  };

  /**
   * \brief Partial sum structure for constant
   *  time computation of the maximal capacity of an interval.
   *
   * \ingroup GCCBndSup
   */
  template<class Card>
  class PartialSum {
  private:
    /// sum[i] contains the partial sum from 0 to i
    int* sum;
    /// the size of the sum
    int size;
  public:
    /// Sentinels indicating whether an end of sum is reached
    int firstValue, lastValue;
    /// \name Initialization
    //@{
    /// Constructor
    PartialSum(void);
    /// Initialization
    void init(Space& home, ViewArray<Card>& k, bool up);
    /// Mark datstructure as requiring reinitialization
    void reinit(void);
    /// Test whether already initialized
    bool initialized(void) const;
    //@}
    /// \name Access
    //@{
    /// Compute the maximum capacity of an interval
    int sumup(int from, int to) const;
    /// Return smallest bound of variables
    int minValue(void) const;
    /// Return largest bound of variables
    int maxValue(void) const;
    /// Skip neigbouring array entries if their values do not differ
    int skipNonNullElementsRight(int v) const;
    /// Skip neigbouring array entries if their values do not differ
    int skipNonNullElementsLeft(int v) const;
    //@}
    /// \name Update
    //@{
    /**
     * \brief Check whether the values in the
     *        partial sum structure containting
     *        the lower cardinality bounds differ
     *        from the actual lower bounds of the
     *        cardinalities.
     */
    bool check_update_min(ViewArray<Card>& k);
    /**
     * \brief Check whether the values in the
     *        partial sum structure containting
     *        the upper cardinality bounds differ
     *        from the actual upper bounds of the
     *        cardinalities.
     */
    bool check_update_max(ViewArray<Card>& k);
    //@}
  };


  template<class Card>
  forceinline
  PartialSum<Card>::PartialSum(void) : sum(NULL), size(-1) {}

  template<class Card>
  forceinline bool
  PartialSum<Card>::initialized(void) const {
    return size != -1;
  }
  template<class Card>
  inline void
  PartialSum<Card>::init(Space& home, ViewArray<Card>& elements, bool up) {
    int i = 0;
    int j = 0;

    // Determine number of holes in the index set
    int holes = 0;
    for (i = 1; i < elements.size(); i++) {
      if (elements[i].card() != elements[i-1].card() + 1)
        holes += elements[i].card()-elements[i-1].card()-1;
    }

    // we add three elements at the beginning and two at the end
    size  = elements.size() + holes + 5;

    // memory allocation
    if (sum == NULL) {
      sum = home.alloc<int>(2*size);
    }
    int* ds  = &sum[size];

    int first = elements[0].card();

    firstValue = first - 3;
    lastValue  = first + elements.size() + holes + 1;

    // the first three elements
    for (i = 3; i--; )
      sum[i] = i;

    /*
     * copy the bounds into sum, filling up holes with zeroes
     */
    int prevCard = elements[0].card()-1;
    i = 0;
    for (j = 2; j < elements.size() + holes + 2; j++) {
      if (elements[i].card() != prevCard + 1) {
        sum[j + 1] = sum[j];
      } else if (up) {
        sum[j + 1] = sum[j] + elements[i].max();
        i++;
      } else {
        sum[j + 1] = sum[j] + elements[i].min();
        i++;
      }
      prevCard++;
    }
    sum[j + 1] = sum[j] + 1;
    sum[j + 2] = sum[j + 1] + 1;

    // Compute distances, eliminating zeroes
    i = elements.size() + holes + 3;
    j = i + 1;
    for ( ; i > 0; ) {
      while(sum[i] == sum[i - 1]) {
        ds[i] = j;
        i--;
      }
      ds[j] = i;
      i--;
      j = ds[j];
    }
    ds[j] = 0;
    ds[0] = 0;
  }

  template<class Card>
  forceinline void
  PartialSum<Card>::reinit(void) {
    size = -1;
  }


  template<class Card>
  forceinline int
  PartialSum<Card>::sumup(int from, int to) const {
    if (from <= to) {
      return sum[to - firstValue] - sum[from - firstValue - 1];
    } else {
      assert(to - firstValue - 1 >= 0);
      assert(to - firstValue - 1 < size);
      assert(from - firstValue >= 0);
      assert(from - firstValue < size);
      return sum[to - firstValue - 1] - sum[from - firstValue];
    }
  }

  template<class Card>
  forceinline int
  PartialSum<Card>::minValue(void) const {
    return firstValue + 3;
  }
  template<class Card>
  forceinline int
  PartialSum<Card>::maxValue(void) const {
    return lastValue - 2;
  }


  template<class Card>
  forceinline int
  PartialSum<Card>::skipNonNullElementsRight(int value) const {
    value -= firstValue;
    int* ds  = &sum[size];
    return (ds[value] < value ? value : ds[value]) + firstValue;
  }
  template<class Card>
  forceinline int
  PartialSum<Card>::skipNonNullElementsLeft(int value) const {
    value -= firstValue;
    int* ds  = &sum[size];
    return (ds[value] > value ? ds[ds[value]] : value) + firstValue;
  }

  template<class Card>
  inline bool
  PartialSum<Card>::check_update_max(ViewArray<Card>& k) {
    int j = 0;
    for (int i = 3; i < size - 2; i++) {
      int max = 0;
      if (k[j].card() == i+firstValue)
        max = k[j++].max();
      if ((sum[i] - sum[i - 1]) != max)
        return true;
    }
    return false;
  }

  template<class Card>
  inline bool
  PartialSum<Card>::check_update_min(ViewArray<Card>& k) {
    int j = 0;
    for (int i = 3; i < size - 2; i++) {
      int min = 0;
      if (k[j].card() == i+firstValue)
        min = k[j++].min();
      if ((sum[i] - sum[i - 1]) != min)
        return true;
    }
    return false;
  }


  /**
   * \brief Container class provding information about the Hall
   *  structure of the problem variables.
   *
   *  This class is used to
   *  keep the number of different arrays small, that is
   *  an array of type %HallInfo replaces integer arrays for each
   *  of the class members.
   *
   * \ingroup GCCBndSup
   */
  class HallInfo {
  public:
    /// Represents the union of all lower and upper domain bounds
    int bounds;
    /**
     * \brief Critical capacity pointer
     * \a t represents a predecessor function where \f$ t_i \f$ denotes the
     * predecessor of i in bounds
     */
    int t;
    /**
     * \brief Difference between critical capacities
     *
     * \a d_i is the difference between the capacities of hall[i].bounds
     * and its predecessor in bounds hall[t[i]].bounds
     *
     */
    int d;
    /**
     * \brief Hall set pointer
     *
     * If hall[i].h < i then the half-open interval
     * [hall[h[i]].bounds,hall[i].bounds) is containd in a Hall
     * set.
     * Otherwise holds a pointer to the Hall intervall it belongs to.
     */
    int h;
    /// Stable Set pointer
    int s;
    /// Potentially Stable Set pointer
    int ps;
    /**
     * \brief Bound update
     *
     * \a newBound contains either a narrowed domain bound
     * or is stores the old domain bound of a variable.
     */
    int newBound;
  };


  /**
   * \name Path compression
   *
   * Each of the nodes on the path from \a start to \a end
   * becomes a direct child of \a to.
   * \ingroup GCCBndSup
   */
  //@{
  /// Path compression for potentially stable set structure
  forceinline void
  pathset_ps(HallInfo hall[], int start, int end, int to) {
    int k, l;
    for (l=start; (k=l) != end; hall[k].ps=to) {
      l = hall[k].ps;
    }
  }
  /// Path compression for stable set structure
  forceinline void
  pathset_s(HallInfo hall[], int start, int end, int to) {
    int k, l;
    for (l=start; (k=l) != end; hall[k].s=to) {
      l = hall[k].s;
    }
  }
  /// Path compression for capacity pointer structure
  forceinline void
  pathset_t(HallInfo hall[], int start, int end, int to) {
    int k, l;
    for (l=start; (k=l) != end; hall[k].t=to) {
      l = hall[k].t;
    }
  }
  /// Path compression for hall pointer structure
  forceinline void
  pathset_h(HallInfo hall[], int start, int end, int to) {
    int k, l;
    for (l=start; (k=l) != end; hall[k].h=to) {
      l = hall[k].h;
      assert(l != k);
    }
  }
  //@}

  /**
   *  \name Path minimum
   *
   *  Returns the smalles reachable index starting from \a i.
   * \ingroup GCCBndSup
   */
  //@{
  /// Path minimum for hall pointer structure
  forceinline int
  pathmin_h(const HallInfo hall[], int i) {
    while (hall[i].h < i)
      i = hall[i].h;
    return i;
  }
  /// Path minimum for capacity pointer structure
  forceinline int
  pathmin_t(const HallInfo hall[], int i) {
    while (hall[i].t < i)
      i = hall[i].t;
    return i;
  }
  //@}

  /**
   *  \name Path maximum
   *
   *  Returns the greatest reachable index starting from \a i.
   * \ingroup GCCBndSup
   */
  //@{
  /// Path maximum for hall pointer structure
  forceinline int
  pathmax_h(const HallInfo hall[], int i) {
    while (hall[i].h > i)
      i = hall[i].h;
    return i;
  }
  /// Path maximum for capacity pointer structure
  forceinline int
  pathmax_t(const HallInfo hall[], int i) {
    while (hall[i].t > i) {
      i = hall[i].t;
    }
    return i;
  }
  /// Path maximum for stable set pointer structure
  forceinline int
  pathmax_s(const HallInfo hall[], int i) {
    while (hall[i].s > i)
      i = hall[i].s;
    return i;
  }
  /// Path maximum for potentially stable set pointer structure
  forceinline int
  pathmax_ps(const HallInfo hall[], int i) {
    while (hall[i].ps > i)
      i = hall[i].ps;
    return i;
  }
  //@}

}}}

// STATISTICS: int-prop

