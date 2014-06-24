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
   * \brief Glover's maximum matching in a bipartite graph
   *
   * Compute a matching in the bipartite convex intersection graph with
   * one partition containing the x views and the other containing
   * the y views. The algorithm works with an implicit array structure
   * of the intersection graph.
   *
   * Union-Find Implementation of F.Glover's matching algorithm.
   *
   * The idea is to mimick a priority queue storing x-indices
   * \f$[i_0,\dots, i_{n-1}]\f$, s.t. the upper domain bounds are sorted
   * \f$D_{i_0} \leq\dots\leq D_{i_{n-1}}\f$ where \f$ D_{i_0}\f$
   * is the top element
   *
   */

  template<class View>
  inline bool
  glover(ViewArray<View>& x, ViewArray<View>& y,
         int tau[], int phi[], OfflineMinItem sequence[], int vertices[]) {

    int xs = x.size();
    OfflineMin seq(sequence, vertices, xs);
    int s  = 0;
    seq.makeset();

    for (int z = 0; z < xs; z++) {  // forall y nodes
      int maxy = y[z].max();
      // creating the sequence of inserts and extractions from the queue
      for( ; s <xs && x[s].min() <= maxy; s++) {
        seq[s].iset = z;
        seq[z].rank++;
      }
    }

    // offline-min-procedure
    for (int i = 0; i < xs; i++) {
      // the upper bound of the x-node should be minimal
      int perm = tau[i];
      // find the iteration where \tau(i) became a maching candidate
      int iter = seq[perm].iset;
      if (iter<0)
        return false;
      int j = 0;
      j = seq.find_pc(iter);
      // check whether the sequence is valid
      if (j >= xs)
        return false;
      // if there is no intersection between the matching candidate
      // and the y node then there exists NO perfect matching
      if (x[perm].max() < y[j].min())
        return false;
      phi[j]         = perm;
      seq[perm].iset = -5;           //remove from candidate set
      int sqjsucc    = seq[j].succ;
      if (sqjsucc < xs) {
        seq.unite(j,sqjsucc,sqjsucc);
      } else {
        seq[seq[j].root].name = sqjsucc; // end of sequence achieved
      }

      // adjust tree list
      int pr = seq[j].pred;
      if (pr != -1)
        seq[pr].succ = sqjsucc;
      if (sqjsucc != xs)
        seq[sqjsucc].pred = pr;
    }
    return true;
  }

  /**
   * \brief Symmetric glover function for the upper domain bounds
   *
   */
  template<class View>
  inline bool
  revglover(ViewArray<View>& x, ViewArray<View>& y,
            int tau[], int phiprime[], OfflineMinItem sequence[],
            int vertices[]) {

    int xs = x.size();
    OfflineMin seq(sequence, vertices, xs);
    int s  = xs - 1;
    seq.makeset();

    int miny = 0;
    for (int z = xs; z--; ) {     // forall y nodes
      miny = y[z].min();
      // creating the sequence of inserts and extractions from the queue
      for ( ; s > -1 && x[tau[s]].max() >= miny; s--) {
        seq[tau[s]].iset = z;
        seq[z].rank++;
      }
    }

    // offline-min-procedure
    for (int i = xs; i--; ) {
      int perm = i;
      int iter = seq[perm].iset;
      if (iter < 0)
        return false;
      int j = 0;
      j = seq.find_pc(iter);
      if (j <= -1)
        return false;
      // if there is no intersection between the matching candidate
      // and the y node then there exists NO perfect matching
      if (x[perm].min() > y[j].max())
        return false;
      phiprime[j]    = perm;
      seq[perm].iset = -5;
      int sqjsucc    = seq[j].pred;
      if (sqjsucc >= 0) {
        seq.unite(j, sqjsucc, sqjsucc);
      } else {
        seq[seq[j].root].name = sqjsucc; // end of sequence achieved
      }

      // adjust tree list
      int pr = seq[j].succ;
      if (pr != xs)
        seq[pr].pred = sqjsucc;
      if (sqjsucc != -1)
        seq[sqjsucc].succ = pr;
    }
    return true;
  }

}}}

// STATISTICS: int-prop

