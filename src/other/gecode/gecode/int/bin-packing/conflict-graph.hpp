/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Stefano Gualandi <stefano.gualandi@gmail.com>
 *
 *  Copyright:
 *     Stefano Gualandi, 2013
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

#include <gecode/int/rel.hh>
#include <gecode/int/distinct.hh>

namespace Gecode { namespace Int { namespace BinPacking {

  forceinline int
  ConflictGraph::nodes(void) const {
    return b.size();
  }

  forceinline
  ConflictGraph::NodeSet::NodeSet(void) {}
  forceinline
  ConflictGraph::NodeSet::NodeSet(Region& r, int n)
    : Support::RawBitSetBase(r,static_cast<unsigned int>(n)) {}
  forceinline
  ConflictGraph::NodeSet::NodeSet(Region& r, int n, 
                                  const NodeSet& ns)
    : Support::RawBitSetBase(r,static_cast<unsigned int>(n),ns) {}
  forceinline void
  ConflictGraph::NodeSet::allocate(Region& r, int n) {
    Support::RawBitSetBase::allocate(r,static_cast<unsigned int>(n));
  }
  forceinline void
  ConflictGraph::NodeSet::init(Region& r, int n) {
    Support::RawBitSetBase::init(r,static_cast<unsigned int>(n));
  }
  forceinline bool
  ConflictGraph::NodeSet::in(int i) const {
    return RawBitSetBase::get(static_cast<unsigned int>(i));
  }
  forceinline void
  ConflictGraph::NodeSet::incl(int i) {
    RawBitSetBase::set(static_cast<unsigned int>(i));
  }
  forceinline void
  ConflictGraph::NodeSet::excl(int i) {
    RawBitSetBase::clear(static_cast<unsigned int>(i));
  }
  forceinline void
  ConflictGraph::NodeSet::copy(int n, const NodeSet& ns) {
    RawBitSetBase::copy(static_cast<unsigned int>(n),ns);
  }
  forceinline void
  ConflictGraph::NodeSet::empty(int n) {
    RawBitSetBase::clearall(static_cast<unsigned int>(n));
  }
  forceinline bool
  ConflictGraph::NodeSet::iwn(NodeSet& cwa, const NodeSet& a, 
                              NodeSet& cwb, const NodeSet& b,
                              const NodeSet& c, int _n) {
    unsigned int n = static_cast<unsigned int>(_n);
    // This excludes the sentinel bit
    unsigned int pos = n / bpb;
    unsigned int bits = n % bpb;

    // Whether any bit is set
    Support::BitSetData abc; abc.init();
    for (unsigned int i=0; i<pos; i++) {
      cwa.data[i] = Support::BitSetData::a(a.data[i],c.data[i]);
      cwb.data[i] = Support::BitSetData::a(b.data[i],c.data[i]);
      abc.o(cwa.data[i]); 
      abc.o(cwb.data[i]);
    }
    // Note that the sentinel bit is copied as well
    cwa.data[pos] = Support::BitSetData::a(a.data[pos],c.data[pos]);
    cwb.data[pos] = Support::BitSetData::a(b.data[pos],c.data[pos]);
    abc.o(cwa.data[pos],bits);
    abc.o(cwb.data[pos],bits);

    assert(cwa.get(n) && cwb.get(n));
    return abc.none();
  }


  forceinline
  ConflictGraph::Node::Node(void) {}

  forceinline
  ConflictGraph::Nodes::Nodes(const NodeSet& ns0)
    : ns(ns0), c(ns.next(0)) {}
  forceinline void
  ConflictGraph::Nodes::operator ++(void) {
    c = ns.next(c+1);
  }
  forceinline int
  ConflictGraph::Nodes::operator ()(void) const {
    return static_cast<int>(c);
  }

  forceinline
  ConflictGraph::Clique::Clique(Region& r, int m)
    : n(r,m), c(0), w(0) {}
  forceinline void
  ConflictGraph::Clique::incl(int i, unsigned int wi) {
    n.incl(i); c++; w += wi;
  }
  forceinline void
  ConflictGraph::Clique::excl(int i, unsigned int wi) {
    n.excl(i); c--; w -= wi;
  }

  forceinline
  ConflictGraph::ConflictGraph(Space& h, Region& reg,
                               const IntVarArgs& b0, int m)
    : home(h), b(b0), 
      bins(static_cast<unsigned int>(m)),
      node(reg.alloc<Node>(nodes())),
      cur(reg,nodes()), max(reg,nodes()) {
    for (int i=nodes(); i--; ) {
      node[i].n.init(reg,nodes());
      node[i].d=node[i].w=0;
    }
  }

  forceinline
  ConflictGraph::~ConflictGraph(void) {
  }

  forceinline void
  ConflictGraph::edge(int i, int j, bool add) {
    if (add) {
      assert(!node[i].n.in(j) && !node[j].n.in(i));
      node[i].n.incl(j); node[i].d++; node[i].w++;
      node[j].n.incl(i); node[j].d++; node[j].w++;
    } else {
      assert(node[i].n.in(j) && node[j].n.in(i));
      node[i].n.excl(j); node[i].d--;
      node[j].n.excl(i); node[j].d--;
    }
  }

  forceinline bool
  ConflictGraph::adjacent(int i, int j) const {
    assert(node[i].n.in(j) == node[j].n.in(i));
    return node[i].n.in(j);
  }

