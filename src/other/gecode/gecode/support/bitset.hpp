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
#include <cmath>

namespace Gecode { namespace Support {

  /// Simple bitsets
  template<class A>
  class BitSet : public BitSetBase {
  protected:
    /// Allocator
    A& a;
  public:
    /// Bit set with space for \a s bits
    BitSet(A& a, unsigned int s);
    /// Copy bit set \a bs
    BitSet(A& a, const BitSet& bs);
    /// Destructor
    ~BitSet(void);
  };

  template<class A>
  forceinline
  BitSet<A>::BitSet(A& a0, unsigned int s)
    : BitSetBase(a0,s), a(a0) {}

  template<class A>
  forceinline
  BitSet<A>::BitSet(A& a0, const BitSet<A>& bs)
    : BitSetBase(a0,bs), a(a0) {}

  template<class A>
  forceinline
  BitSet<A>::~BitSet(void) {
    dispose(a);
  }

}}

// STATISTICS: support-any

