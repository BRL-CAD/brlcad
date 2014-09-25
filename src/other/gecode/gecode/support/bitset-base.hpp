/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Mikael Lagerkvist, 2007
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

#ifdef _MSC_VER

#include <intrin.h>

#if defined(_M_IX86)
#pragma intrinsic(_BitScanForward)
#define GECODE_SUPPORT_MSVC_32
#endif

#if defined(_M_X64) || defined(_M_IA64)
#pragma intrinsic(_BitScanForward64)
#define GECODE_SUPPORT_MSVC_64
#endif

#endif

namespace Gecode { namespace Support {

  class RawBitSetBase;

  /// Date item for bitsets
  class BitSetData {
    friend class RawBitSetBase;
  protected:
#ifdef GECODE_SUPPORT_MSVC_64
    /// Basetype for bits
    typedef unsigned __int64 Base;
#else
    /// Basetype for bits
    typedef unsigned long int Base;
#endif
    /// The bits
    Base bits;
    /// Bits per base
    static const unsigned int bpb = 
      static_cast<unsigned int>(CHAR_BIT * sizeof(Base));
  public:
    /// Initialize with all bits set if \a setbits
    void init(bool setbits=false);
    /// Get number of data elements for \a s bits
    static unsigned int data(unsigned int s);
    /// Test wether any bit with position greater or equal to \a i is set
    bool operator ()(unsigned int i=0U) const;
    /// Access value at bit \a i
    bool get(unsigned int i) const;
    /// Set bit \a i
    void set(unsigned int i);
    /// Clear bit \a i
    void clear(unsigned int i);
    /// Return next set bit with position greater or equal to \a i (there must be a bit)
    unsigned int next(unsigned int i=0U) const;
    /// Whether all bits are set
    bool all(void) const;
    /// Whether all bits from bit 0 to bit \a i are set
    bool all(unsigned int i) const;
    /// Whether no bits are set
    bool none(void) const;
    /// Whether no bits from bit 0 to bit \a i are set
    bool none(unsigned int i) const;
    /// Perform "and" with \a a
    void a(BitSetData a);
    /// Perform "and" with \a a for bits 0 to \a i
    void a(BitSetData a, unsigned int i);
    /// Return "and" of \a a and \a b
    static BitSetData a(BitSetData a, BitSetData b);
    /// Perform "or" with \a a
    void o(BitSetData a);
    /// Perform "or" with \a a for bits 0 to \a i
    void o(BitSetData a, unsigned int i);
    /// Return "or" of \a a and \a b
    static BitSetData o(BitSetData a, BitSetData b);
  };

  /// Status of a bitset
  enum BitSetStatus {
    BSS_NONE, ///< No bits set
    BSS_ALL,  ///< All bits set
    BSS_SOME  ///< Some but not all bits set
  };

  /// Basic bitset support  (without stored size information)
  class RawBitSetBase {
  protected:
    /// Bits per base
    static const unsigned int bpb = BitSetData::bpb;
    /// Stored bits
    BitSetData* data;
  private:
    /// Copy constructor (disabled)
    RawBitSetBase(const RawBitSetBase&);
    /// Assignment operator (disabled)
    RawBitSetBase& operator =(const RawBitSetBase&);
  public:
    /// Default constructor (yields empty set)
    RawBitSetBase(void);
    /// Initialize for \a sz bits and allocator \a a
    template<class A>
    RawBitSetBase(A& a, unsigned int sz, bool setbits=false);
    /// Copy from bitset \a bs with allocator \a a
    template<class A>
    RawBitSetBase(A& a, unsigned int sz, const RawBitSetBase& bs);
    /// Allocate for \a sz bits and allocator \a a (only after default constructor)
    template<class A>
    void allocate(A& a, unsigned int sz);
    /// Initialize for \a sz bits and allocator \a a (only after default constructor)
    template<class A>
    void init(A& a, unsigned int sz, bool setbits=false);
    /// Clear \a sz bits
    void clearall(unsigned int sz, bool setbits=false);
    /// Copy \a sz bits from \a bs
    void copy(unsigned int sz, const RawBitSetBase& bs);
    /// Access value at bit \a i
    bool get(unsigned int i) const;
    /// Set bit \a i
    void set(unsigned int i);
    /// Clear bit \a i
    void clear(unsigned int i);
    /// Return position greater or equal \a i of next set bit (\a i is allowed to be equal to size)
    unsigned int next(unsigned int i) const;
    /// Return status of bitset
    BitSetStatus status(unsigned int sz) const;
    /// Test whether all bits are set
    bool all(unsigned int sz) const;
    /// Test whether no bits are set
    bool none(unsigned int sz) const;
    /// Resize bitset from \a sz to \a n elememts
    template<class A>
    void resize(A& a, unsigned int sz, unsigned int n, bool setbits=false);
    /// Dispose memory for bit set
    template<class A>
    void dispose(A& a, unsigned int sz);
  };

