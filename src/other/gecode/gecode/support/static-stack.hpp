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
   * \brief Stack with fixed number of elements
   *
   * \ingroup FuncSupport
   */
  template<class T, class A>
  class StaticStack {
  private:
    /// Allocator
    A& a;
    /// Number of elements
    int n;
    /// Top of stack
    unsigned int tos;
    /// Stack space
    T* stack;
  public:
    /// Initialize for \a n elements
    StaticStack(A& a, int n);
    /// Release memory
    ~StaticStack(void);

    /// Reset stack (pop all elements)
    void reset(void);
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

  private:
    /// Allocate memory from heap (disabled)
    static void* operator new(size_t s) throw() { (void) s; return NULL; }
    /// Free memory allocated from heap (disabled)
    static void  operator delete(void* p) { (void) p; };
    /// Copy constructor (disabled)
    StaticStack(const StaticStack& s) : a(s.a) {}
    /// Assignment operator (disabled)
    const StaticStack& operator =(const StaticStack&) { return *this; }
  };

  template<class T, class A>
  forceinline
  StaticStack<T,A>::StaticStack(A& a0, int n0)
    : a(a0), n(n0), tos(0), stack(a.template alloc<T>(n)) {}

  template<class T, class A>
  forceinline
  StaticStack<T,A>::~StaticStack(void) {
    a.free(stack,n);
  }

  template<class T, class A>
  forceinline bool
  StaticStack<T,A>::empty(void) const {
    return tos==0;
  }

  template<class T, class A>
  forceinline int
  StaticStack<T,A>::entries(void) const {
    return tos;
  }

  template<class T, class A>
  forceinline void
  StaticStack<T,A>::reset(void) {
    tos = 0;
  }

  template<class T, class A>
  forceinline T
  StaticStack<T,A>::pop(void) {
    assert((tos > 0) && (tos <= static_cast<unsigned int>(n)));
    return stack[--tos];
  }

  template<class T, class A>
  forceinline T&
  StaticStack<T,A>::top(void) const {
    assert((tos > 0) && (tos <= static_cast<unsigned int>(n)));
    return stack[tos-1];
  }

  template<class T, class A>
  forceinline T&
  StaticStack<T,A>::last(void) const {
    assert((tos >= 0) && (tos < static_cast<unsigned int>(n)));
    return stack[tos];
  }

  template<class T, class A>
  forceinline void
  StaticStack<T,A>::push(const T& x) {
    assert(tos < static_cast<unsigned int>(n));
    stack[tos++] = x;
  }

}}

// STATISTICS: support-any
