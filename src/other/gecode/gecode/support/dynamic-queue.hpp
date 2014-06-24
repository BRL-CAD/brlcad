/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
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
   * \brief Queue with arbitrary number of elements
   *
   * \ingroup FuncSupport
   */
  template<class T, class A>
  class DynamicQueue {
  private:
    /// Memory allocator
    A& a;
    /// Current size of queue (must be power of two)
    int limit;
    /// First element in queue
    int fst;
    /// Last element in queue
    int lst;
    /// Elements in queue
    T*  q;
    /// Resize queue (double size)
    void resize(void);
    /// Move index \a i to next element
    void move(int& i);
  public:
    /// Initialize queue
    DynamicQueue(A& a);
    /// Release memory
    ~DynamicQueue(void);

    /// Test whether queue is empty
    bool empty(void) const;

    /// Reset queue to be empty
    void reset(void);

    /// Pop element added first from queue and return it
    T pop(void);
    /// Push element \a x to queue
    void push(const T& x);
  private:
    /// Allocate memory from heap (disabled)
    static void* operator new(size_t s) throw() { (void) s; return NULL; }
    /// Free memory allocated from heap (disabled)
    static void  operator delete(void* p) { (void) p; };
    /// Copy constructor (disabled)
    DynamicQueue(const DynamicQueue& s) : a(s.a) {}
    /// Assignment operator (disabled)
    const DynamicQueue& operator =(const DynamicQueue&) { return *this; }
  };


  template<class T, class A>
  forceinline void
  DynamicQueue<T,A>::move(int& i) {
    i = (i+1) & (limit - 1);
  }

  template<class T, class A>
  void
  DynamicQueue<T,A>::resize(void) {
    assert(fst == lst);
    T* nq = a.template alloc<T>(limit << 1);
    int j=0;
    for (int i = fst; i<limit; i++)
      nq[j++] = q[i];
    for (int i = 0; i<lst; i++)
      nq[j++] = q[i];
    a.template free<T>(q,limit);
    q = nq;
    fst = 0;
    lst = limit;
    limit <<= 1;
  }

  template<class T, class A>
  forceinline
  DynamicQueue<T,A>::DynamicQueue(A& a0)
    : a(a0), limit(8), fst(0), lst(0), q(a.template alloc<T>(limit)) {}

  template<class T, class A>
  forceinline
  DynamicQueue<T,A>::~DynamicQueue(void) {
    a.free(q,limit);
  }

  template<class T, class A>
  forceinline bool
  DynamicQueue<T,A>::empty(void) const {
    return fst == lst;
  }

  template<class T, class A>
  forceinline void
  DynamicQueue<T,A>::reset(void) {
    fst = lst = 0;
  }

  template<class T, class A>
  forceinline T
  DynamicQueue<T,A>::pop(void) {
    assert(!empty());
    T t = q[fst];
    move(fst);
    return t;
  }

  template<class T, class A>
  forceinline void
  DynamicQueue<T,A>::push(const T& x) {
    q[lst] = x;
    move(lst);
    if (fst == lst)
      resize();
  }

}}

// STATISTICS: support-any