  /// Basic bitset support
  class BitSetBase : public RawBitSetBase {
  protected:
    /// Size of bitset (number of bits)
    unsigned int sz;
  private:
    /// Copy constructor (disabled)
    BitSetBase(const BitSetBase&);
    /// Assignment operator (disabled)
    BitSetBase& operator =(const BitSetBase&);
  public:
    /// Default constructor (yields empty set)
    BitSetBase(void);
    /// Initialize for \a s bits and allocator \a a
    template<class A>
    BitSetBase(A& a, unsigned int s, bool setbits=false);
    /// Copy from bitset \a bs with allocator \a a
    template<class A>
    BitSetBase(A& a, const BitSetBase& bs);
    /// Initialize for \a s bits and allocator \a a (only after default constructor)
    template<class A>
    void init(A& a, unsigned int s, bool setbits=false);
    /// Clear \a sz bits
    void clearall(bool setbits=false);
    /// Copy \a sz bits from \a bs
    void copy(const BitSetBase& bs);
    /// Return size of bitset (number of bits)
    unsigned int size(void) const;
    /// Access value at bit \a i
    bool get(unsigned int i) const;
    /// Set bit \a i
    void set(unsigned int i);
    /// Clear bit \a i
    void clear(unsigned int i);
    /// Return position greater or equal \a i of next set bit (\a i is allowed to be equal to size)
    unsigned int next(unsigned int i) const;
    /// Return status of bitset
    BitSetStatus status(void) const;
    /// Test whether all bits are set
    bool all(void) const;
    /// Test whether no bits are set
    bool none(void) const;
    /// Resize bitset to \a n elememts
    template<class A>
    void resize(A& a, unsigned int n, bool setbits=false);
    /// Dispose memory for bit set
    template<class A>
    void dispose(A& a);
  };


  /*
   * Bitset data
   *
   */

  forceinline void
  BitSetData::init(bool setbits) {
    bits = setbits ? ~static_cast<Base>(0) : static_cast<Base>(0);
  }
  forceinline unsigned int
  BitSetData::data(unsigned int s) {
    return s == 0 ? 0 : ((s-1) / bpb + 1);
  }
  forceinline bool
  BitSetData::operator ()(unsigned int i) const {
    return (bits >> i) != static_cast<Base>(0U);
  }
  forceinline bool
  BitSetData::get(unsigned int i) const {
    return (bits & (static_cast<Base>(1U) << i)) != static_cast<Base>(0U);
  }
  forceinline void
  BitSetData::set(unsigned int i) {
    bits |= (static_cast<Base>(1U) << i);
  }
  forceinline void
  BitSetData::clear(unsigned int i) {
    bits &= ~(static_cast<Base>(1U) << i);
  }
  forceinline unsigned int
  BitSetData::next(unsigned int i) const {
    assert(bits != static_cast<Base>(0));
#if defined(GECODE_SUPPORT_MSVC_32)
    assert(bpb == 32);
    unsigned long int p;
    _BitScanForward(&p,bits >> i);
    return static_cast<unsigned int>(p)+i;
#elif defined(GECODE_SUPPORT_MSVC_64)
    assert(bpb == 64);
    unsigned long int p;
    _BitScanForward64(&p,bits >> i);
    return static_cast<unsigned int>(p)+i;
#elif defined(GECODE_HAS_BUILTIN_FFSL)
    if ((bpb == 32) || (bpb == 64)) {
      int p = __builtin_ffsl(bits >> i);
      assert(p > 0);
      return static_cast<unsigned int>(p-1)+i;
    }
#else
    while (!get(i)) i++;
    return i;
#endif
  }
  forceinline bool
  BitSetData::all(void) const {
    return bits == ~static_cast<Base>(0U);
  }
  forceinline bool
  BitSetData::all(unsigned int i) const {
    const Base mask = (static_cast<Base>(1U) << i) - static_cast<Base>(1U);
    return (bits & mask) == mask;
  }
  forceinline bool
  BitSetData::none(void) const {
    return bits == static_cast<Base>(0U);
  }
  forceinline bool
  BitSetData::none(unsigned int i) const {
    const Base mask = (static_cast<Base>(1U) << i) - static_cast<Base>(1U);
    return (bits & mask) == static_cast<Base>(0U);
  }

