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

#include <cmath>

namespace Gecode {

  /// Globally shared object for propagator information
  class GlobalAFC {
  public:
    /// Class for storing timed-decay value
    class Counter {
    public:
      /// The counter value
      double c;
      /// The time-stamp
      unsigned long int t;
      /// Initialize
      void init(void);
    };
  private:
    /// Block of propagator information (of variable size)
    class Block {
    public:
      /// Next block
      Block* next;
      /// Start of counter entries
      Counter c[1];
      /// Allocate block with \a n entries and previous block \a p
      static Block* allocate(unsigned int n, Block* p=NULL);
    };
    /// Class for managing timed-decay values
    class DecayManager {
    protected:
      /// The decay factor
      double d;
      /// The number of precomputed decay powers
      static const unsigned int n_dpow = 8U;
      /// Precomputed decay powers
      double dpow[n_dpow];
      /// The global time-stamp
      unsigned long int t;
      /// Decay counter value
      void decay(Counter& c);
    public:
      /// Initialize
      DecayManager(void);
      /// Set decay factor to \a d
      void decay(double d);
      /// Return current decay factor
      double decay(void) const;
      /// Increment counter and perform decay
      void inc(Counter& c);
      /// Set failure count to \a a
      void set(Counter& c, double a);
      /// Return counter value 
      double val(Counter& c);
      /// Allocate memory from heap
      static void* operator new(size_t s);
      /// Free memory allocated from heap
      static void  operator delete(void* p);
    };
    /// Initial smallest number of entries per block
    static const unsigned int size_min = 32;
    /// Largest possible number of entries per block
    static const unsigned int size_max = 32 * 1024;
    /// The actual object to store the required information
    class Object {
    public:
      /// Mutex to synchronize globally shared access
      Support::FastMutex* mutex;
      /// Pointer to timed decay manager
      DecayManager* decay;
      /// Link to previous object (NULL if none)
      Object* parent;
      /// How many spaces or objects use this object
      unsigned int use_cnt;
      /// Size of current block
      unsigned int size;
      /// Number of free entries in current block
      unsigned int free;
      /// Currently used block
      Block* cur;
      /// Constructor
      Object(Support::FastMutex* m, Object* p=NULL);
      /// Allocate memory from heap
      static void* operator new(size_t s);
      /// Free memory allocated from heap
      static void  operator delete(void* p);
    };
    /// Pointer to object, possibly marked
    void* mo;
    /// Pointer to object without mark
    Object* object(void) const;
    /// Whether pointer points to local object
    bool local(void) const;
    /// Set pointer to local object
    void local(Object* o);
    /// Set global pointer to possibly marked object
    void global(void* mo);
  public:
    /// Initialize
    GlobalAFC(void);
    /// Copy during cloning
    GlobalAFC(const GlobalAFC& ga);
    /// Destructor
    ~GlobalAFC(void);
    /// Set decay factor to \a d
    void decay(double d);
    /// Return decay factor
    double decay(void) const;
    /// Increment failure count
    void fail(Counter& c);
    /// Set failure count to \a a
    void set(Counter& c, double a);
    /// Return failure count
    double afc(Counter& c);
    /// Allocate new propagator info
    Counter& allocate(void);
  };



  forceinline void
  GlobalAFC::Counter::init(void) {
    c=1.0; t=0UL;
  }

  forceinline void
  GlobalAFC::DecayManager::decay(double d0) {
    d = d0;
    if (d != 1.0) {
      double p = d;
      unsigned int i=0;
      do {
        dpow[i++]=p; p*=d;
      } while (i<n_dpow);
    }
  }
  forceinline
  GlobalAFC::DecayManager::DecayManager(void)
    : d(1.0), t(0UL) {}

  forceinline double
  GlobalAFC::DecayManager::decay(void) const {
    return d;
  }
  forceinline void
  GlobalAFC::DecayManager::decay(Counter& c) {
    assert((t >= c.t) && (d != 1.0));
    unsigned int n = t - c.t;
    if (n > 0) {
      if (n <= n_dpow) {
        c.c *= dpow[n-1];
      } else {
        c.c *= pow(d,static_cast<double>(n));
      }
      c.t = t;
    }
  }
  forceinline void
  GlobalAFC::DecayManager::inc(Counter& c) {
    if (d == 1.0) {
      c.c += 1.0;
    } else {
      decay(c);
      c.c += 1.0; c.t = ++t;
    }
  }
  forceinline double
  GlobalAFC::DecayManager::val(Counter& c) {
    if (d != 1.0)
      decay(c);
    return c.c;
  }
  forceinline void
  GlobalAFC::DecayManager::set(Counter& c, double a) {
    c.c = a;
  }
  forceinline void*
  GlobalAFC::DecayManager::operator new(size_t s) {
    return Gecode::heap.ralloc(s);
  }
  forceinline void
  GlobalAFC::DecayManager::operator delete(void* p) {
    Gecode::heap.rfree(p);
  }


