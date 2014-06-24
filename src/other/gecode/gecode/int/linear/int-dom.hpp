/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
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

namespace Gecode { namespace Int { namespace Linear {

  /**
   * \brief %Set for support information
   *
   * Records supported positions of values such that with iteration
   * the supported values can be reconstructed.
   *
   */
  class SupportSet {
  private:
    /// Bitset for storing support information
    Support::BitSetBase bs;
  public:
    /// Default constructor
    SupportSet(void);
    /// Initialize support set with cardinality \a n
    void init(Region& r, unsigned int n);
    /// Record that there is support at position \a i
    void support(unsigned int i);
    /// Check whether position \arg i has support
    bool supported(unsigned int i) const;

  private:
    /// Support-based iterator: iterates values to be removed
    class ResultIter : public ViewValues<IntView> {
    protected:
      /// The support set used
      const SupportSet& s;
      /// The current position of the value
      unsigned int p;
    public:
      /// Initialize iterator
      ResultIter(const SupportSet& s0, const IntView& x);
      /// Increment to next supported value
      void operator ++(void);
    };

  public:
    /// Perform tell according to recorded support information on \arg x
    ModEvent tell(Space& home, IntView& x) const;
  };

  /**
   * \brief Base-class for support-based iterator
   *
   */
  template<class Val>
  class SupportIter {
  protected:
    /// Integer coefficient for view
    int           a;
    /// Integer view
    IntView       x;
    /// Set of support for values in x
    SupportSet    s;
    /// Current value
    int           c;
    /// Position of current value
    unsigned int  p;
    /// Lower bound information for value
    Val           l;
    /// Upper bound information for value
    Val           u;
  public:
    /// Initialize view
    void init(Region& r, int a, const IntView& x, Val l, Val u);
    /// Record value at current position as supported
    void support(void);
    /// Tell back new variable domain according to support found
    ModEvent tell(Space& home);
  };


  /**
   * \brief Support-based iterator for positive view
   *
   */
  template<class Val>
  class PosSupportIter : public SupportIter<Val> {
  private:
    /// Iterate ranges of integer view in increasing order
    IntVarImpFwd i;
    // Using-declarations for dependant names
    using SupportIter<Val>::a;
    using SupportIter<Val>::x;
    using SupportIter<Val>::s;
    using SupportIter<Val>::c;
    using SupportIter<Val>::p;
    using SupportIter<Val>::l;
    using SupportIter<Val>::u;
  public:
    /// Reset iterator to beginning and adjust \arg d accordingly
    bool reset(Val& d);
    /// Adjust \arg d and return true if next value still works
    bool adjust(Val& d);
  };


  /**
   * \brief Support-based iterator for negative view
   *
   */
  template<class Val>
  class NegSupportIter : public SupportIter<Val> {
  private:
    /// Iterate ranges of integer view in decreasing order
    IntVarImpBwd i;
    // Using-declarations for dependant names
    using SupportIter<Val>::a;
    using SupportIter<Val>::x;
    using SupportIter<Val>::s;
    using SupportIter<Val>::c;
    using SupportIter<Val>::p;
    using SupportIter<Val>::l;
    using SupportIter<Val>::u;
  public:
    /// Reset iterator to beginning and adjust \arg d accordingly
    bool reset(Val& d);
    /// Adjust \arg d and return true if next value still works
    bool adjust(Val& d);
  };


  /*
   * Support set
   *
   */
  forceinline
  SupportSet::SupportSet(void) {}
  forceinline void
  SupportSet::init(Region& r, unsigned int n) {
    bs.init(r,n);
  }
  forceinline void
  SupportSet::support(unsigned int i) {
    bs.set(i);
  }
  forceinline bool
  SupportSet::supported(unsigned int i) const {
    return bs.get(i);
  }

  forceinline
  SupportSet::ResultIter::ResultIter(const SupportSet& s0, const IntView& x)
    : ViewValues<IntView>(x), s(s0), p(0) {
    while (ViewValues<IntView>::operator ()() && s.supported(p)) {
      ViewValues<IntView>::operator ++(); ++p;
    }
  }
  forceinline void
  SupportSet::ResultIter::operator ++(void) {
    do {
      ViewValues<IntView>::operator ++(); ++p;
    } while (ViewValues<IntView>::operator ()() && s.supported(p));
  }


