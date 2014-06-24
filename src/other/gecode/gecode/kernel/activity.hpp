/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2012
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

  /**
   * \brief Class for activity management
   *
   */
  class Activity {
  protected:
    template<class View>
    class Recorder;
    /// Object for storing activity values
    class Storage {
    public:
      /// Mutex to synchronize globally shared access
      Support::Mutex m;
      /// How many references exist for this object
      unsigned int use_cnt;
      /// Activity values
      double* a;
      /// Number of activity values
      int n;
      /// Decay factor
      double d;
      /// Allocate activity values
      template<class View>
      Storage(Home home, ViewArray<View>& x, double d,
              typename BranchTraits<typename View::VarType>::Merit bm);
      /// Delete object
      ~Storage(void);
      /// Allocate memory from heap
      static void* operator new(size_t s);
      /// Free memory allocated from heap
      static void  operator delete(void* p);
    };

    /// Pointer to storage object
    Storage* storage;
    /// Update activity value at position \a i
    void update(int i);
    /// Decay activity value at position \a i
    void decay(int i);
    /// Acquire mutex
    void acquire(void);
    /// Release mutex
    void release(void);
  public:
    /// \name Constructors and initialization
    //@{
    /**
     * \brief Construct as not yet intialized
     *
     * The only member functions that can be used on a constructed but not
     * yet initialized activity storage is init and the assignment operator.
     *
     */
    Activity(void);
    /// Copy constructor
    GECODE_KERNEL_EXPORT
    Activity(const Activity& a);
    /// Assignment operator
    GECODE_KERNEL_EXPORT
    Activity& operator =(const Activity& a);
    /// Initialize for views \a x and decay factor \a d and activity as defined by \a bm
    template<class View>
    Activity(Home home, ViewArray<View>& x, double d,
             typename BranchTraits<typename View::VarType>::Merit bm);
    /// Initialize for views \a x and decay factor \a d and activity as defined by \a bm
    template<class View>
    void init(Home home, ViewArray<View>& x, double d,
             typename BranchTraits<typename View::VarType>::Merit bm);
    /// Test whether already initialized
    bool initialized(void) const;
    /// Set activity to \a a
    GECODE_KERNEL_EXPORT
    void set(Space& home, double a=0.0);
    /// Default (empty) activity information
    GECODE_KERNEL_EXPORT static const Activity def;
    //@}

    /// \name Update and delete activity information
    //@{
    /// Updating during cloning
    GECODE_KERNEL_EXPORT
    void update(Space& home, bool share, Activity& a);
    /// Destructor
    GECODE_KERNEL_EXPORT
    ~Activity(void);
    //@}
    
    /// \name Information access
    //@{
    /// Return activity value at position \a i
    double operator [](int i) const;
    /// Return number of activity values
    int size(void) const;
    //@}

    /// \name Decay factor for aging
    //@{
    /// Set decay factor to \a d
    GECODE_KERNEL_EXPORT
    void decay(Space& home, double d);
    /// Return decay factor
    GECODE_KERNEL_EXPORT
    double decay(const Space& home) const;
    //@}
  };

  /// Propagator for recording activity information
  template<class View>
  class Activity::Recorder : public NaryPropagator<View,PC_GEN_NONE> {
  protected:
    using NaryPropagator<View,PC_GEN_NONE>::x;
    /// Advisor with index and change information
    class Idx : public Advisor {
    protected:
      /// Index and mark information
      int _info;
    public:
      /// Constructor for creation
      Idx(Space& home, Propagator& p, Council<Idx>& c, int i);
      /// Constructor for cloning \a a
      Idx(Space& home, bool share, Idx& a);
      /// Mark advisor as modified
      void mark(void);
      /// Mark advisor as unmodified
      void unmark(void);
      /// Whether advisor's view has been marked
      bool marked(void) const;
      /// Get index of view
      int idx(void) const;
    };
    /// Access to activity information
    Activity a;
    /// The advisor council
    Council<Idx> c;
    /// Constructor for cloning \a p
    Recorder(Space& home, bool share, Recorder<View>& p);
  public:
    /// Constructor for creation
    Recorder(Home home, ViewArray<View>& x, Activity& a);
    /// Copy propagator during cloning
    virtual Propagator* copy(Space& home, bool share);
    /// Cost function (crazy so that propagator is likely to run last)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Give advice to propagator
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
    /// Post activity recorder propagator
    static ExecStatus post(Home home, ViewArray<View>& x, Activity& a);
  };
    
  /**
   * \brief Print activity values enclosed in curly brackets
   * \relates Activity
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
             const Activity& a);


  /*
   * Advisor for activity recorder
   *
   */
  template<class View>
  forceinline
  Activity::Recorder<View>::Idx::Idx(Space& home, Propagator& p, 
                                     Council<Idx>& c, int i)
    : Advisor(home,p,c), _info(i << 1) {}
  template<class View>
  forceinline
  Activity::Recorder<View>::Idx::Idx(Space& home, bool share, Idx& a)
    : Advisor(home,share,a), _info(a._info) {
  }
  template<class View>
  forceinline void
  Activity::Recorder<View>::Idx::mark(void) {
    _info |= 1;
  }
  template<class View>
  forceinline void
  Activity::Recorder<View>::Idx::unmark(void) {
    _info &= ~1;
  }
  template<class View>
  forceinline bool
  Activity::Recorder<View>::Idx::marked(void) const {
    return (_info & 1) != 0;
  }
  template<class View>
  forceinline int
  Activity::Recorder<View>::Idx::idx(void) const {
    return _info >> 1;
  }



  /*
   * Posting of activity recorder propagator
   *
   */
  template<class View>
  forceinline
  Activity::Recorder<View>::Recorder(Home home, ViewArray<View>& x, 
                                     Activity& a0)
    : NaryPropagator<View,PC_GEN_NONE>(home,x), a(a0), c(home) {
    home.notice(*this,AP_DISPOSE);
    for (int i=x.size(); i--; )
      if (!x[i].assigned())
        x[i].subscribe(home,*new (home) Idx(home,*this,c,i));
  }

  template<class View>
  forceinline ExecStatus
  Activity::Recorder<View>::post(Home home, ViewArray<View>& x, 
                                 Activity& a) {
    (void) new (home) Recorder<View>(home,x,a);
    return ES_OK;
  }


  /*
   * Activity value storage
   *
   */
  forceinline void*
  Activity::Storage::operator new(size_t s) {
    return Gecode::heap.ralloc(s);
  }
  forceinline void
  Activity::Storage::operator delete(void* p) {
    Gecode::heap.rfree(p);
  }
  template<class View>
  forceinline
  Activity::Storage::Storage(Home home, ViewArray<View>& x, double d0,
                             typename 
                             BranchTraits<typename View::VarType>::Merit bm)
    : use_cnt(1), a(heap.alloc<double>(x.size())), n(x.size()), d(d0) {
    if (bm != NULL)
      for (int i=n; i--; ) {
        typename View::VarType xi(x[i].varimp());
        a[i] = bm(home,xi,i);
      }
    else
      for (int i=n; i--; )
        a[i] = 0.0;
  }
  forceinline
  Activity::Storage::~Storage(void) {
    heap.free<double>(a,n);
  }


  /*
   * Activity
   *
   */

  forceinline void
  Activity::update(int i) {
    assert(storage != NULL);
    assert((i >= 0) && (i < storage->n));
    storage->a[i] += 1.0;
  }
  forceinline void
  Activity::decay(int i) {
    assert(storage != NULL);
    assert((i >= 0) && (i < storage->n));
    storage->a[i] *= storage->d;
  }
  forceinline double
  Activity::operator [](int i) const {
    assert(storage != NULL);
    assert((i >= 0) && (i < storage->n));
    return storage->a[i];
  }
  forceinline int
  Activity::size(void) const {
    return storage->n;
  }
  forceinline void
  Activity::acquire(void) {
    storage->m.acquire();
  }
  forceinline void
  Activity::release(void) {
    storage->m.release();
  }


  forceinline
  Activity::Activity(void) : storage(NULL) {}

  forceinline bool
  Activity::initialized(void) const {
    return storage != NULL;
  }

  template<class View>
  forceinline
  Activity::Activity(Home home, ViewArray<View>& x, double d,
                     typename BranchTraits<typename View::VarType>::Merit bm) {
    assert(storage == NULL);
    storage = new Storage(home,x,d,bm);
    (void) Recorder<View>::post(home,x,*this);
  }
  template<class View>
  forceinline void
  Activity::init(Home home, ViewArray<View>& x, double d,
                 typename BranchTraits<typename View::VarType>::Merit bm) {
    assert(storage == NULL);
    storage = new Storage(home,x,d,bm);
    (void) Recorder<View>::post(home,x,*this);
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
              const Activity& a) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << '{';
    if (a.size() > 0) {
      s << a[0];
      for (int i=1; i<a.size(); i++)
        s << ", " << a[i];
    }
    s << '}';
    return os << s.str();
  }
  

  /*
   * Propagation for activity recorder
   *
   */
  template<class View>
  forceinline
  Activity::Recorder<View>::Recorder(Space& home, bool share,
                                     Recorder<View>& p) 
    : NaryPropagator<View,PC_GEN_NONE>(home,share,p) {
    a.update(home, share, p.a);
    c.update(home, share, p.c);
  }

  template<class View>
  Propagator*
  Activity::Recorder<View>::copy(Space& home, bool share) {
    return new (home) Recorder<View>(home, share, *this);
  }

  template<class View>
  inline size_t
  Activity::Recorder<View>::dispose(Space& home) {
    // Delete access to activity information
    home.ignore(*this,AP_DISPOSE);
    a.~Activity();
    // Cancel remaining advisors
    for (Advisors<Idx> as(c); as(); ++as)
      x[as.advisor().idx()].cancel(home,as.advisor());
    c.dispose(home);
    (void) NaryPropagator<View,PC_GEN_NONE>::dispose(home);
    return sizeof(*this);
  }

  template<class View>
  PropCost 
  Activity::Recorder<View>::cost(const Space&, const ModEventDelta&) const {
    return PropCost::crazy(PropCost::HI,1000);
  }

  template<class View>
  ExecStatus
  Activity::Recorder<View>::advise(Space&, Advisor& a, const Delta&) {
    static_cast<Idx&>(a).mark();
    return ES_NOFIX;
  }

  template<class View>
  ExecStatus
  Activity::Recorder<View>::propagate(Space& home, const ModEventDelta&) {
    // Lock activity information
    a.acquire();
    for (Advisors<Idx> as(c); as(); ++as) {
      int i = as.advisor().idx();
      if (as.advisor().marked()) {
        as.advisor().unmark();
        a.update(i);
        if (x[i].assigned())
          as.advisor().dispose(home,c);
      } else {
        assert(!x[i].assigned());
        a.decay(i);
      }
    }
    a.release();
    return c.empty() ? home.ES_SUBSUMED(*this) : ES_FIX;
  }
  
}

// STATISTICS: kernel-branch
