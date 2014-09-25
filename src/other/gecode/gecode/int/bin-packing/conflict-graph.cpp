/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2014
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

namespace Gecode { namespace Int { namespace BinPacking {

  ExecStatus
  ConflictGraph::bk(NodeSet& p, NodeSet& x) {
    assert(!(p.none(nodes()) && x.none(nodes())));
    // Iterate over neighbors of pivot node
    Nodes n(node[pivot(p,x)].n);
    // Iterate over elements of p 
    Nodes i(p);
    // The loop iterates over elements in i - n
    while (i() < nodes()) {
      int iv = i(), nv = n();
      if ((n() < nodes()) && (iv == nv)) {
        ++i; ++n;
      } else if ((n() < nodes()) && (iv > nv)) {
        ++n;
      } else {
        ++i; ++n;

        Region reg(home);

        // Found i.val() to be in i - n
       
        NodeSet np, nx;
        np.allocate(reg,nodes());
        nx.allocate(reg,nodes());

        bool empty = NodeSet::iwn(np,p,nx,x,node[iv].n,nodes());

        p.excl(iv); x.incl(iv);

        // Update current clique
        cur.incl(iv,node[iv].w);

        if (empty) {
          // Found a max clique
          GECODE_ES_CHECK(clique());
        } else {
          GECODE_ES_CHECK(bk(np,nx));
        }

        // Reset current clique
        cur.excl(iv,node[iv].w);
      }
    }
    return ES_OK;
  }
  
}}}

// STATISTICS: int-prop

