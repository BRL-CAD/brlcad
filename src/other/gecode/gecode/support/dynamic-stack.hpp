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

namespace Gecode { namespace Support {

  /**
   * \brief Stack with arbitrary number of elements
   *
   * \ingroup FuncSupport
   */
  template<class T, class A>
  class DynamicStack {
  private:
    /// Memory allocator
    A& a;
    /// Current size of the stack
    int limit;
    /// Top of stack
    int tos;
    /// Elements on stack
    T*  stack;
    /// Resize stack (increase size)
    void resize(void);
  public:
    /// Initialize stack with \a n elements
    DynamicStack(A& a, int n=64);
    /// Release memory
    ~DynamicStack(void);

    /// Test whether stack is empty
    bool empty(void) const;
    /// Return number of entries currently on stack
    int entries(void) const;

    /// Pop topmost element from stack and return it
    T pop(void);
    /// Return element on top of stack
    T& top(void) const;
    /// Return element that has just been popped
    T& last(void) const;
    /// Push element \a x on top of stack
    void push(const T& x);


    /** \brief Return entry at position \a i
     *
     * Position 0 corresponds to the element first pushed,
     * whereas position \c entries()-1 corresponds to the
     * element pushed last.
     */
    T& operator [](int i);
    /** \brief Return entry at position \a i
     *
     * Position 0 corresponds to the element first pushed,
     * whereas position \c entries()-1 corresponds to the
     * element pushed last.
     */
    const T& operator [](int i) const;
  private:
    /// Allocate memory from heap (disabled)
    static void* operator new(size_t s) throw() { (void) s; return NULL; }
    /// Free memory allocated from heap (disabled)
    static void  operator delete(void* p) { (void) p; };
    /// Copy constructor (disabled)
    DynamicStack(const DynamicStack& s) : a(s.a) {}
    /// Assignment operator (disabled)
    const DynamicStack& operator =(const DynamicStack&) { return *this; }
  };


  template<class T, class A>
  void
  DynamicStack<T,A>::resize(void) {
    int nl = (limit * 3) / 2;
    stack = a.template realloc<T>(stack,limit,nl);
    limit = nl;
  }

  template<class T, class A>
  forceinline
  DynamicStack<T,A>::DynamicStack(A& a0, int n)
    : a(a0), limit(n), tos(0), stack(a.template alloc<T>(n)) {}

  template<class T, class A>
  forceinline
  DynamicStack<T,A>::~DynamicStack(void) {
    a.free(stack,limit);
  }

  template<class T, class A>
  forceinline T
  DynamicStack<T,A>::pop(void) {
    return stack[--tos];
  }

  template<class T, class A>
  forceinline T&
  DynamicStack<T,A>::top(void) const {
    return stack[tos-1];
  }

  template<class T, class A>
  forceinline T&
  DynamicStack<T,A>::last(void) const {
    return stack[tos];
  }

  template<class T, class A>
  forceinline void
  DynamicStack<T,A>::push(const T& x) {
    stack[tos++] = x;
    if (tos==limit)
      resize();
  }

  template<class T, class A>
  forceinline bool
  DynamicStack<T,A>::empty(void) const {
    return tos==0;
  }

  template<class T, class A>
  forceinline int
  DynamicStack<T,A>::entries(void) const {
    return tos;
  }

  template<class T, class A>
  forceinline T&
  DynamicStack<T,A>::operator [](int i) {
    return stack[i];
  }

  template<class T, class A>
  forceinline const T&
  DynamicStack<T,A>::operator [](int i) const {
    return stack[i];
  }

}}

// STATISTICS: support-any
