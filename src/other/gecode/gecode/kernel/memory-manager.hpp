/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Bugfixes provided by:
 *     Zandra Norman
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Guido Tack, 2004
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

namespace Gecode {

  /// Memory chunk with size information
  class MemoryChunk {
  public:
    /// Next chunk
    MemoryChunk* next;
    /// Size of chunk
    size_t size;
  };

  /// Memory chunk allocated from heap with proper alignment
  class HeapChunk : public MemoryChunk {
  public:
    /// Start of memory area inside chunk
    double area[1];
  };

  class Region;

  /// Shared object for several memory areas
  class SharedMemory {
    friend class Region;
  private:
    /// How many spaces use this shared memory object
    unsigned int use_cnt;
    /// The components for the shared region area
    struct {
      /// Amount of free memory
      size_t free;
      /// The actual memory area (allocated from top to bottom)
      double area[MemoryConfig::region_area_size / sizeof(double)];
    } region;
    /// The components for shared heap memory
    struct {
      /// How many heap chunks are available for caching
      unsigned int n_hc;
      /// A list of cached heap chunks
      HeapChunk* hc;
    } heap;
  public:
    /// Initialize
    SharedMemory(void);
    /// Flush all cached memory
    void flush(void);
    /// Destructor
    ~SharedMemory(void);
    /// \name Region management
    //@
    /// Return memory chunk if available
    bool region_alloc(size_t s, void*& p);
    //@}
    /// \name Heap management
    //@
    /// Return heap chunk, preferable of size \a s, but at least of size \a l
    HeapChunk* heap_alloc(size_t s, size_t l);
    /// Free heap chunk (or cache for later)
    void heap_free(HeapChunk* hc);
    //@}
    /// Return copy during cloning
    SharedMemory* copy(bool share);
    /// Release by one space
    bool release(void);
    /// Allocate memory from heap
    static void* operator new(size_t s);
    /// Free memory allocated from heap
    static void  operator delete(void* p);
  };


  /**
   * \brief Base-class for freelist-managed objects
   *
   * Freelist-managed object must inherit from this class. The size
   * of objects of subclasses is defined by the parameters in
   * Gecode::MemoryConfig.
   * \ingroup FuncMemSpace
   */
  class FreeList {
  protected:
    /// Pointer to next freelist object
    FreeList* _next;
  public:
    /// Use uninitialized
    FreeList(void);
    /// Initialize with next freelist object \a n
    FreeList(FreeList* n);
    /// Return next freelist object
    FreeList* next(void) const;
    /// Return pointer to next link in freelist object
    FreeList** nextRef(void);
    /// Set next freelist object to \a n
    void next(FreeList* n);
  };

  /// Manage memory for space
  class MemoryManager {
  public:
    /// Constructor initialization
    MemoryManager(SharedMemory* sm);
    /// Constructor during cloning \a mm with shared memory \a sm and for a memory area for subscriptions of size \a s_sub
    MemoryManager(SharedMemory* sm, MemoryManager& mm, size_t s_sub);
    /// Release all allocated heap chunks
    void release(SharedMemory* sm);

  private:
    size_t     cur_hcsz;  ///< Current heap chunk size
    HeapChunk* cur_hc;    ///< Current heap chunk
    size_t     requested; ///< Total amount of heap memory requested

    char*  start; ///< Start of current heap area used for allocation
    size_t lsz;   ///< Size left for allocation

    /// Refill current heap area (outlined) issued by request of size \a s
    GECODE_KERNEL_EXPORT
    void alloc_refill(SharedMemory* sm, size_t s);
    /// Do the real work for refilling
    void alloc_fill(SharedMemory* sm, size_t s, bool first);

  public:
    /// Allocate memory of size \a s
    void* alloc(SharedMemory* sm, size_t s);
    /// Get the memory area for subscriptions
    void* subscriptions(void) const;

  private:
    /// Start of free lists
    FreeList* fl[MemoryConfig::fl_size_max-MemoryConfig::fl_size_min+1];
    /// Refill free list
    template<size_t> void fl_refill(SharedMemory* sm);
    /// Translate size to index in free list
    static size_t sz2i(size_t);
    /// Translate index in free list to size
    static size_t i2sz(size_t);

  public:
    /// Allocate free list element of size \a s
    template<size_t s>
    void* fl_alloc(SharedMemory* sm);
    /// Release all free list elements of size \a s between f and l (inclusive)
    template<size_t> void  fl_dispose(FreeList* f, FreeList* l);

  private:
    /// Slack memory chunks
    MemoryChunk* slack;
  public:
    /// Store for reusal, if of sufficient size for free list
    void reuse(void* p, size_t s);
  };


  /*
   * Shared memory area
   *
   */