  forceinline int
  ConflictGraph::pivot(const NodeSet& a, const NodeSet& b) const {
    Nodes i(a), j(b);
    assert((i() < nodes()) || (j() < nodes()));
    int p;
    if (i() < nodes()) {
      p = i(); ++i;
    } else {
      p = j(); ++j;
    }
    unsigned int m = node[p].d;
    while (i() < nodes()) {
      if (node[i()].d > m) {
        p=i(); m=node[p].d; 
      }
      ++i;
    }
    while (j() < nodes()) {
      if (node[j()].d > m) {
        p=j(); m=node[p].d; 
      }
      ++j;
    }
    return p;
  }

  forceinline ExecStatus
  ConflictGraph::clique(void) {
    // Remember clique
    if ((cur.c > max.c) || ((cur.c == max.c) && (cur.w > max.w))) {
      max.n.copy(nodes(),cur.n); max.c=cur.c; max.w=cur.w;
      if (max.c > bins)
        return ES_FAILED;
    }
    // Compute corresponding variables
    ViewArray<IntView> bv(home,cur.c);
    int i=0;
    for (Nodes c(cur.n); c() < nodes(); ++c)
      bv[i++] = b[c()];
    assert(i == static_cast<int>(cur.c));
    return Distinct::Dom<IntView>::post(home,bv);
  }

  forceinline ExecStatus
  ConflictGraph::clique(int i) {
    if (1 > max.c) {
      assert(max.n.none(nodes()));
      max.n.incl(i); max.c=1; max.w=0;
    }
    return ES_OK;
  }

  forceinline ExecStatus
  ConflictGraph::clique(int i, int j) {
    unsigned int w = node[i].w + node[j].w;
    if ((2 > max.c) || ((2 == max.c) && (cur.w > max.w))) {
      max.n.empty(nodes()); 
      max.n.incl(i); max.n.incl(j); 
      max.c=2; max.w=w;
      if (max.c > bins)
        return ES_FAILED;
    }
    return Rel::Nq<IntView>::post(home,b[i],b[j]);
  }

  forceinline ExecStatus
  ConflictGraph::clique(int i, int j, int k) {
    unsigned int w = node[i].w + node[j].w + node[k].w;
    if ((3 > max.c) || ((3 == max.c) && (cur.w > max.w))) {
      max.n.empty(nodes()); 
      max.n.incl(i); max.n.incl(j); max.n.incl(k); 
      max.c=3; max.w=w;
      if (max.c > bins)
        return ES_FAILED;
    }
    // Compute corresponding variables
    ViewArray<IntView> bv(home,3);
    bv[0]=b[i]; bv[1]=b[j]; bv[2]=b[k];
    return Distinct::Dom<IntView>::post(home,bv);
  }

  forceinline ExecStatus
  ConflictGraph::post(void) {
    // Find some simple cliques of sizes 2 and 3.
    {
      Region reg(home);
      // Nodes can be entered twice (for degree 2 and later for degree 1)
      Support::StaticStack<int,Region> n(reg,2*nodes());
      // Make a copy of the degree information to be used as weights
      // and record all nodes of degree 1 and 2.
      for (int i=nodes(); i--; ) {
        if ((node[i].d == 1) || (node[i].d == 2))
          n.push(i);
      }
      while (!n.empty()) {
        int i = n.pop();
        switch (node[i].d) {
        case 0:
          // Might happen as the edges have already been removed
          break;
        case 1: {
          Nodes ii(node[i].n);
          int j=ii();
          GECODE_ES_CHECK(clique(i,j));
          // Remove edge
          edge(i,j,false);
          if ((node[j].d == 1) || (node[j].d == 2))
            n.push(j);
          break;
        }
        case 2: {
          Nodes ii(node[i].n);
          int j=ii(); ++ii;
          int k=ii();
          if (adjacent(j,k)) {
            GECODE_ES_CHECK(clique(i,j,k));
            // Can the edge j-k still be member of another clique?
            if ((node[j].d == 2) || (node[k].d == 2))
              edge(j,k,false);
          } else {
            GECODE_ES_CHECK(clique(i,j));
            GECODE_ES_CHECK(clique(i,k));
          }
          edge(i,j,false);
          edge(i,k,false);
          if ((node[j].d == 1) || (node[j].d == 2))
            n.push(j);
          if ((node[k].d == 1) || (node[k].d == 2))
            n.push(k);
          break;
        }
        default: GECODE_NEVER;
        }
      }
    }
    // Initialize for Bron-Kerbosch
    {
      Region reg(home);
      NodeSet p(reg,nodes()), x(reg,nodes());
      bool empty = true;
      for (int i=nodes(); i--; )
        if (node[i].d > 0) {
          p.incl(i); empty = false;
        } else {
          // Report clique of size 1
          GECODE_ES_CHECK(clique(i));
        }
      if (empty)
        return ES_OK;
      GECODE_ES_CHECK(bk(p,x));
    }
    return ES_OK;
  }

  forceinline IntSet
  ConflictGraph::maxclique(void) const {
    Region reg(home);
    int* n=reg.alloc<int>(max.c);
    int j=0;
    for (Nodes i(max.n); i() < nodes(); ++i)
      n[j++]=i();
    assert(j == static_cast<int>(max.c));
    IntSet s(n,max.c);
    return s;
  }

}}}

// STATISTICS: int-prop

