/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christopher Mears <chris.mears@monash.edu>
 *
 *  Copyright:
 *     Christopher Mears, 2012
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

#include <gecode/set/ldsb.hh>

namespace Gecode { namespace Set { namespace LDSB {
  // XXX: implementation of equalValues could be improved?

  /// Do two set variables have equal least-upper-bounds?
  bool
  equalLUB(const Set::SetView& x, const Set::SetView& y) {
    unsigned int n = x.lubSize();
    if (n != y.lubSize()) return false;
    for (unsigned int i = 0 ; i < n ; i++)
      if (x.lubMinN(i) != y.lubMinN(i))
        return false;
    return true;
  }
}}}

// Note carefully that this is in the Gecode::Int::LDSB namespace, not
// Gecode::Set::LDSB.
namespace Gecode { namespace Int { namespace LDSB {
  template <>
  ArgArray<Literal>
  VariableSymmetryImp<Set::SetView>
  ::symmetric(Literal l, const ViewArray<Set::SetView>& x) const {
    (void) x;
    if (indices.valid(l._variable) && indices.get(l._variable)) {
      int n = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(indices) ; i() ; ++i)
        n++;
      ArgArray<Literal> lits(n);
      int j = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(indices) ; i() ; ++i) {
        lits[j++] = Literal(i.val(), l._value);
      }
      return lits;
    } else {
      return ArgArray<Literal>(0);
    }
  }

  template <>
  ArgArray<Literal>
  ValueSymmetryImp<Set::SetView>
  ::symmetric(Literal l, const ViewArray<Set::SetView>& x) const {
    (void) x;
    if (values.valid(l._value) && values.get(l._value)) {
      int n = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(values) ; i() ; ++i)
        n++;
      ArgArray<Literal> lits(n);
      int j = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(values) ; i() ; ++i) {
        lits[j++] = Literal(l._variable, i.val());
      }
      return lits;
    } else {
      return ArgArray<Literal>(0);
    }
  }

  template <>
  ArgArray<Literal>
  VariableSequenceSymmetryImp<Set::SetView>
  ::symmetric(Literal l, const ViewArray<Set::SetView>& x) const {
    Support::DynamicStack<Literal, Heap> s(heap);
    if (l._variable < (int)lookup_size) {
      int posIt = lookup[l._variable];
      if (posIt == -1) {
        return dynamicStackToArgArray(s);
      }
      unsigned int seqNum = posIt / seq_size;
      unsigned int seqPos = posIt % seq_size;
      for (unsigned int seq = 0 ; seq < n_seqs ; seq++) {
        if (seq == seqNum) {
          continue;
        }
        if (x[getVal(seq, seqPos)].assigned()) {
          continue;
        }
        bool active = true;
        const unsigned int *firstSeq = &indices[seqNum*seq_size];
        const unsigned int *secondSeq = &indices[seq*seq_size];
        for (unsigned int i = 0 ; i < seq_size ; i++) {
          const Set::SetView& xv = x[firstSeq[i]];
          const Set::SetView& yv = x[secondSeq[i]];
          if ((!xv.assigned() && !yv.assigned())
              || (xv.assigned() && yv.assigned() && Set::LDSB::equalLUB(xv, yv))) {
            continue;
          } else {
            active = false;
            break;
          }
        }
        
        if (active) {
          s.push(Literal(secondSeq[seqPos], l._value));
        } 
      }
    }
    return dynamicStackToArgArray(s);
  }

  template <>
  ArgArray<Literal>
  ValueSequenceSymmetryImp<Set::SetView>
  ::symmetric(Literal l, const ViewArray<Set::SetView>& x) const {
    (void) x;
    Support::DynamicStack<Literal, Heap> s(heap);
    std::pair<int,int> location = findVar(values, n_values, seq_size, l._value);
    if (location.first == -1) return dynamicStackToArgArray(s);
    unsigned int seqNum = location.first;
    unsigned int seqPos = location.second;
    for (unsigned int seq = 0 ; seq < n_seqs ; seq++) {
      if (seq == seqNum) continue;
      if (dead_sequences.get(seq)) continue;
      s.push(Literal(l._variable, getVal(seq,seqPos)));
    }
    return dynamicStackToArgArray(s);
  }
}}}

// STATISTICS: set-branch
