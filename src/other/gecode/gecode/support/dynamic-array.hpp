/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

#include <algorithm>

namespace Gecode { namespace Support {

  /**
   * \brief Array with arbitrary number of elements
   *
   * \ingroup FuncSupport
   */
  template<class T, class A>
  class DynamicArray {
  private:
    /// Memory allocator
    A& a;
    /// Size of array
    int n;
    /// Array elements
    T*  x;
    /// Resize to at least \a n + 1 elements
    void resize(int n);
  public:
    /// Initialize with size \a n
    DynamicArray(A& a0, int n = 32);
    /// Copy elements from array \a da
    DynamicArray(const DynamicArray<T,A>& da);
    /// Release memory
    ~DynamicArray(void);

    /// Assign array (copy elements from \a a)
    const DynamicArray<T,A>& operator =(const DynamicArray<T,A>& da);

    /// Return element at position \a i (possibly resize)
    T& operator [](int i);
    /// Return element at position \a i
    const T& operator [](int) const;

    /// Cast in to pointer of type \a T
    operator T*(void);
  };


  template<class T, class A>
  forceinline
  DynamicArray<T,A>::DynamicArray(A& a0, int n0)
    : a(a0), n(n0), x(a.template alloc<T>(n)) {}

  template<class T, class A>
  forceinline
  DynamicArray<T,A>::DynamicArray(const DynamicArray<T,A>& da)
    : a(da.a), n(da.n), x(a.template alloc<T>(n)) {
    (void) heap.copy<T>(x,da.x,n);
  }

  template<class T, class A>
  forceinline
  DynamicArray<T,A>::~DynamicArray(void) {
    a.free(x,n);
  }

  template<class T, class A>
  forceinline const DynamicArray<T,A>&
  DynamicArray<T,A>::operator =(const DynamicArray<T,A>& da) {
    if (this != &da) {
      if (n < da.n) {
        a.free(x,n); n = da.n; x = a.template alloc<T>(n);
      }
      (void) heap.copy(x,da.x,n);
    }
    return *this;
  }

  template<class T, class A>
  void
  DynamicArray<T,A>::resize(int i) {
    int m = std::max(i+1, (3*n)/2);
    x = a.realloc(x,n,m);
    n = m;
  }

  template<class T, class A>
  forceinline T&
  DynamicArray<T,A>::operator [](int i) {
    if (i >= n) resize(i);
    assert(n > i);
    return x[i];
  }

  template<class T, class A>
  forceinline const T&
  DynamicArray<T,A>::operator [](int i) const {
    assert(n > i);
    return x[i];
  }

  template<class T, class A>
  forceinline
  DynamicArray<T,A>::operator T*(void) {
    return x;
  }

}}

// STATISTICS: support-any
