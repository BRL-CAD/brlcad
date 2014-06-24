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
   * \brief Storage class for mininmum and maximum of a variable.
   */
  class Rank {
  public:
    /// stores the mininmum of a variable
    int min;
    /// stores the mininmum of a variable
    int max;
  };

  /**
   *  \brief Representation of a strongly connected component
   *
   *  Used with the implicit array representation of the
   *  bipartite oriented intersection graph.
   */
  class SccComponent {
  public:
    /// Leftmost y-node in a scc
    int leftmost;
    /// Direct left neighbour of an y-node in a scc
    int left;
    /// Direct right neighbour of an y-node in a scc
    int right;
    /// Rightmost reachable y-node in a scc
    int rightmost;
  };

  /**
   * \brief Subsumption test
   *
   * The propagator for sorted is subsumed if all
   * variables of the ViewArrays
   * \a x, \a y and \a z are determined and the constraint holds.
   * In addition to the subsumption test check_subsumption
   * determines, whether we can reduce the orginial problem
   * to a smaller one, by dropping already matched variables.
   */

  template<class View, bool Perm>
  inline bool
  check_subsumption(ViewArray<View>& x, ViewArray<View>& y, ViewArray<View>& z,
                    bool& subsumed, int& dropfst) {

    dropfst  = 0;
    subsumed = true;
    int  xs  = x.size();
    for (int i = 0; i < xs ; i++) {
      if (Perm) {
        subsumed &= (x[i].assigned() &&
                     z[i].assigned() &&
                     y[z[i].val()].assigned());
        if (subsumed) {
          if (x[i].val() != y[z[i].val()].val()) {
            return false;
          } else {
            if (z[i].val() == i) {
              dropfst++;
            }
          }
        }
      } else {
        subsumed &= (x[i].assigned() && y[i].assigned());
        if (subsumed) {
          if (x[i].val() != y[i].val()) {
            return false;
          } else {
            dropfst++;
          }
        }
      }
    }
    return true;
  }

  /**
   *   \brief Item used to construct the %OfflineMin sequence
   *
   */

  class OfflineMinItem {
  public:
    /// Root node representing the set the vertex belongs to
    int root;
    /// Predecessor in the tree representation of the set
    int parent;
    /// Ranking of the set given by its cardinality
    int rank;
    /// Name or label of a set
    int name;
    /**
     *  \brief Initial set label
     *
     *   This label represents the iteration index \f$i\f$
     *   and hence the index of an insert instruction
     *   in the complete Offline-Min sequence
     */
    int iset;
    /// Successor in the Offline-Min sequence
    int succ;
    /// Predecessor in the Offline-Min sequence
    int pred;
  };

  /**
   *  \brief Offline-Min datastructure
   *  Used to compute the perfect matching between the unsorted views
   *  \a x and the sorted views \a y.
   */
  class OfflineMin {
  private:
    OfflineMinItem* sequence;
    int* vertices;
    int  n;
  public:
    OfflineMin(void);
    OfflineMin(OfflineMinItem[], int[], int);
    /**
     *  Find the set x belongs to
     *  (wihtout path compression)
     */
    int  find(int x);
    /**
     *  Find the set x belongs to
     *  (using path compression)
     */
    int  find_pc(int x);
    /// Unite two sets \a a and \a b and label the union with \a c
    void unite(int a, int b, int c);
    /// Initialization of the datastructure
    void makeset(void);
    /// Return the size of the Offline-Min item
    int  size(void);
    OfflineMinItem& operator [](int);
  };

  OfflineMin::OfflineMin(void){
    n = 0;
    sequence = NULL;
    vertices = NULL;
  }

  OfflineMin::OfflineMin(OfflineMinItem s[], int v[], int size){
    n = size;
    sequence = &s[0];
    vertices = &v[0];
  }

  forceinline int
  OfflineMin::find(int x) {
    while (sequence[x].parent != x) {
      x = sequence[x].parent;
    }
    // x is now the root of the tree
    // return the set, x belongs to
    return sequence[x].name;
  }

  forceinline int
  OfflineMin::find_pc(int x){
    int vsize = 0;
    while (sequence[x].parent != x) {
      vertices[x] = x;
      x = sequence[x].parent;
    }
    // x is now the root of the tree
    for (int i = vsize; i--; ) {
      sequence[vertices[i]].parent = x;
    }
    // return the set, x belongs to
    return sequence[x].name;
  }

  forceinline void
  OfflineMin::unite(int a, int b, int c){
    // c is the union of a and b
    int ra = sequence[a].root;
    int rb = sequence[b].root;
    int large = rb;
    int small = ra;
    if (sequence[ra].rank > sequence[rb].rank) {
      large = ra;
      small = rb;
    }
    sequence[small].parent =  large;
    sequence[large].rank   += sequence[small].rank;
    sequence[large].name   =  c;
    sequence[c].root       =  large;
  }

  forceinline void
  OfflineMin::makeset(void){
    for(int i = n; i--; ){
      OfflineMinItem& cur = sequence[i];
      cur.rank   = 0;     // initially each set is empty
      cur.name   = i;     // it has its own name
      cur.root   = i;     // it is the root node
      cur.parent = i;     // it is its own parent
      cur.pred   = i - 1;
      cur.succ   = i + 1;
      cur.iset   = -5;
    }
  }

  forceinline int
  OfflineMin::size(void){
    return n;
  }

  forceinline OfflineMinItem&
  OfflineMin::operator [](int i){
    return sequence[i];
  }

  /**
   *  \brief Index comparison for %ViewArray&lt;Tuple&gt;
   *
   *  Checks whether for two indices \a i and \a j
   *  the first component of the corresponding viewtuples
   *  \f$x_i\f$ and \f$x_j\f$ the upper domain bound of \f$x_j\f$
   *  is larger than the upper domain bound of \f$x_i\f$
   *
   */
  template<class View>
  class TupleMaxInc {
  protected:
    ViewArray<View> x;
  public:
    TupleMaxInc(const ViewArray<View>& x0) : x(x0) {}
    bool operator ()(const int i, const int j) {
      if (x[i].max() == x[j].max()) {
        return x[i].min() < x[j].min();
      } else {
        return x[i].max() < x[j].max();
      }
    }
  };


  /**
   *  \brief Extended Index comparison for %ViewArray&lt;Tuple&gt;
   *
   *  Checks whether for two indices \a i and \a j
   *  the first component of the corresponding viewtuples
   *  \f$x_i\f$ and \f$x_j\f$ the upper domain bound of \f$x_j\f$
   *  is larger than the upper domain bound of \f$x_i\f$
   *
   */
  template<class View>
  class TupleMaxIncExt {
  protected:
    ViewArray<View> x;
    ViewArray<View> z;
  public:
    TupleMaxIncExt(const ViewArray<View>& x0,
                   const ViewArray<View>& z0) : x(x0), z(z0) {}
    bool operator ()(const int i, const int j) {
      if (x[i].max() == x[j].max()) {
        if (x[i].min() == x[j].min()) {
          if (z[i].max() == z[j].max()) {
            return z[i].min() < z[j].min();
          } else {
            return z[i].max() < z[j].max();
          }
        } else {
          return x[i].min() < x[j].min();
        }
      } else {
        return x[i].max() < x[j].max();
      }
    }
  };

  /**
   * \brief View comparison on ViewTuples
   *
   * Checks whether the lower domain bound of the
   * first component of \a x (the variable itself)
   * is smaller than the lower domain bound of the
   * first component of \a y
   */

  template<class View>
  class TupleMinInc {
  public:
    bool operator ()(const View& x, const View& y) {
      if (x.min() == y.min()) {
        return x.max() < y.max();
      } else {
        return x.min() < y.min();
      }
    }
  };


  /// Pair of views
  template<class View>
  class ViewPair {
  public:
    View x;
    View z;
  };

  /**
   * \brief Extended view comparison on pairs of views
   *
   * Checks whether the lower domain bound of the
   * first component of \a x (the variable itself)
   * is smaller than the lower domain bound of the
   * first component of \a y. If they are equal
   * it checks their upper bounds. If they are also
   * equal we sort the views by the permutation variables.
   */
  template<class View>
  class TupleMinIncExt {
  public:
    bool operator ()(const ViewPair<View>& x, const ViewPair<View>& y) {
      if (x.x.min() == y.x.min()) {
        if (x.x.max() == y.x.max()) {
          if (x.z.min() == y.z.min()) {
            return x.z.max() < y.z.max();
          } else {
            return x.z.min() < y.z.min();
          }
        } else {
          return x.x.max() < y.x.max();
        }
      } else {
        return x.x.min() < y.x.min();
      }
    }
  };

  /**
   * \brief Check for assignment of a variable array
   *
   * Check whether one of the argument arrays is completely assigned
   * and udpates the other array respectively.
   */

  template<class View, bool Perm>
  inline bool
  array_assigned(Space& home,
                 ViewArray<View>& x,
                 ViewArray<View>& y,
                 ViewArray<View>& z,
                 bool& subsumed,
                 bool& match_fixed,
                 bool&,
                 bool& noperm_bc) {

    bool x_complete = true;
    bool y_complete = true;
    bool z_complete = true;

    for (int i = y.size(); i--; ) {
      x_complete &= x[i].assigned();
      y_complete &= y[i].assigned();
      if (Perm) {
        z_complete &= z[i].assigned();
      }
    }

    if (x_complete) {
      for (int i = x.size(); i--; ) {
        ModEvent me = y[i].eq(home, x[i].val());
        if (me_failed(me)) {
          return false;
        }
      }
      if (Perm) {
        subsumed = false;
      } else {
        subsumed = true;
      }
    }

    if (y_complete) {
      bool y_equality = true;
      for (int i = y.size() - 1; i--; ) {
        y_equality &= (y[i].val() == y[i + 1].val());
      }
      if (y_equality) {
        for (int i = x.size(); i--; ) {
          ModEvent me = x[i].eq(home, y[i].val());
          if (me_failed(me)) {
            return false;
          }
        }
        if (Perm) {
          subsumed = false;
        } else {
          subsumed = true;
        }
        noperm_bc = true;
      }
    }

    if (Perm) {
      if (z_complete) {
        if (x_complete) {
          for (int i = x.size(); i--; ) {
            ModEvent me = y[z[i].val()].eq(home, x[i].val());
            if (me_failed(me)) {
              return false;
            }
          }
          subsumed = true;
          return subsumed;
        }
        if (y_complete) {
          for (int i = x.size(); i--; ) {
            ModEvent me = x[i].eq(home, y[z[i].val()].val());
            if (me_failed(me)) {
              return false;
            }
          }
          subsumed = true;
          return subsumed;
        }

        // validate the permutation
        int sum = 0;
        for (int i = x.size(); i--; ) {
          int pi = z[i].val();
          if (x[i].max() < y[pi].min() ||
              x[i].min() > y[pi].max()) {
            return false;
          }
          sum += pi;
        }
        int n = x.size();
        int gauss = ( (n * (n + 1)) / 2);
        // if the sum over all assigned permutation variables is not
        // equal to the gaussian sum - n they are not distinct, hence invalid
        if (sum != gauss - n) {
          return false;
        }
        match_fixed = true;
      }
    }
    return true;
  }

  /**
   * \brief Channel between \a x, \a y and \a z
   *
   * Keep variables consisting by channeling information
   *
   */

  template<class View>
  forceinline bool
  channel(Space& home, ViewArray<View>& x, ViewArray<View>& y,
          ViewArray<View>& z, bool& nofix) {
    int n = x.size();
    for (int i = n; i--; ) {
      if (z[i].assigned()) {
        int v = z[i].val();
        if (x[i].assigned()) {
          // channel equality from x to y
          ModEvent me = y[v].eq(home, x[i].val());
          if (me_failed(me))
            return false;
          nofix |= me_modified(me);
        } else {
          if (y[v].assigned()) {
            // channel equality from y to x
            ModEvent me = x[i].eq(home, y[v].val());
            if (me_failed(me))
              return false;
            nofix |= me_modified(me);
          } else {
            // constrain upper bound
            ModEvent me = x[i].lq(home, y[v].max());
            if (me_failed(me))
              return false;
            nofix |= me_modified(me);

            // constrain lower bound
            me = x[i].gq(home, y[v].min());
            if (me_failed(me))
              return false;
            nofix |= me_modified(me);

            // constrain the sorted variable
            // constrain upper bound
            me = y[v].lq(home, x[i].max());
            if (me_failed(me))
              return false;
            nofix |= me_modified(me);

            // constrain lower bound
            me = y[v].gq(home, x[i].min());
            if (me_failed(me))
              return false;
            nofix |= me_modified(me);
          }
        }
      } else {
        // if the permutation variable is undetermined
        int l = z[i].min();
        int r = z[i].max();
        // upper bound
        ModEvent me = x[i].lq(home, y[r].max());
        if (me_failed(me))
          return false;
        nofix |= me_modified(me);

        // lower bound
        me = x[i].gq(home, y[l].min());
        if (me_failed(me))
          return false;
        nofix |= me_modified(me);
      }
    }
    return true;
  }


}}}


// STATISTICS: int-prop
