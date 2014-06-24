/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
 *
 *  Last modified:
 *     $Date$
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

namespace Gecode { namespace Int { namespace Sequence {

  /// Simple bitsets for recording violations
  class Violations : public Support::BitSetBase {
  protected:
    /// The (possibly) first set bit (set is empty if fst == sz)
    mutable unsigned int fst;
  public:
    /// Default constructor
    Violations(void);
    /// Initialize violation set for \a n violations
    void init(Space& home, unsigned int n);
    /// Update violation set during cloning
    void update(Space& home, bool shared, Violations& v);
    /// Return whether set is empty
    bool empty(void) const;
    /// Add \a i to violation set
    void add(unsigned int i);
    /// Get first element from violation set and remove it
    unsigned int get(void);
  };


  forceinline
  Violations::Violations(void) : fst(0) {}

  forceinline void
  Violations::init(Space& home, unsigned int n) {
    Support::BitSetBase::init(home,n);
    fst=size();
  }

  forceinline bool
  Violations::empty(void) const {
    fst = next(fst);
    return fst >= size();
  }
  
  forceinline void
  Violations::update(Space& home, bool, Violations& v) {
    assert(v.empty());
    init(home,v.size());
  }

  forceinline void
  Violations::add(unsigned int i) {
    set(i); if (i < fst) fst = i;
  }
  
  forceinline unsigned int
  Violations::get(void) {
    assert(!empty());
    fst = next(fst);
    assert(Support::BitSetBase::get(fst));
    clear(fst); fst++;
    return fst-1;
  }
  
}}}

// STATISTICS: int-prop