  forceinline ModEvent
  SupportSet::tell(Space& home, IntView& x) const {
    switch (bs.status()) {
    case Support::BSS_NONE:
      return ME_INT_FAILED;
    case Support::BSS_ALL:
      return ME_INT_NONE;
    case Support::BSS_SOME:
      {
        ResultIter i(*this,x);
        return x.minus_v(home,i);
      }
    default:
      GECODE_NEVER;
    }
    return ME_INT_NONE;
  }


  /*
   * Base-class for support-based iterator
   *
   */
  template<class Val>
  forceinline void
  SupportIter<Val>::init(Region& r,
                         int a0, const IntView& x0, Val l0, Val u0) {
    a=a0; x=x0; l=l0; u=u0;
    s.init(r,x.size());
  }
  template<class Val>
  forceinline void
  SupportIter<Val>::support(void) {
    s.support(p);
  }
  template<class Val>
  forceinline ModEvent
  SupportIter<Val>::tell(Space& home) {
    return s.tell(home,x);
  }


  /*
   * Support-based iterator for positive view
   *
   */
  template<class Val>
  forceinline bool
  PosSupportIter<Val>::reset(Val& d) {
    // Way too small, no value can make it big enough
    if (d + static_cast<Val>(a)*x.max() < u)
      return false;
    // Restart iterator and position of values
    i.init(x.varimp()); p = 0;
    // Skip all ranges which are too small
    while (d + static_cast<Val>(a)*i.max() < u) {
      p += i.width(); ++i;
    }
    // There is at least one range left (check of max)
    assert(i());
    // Initialize current range and adjust value
    c = i.min();
    // Skip all values which are too small
    while (d + static_cast<Val>(a)*c < u) {
      p++; c++;
    }
    // Adjust to new value
    d += static_cast<Val>(a) * c;
    return true;
  }
  template<class Val>
  forceinline bool
  PosSupportIter<Val>::adjust(Val& d) {
    // Current value
    Val v = static_cast<Val>(a) * c;
    // Subtract current value from d
    d -= v;
    // Move to next position (number of value)
    p++;
    // Still in the same range
    if (c < i.max()) {
      // Decrement current values
      c += 1; v += a;
    } else {
      // Go to next range
      ++i;
      if (!i())
        return false;
      c = i.min(); v = static_cast<Val>(a) * c;
    }
    // Is d with the current value too large?
    if (d + v > l)
      return false;
    // Update d
    d += v;
    return true;
  }


  /*
   * Support-based iterator for negative view
   *
   */
  template<class Val>
  forceinline bool
  NegSupportIter<Val>::reset(Val& d) {
    // Way too small, no value can make it big enough
    if (d + static_cast<Val>(a)*x.min() < u)
      return false;
    // Restart iterator and position of values
    i.init(x.varimp()); p = x.size()-1;
    // Skip all ranges which are too small
    while (d + static_cast<Val>(a)*i.min() < u) {
      p -= i.width(); ++i;
    }
    // There is at least one range left (check of max)
    assert(i());
    // Initialize current range
    c = i.max();
    // Skip all values which are too small
    while (d + static_cast<Val>(a)*c < u) {
      p--; c--;
    }
    // Adjust to new value
    d += static_cast<Val>(a) * c;
    return true;
  }
  template<class Val>
  forceinline bool
  NegSupportIter<Val>::adjust(Val& d) {
    // Current value
    Val v = static_cast<Val>(a) * c;
    // Subtract current value from d
    d -= v;
    // Move to next position (number of value)
    p--;
    // Still in the same range
    if (c > i.min()) {
      // Decrement current values
      c -= 1; v -= a;
    } else {
      // Go to next range
      ++i;
      if (!i())
        return false;
      c = i.max(); v = static_cast<Val>(a) * c;
    }
    // Is d with the current value too large?
    if (d + v > l)
      return false;
    // Update d
    d += v;
    return true;
  }