  /*
   * Global AFC information
   *
   */

  forceinline void*
  GlobalAFC::Object::operator new(size_t s) {
    return Gecode::heap.ralloc(s);
  }

  forceinline void
  GlobalAFC::Object::operator delete(void* p) {
    Gecode::heap.rfree(p);
  }

  forceinline GlobalAFC::Block*
  GlobalAFC::Block::allocate(unsigned int n, GlobalAFC::Block* p) {
    Block* b = static_cast<Block*>(heap.ralloc(sizeof(Block)+
                                               (n-1)*sizeof(Counter)));
    b->next = p;
    return b;
  }

  forceinline
  GlobalAFC::Object::Object(Support::FastMutex* m, Object* p)
    : mutex(m), parent(p), use_cnt(1), size(size_min), free(size_min),
      cur(Block::allocate(size)) {
    if (parent == NULL) {
      decay = new DecayManager;
    } else {
      decay = parent->decay;
    }
  }

  forceinline GlobalAFC::Object*
  GlobalAFC::object(void) const {
    return static_cast<Object*>(Support::funmark(mo));
  }
  forceinline bool
  GlobalAFC::local(void) const {
    return !Support::marked(mo);
  }
  forceinline void
  GlobalAFC::local(Object* o) {
    assert(!Support::marked(o));
    mo = o;
  }
  forceinline void
  GlobalAFC::global(void* o) {
    mo = Support::fmark(o);
  }

  forceinline
  GlobalAFC::GlobalAFC(void) {
    // No synchronization needed as single thread is creating this object
    local(new Object(new Support::FastMutex));
  }

  forceinline
  GlobalAFC::GlobalAFC(const GlobalAFC& gpi) {
    global(gpi.mo);
    Object* o = object();
    o->mutex->acquire();
    o->use_cnt++;
    o->mutex->release();
  }

  forceinline
  GlobalAFC::~GlobalAFC(void) {
    Support::FastMutex* m = object()->mutex;
    m->acquire();
    Object* c = object();
    DecayManager* decay = c->decay;
    while ((c != NULL) && (--c->use_cnt == 0)) {
      // Delete all blocks for c
      Block* b = c->cur;
      while (b != NULL) {
        Block* d = b; b=b->next;
        heap.rfree(d);
      }
      // Delete object
      Object* d = c; c = c->parent;
      delete d; 
    }
    m->release();
    // All objects are deleted, so also delete mutex and decya info
    if (c == NULL) {
      delete decay;
      delete m;
    }
  }

  forceinline void
  GlobalAFC::fail(Counter& c) {
    Support::FastMutex& m = *object()->mutex;
    m.acquire();
    object()->decay->inc(c);
    m.release();
  }

  forceinline void
  GlobalAFC::set(Counter& c, double a) {
    Support::FastMutex& m = *object()->mutex;
    m.acquire();
    object()->decay->set(c,a);
    m.release();
  }

  forceinline double
  GlobalAFC::afc(Counter& c) {
    Support::FastMutex& m = *object()->mutex;
    double d;
    m.acquire();
    d = object()->decay->val(c);
    m.release();
    return d;
  }

  forceinline double
  GlobalAFC::decay(void) const {
    Support::FastMutex& m = *object()->mutex;
    double d;
    m.acquire();
    d = object()->decay->decay();
    m.release();
    return d;
  }

  forceinline void
  GlobalAFC::decay(double d) {
    Support::FastMutex& m = *object()->mutex;
    m.acquire();
    object()->decay->decay(d);
    m.release();
  }

  forceinline GlobalAFC::Counter&
  GlobalAFC::allocate(void) {
    /*
     * If there is no local object, create one.
     *
     * There is no synchronization needed as only ONE space has access
     * to the marked pointer AND the local object.
     */
    if (!local())
      local(new Object(object()->mutex,object()));

    assert(local());

    Object* o = object();

    if (o->free == 0) {
      if (2*o->size <= size_max)
        o->size *= 2;
      o->free = o->size;
      o->cur  = Block::allocate(o->size,o->cur);
    }

    Counter* c = &o->cur->c[--o->free];
    c->init();

    return *c;
  }

}

// STATISTICS: kernel-prop
