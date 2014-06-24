/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

namespace Gecode { namespace Int { namespace ViewValGraph {

  template<class T>
  forceinline
  CombPtrFlag<T>::CombPtrFlag(T* p1, T* p2)
    : cpf(reinterpret_cast<ptrdiff_t>(p1) ^ reinterpret_cast<ptrdiff_t>(p2)) {}

  template<class T>
  forceinline T*
  CombPtrFlag<T>::ptr(T* p) const {
    return reinterpret_cast<T*>((cpf&~1) ^ reinterpret_cast<ptrdiff_t>(p));
  }

  template<class T>
  forceinline int
  CombPtrFlag<T>::is_set(void) const {
    return static_cast<int>(cpf&1);
  }

  template<class T>
  forceinline void
  CombPtrFlag<T>::set(void) {
    cpf |= 1;
  }

  template<class T>
  forceinline void
  CombPtrFlag<T>::unset(void) {
    cpf &= ~1;
  }

  template<class T>
  forceinline void
  CombPtrFlag<T>::init(T* p1, T* p2) {
    cpf = reinterpret_cast<ptrdiff_t>(p1) ^ reinterpret_cast<ptrdiff_t>(p2);
  }

}}}

// STATISTICS: int-prop

