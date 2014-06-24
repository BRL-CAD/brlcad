/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2010
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

namespace Gecode { namespace Iter { namespace Ranges {

  /**
   * \brief Iterator over range lists
   *
   * \ingroup FuncIterRanges
   */
  class RangeListIter {
  protected:
    /// Range list class
    class RangeList : public Support::BlockClient<RangeList,Region> {
    public:
      /// Minimum and maximum of a range
      int min, max;
      /// Next element
      RangeList* next;
    };
    /// Shared object for allocation
    class RLIO : public Support::BlockAllocator<RangeList,Region> {
    public:
      /// Counter used for reference counting
      unsigned int use_cnt;
      /// Initialize
      RLIO(Region& r);
    };
    /// Reference to shared object
    RLIO* rlio;
    /// Head of range list
    RangeList* h;
    /// Current list element
    RangeList* c;
    /// Set range lists
    void set(RangeList* l);
    /// Get head of current range list
    RangeList* get(void) const;
    /// Create new range possibly from freelist \a f and init
    RangeList* range(int min, int max, RangeList*& f);
    /// Create new range possibly and init
    RangeList* range(int min, int max);
    /// Create new range possibly from freelist \a f and init
    template<class I>
    RangeList* range(I& i, RangeList*& f);
    /// Create new range possibly and init
    template<class I>
    RangeList* range(I& i);
    /// Copy the iterator \a i to a range list
    template<class I>
    RangeList* copy(I& i);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    RangeListIter(void);
    /// Copy constructor
    RangeListIter(const RangeListIter& i);
    /// Initialize
    RangeListIter(Region& r);
    /// Initialize
    void init(Region& r);
    /// Assignment operator
    RangeListIter& operator =(const RangeListIter& i);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
    /// Move iterator to next range (if possible)
    void operator ++(void);
    /// Reset iterator to start
    void reset(void);
    //@}

    /// \name Range access
    //@{
    /// Return smallest value of range
    int min(void) const;
    /// Return largest value of range
    int max(void) const;
    /// Return width of range (distance between minimum and maximum)
    unsigned int width(void) const;
    //@}

    /// Destructor
    ~RangeListIter(void);
  };


  forceinline
  RangeListIter::RLIO::RLIO(Region& r) 
    : Support::BlockAllocator<RangeList,Region>(r), use_cnt(1) {}


  forceinline
  RangeListIter::RangeListIter(void) 
    : rlio(NULL), h(NULL), c(NULL) {}

  forceinline
  RangeListIter::RangeListIter(Region& r) 
    : rlio(new (r.ralloc(sizeof(RLIO))) RLIO(r)), h(NULL), c(NULL) {}

  forceinline void
  RangeListIter::init(Region& r) {
    rlio = new (r.ralloc(sizeof(RLIO))) RLIO(r);
    h = c = NULL;
  }

  forceinline
  RangeListIter::RangeListIter(const RangeListIter& i) 
    : rlio(i.rlio), h(i.h), c(i.c)  {
    if (rlio != NULL)
      rlio->use_cnt++;
  }

  forceinline RangeListIter&
  RangeListIter::operator =(const RangeListIter& i) {
    if (&i != this) {
      if ((rlio != NULL) && (--rlio->use_cnt == 0)) {
        Region& r = rlio->allocator();
        rlio->~RLIO();
        r.rfree(rlio,sizeof(RLIO));
      }
      rlio = i.rlio;
      if (rlio != NULL)
        rlio->use_cnt++;
      c=i.c; h=i.h;
    }
    return *this;
  }

  forceinline
  RangeListIter::~RangeListIter(void) {
    if ((rlio != NULL) && (--rlio->use_cnt == 0)) {
      Region& r = rlio->allocator();
      rlio->~RLIO();
      r.rfree(rlio,sizeof(RLIO));
    }
  }


  forceinline void
  RangeListIter::set(RangeList* l) {
    h = c = l;
  }

  forceinline RangeListIter::RangeList*
  RangeListIter::get(void) const {
    return h;
  }

  forceinline RangeListIter::RangeList*
  RangeListIter::range(int min, int max, RangeList*& f) {
    RangeList* t;
    // Take element from freelist if possible
    if (f != NULL) {
      t = f; f = f->next;
    } else {
      t = new (*rlio) RangeList;
    }
    t->min = min; t->max = max;
    return t;
  }

  forceinline RangeListIter::RangeList*
  RangeListIter::range(int min, int max) {
    RangeList* t = new (*rlio) RangeList;
    t->min = min; t->max = max;
    return t;
  }

  template<class I>
  forceinline RangeListIter::RangeList*
  RangeListIter::range(I& i, RangeList*& f) {
    return range(i.min(),i.max(),f);
  }

  template<class I>
  forceinline RangeListIter::RangeList*
  RangeListIter::range(I& i) {
    return range(i.min(),i.max());
  }

  template<class I>
  inline RangeListIter::RangeList*
  RangeListIter::copy(I& i) {
    RangeList*  h;
    RangeList** c = &h; 
    for ( ; i(); ++i) {
      RangeList* t = range(i);
      *c = t; c = &t->next;
    }
    *c = NULL;
    return h;
  }

  forceinline bool
  RangeListIter::operator ()(void) const {
    return c != NULL;
  }

  forceinline void
  RangeListIter::operator ++(void) {
    c = c->next;
  }

  forceinline void
  RangeListIter::reset(void) {
    c = h;
  }

  forceinline int
  RangeListIter::min(void) const {
    return c->min;
  }
  forceinline int
  RangeListIter::max(void) const {
    return c->max;
  }
  forceinline unsigned int
  RangeListIter::width(void) const {
    return static_cast<unsigned int>(c->max-c->min)+1;
  }

}}}

// STATISTICS: iter-any

