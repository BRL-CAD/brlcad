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

namespace Gecode { namespace Int { namespace LDSB {

  /// Convert a \a DynamicStack<T,A> into an \a ArgArray<T>
  template <class T, class A>
  ArgArray<T>
  dynamicStackToArgArray(const Support::DynamicStack<T,A>& s) {
    ArgArray<T> a(s.entries());
    for (int i = 0 ; i < s.entries() ; ++i) {
      a[i] = s[i];
    }
    return a;
  }

  template<class View>
  void*
  SymmetryImp<View>::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

  template<class View>
  void
  SymmetryImp<View>::operator delete(void*,Space&) {}

  template<class View>
  void
  SymmetryImp<View>::operator delete(void*) {}

  template <class View>
  VariableSymmetryImp<View>
  ::VariableSymmetryImp(Space& home, int* _indices, unsigned int n)
    : indices(home, 0, 0) {
    // Find minimum and maximum value in _indices: the minimum is the
    // offset, and the maximum dictates how large the bitset needs to
    // be.
    int maximum = _indices[0];
    int minimum = _indices[0];
    for (unsigned int i = 1 ; i < n ; i++) {
      if (_indices[i] > maximum) maximum = _indices[i];
      if (_indices[i] < minimum) minimum = _indices[i];
    }
    indices.resize(home, maximum-minimum+1, minimum);

    // Set the bits for the included indices.
    for (unsigned int i = 0 ; i < n ; i++) {
      indices.set(_indices[i]);
    }
  }

  

  template <class View>
  inline
  VariableSymmetryImp<View>
  ::VariableSymmetryImp(Space& home, const VariableSymmetryImp& other) :
    indices(home, other.indices) {}

  template <class View>
  size_t
  VariableSymmetryImp<View>
  ::dispose(Space& home) {
    indices.dispose(home);
    return sizeof(*this);
  }

  template <class View>
  void
  VariableSymmetryImp<View>
  ::update(Literal l) {
    if (indices.valid(l._variable)) {
      indices.clear(l._variable);
    }
  }

  template <class View>
  SymmetryImp<View>*
  VariableSymmetryImp<View>
  ::copy(Space& home, bool share) const {
    (void) share;
    return new (home) VariableSymmetryImp<View>(home, *this);
  }



  // The minimum value in vs is the bitset's offset, and the maximum
  // dictates how large the bitset needs to be.
  template <class View>
  ValueSymmetryImp<View>
  ::ValueSymmetryImp(Space& home, int* vs, unsigned int n)
    : values(home, 0, 0) {
    // Find minimum and maximum value in vs: the minimum is the
    // offset, and the maximum dictates how large the bitset needs to
    // be.
    assert(n > 0);
    int maximum = vs[0];
    int minimum = vs[0];
    for (unsigned int i = 1 ; i < n ; i++) {
      if (vs[i] > maximum) maximum = vs[i];
      if (vs[i] < minimum) minimum = vs[i];
    }
    values.resize(home, maximum-minimum+1, minimum);

    // Set the bits for the included values.
    for (unsigned int i = 0 ; i < n ; i++) {
      values.set(vs[i]);
    }
  }

  template <class View>
  ValueSymmetryImp<View>
  ::ValueSymmetryImp(Space& home, const ValueSymmetryImp<View>& other)
    : values(home, other.values) { }

  template <class View>
  size_t
  ValueSymmetryImp<View>
  ::dispose(Space& home) {
    values.dispose(home);
    return sizeof(*this);
  }

  template <class View>
  void
  ValueSymmetryImp<View>
  ::update(Literal l) {
    if (values.valid(l._value))
      values.clear(l._value);
  }

  template <class View>
  SymmetryImp<View>*
  ValueSymmetryImp<View>
  ::copy(Space& home, bool share) const {
    (void) share;
    return new (home) ValueSymmetryImp(home, *this);
  }



  template <class View>
  int
  VariableSequenceSymmetryImp<View>
  ::getVal(unsigned int sequence, unsigned int position) const {
    return indices[sequence*seq_size + position];
  }