  forceinline void*
  SharedMemory::operator new(size_t s) {
    return Gecode::heap.ralloc(s);
  }
  forceinline void
  SharedMemory::operator delete(void* p) {
    Gecode::heap.rfree(p);
  }
  forceinline
  SharedMemory::SharedMemory(void)
    : use_cnt(1) {
    region.free = MemoryConfig::region_area_size;
    heap.n_hc = 0;
    heap.hc = NULL;
  }
  forceinline void
  SharedMemory::flush(void) {
    heap.n_hc = 0;
    while (heap.hc != NULL) {
      HeapChunk* hc = heap.hc;
      heap.hc = static_cast<HeapChunk*>(hc->next);
      Gecode::heap.rfree(hc);
    }
  }
  forceinline
  SharedMemory::~SharedMemory(void) {
    flush();
  }
  forceinline SharedMemory*
  SharedMemory::copy(bool share) {
    if (share) {
      use_cnt++;
      return this;
    } else {
      return new SharedMemory();
    }
  }
  forceinline bool
  SharedMemory::release(void) {
    return --use_cnt == 0;
  }
  forceinline bool
  SharedMemory::region_alloc(size_t s, void*& p) {
    MemoryConfig::align(s);
    if (s > region.free)
      return false;
    region.free -= s;
    p = ptr_cast<char*>(&region.area[0]) + region.free;
    return true;
  }
  forceinline HeapChunk*
  SharedMemory::heap_alloc(size_t s, size_t l) {
    while ((heap.hc != NULL) && (heap.hc->size < l)) {
      heap.n_hc--;
      HeapChunk* hc = heap.hc;
      heap.hc = static_cast<HeapChunk*>(hc->next);
      Gecode::heap.rfree(hc);
    }
    if (heap.hc == NULL) {
      assert(heap.n_hc == 0);
      HeapChunk* hc = static_cast<HeapChunk*>(Gecode::heap.ralloc(s));
      hc->size = s;
      return hc;
    } else {
      heap.n_hc--;
      HeapChunk* hc = heap.hc;
      heap.hc = static_cast<HeapChunk*>(hc->next);
      return hc;
    }
  }
  forceinline void
  SharedMemory::heap_free(HeapChunk* hc) {
    if (heap.n_hc == MemoryConfig::n_hc_cache) {
      Gecode::heap.rfree(hc);
    } else {
      heap.n_hc++;
      hc->next = heap.hc; heap.hc = hc;
    }
  }


  /*
   * Freelists
   *
   */

  forceinline
  FreeList::FreeList(void) {}

  forceinline
  FreeList::FreeList(FreeList* n)
    : _next(n) {}

  forceinline FreeList*
  FreeList::next(void) const {
    return _next;
  }

  forceinline FreeList**
  FreeList::nextRef(void) {
    return &_next;
  }

  forceinline void
  FreeList::next(FreeList* n) {
    _next = n;
  }

  forceinline size_t
  MemoryManager::sz2i(size_t s) {
    assert(s >= (MemoryConfig::fl_size_min << MemoryConfig::fl_unit_size));
    assert(s <= (MemoryConfig::fl_size_max << MemoryConfig::fl_unit_size));
    return (s >> MemoryConfig::fl_unit_size) - MemoryConfig::fl_size_min;
  }

  forceinline size_t
  MemoryManager::i2sz(size_t i) {
    return (i + MemoryConfig::fl_size_min) << MemoryConfig::fl_unit_size;
  }


  /*
   * The active memory manager
   *
   */

  forceinline void*
  MemoryManager::alloc(SharedMemory* sm, size_t sz) {
    assert(sz > 0);
    // Perform alignment
    MemoryConfig::align(sz);
    // Check whether sufficient memory left
    if (sz > lsz)
      alloc_refill(sm,sz);
    lsz -= sz;
    return start + lsz;
  }

  forceinline void*
  MemoryManager::subscriptions(void) const {
    return &cur_hc->area[0];
  }

  forceinline void
  MemoryManager::alloc_fill(SharedMemory* sm, size_t sz, bool first) {
    // Adjust current heap chunk size
    if (((requested > MemoryConfig::hcsz_inc_ratio*cur_hcsz) ||
         (sz > cur_hcsz)) &&
        (cur_hcsz < MemoryConfig::hcsz_max) &&
        !first) {
      cur_hcsz <<= 1;
    }
    // Increment the size that it caters for the initial overhead
    size_t overhead = sizeof(HeapChunk) - sizeof(double);
    sz += overhead;
    // Round size to next multiple of current heap chunk size
    size_t allocate = ((sz > cur_hcsz) ?
                       (((size_t) (sz / cur_hcsz)) + 1) * cur_hcsz : cur_hcsz);
    // Request a chunk of preferably size allocate, but at least size sz
    HeapChunk* hc = sm->heap_alloc(allocate,sz);
    start = ptr_cast<char*>(&hc->area[0]);
    lsz   = hc->size - overhead;
    // Link heap chunk, where the first heap chunk is kept in place
    if (first) {
      requested = hc->size;
      hc->next = NULL; cur_hc = hc;
    } else {
      requested += hc->size;
      hc->next = cur_hc->next; cur_hc->next = hc;
    }
#ifdef GECODE_MEMORY_CHECK
    for (char* c = start; c < (start+lsz); c++)
      *c = 0;
#endif
  }

