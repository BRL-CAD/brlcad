/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
 *
 *  Converted into offset version:
 *     Christopher Mears <chris.mears@monash.edu>
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

namespace Gecode { namespace Iter { namespace Values {

  /**
   * \brief Value iterator for values in an offset bitset
   *
   * \ingroup FuncIterValues
   */
  template<class BS>
  class BitSetOffset {
  protected:
    /// Bitset
    const BS& bs;
    /// Current value
    int cur;
    /// Limit value
    int limit;
    /// Move to next set bit
    void move(void);
  public:
    /// \name Constructors and initialization
    //@{
    /// Initialize with bitset \a bs
    BitSetOffset(const BS& bs);
    /// Initialize with bitset \a bs and start \a n and stop \a m
    BitSetOffset(const BS& bs, int n, int m);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a value or done
    bool operator ()(void) const;
    /// Move iterator to next value (if possible)
    void operator ++(void);
    //@}

    /// \name Value access
    //@{
    /// Return current value
    int val(void) const;
    //@}
  };


  template<class BS>
  forceinline
  BitSetOffset<BS>::BitSetOffset(const BS& bs0) 
    : bs(bs0), cur(bs.next(bs0.offset())), limit(bs.offset()+bs.size()) {
  }

  template<class BS>
  forceinline
  BitSetOffset<BS>::BitSetOffset(const BS& bs0, int n, int m) 
    : bs(bs0), 
      cur(bs.next(n)), 
      limit(std::min(bs.offset()+bs.size(),m+1)) {
  }

  template<class BS>
  forceinline void
  BitSetOffset<BS>::operator ++(void) {
    cur=bs.next(cur+1);
  }
  template<class BS>
  forceinline bool
  BitSetOffset<BS>::operator ()(void) const {
    return cur < limit;
  }

  template<class BS>
  forceinline int
  BitSetOffset<BS>::val(void) const {
    return static_cast<int>(cur);
  }

}}}

// STATISTICS: iter-any