  forceinline void
  BitSetData::a(BitSetData a) {
    bits &= a.bits;
  }
  forceinline void
  BitSetData::a(BitSetData a, unsigned int i) {
    const Base mask = (static_cast<Base>(1U) << i) - static_cast<Base>(1U);
    bits &= (a.bits & mask);
  }
  forceinline BitSetData
  BitSetData::a(BitSetData a, BitSetData b) {
    BitSetData ab;
    ab.bits = a.bits & b.bits;
    return ab;
  }

  forceinline void
  BitSetData::o(BitSetData a) {
    bits |= a.bits;
  }
  forceinline void
  BitSetData::o(BitSetData a, unsigned int i) {
    const Base mask = (static_cast<Base>(1U) << i) - static_cast<Base>(1U);
    bits |= (a.bits & mask);
  }
  forceinline BitSetData
  BitSetData::o(BitSetData a, BitSetData b) {
    BitSetData ab;
    ab.bits = a.bits | b.bits;
    return ab;
  }


  /*
   * Basic bit sets
   *
   */

  forceinline bool
  RawBitSetBase::get(unsigned int i) const {
    return data[i / bpb].get(i % bpb);
  }
  forceinline void
  RawBitSetBase::set(unsigned int i) {
    data[i / bpb].set(i % bpb);
  }
  forceinline void
  RawBitSetBase::clear(unsigned int i) {
    data[i / bpb].clear(i % bpb);
  }

  template<class A>
  void
  RawBitSetBase::resize(A& a, unsigned int sz, unsigned int n, bool setbits) {
    if (n>sz) {
      data = a.template realloc<BitSetData>(data,BitSetData::data(sz+1),
                                            BitSetData::data(n+1));
      for (unsigned int i=BitSetData::data(sz)+1;
           i<BitSetData::data(n+1); i++) {
        data[i].init(setbits);
      }
      for (unsigned int i=(sz%bpb); i<bpb; i++)
        if (setbits)
          data[sz / bpb].set(i);
        else
          data[sz / bpb].clear(i);
    }
    set(n);
  }

  template<class A>
  forceinline void
  RawBitSetBase::dispose(A& a, unsigned int sz) {
    a.template free<BitSetData>(data,BitSetData::data(sz+1));
  }

  forceinline
  RawBitSetBase::RawBitSetBase(void)
    : data(NULL) {}

  template<class A>
  forceinline
  RawBitSetBase::RawBitSetBase(A& a, unsigned int sz, bool setbits)
    : data(a.template alloc<BitSetData>(BitSetData::data(sz+1))) {
    for (unsigned int i = BitSetData::data(sz+1); i--; ) 
      data[i].init(setbits);
    // Set a bit at position sz as sentinel (for efficient next)
    set(sz);
  }

  template<class A>
  forceinline
  RawBitSetBase::RawBitSetBase(A& a, unsigned int sz, const RawBitSetBase& bs)
    : data(a.template alloc<BitSetData>(BitSetData::data(sz+1))) {
    for (unsigned int i = BitSetData::data(sz+1); i--; ) 
      data[i] = bs.data[i];
    // Set a bit at position sz as sentinel (for efficient next)
    set(sz);
  }

  template<class A>
  forceinline void
  RawBitSetBase::allocate(A& a, unsigned int sz) {
    assert(data == NULL);
    data=a.template alloc<BitSetData>(BitSetData::data(sz+1));
  }

  template<class A>
  forceinline void
  RawBitSetBase::init(A& a, unsigned int sz, bool setbits) {
    assert(data == NULL);
    data=a.template alloc<BitSetData>(BitSetData::data(sz+1));
    for (unsigned int i=BitSetData::data(sz+1); i--; )
      data[i].init(setbits);
    // Set a bit at position sz as sentinel (for efficient next)
    set(sz);
  }

  forceinline void
  RawBitSetBase::copy(unsigned int sz, const RawBitSetBase& bs) {
    for (unsigned int i=BitSetData::data(sz+1); i--; )
      data[i] = bs.data[i];
  }

