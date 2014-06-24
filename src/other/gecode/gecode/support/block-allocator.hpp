/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
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
   * \brief Manage memory organized into block lists (allocator)
   *
   * The allocation policy is to free all memory allocated when
   * the block allocator is deleted.
   *
   * \ingroup FuncSupport
   */
  template<class T, class A, int blocksize = 512>
  class BlockAllocator {
  private:
    /// Memory allocator
    A& a;
    /// One block of memory
    class Block {
    public:
      T      b[blocksize];
      Block* next;
    };
    /// Pointer to current block
    Block* b;
    /// Pointer to next part of block (decreasing)
    T*     n;
    /// Size of allocated blocks
    size_t _size;
    /// Allocate additional block
    void allocate(void);
  public:
    /// Initialize
    BlockAllocator(A& a);
    /// Free all allocated blocks
    ~BlockAllocator(void);
    /// Return allocator used
    A& allocator(void);
    /// Return memory of size required by \a T
    T* operator ()(void);
    /// Return size of memory required by allocator
    size_t size(void) const;
  };

  /**
   * \brief Client for block allocator of type \a T
   *
   * Provides memory management for objects of type \a T.
   *
   * \ingroup FuncSupport
   */
  template<class T, class A, int blocksize = 512>
  class BlockClient {
  public:
    /// Allocate memory from block allocator \a ba
    static void* operator new(size_t s, BlockAllocator<T,A,blocksize>& ba);
    /// Noop (memory freed only when block allocator deleted)
    static void  operator delete(void*, BlockAllocator<T,A,blocksize>& ba);
    /// Noop (memory freed only when block allocator deleted)
    static void  operator delete(void*);
  };



  template<class T, class A, int blocksize>
  forceinline
  BlockAllocator<T,A,blocksize>::BlockAllocator(A& a0) : a(a0) {
    b = static_cast<Block*>(a.ralloc(sizeof(Block)));
    b->next = NULL;
    n       = &b->b[blocksize];
    _size = sizeof(Block);
  }

  template<class T, class A, int blocksize>
  forceinline
  BlockAllocator<T,A,blocksize>::~BlockAllocator(void) {
    while (b != NULL) {
      Block* f = b; b = b->next;
      a.rfree(f,sizeof(Block));
    }
  }

  template<class T, class A, int blocksize>
  forceinline A&
  BlockAllocator<T,A,blocksize>::allocator(void) {
    return a;
  }

  template<class T, class A, int blocksize>
  forceinline T*
  BlockAllocator<T,A,blocksize>::operator ()(void) {
    T* t = --n;
    if (t == &b->b[0])
      allocate();
    return t;
  }

  template<class T, class A, int blocksize>
  void
  BlockAllocator<T,A,blocksize>::allocate(void) {
    // Allocate another block
    Block* nb = static_cast<Block*>(a.ralloc(sizeof(Block)));
    nb->next = b; b = nb;
    n = &nb->b[blocksize];
    _size += sizeof(Block);
  }

  template<class T, class A, int blocksize>
  forceinline size_t
  BlockAllocator<T,A,blocksize>::size(void) const {
    return _size;
  }



  template<class T, class A, int blocksize>
  forceinline void
  BlockClient<T,A,blocksize>::operator 
  delete(void*,BlockAllocator<T,A,blocksize>&) {
  }
  template<class T, class A, int blocksize>
  forceinline void
  BlockClient<T,A,blocksize>::operator delete(void*) {
  }
  template<class T, class A, int blocksize>
  forceinline void*
  BlockClient<T,A,blocksize>::operator new(size_t,
                                           BlockAllocator<T,A,blocksize>& ba) {
    return ba();
  }

}}

// STATISTICS: support-any