  template <class View>
  VariableSequenceSymmetryImp<View>
  ::VariableSequenceSymmetryImp(Space& home, int* _indices, unsigned int n, 
                                unsigned int seqsize)
    : n_indices(n), seq_size(seqsize), n_seqs(n/seqsize) {
    indices = home.alloc<unsigned int>(n_indices);
    unsigned int max_index = _indices[0];
    for (unsigned int i = 0 ; i < n_indices ; i++) {
      indices[i] = _indices[i];
      if (indices[i] > max_index)
        max_index = indices[i];
    }

    lookup_size = max_index+1;
    lookup = home.alloc<int>(lookup_size);
    for (unsigned int i = 0 ; i < lookup_size ; i++)
      lookup[i] = -1;
    for (unsigned int i = 0 ; i < n_indices ; i++) {
      if (lookup[indices[i]] == -1)
        lookup[indices[i]] = i;
    }
  }

  template <class View>
  VariableSequenceSymmetryImp<View>
  ::VariableSequenceSymmetryImp(Space& home, bool share,
                                const VariableSequenceSymmetryImp& s)
    : n_indices(s.n_indices), seq_size(s.seq_size), n_seqs(s.n_seqs),
      lookup_size(s.lookup_size) {
    (void) share;
    indices = home.alloc<unsigned int>(n_indices);
    memcpy(indices, s.indices, n_indices * sizeof(int));
    lookup = home.alloc<int>(lookup_size);
    memcpy(lookup, s.lookup, lookup_size * sizeof(int));
  }

  template <class View>
  size_t
  VariableSequenceSymmetryImp<View>
  ::dispose(Space& home) {
    home.free<unsigned int>(indices, n_indices);
    home.free<int>(lookup, lookup_size);
    return sizeof(*this);
  }

  /// Compute symmetric literals
  template <class View>
  ArgArray<Literal>
  VariableSequenceSymmetryImp<View>
  ::symmetric(Literal l, const ViewArray<View>& x) const {
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
          const View& xv = x[firstSeq[i]];
          const View& yv = x[secondSeq[i]];
          if ((!xv.assigned() && !yv.assigned())
              || (xv.assigned() && yv.assigned() && xv.val() == yv.val())) {
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


  template <class View>
  void
  VariableSequenceSymmetryImp<View>
  ::update(Literal l) {
    // Do nothing.
    (void) l;
  }

  template <class View>
  SymmetryImp<View>*
  VariableSequenceSymmetryImp<View>
  ::copy(Space& home, bool share) const {
    return new (home) VariableSequenceSymmetryImp<View>(home, share, *this);
  }
  


  template <class View>
  int
  ValueSequenceSymmetryImp<View>
  ::getVal(unsigned int sequence, unsigned int position) const {
    return values[sequence*seq_size + position];
  }

  template <class View>
  ValueSequenceSymmetryImp<View>
  ::ValueSequenceSymmetryImp(Space& home, int* _values, unsigned int n, 
                             unsigned int seqsize)
    : n_values(n), seq_size(seqsize), n_seqs(n/seqsize),
      dead_sequences(home, n_seqs) {
    values = home.alloc<int>(n_values);
    for (unsigned int i = 0 ; i < n_values ; i++)
      values[i] = _values[i];
  }

  template <class View>
  ValueSequenceSymmetryImp<View>
  ::ValueSequenceSymmetryImp(Space& home,
                             const ValueSequenceSymmetryImp<View>& vss)
    : n_values(vss.n_values),
      seq_size(vss.seq_size),
      n_seqs(vss.n_seqs),
      dead_sequences(home, vss.dead_sequences) {
    values = home.alloc<int>(n_values);
    for (unsigned int i = 0 ; i < n_values ; i++)
      values[i] = vss.values[i];
  }

  template <class View>
  size_t
  ValueSequenceSymmetryImp<View>
  ::dispose(Space& home) {
    home.free(values, n_values);
    return sizeof(*this);
  }

  template <class View>
  void
  ValueSequenceSymmetryImp<View>
  ::update(Literal l) {
    unsigned int seq = 0;
    unsigned int pos = 0;
    for (unsigned int i = 0 ; i < n_values ; i++) {
      if (values[i] == l._value) {
        dead_sequences.set(seq);
        // TODO: This can be slightly optimised.
        while (pos < seq_size) {
          i++;
          pos++;
        }
      }
      pos++;
      if (pos == seq_size) {
        pos = 0;
        seq++;
      }
    }
  }

  template <class View>
  SymmetryImp<View>*
  ValueSequenceSymmetryImp<View>
  ::copy(Space& home, bool share) const {
    (void) share;
    return new (home) ValueSequenceSymmetryImp<View>(home, *this);
  }

}}}

// STATISTICS: int-branch
