/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christopher Mears <chris.mears@monash.edu>
 *
 *  Contributing authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Mikael Lagerkvist, 2007
 *     Christopher Mears, 2012
 *     Christian Schulte, 2007
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

#include <climits>
#include <cmath>
#include <iostream>

namespace Gecode { namespace Support {

  /// Bitsets with index offset.
  ///
  /// The valid range of indices for a \a BitSetOffset with \a s bits
  /// and an offset of \a o is [o, o+1, ..., o+s-1].
  template<class A>
  class BitSetOffset : public BitSetBase {
  protected:
    /// Allocator
    A& a;
    /// Offset
    int _offset;
  public:
    /// Bit set with space for \a s bits with offset of \o
    BitSetOffset(A& a, unsigned int s, int o);
    /// Copy bit set \a bs
    BitSetOffset(A& a, const BitSetOffset& bs);
    /// Destructor
    ~BitSetOffset(void);

    // As for the ordinary bitset, most operations can be inherited
    // directly from BitSetBase.  We only modify the operations that
    // involve indices or the offset itself.

    /// Access value at bit \a i
    bool get(int i) const;
    /// Set bit \a i
    void set(int i);
    /// Clear bit \a i
    void clear(int i);
    /// Return position greater or equal \a i of next set bit (\a i is allowed to be equal to size)
    int next(int i) const;
    /// Resize bitset to \a n elements with specified offset.
    void resize(A& a, unsigned int n, int offset, bool set=false);

    /// Retrieve the minimum valid index (the offset).
    int offset(void) const;
    /// Retrieve the maximum valid index.
    int max_bit(void) const;
    /// Is the bit index valid for this bitset?
    bool valid(int i) const;
  };

  template<class A>
  forceinline
  BitSetOffset<A>::BitSetOffset(A& a0, unsigned int s, int o)
    : BitSetBase(a0,s), a(a0), _offset(o) {}

  template<class A>
  forceinline
  BitSetOffset<A>::BitSetOffset(A& a0, const BitSetOffset<A>& bs)
    : BitSetBase(a0,bs), a(a0), _offset(bs._offset) {}

  template<class A>
  forceinline
  BitSetOffset<A>::~BitSetOffset(void) {
    dispose(a);
  }

  template<class A>
  forceinline bool
  BitSetOffset<A>::get(int i) const { return BitSetBase::get(i-_offset); }

  template<class A>
  forceinline void
  BitSetOffset<A>::set(int i) { BitSetBase::set(i-_offset); }

  template<class A>
  forceinline void
  BitSetOffset<A>::clear(int i) { BitSetBase::clear(i-_offset); }

  template<class A>
  forceinline int
  BitSetOffset<A>::next(int i) const { return _offset + BitSetBase::next(i-_offset); }

  template<class A>
  void
  BitSetOffset<A>::resize(A& a, unsigned int n, int offset, bool set) {
    BitSetBase::resize(a, n, set);
    _offset = offset;
  }

  template<class A>
  forceinline int
  BitSetOffset<A>::offset(void) const { return _offset; }

  template<class A>
  forceinline int
  BitSetOffset<A>::max_bit(void) const { return _offset + size() - 1; }

  template<class A>
  forceinline bool
  BitSetOffset<A>::valid(int i) const { return _offset <= i && i <= _offset + (int)size() - 1; }

  template <class A, class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const BitSetOffset<A>& bs) {
    for (int i = bs.offset() ; i < bs.offset()+static_cast<int>(bs.size()) ; i++)
      if (bs.get(i))
        os << i << " ";
    return os;
  }

}}

// STATISTICS: support-any

