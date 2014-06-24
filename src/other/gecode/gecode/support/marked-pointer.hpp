/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
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

#include <cstddef>

namespace Gecode { namespace Support {

  /// Check whether \a p is marked
  bool marked(void* p);
  /// Return marked pointer for \a p
  void* mark(void* p);
  /// Return unmarked pointer for a marked pointer \a p
  void* unmark(void* p);
  /// Return marked pointer for \a p (possibly already marked)
  void* fmark(void* p);
  /// Return unmarked pointer for a possibly marked pointer \a p
  void* funmark(void* p);

  forceinline bool
  marked(void* p) {
    return (reinterpret_cast<ptrdiff_t>(p) & 1) != 0;
  }
  forceinline void*
  mark(void* p) {
    assert(!marked(p));
    return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(p)+1);
  }
  forceinline void*
  unmark(void* p) {
    assert(marked(p));
    return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(p)-1);
  }
  forceinline void*
  fmark(void* p) {
    return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(p)|1);
  }
  forceinline void*
  funmark(void* p) {
    return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(p)&
                                   ~static_cast<ptrdiff_t>(1));
  }

}}

// STATISTICS: support-any