  forceinline
  MemoryManager::MemoryManager(SharedMemory* sm)
    : cur_hcsz(MemoryConfig::hcsz_min), requested(0), slack(NULL) {
    alloc_fill(sm,cur_hcsz,true);
    for (size_t i = MemoryConfig::fl_size_max-MemoryConfig::fl_size_min+1;
         i--; )
      fl[i] = NULL;
  }

  forceinline
  MemoryManager::MemoryManager(SharedMemory* sm, MemoryManager& mm, 
                               size_t s_sub)
    : cur_hcsz(mm.cur_hcsz), requested(0), slack(NULL) {
    MemoryConfig::align(s_sub);
    if ((mm.requested < MemoryConfig::hcsz_dec_ratio*mm.cur_hcsz) &&
        (cur_hcsz > MemoryConfig::hcsz_min) &&
        (s_sub*2 < cur_hcsz))
      cur_hcsz >>= 1;
    alloc_fill(sm,cur_hcsz+s_sub,true);
    // Skip the memory area at the beginning for subscriptions
    lsz   -= s_sub;
    start += s_sub;
    for (size_t i = MemoryConfig::fl_size_max-MemoryConfig::fl_size_min+1;
         i--; )
      fl[i] = NULL;
  }

  forceinline void
  MemoryManager::release(SharedMemory* sm) {
    // Release all allocated heap chunks
    HeapChunk* hc = cur_hc;
    do {
      HeapChunk* t = hc; hc = static_cast<HeapChunk*>(hc->next);
      sm->heap_free(t);
    } while (hc != NULL);
  }



  /*
   * Slack memory management
   *
   */
  forceinline void
  MemoryManager::reuse(void* p, size_t s) {
#ifdef GECODE_MEMORY_CHECK
    {
      char* c = static_cast<char*>(p);
      char* e = c + s;
      while (c < e) {
        *c = 0; c++;
      }
    }
#endif
    if (s < (MemoryConfig::fl_size_min<<MemoryConfig::fl_unit_size))
      return;
    if (s > (MemoryConfig::fl_size_max<<MemoryConfig::fl_unit_size)) {
      MemoryChunk* rc = static_cast<MemoryChunk*>(p);
      rc->next = slack;
      rc->size = s;
      slack = rc;
    } else {
      size_t i = sz2i(s);
      FreeList* f = static_cast<FreeList*>(p);
      f->next(fl[i]); fl[i]=f;
    }
  }


  /*
   * Freelist management
   *
   */

  template<size_t s>
  forceinline void*
  MemoryManager::fl_alloc(SharedMemory* sm) {
    size_t i = sz2i(s);
    FreeList* f = fl[i];
    if (f == NULL) {
      fl_refill<s>(sm); f = fl[i];
    }
    FreeList* n = f->next();
    fl[i] = n;
    return f;
  }

  template<size_t s>
  forceinline void
  MemoryManager::fl_dispose(FreeList* f, FreeList* l) {
    size_t i = sz2i(s);
#ifdef GECODE_AUDIT
    FreeList* cur = f;
    while (cur != l) {
      assert(cur->next());
      cur = cur->next();
    }
#endif
    l->next(fl[i]); fl[i] = f;
  }

  template<size_t sz>
  void
  MemoryManager::fl_refill(SharedMemory* sm) {
    // Try to acquire memory from slack
    if (slack != NULL) {
      MemoryChunk* m = slack;
      slack = NULL;
      do {
        char*  block = ptr_cast<char*>(m);
        size_t s     = m->size;
        assert(s >= sz);
        m = m->next;
        fl[sz2i(sz)] = ptr_cast<FreeList*>(block);
        while (s >= 2*sz) {
          ptr_cast<FreeList*>(block)->next(ptr_cast<FreeList*>(block+sz));
          block += sz;
          s     -= sz;
        }
        ptr_cast<FreeList*>(block)->next(NULL);
      } while (m != NULL);
    } else {
      char* block = static_cast<char*>(alloc(sm,MemoryConfig::fl_refill*sz));
      fl[sz2i(sz)] = ptr_cast<FreeList*>(block);
      int i = MemoryConfig::fl_refill-2;
      do {
        ptr_cast<FreeList*>(block+i*sz)->next(ptr_cast<FreeList*>(block+(i+1)*sz));
      } while (--i >= 0);
      ptr_cast<FreeList*>(block+
                          (MemoryConfig::fl_refill-1)*sz)->next
        (ptr_cast<FreeList*>(NULL));
    }
  }

}

// STATISTICS: kernel-memory
