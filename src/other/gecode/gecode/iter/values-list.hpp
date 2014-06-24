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

namespace Gecode { namespace Iter { namespace Values {

  /**
   * \brief Iterator over value lists
   *
   * \ingroup FuncIterVlues
   */
  class ValueListIter {
  protected:
    /// Value list class
    class ValueList : public Support::BlockClient<ValueList,Region> {
    public:
      /// Value
      int val;
      /// Next element
      ValueList* next;
    };
    /// Shared object for allocation
    class VLIO : public Support::BlockAllocator<ValueList,Region> {
    public:
      /// Counter used for reference counting
      unsigned int use_cnt;
      /// Initialize
      VLIO(Region& r);
    };
    /// Reference to shared object
    VLIO* vlio;
    /// Head of value list
    ValueList* h;
    /// Current list element
    ValueList* c;
    /// Set value lists
    void set(ValueList* l);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ValueListIter(void);
    /// Copy constructor
    ValueListIter(const ValueListIter& i);
    /// Initialize
    ValueListIter(Region& r);
    /// Initialize
    void init(Region& r);
    /// Assignment operator (both iterators must be allocated from the same region)
    ValueListIter& operator =(const ValueListIter& i);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a value or done
    bool operator ()(void) const;
    /// Move iterator to next value (if possible)
    void operator ++(void);
    /// Reset iterator to start
    void reset(void);
    //@}

    /// \name Value access
    //@{
    /// Return value
    int val(void) const;
    //@}

    /// Destructor
    ~ValueListIter(void);
  };


  forceinline
  ValueListIter::VLIO::VLIO(Region& r) 
    : Support::BlockAllocator<ValueList,Region>(r), use_cnt(1) {}


  forceinline
  ValueListIter::ValueListIter(void) 
    : vlio(NULL) {}

  forceinline
  ValueListIter::ValueListIter(Region& r) 
    : vlio(new (r.ralloc(sizeof(VLIO))) VLIO(r)), 
      h(NULL), c(NULL) {}

  forceinline void
  ValueListIter::init(Region& r) {
    vlio = new (r.ralloc(sizeof(VLIO))) VLIO(r);
    h = c = NULL;
  }

  forceinline
  ValueListIter::ValueListIter(const ValueListIter& i) 
    : vlio(i.vlio), h(i.h), c(i.c)  {
    vlio->use_cnt++;
  }

  forceinline ValueListIter&
  ValueListIter::operator =(const ValueListIter& i) {
    if (&i != this) {
      if (--vlio->use_cnt == 0) {
        Region& r = vlio->allocator();
        vlio->~VLIO();
        r.rfree(vlio,sizeof(VLIO));
      }
      vlio = i.vlio;
      vlio->use_cnt++;
      c=i.c; h=i.h;
    }
    return *this;
  }

  forceinline
  ValueListIter::~ValueListIter(void) {
    if (--vlio->use_cnt == 0) {
      Region& r = vlio->allocator();
      vlio->~VLIO();
      r.rfree(vlio,sizeof(VLIO));
    }
  }


  forceinline void
  ValueListIter::set(ValueList* l) {
    h = c = l;
  }

  forceinline bool
  ValueListIter::operator ()(void) const {
    return c != NULL;
  }

  forceinline void
  ValueListIter::operator ++(void) {
    c = c->next;
  }

  forceinline void
  ValueListIter::reset(void) {
    c = h;
  }

  forceinline int
  ValueListIter::val(void) const {
    return c->val;
  }

}}}

// STATISTICS: iter-any