  forceinline void
  RawBitSetBase::clearall(unsigned int sz, bool setbits) {
    for (unsigned int i=BitSetData::data(sz+1); i--; )
      data[i].init(setbits);
  }

  forceinline unsigned int
  RawBitSetBase::next(unsigned int i) const {
    unsigned int pos = i / bpb;
    unsigned int bit = i % bpb;
    if (data[pos](bit))
      return pos * bpb + data[pos].next(bit);
    // The sentinel bit guarantees that this loop always terminates
    do { 
      pos++; 
    } while (!data[pos]());
    return pos * bpb + data[pos].next();
  }

  forceinline BitSetStatus
  RawBitSetBase::status(unsigned int sz) const {
    unsigned int pos = sz / bpb;
    unsigned int bits = sz % bpb;
    if (pos > 0) {
      if (data[0].all()) {
        for (unsigned int i=1; i<pos; i++)
          if (!data[i].all())
            return BSS_SOME;
        return data[pos].all(bits) ? BSS_ALL : BSS_SOME;
      } else if (data[0].none()) {
        for (unsigned int i=1; i<pos; i++)
          if (!data[i].none())
            return BSS_SOME;
        return data[pos].none(bits) ? BSS_NONE : BSS_SOME;
      } else {
        return BSS_SOME;
      }
    }
    if (data[0].all(bits))
      return BSS_ALL;
    if (data[0].none(bits))
      return BSS_NONE;
    return BSS_SOME;
  }

  forceinline bool
  RawBitSetBase::all(unsigned int sz) const {
    return status(sz) == BSS_ALL;
  }

  forceinline bool
  RawBitSetBase::none(unsigned int sz) const {
    return status(sz) == BSS_NONE;
  }


  template<class A>
  void
  BitSetBase::resize(A& a, unsigned int n, bool setbits) {
    RawBitSetBase::resize(a,sz,n,setbits); sz=n;
  }

  template<class A>
  forceinline void
  BitSetBase::dispose(A& a) {
    RawBitSetBase::dispose(a,sz);
  }

  forceinline
  BitSetBase::BitSetBase(void)
    : sz(0U) {}

  template<class A>
  forceinline
  BitSetBase::BitSetBase(A& a, unsigned int s, bool setbits)
    : RawBitSetBase(a,s,setbits), sz(s) {}

  template<class A>
  forceinline
  BitSetBase::BitSetBase(A& a, const BitSetBase& bs)
    : RawBitSetBase(a,bs.sz,bs), sz(bs.sz) {}

  template<class A>
  forceinline void
  BitSetBase::init(A& a, unsigned int s, bool setbits) {
    assert(sz == 0); 
    RawBitSetBase::init(a,s,setbits); sz=s;
  }

  forceinline void
  BitSetBase::copy(const BitSetBase& bs) {
    assert(sz == bs.sz);
    RawBitSetBase::copy(sz,bs);
  }

  forceinline void
  BitSetBase::clearall(bool setbits) {
    RawBitSetBase::clearall(sz,setbits);
  }

  forceinline unsigned int
  BitSetBase::size(void) const {
    return sz;
  }

  forceinline bool
  BitSetBase::get(unsigned int i) const {
    assert(i < sz);
    return RawBitSetBase::get(i);
  }
  forceinline void
  BitSetBase::set(unsigned int i) {
    assert(i < sz);
    RawBitSetBase::set(i);
  }
  forceinline void
  BitSetBase::clear(unsigned int i) {
    assert(i < sz);
    RawBitSetBase::clear(i);
  }

  forceinline unsigned int
  BitSetBase::next(unsigned int i) const {
    assert(i <= sz);
    return RawBitSetBase::next(i);
  }

  forceinline BitSetStatus
  BitSetBase::status(void) const {
    return RawBitSetBase::status(sz);
  }

  forceinline bool
  BitSetBase::all(void) const {
    return RawBitSetBase::all(sz);
  }

  forceinline bool
  BitSetBase::none(void) const {
    return RawBitSetBase::none(sz);
  }

}}

#ifdef GECODE_SUPPORT_MSVC_32
#undef GECODE_SUPPORT_MSVC_32
#endif
#ifdef GECODE_SUPPORT_MSVC_64
#undef GECODE_SUPPORT_MSVC_64
#endif

// STATISTICS: support-any