  /*
   * The domain consisten equality propagator
   *
   */
  template<class Val, class View>
  forceinline
  DomEq<Val,View>::DomEq(Home home,
                         ViewArray<View >& x, ViewArray<View >& y,
                         Val c)
    : Lin<Val,View,View,PC_INT_DOM>(home,x,y,c) {}

  template<class Val, class View>
  ExecStatus
  DomEq<Val,View>::post(Home home,
                        ViewArray<View>& x, ViewArray<View>& y,
                        Val c) {
    (void) new (home) DomEq<Val,View>(home,x,y,c);
    return ES_OK;
  }

  template<class Val, class View>
  forceinline
  DomEq<Val,View>::DomEq(Space& home, bool share, DomEq<Val,View>& p)
    : Lin<Val,View,View,PC_INT_DOM>(home,share,p) {}

  template<class Val, class View>
  Actor*
  DomEq<Val,View>::copy(Space& home, bool share) {
    return new (home) DomEq<Val,View>(home,share,*this);
  }

  template<class Val, class View>
  PropCost
  DomEq<Val,View>::cost(const Space&, const ModEventDelta& med) const {
    if (View::me(med) != ME_INT_DOM)
      return PropCost::linear(PropCost::LO, x.size()+y.size());
    else
      return PropCost::crazy(PropCost::HI, x.size()+y.size());
  }

  template<class Val, class View>
  ExecStatus
  DomEq<Val,View>::propagate(Space& home, const ModEventDelta& med) {
    if (View::me(med) != ME_INT_DOM) {
      ExecStatus es = prop_bnd<Val,View,View>(home,med,*this,x,y,c);
      GECODE_ES_CHECK(es);
      return home.ES_FIX_PARTIAL(*this,View::med(ME_INT_DOM));
    }

    // Value of equation for partial assignment
    Val d = -c;

    int n = x.size();
    int m = y.size();

    Region r(home);
    // Create support-base iterators
    PosSupportIter<Val>* xp = r.alloc<PosSupportIter<Val> >(n);
    NegSupportIter<Val>* yp = r.alloc<NegSupportIter<Val> >(m);

    // Initialize views for assignments
    {
      Val l = 0;
      Val u = 0;
      for (int j=m; j--; ) {
        yp[j].init(r,-y[j].scale(),y[j].base(),l,u);
        l += y[j].max(); u += y[j].min();
      }
      for (int i=n; i--; ) {
        xp[i].init(r,x[i].scale(),x[i].base(),l,u);
        l -= x[i].min(); u -= x[i].max();
      }
    }

    // Collect support information by iterating assignments
    {
      // Force reset of all iterators in first round
      int i = 0;
      int j = 0;

    next_i:
      // Reset all iterators for positive views and update d
      while (i<n) {
        if (!xp[i].reset(d)) goto prev_i;
        i++;
      }
    next_j:
      // Reset all iterators for negative views and update d
      while (j<m) {
        if (!yp[j].reset(d)) goto prev_j;
        j++;
      }
      // Check whether current assignment is solution
      if (d == 0) {
        // Record support
        for (int is=n; is--; ) xp[is].support();
        for (int js=m; js--; ) yp[js].support();
      }
    prev_j:
      // Try iterating to next assignment: negative views
      while (j>0) {
        if (yp[j-1].adjust(d)) goto next_j;
        j--;
      }
    prev_i:
      // Try iterating to next assignment: positive views
      while (i>0) {
        if (xp[i-1].adjust(d)) goto next_i;
        i--;
      }
    }

    // Tell back new variable domains
    bool assigned = true;
    for (int i=n; i--; ) {
      GECODE_ME_CHECK(xp[i].tell(home));
      if (!x[i].assigned())
        assigned = false;
    }
    for (int j=m; j--; ) {
      GECODE_ME_CHECK(yp[j].tell(home));
      if (!y[j].assigned())
        assigned = false;
    }
    if (assigned)
      return home.ES_SUBSUMED(*this);
    return ES_FIX;
  }

}}}

// STATISTICS: int-prop
