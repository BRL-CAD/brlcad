/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Gregory Crosswhite <gcross@phys.washington.edu>
 *
 *  Copyright:
 *     Gregory Crosswhite, 2011
 *     Christian Schulte, 2003
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

#include <cstdarg>
#include <iostream>
#include <iterator>
#include <vector>
#include <sstream>

namespace Gecode {

  template<class Var> class VarArray;
  template<class Var> class VarArgArray;

  /** \brief Traits of arrays in %Gecode
   *
   * This class collects the traits of an array in Gecode.
   * The traits used are the following.
   *     - <code>typedef Type StorageType</code>  where \c Type is the type
   *       of an appropriate storage type for this array.
   *     - <code>typedef Type ValueType</code>  where \c Type is the type
   *       of the elements of this array.
   *     - <code>typedef Type ArgsType</code>  where \c Type is the type
   *       of the appropriate Args-array type (e.g., \c BoolVarArgs if \c A is
   *       \c BoolVarArray).
   */
  template<class A>
  class ArrayTraits {};

  /**
   * \brief %Variable arrays
   *
   * %Variable arrays store variables. They are typically used
   * for storing the variables being part of a solution.
   *
   * Never use them for temporary purposes, use argument arrays
   * instead.
   *
   * %Variable arrays can be enlarged dynamically. For memory efficiency, the
   * initial array is allocated in the space. When adding variables, it is
   * automatically resized and allocated on the heap.
   *
   * \ingroup TaskVar
   */
  template<class Var>
  class VarArray {
  protected:
    /// Number of variables (size)
    int n;
    /// Array of variables
    Var* x;
  public:
    /// \name Associated types
    //@{
    /// Type of the variable stored in this array
    typedef Var value_type;
    /// Type of a reference to the value type
    typedef Var& reference;
    /// Type of a constant reference to the value type
    typedef const Var& const_reference;
    /// Type of a pointer to the value type
    typedef Var* pointer;
    /// Type of a read-only pointer to the value type
    typedef const Var* const_pointer;
    /// Type of the iterator used to iterate through this array's elements
    typedef Var* iterator;
    /// Type of the iterator used to iterate read-only through this array's elements
    typedef const Var* const_iterator;
    /// Type of the iterator used to iterate backwards through this array's elements
    typedef std::reverse_iterator<Var*> reverse_iterator;
    /// Type of the iterator used to iterate backwards and read-only through this array's elements
    typedef std::reverse_iterator<const Var*> const_reverse_iterator;
    //@}

    //@{
    /// \name Constructors and initialization
    //@{
    /// Default constructor (array of size 0)
    VarArray(void);
    /// Allocate array with \a m variables
    VarArray(Space& home, int m);
    /// Initialize from variable argument array \a a (copy elements)
    VarArray(Space& home, const VarArgArray<Var>&);
    /// Initialize from variable array \a a (share elements)
    VarArray(const VarArray<Var>& a);
    /// Initialize from variable array \a a (share elements)
    const VarArray<Var>& operator =(const VarArray<Var>& a);
    //@}

    /// \name Array size
    //@{
    /// Return size of array (number of elements)
    int size(void) const;
    //@}

    /// \name Array elements
    //@{
    /// Return variable at position \a i
    Var& operator [](int i);
    /// Return variable at position \a i
    const Var& operator [](int i) const;
    /** Return slice \f$y\f$ of length at most \a n such that forall \f$0\leq i<n\f$, \f$y_i=x_{\text{start}+i\cdot\text{inc}}\f$
     *
     * If \a n is -1, then all possible elements starting from \a start
     * with increment \a inc are returned.
     */
    typename ArrayTraits<VarArgArray<Var> >::ArgsType
    slice(int start, int inc=1, int n=-1);
    //@}

    /// \name Array iteration
    //@{
    /// Return an iterator at the beginning of the array
    iterator begin(void);
    /// Return a read-only iterator at the beginning of the array
    const_iterator begin(void) const;
    /// Return an iterator past the end of the array
    iterator end(void);
    /// Return a read-only iterator past the end of the array
    const_iterator end(void) const;
    /// Return a reverse iterator at the end of the array
    reverse_iterator rbegin(void);
    /// Return a reverse and read-only iterator at the end of the array
    const_reverse_iterator rbegin(void) const;
    /// Return a reverse iterator past the beginning of the array
    reverse_iterator rend(void);
    /// Return a reverse and read-only iterator past the beginning of the array
    const_reverse_iterator rend(void) const;
    //@}

    /// Test if all variables are assigned
    bool assigned(void) const;

    /// \name Cloning
    //@{
    /**
     * \brief Update array to be a clone of array \a a
     *
     * If \a share is true, sharing is retained for all shared
     * data structures. Otherwise, for each of them an independent
     * copy is created.
     */
    void update(Space&, bool share, VarArray<Var>& a);
    //@}
  private:
    static void* operator new(size_t);
    static void  operator delete(void*,size_t);
  };

  /** Concatenate \a x and \a y and return result
   * \relates VarArray
   */
  template<class T>
  typename ArrayTraits<VarArray<T> >::ArgsType
  operator +(const VarArray<T>& x, const VarArgArray<T>& y);

  /** Concatenate \a x and \a y and return result
   * \relates VarArray
   */
  template<class T>
  typename ArrayTraits<VarArray<T> >::ArgsType
  operator +(const VarArray<T>& x, const VarArray<T>& y);

  /** Concatenate \a x and \a y and return result
   * \relates VarArray
   */
  template<class T>
  typename ArrayTraits<VarArray<T> >::ArgsType
  operator +(const VarArgArray<T>& x, const VarArray<T>& y);

  /** Concatenate \a x and \a y and return result
   * \relates VarArray
   */
  template<class T>
  typename ArrayTraits<VarArray<T> >::ArgsType
  operator +(const VarArray<T>& x, const T& y);

  /** Concatenate \a x and \a y and return result
   * \relates VarArray
   */
  template<class T>
  typename ArrayTraits<VarArray<T> >::ArgsType
  operator +(const T& x, const VarArray<T>& y);

  /**
   * \brief View arrays
   *
   * View arrays store views. They are typically used for storing the
   * views with which propagators and branchers compute.
   * \ingroup TaskActor
   */

  template<class View>
  class ViewArray {
  private:
    /// Number of views (size)
    int  n;
    /// Views
    View* x;
    /// Sort order for views
    template<class X>
    class ViewLess {
    public:
      bool operator ()(const X&, const X&);
    };
    /// Sort \a n views \a x according to \a ViewLess
    static void sort(View* x, int n);
  public:
    /// \name Associated types
    //@{
    /// Type of the view stored in this array
    typedef View value_type;
    /// Type of a reference to the value type
    typedef View& reference;
    /// Type of a constant reference to the value type
    typedef const View& const_reference;
    /// Type of a pointer to the value type
    typedef View* pointer;
    /// Type of a read-only pointer to the value type
    typedef const View* const_pointer;
    /// Type of the iterator used to iterate through this array's elements
    typedef View* iterator;
    /// Type of the iterator used to iterate read-only through this array's elements
    typedef const View* const_iterator;
    /// Type of the iterator used to iterate backwards through this array's elements
    typedef std::reverse_iterator<View*> reverse_iterator;
    /// Type of the iterator used to iterate backwards and read-only through this array's elements
    typedef std::reverse_iterator<const View*> const_reverse_iterator;
    //@}

    /// \name Constructors and initialization
    //@{
    /// Default constructor (array of size 0)
    ViewArray(void);
    /// Allocate array with \a m views
    ViewArray(Space& home, int m);
    /// Allocate array with \a m views
    ViewArray(Region& r, int m);
    /// Initialize from view array \a a (share elements)
    ViewArray(const ViewArray<View>& a);
    /// Initialize from view array \a a (copy elements)
    ViewArray(Space& home, const ViewArray<View>& a);
    /// Initialize from view array \a a (copy elements)
    ViewArray(Region& r, const ViewArray<View>& a);
    /// Initialize from view array \a a (share elements)
    const ViewArray<View>& operator =(const ViewArray<View>& a);
    /**
     * \brief Initialize from variable argument array \a a (copy elements)
     *
     * Note that the view type \a View must provide a constructor
     * for the associated \a Var type.
     */
    template<class Var>
    ViewArray(Space& home, const VarArgArray<Var>& a)
      : n(a.size()) {
      // This may not be in the hpp file (to satisfy the MS compiler)
      if (n>0) {
        x = home.alloc<View>(n);
        for (int i=n; i--; )
          x[i]=a[i];
      } else {
        x = NULL;
      }
    }
    /**
     * \brief Initialize from variable argument array \a a (copy elements)
     *
     * Note that the view type \a View must provide a constructor
     * for the associated \a Var type.
     */
    template<class Var>
    ViewArray(Region& r, const VarArgArray<Var>& a)
      : n(a.size()) {
      // This may not be in the hpp file (to satisfy the MS compiler)
      if (n>0) {
        x = r.alloc<View>(n);
        for (int i=n; i--; )
          x[i]=a[i];
      } else {
        x = NULL;
      }
    }
    //@}

    /// \name Array size
    //@{
    /// Return size of array (number of elements)
    int size(void) const;
    /// Decrease size of array (number of elements)
    void size(int n);
    //@}

    /// \name Array elements
    //@{
    /// Return view at position \a i
    View& operator [](int i);
    /// Return view at position \a i
    const View& operator [](int i) const;
    //@}

    /// \name Array iteration
    //@{
    /// Return an iterator at the beginning of the array
    iterator begin(void);
    /// Return a read-only iterator at the beginning of the array
    const_iterator begin(void) const;
    /// Return an iterator past the end of the array
    iterator end(void);
    /// Return a read-only iterator past the end of the array
    const_iterator end(void) const;
    /// Return a reverse iterator at the end of the array
    reverse_iterator rbegin(void);
    /// Return a reverse and read-only iterator at the end of the array
    const_reverse_iterator rbegin(void) const;
    /// Return a reverse iterator past the beginning of the array
    reverse_iterator rend(void);
    /// Return a reverse and read-only iterator past the beginning of the array
    const_reverse_iterator rend(void) const;
    //@}

    /// \name Dependencies
    //@{
    /**
     * \brief Subscribe propagator \a p with propagation condition \a pc to variable
     *
     * In case \a process is false, the propagator is just subscribed but
     * not processed for execution (this must be used when creating
     * subscriptions during propagation).
     */
    void subscribe(Space& home, Propagator& p, PropCond pc, bool process=true);
    /// Cancel subscription of propagator \a p with propagation condition \a pc to all views
    void cancel(Space& home, Propagator& p, PropCond pc);
    /// Subscribe advisor \a a to variable
    void subscribe(Space& home, Advisor& a);
    /// Cancel subscription of advisor \a a
    void cancel(Space& home, Advisor& a);
    //@}

    /// \name Cloning
    //@{
    /**
     * \brief Update array to be a clone of array \a a
     *
     * If \a share is true, sharing is retained for all shared
     * data structures. Otherwise, for each of them an independent
     * copy is created.
     */
    void update(Space&, bool share, ViewArray<View>& a);
    //@}


    /// \name Moving elements
    //@{
    /// Move view from position 0 to position \a i (shift elements to the left)
    void move_fst(int i);
    /// Move view from position \c size()-1 to position \a i (truncate array by one)
    void move_lst(int i);
    /** \brief Move view from position 0 to position \a i (shift elements to the left)
     *
     * Before moving, cancel subscription of propagator \a p with
     * propagation condition \a pc to view at position \a i.
     */
    void move_fst(int i, Space& home, Propagator& p, PropCond pc);
    /** \brief Move view from position \c size()-1 to position \a i (truncate array by one)
     *
     * Before moving, cancel subscription of propagator \a p with
     * propagation condition \a pc to view at position \a i.
     */
    void move_lst(int i, Space& home, Propagator& p, PropCond pc);
    /** \brief Move view from position 0 to position \a i (shift elements to the left)
     *
     * Before moving, cancel subscription of advisor \a a
     * to view at position \a i.
     */
    void move_fst(int i, Space& home, Advisor& a);
    /** \brief Move view from position \c size()-1 to position \a i (truncate array by one)
     *
     * Before moving, cancel subscription of advisor \a a to view
     * at position \a i.
     */
    void move_lst(int i, Space& home, Advisor& a);
    //@}

    /// \name Dropping elements
    //@{
    /// Drop views from positions 0 to \a i-1 from array
    void drop_fst(int i);
    /// Drop views from positions \a i+1 to \c size()-1 from array
    void drop_lst(int i);
    /** \brief Drop views from positions 0 to \a i-1 from array
     *
     * Before moving, cancel subscription of propagator \a p with
     * propagation condition \a pc to views at positions 0 to \a i-1.
     */
    void drop_fst(int i, Space& home, Propagator& p, PropCond pc);
    /** \brief Drop assigned views from positions \a i+1 to \c size()-1 from array
     *
     * Before moving, cancel subscription of propagator \a p with
     * propagation condition \a pc to views at positions \a i+1 to
     * \c size()-1.
     */
    void drop_lst(int i, Space& home, Propagator& p, PropCond pc);
    /** \brief Drop views from positions 0 to \a i-1 from array
     *
     * Before moving, cancel subscription of advisor \a a to views at
     * positions 0 to \a i-1.
     */
    void drop_fst(int i, Space& home, Advisor& a);
    /** \brief Drop assigned views from positions \a i+1 to \c size()-1 from array
     *
     * Before moving, cancel subscription of advisor \a a to views at
     * positions \a i+1 to \c size()-1.
     */
    void drop_lst(int i, Space& home, Advisor& a);
    //@}

    /// Test if all variables are assigned
    bool assigned(void) const;

    /// \name View equality
    //@{
    /**
     * \brief Test whether array has multiple occurence of the same view
     *
     * Note that assigned views are ignored.
     */
    bool same(const Space& home) const;
    /**
     * \brief Test whether array contains a view being the same as \a y
     *
     * Note that assigned views are ignored.
     */
    bool same(const Space& home, const View& y) const;
    /// Remove all duplicate views from array (changes element order)
    void unique(const Space& home);
    //@}

    /// \name View sharing
    //@{
    /**
     * \brief Test whether array contains shared views
     *
     * Note that assigned views are ignored.
     */
    bool shared(const Space& home) const;
    /**
     * \brief Test whether array contains a view being shared with \a y
     *
     * Note that assigned views are ignored.
     */
    template<class ViewY>
    bool shared(const Space& home, const ViewY& y) const;
    /**
     * \brief Test whether array together with array \a y contains shared views
     *
     * Note that assigned views are ignored.
     */
    template<class ViewY>
    bool shared(const Space& home, const ViewArray<ViewY>& y) const;
    //@}

  private:
    static void* operator new(size_t);
    static void  operator delete(void*,size_t);
  };

  /**
   * \brief Base-class for argument arrays
   *
   * Argument arrays are used as convenient mechanism of passing arguments
   * when calling functions as they combine both the size and the elements
   * of an array. For a small number of elements, memory is allocated by
   * creating an argument array object. Otherwise the memory is allocated
   * from the heap.
   *
   * This base-class is not to be used directly, use PrimArgArray for
   * argument arrays of primitive types, VarArgArray for argument
   * arrays storing variables, and ArgArray for any other type.
   */
  template<class T>
  class ArgArrayBase {
  protected:
    /// Number of elements
    int n;
    /// Allocated size of the array
    int capacity;
    /// Element array
    T*  a;
    /// How many elements are possible inside array
    static const int onstack_size = 16;
    /// In-array storage for elements
    T onstack[onstack_size];
    /// Allocate memory for \a n elements
    T* allocate(int n);
    /// Resize to hold at least \a i additional elements
    void resize(int i);
    /// Return this array concatenated with \a x
    template<class A>
    A concat(const ArgArrayBase<T>& x) const;
    /// Return this array concatenated with \a x
    template<class A>
    A concat(const T& x) const;
    /// Insert a new element \a x at the end of the array (increase size by 1)
    template<class A>
    A& append(const T& x);
    /// Append \a x to the end of the array
    template<class A>
    A& append(const ArgArrayBase<T>& x);
    /** Return slice \f$y\f$ of length at most \a n such that forall \f$0\leq i<n\f$, \f$y_i=x_{\text{start}+i\cdot\text{inc}}\f$
     *
     * If \a n is -1, then all possible elements starting from \a start
     * with increment \a inc are returned.
     */
    template<class A>
    A slice(int start, int inc=1, int n=-1);
  public:
    /// \name Associated types
    //@{
    /// Type of the view stored in this array
    typedef T value_type;
    /// Type of a reference to the value type
    typedef T& reference;
    /// Type of a constant reference to the value type
    typedef const T& const_reference;
    /// Type of a pointer to the value type
    typedef T* pointer;
    /// Type of a read-only pointer to the value type
    typedef const T* const_pointer;
    /// Type of the iterator used to iterate through this array's elements
    typedef T* iterator;
    /// Type of the iterator used to iterate read-only through this array's elements
    typedef const T* const_iterator;
    /// Type of the iterator used to iterate backwards through this array's elements
    typedef std::reverse_iterator<T*> reverse_iterator;
    /// Type of the iterator used to iterate backwards and read-only through this array's elements
    typedef std::reverse_iterator<const T*> const_reverse_iterator;
    //@}

    /// \name Constructors and initialization
    //@{
    /// Allocate empty array
    ArgArrayBase(void);
    /// Allocate array with \a n elements
    explicit ArgArrayBase(int n);
    /// Initialize from argument array \a a (copy elements)
    ArgArrayBase(const ArgArrayBase<T>& a);
    /// Initialize from view array \a a (copy elements)
    const ArgArrayBase<T>& operator =(const ArgArrayBase<T>& a);
    /// Initialize from vector \a a
    ArgArrayBase(const std::vector<T>& a);
    /// Initialize from InputIterator \a begin and \a end
    template<class InputIterator>
    ArgArrayBase(InputIterator first, InputIterator last);
    //@}

    /// \name Array size
    //@{
    /// Return size of array (number of elements)
    int size(void) const;
    //@}

    /// \name Array elements
    //@{
    /// Return element at position \a i
    T& operator [](int i);
    /// Return element at position \a i
    const T& operator [](int i) const;
    //@}

    /// \name Array iteration
    //@{
    /// Return an iterator at the beginning of the array
    iterator begin(void);
    /// Return a read-only iterator at the beginning of the array
    const_iterator begin(void) const;
    /// Return an iterator past the end of the array
    iterator end(void);
    /// Return a read-only iterator past the end of the array
    const_iterator end(void) const;
    /// Return a reverse iterator at the end of the array
    reverse_iterator rbegin(void);
    /// Return a reverse and read-only iterator at the end of the array
    const_reverse_iterator rbegin(void) const;
    /// Return a reverse iterator past the beginning of the array
    reverse_iterator rend(void);
    /// Return a reverse and read-only iterator past the beginning of the array
    const_reverse_iterator rend(void) const;
    //@}

    /// \name Destructor
    //@{
    /// Destructor
    ~ArgArrayBase(void);
    //@}
  private:
    static void* operator new(size_t);
    static void  operator delete(void*,size_t);
  };

  template<class> class PrimArgArray;

  /** Concatenate \a x and \a y and return result
   * \relates PrimArgArray
   */
  template<class T>
  typename ArrayTraits<PrimArgArray<T> >::ArgsType
  operator +(const PrimArgArray<T>& x, const PrimArgArray<T>& y);

  /** Concatenate \a x and \a y and return result
   * \relates PrimArgArray
   */
  template<class T>
  typename ArrayTraits<PrimArgArray<T> >::ArgsType
  operator +(const PrimArgArray<T>& x, const T& y);

  /** Concatenate \a x and \a y and return result
   * \relates PrimArgArray
   */
  template<class T>
  typename ArrayTraits<PrimArgArray<T> >::ArgsType
  operator +(const T& x, const PrimArgArray<T>& y);

  /**
   * \brief Argument array for primtive types
   *
   * Argument arrays are used as convenient mechanism of passing arguments
   * when calling functions as they combine both the size and the elements
   * of an array. For a small number of elements, memory is allocated by
   * creating an argument array object. Otherwise the memory is allocated
   * from the heap.
   *
   * \ingroup TaskVar
   */
  template<class T>
  class PrimArgArray : public ArgArrayBase<T> {
  protected:
    using ArgArrayBase<T>::a;
  public:
    using ArgArrayBase<T>::size;
    /// \name Constructors and initialization
    //@{
    /// Allocate empty array
    PrimArgArray(void);
    /// Allocate array with \a n elements
    explicit PrimArgArray(int n);
    /// Allocate array with \a n elements and initialize with \a e0, ...
    PrimArgArray(int n, T e0, ...);
    /// Allocate array with \a n elements and initialize with elements from array \a e
    PrimArgArray(int n, const T* e);
    /// Initialize from primitive argument array \a a (copy elements)
    PrimArgArray(const PrimArgArray<T>& a);
    /// Initialize from vector \a a
    PrimArgArray(const std::vector<T>& a);
    /// Initialize from InputIterator \a first and \a last
    template<class InputIterator>
    PrimArgArray(InputIterator first, InputIterator last);
    //@}
    /// \name Array elements
    //@{
    /** Return slice \f$y\f$ of length at most \a n such that forall \f$0\leq i<n\f$, \f$y_i=x_{\text{start}+i\cdot\text{inc}}\f$
     *
     * If \a n is -1, then all possible elements starting from \a start
     * with increment \a inc are returned.
     */
    typename ArrayTraits<PrimArgArray<T> >::ArgsType
    slice(int start, int inc=1, int n=-1);
    //@}
    /// \name Appending elements
    //@{
    /// Insert a new element \a x at the end of the array (increase size by 1)
    typename ArrayTraits<PrimArgArray<T> >::ArgsType&
    operator <<(const T& x);
    /// Append \a x to the end of the array
    typename ArrayTraits<PrimArgArray<T> >::ArgsType&
    operator <<(const PrimArgArray<T>& x);
    //@}

    friend typename ArrayTraits<PrimArgArray<T> >::ArgsType
    operator + <>(const PrimArgArray<T>& x, const PrimArgArray<T>& y);
    friend typename ArrayTraits<PrimArgArray<T> >::ArgsType
    operator + <>(const PrimArgArray<T>& x, const T& y);
    friend
    typename ArrayTraits<PrimArgArray<T> >::ArgsType
    operator + <>(const T& x, const PrimArgArray<T>& y);
  };

  template<class> class ArgArray;

  /** Concatenate \a x and \a y and return result
   * \relates ArgArray
   */
  template<class T>
  typename ArrayTraits<ArgArray<T> >::ArgsType
  operator +(const ArgArray<T>& x, const ArgArray<T>& y);

  /** Concatenate \a x and \a y and return result
   * \relates ArgArray
   */
  template<class T>
  typename ArrayTraits<ArgArray<T> >::ArgsType
  operator +(const ArgArray<T>& x, const T& y);

  /** Concatenate \a x and \a y and return result
   * \relates ArgArray
   */
  template<class T>
  typename ArrayTraits<ArgArray<T> >::ArgsType
  operator +(const T& x, const ArgArray<T>& y);

  /**
   * \brief Argument array for non-primitive types
   *
   * Argument arrays are used as convenient mechanism of passing arguments
   * when calling functions as they combine both the size and the elements
   * of an array. For a small number of elements, memory is allocated by
   * creating an argument array object. Otherwise the memory is allocated
   * from the heap.
   *
   * \ingroup TaskVar
   */
  template<class T>
  class ArgArray : public ArgArrayBase<T> {
  protected:
    using ArgArrayBase<T>::a;
  public:
    using ArgArrayBase<T>::size;
    /// \name Constructors and initialization
    //@{
    /// Allocate empty array
    ArgArray(void);
    /// Allocate array with \a n elements
    explicit ArgArray(int n);
    /// Allocate array with \a n elements and initialize with elements from array \a e
    ArgArray(int n, const T* e);
    /// Initialize from primitive argument array \a a (copy elements)
    ArgArray(const ArgArray<T>& a);
    /// Initialize from vector \a a
    ArgArray(const std::vector<T>& a);
    /// Initialize from InputIterator \a first and \a last
    template<class InputIterator>
    ArgArray(InputIterator first, InputIterator last);
    //@}
    /// \name Array elements
    //@{
    /// Return slice \f$y\f$ of length \a n such that forall \f$0\leq i<n\f$, \f$y_i=x_{\text{start}+i\cdot\text{inc}}\f$
    typename ArrayTraits<ArgArray<T> >::ArgsType
    slice(int start, int inc=1, int n=-1);
    //@}
    /// \name Appending elements
    //@{
    /// Insert a new element \a x at the end of the array (increase size by 1)
    typename ArrayTraits<ArgArray<T> >::ArgsType&
    operator <<(const T& x);
    /// Append \a x to the end of the array
    typename ArrayTraits<ArgArray<T> >::ArgsType&
    operator <<(const ArgArray<T>& x);
    //@}

    friend typename ArrayTraits<ArgArray<T> >::ArgsType
    operator + <>(const ArgArray<T>& x, const ArgArray<T>& y);
    friend typename ArrayTraits<ArgArray<T> >::ArgsType
    operator + <>(const ArgArray<T>& x, const T& y);
    friend
    typename ArrayTraits<ArgArray<T> >::ArgsType
    operator + <>(const T& x, const ArgArray<T>& y);
  };

  template<class> class VarArgArray;

  /** Concatenate \a x and \a y and return result
   * \relates ArgArray
   */
  template<class Var>
  typename ArrayTraits<VarArgArray<Var> >::ArgsType
  operator +(const VarArgArray<Var>& x, const VarArgArray<Var>& y);

  /** Concatenate \a x and \a y and return result
   * \relates ArgArray
   */
  template<class Var>
  typename ArrayTraits<VarArgArray<Var> >::ArgsType
  operator +(const VarArgArray<Var>& x, const Var& y);

  /** Concatenate \a x and \a y and return result
   * \relates ArgArray
   */
  template<class Var>
  typename ArrayTraits<VarArgArray<Var> >::ArgsType
  operator +(const Var& x, const VarArgArray<Var>& y);

  /**
   * \brief Argument array for variables
   *
   * Argument arrays are used as convenient mechanism of passing arguments
   * when calling functions as they combine both the size and the elements
   * of an array. For a small number of elements, memory is allocated by
   * creating an argument array object. Otherwise the memory is allocated
   * from the heap.
   *
   * \ingroup TaskVar
   */
  template<class Var>
  class VarArgArray : public ArgArrayBase<Var> {
  protected:
    using ArgArrayBase<Var>::a;
    using ArgArrayBase<Var>::n;
    /// Sort order for variables
    class VarLess {
    public:
      bool operator ()(const Var&, const Var&);
    };
  public:
    using ArgArrayBase<Var>::size;
    /// \name Constructors and initialization
    //@{
    /// Allocate empty array
    VarArgArray(void);
    /// Allocate array with \a n elements
    explicit VarArgArray(int n);
    /// Initialize from variable argument array \a a (copy elements)
    VarArgArray(const VarArgArray<Var>& a);
    /// Initialize from variable array \a a (copy elements)
    VarArgArray(const VarArray<Var>& a);
    /// Initialize from vector \a a
    VarArgArray(const std::vector<Var>& a);
    /// Initialize from InputIterator \a first and \a last
    template<class InputIterator>
    VarArgArray(InputIterator first, InputIterator last);
    //@}
    /// \name Array elements
    //@{
    /// Return slice \f$y\f$ of length \a n such that forall \f$0\leq i<n\f$, \f$y_i=x_{\text{start}+i\cdot\text{inc}}\f$
    typename ArrayTraits<VarArgArray<Var> >::ArgsType
    slice(int start, int inc=1, int n=-1);
    //@}
    /// \name Appending elements
    //@{
    /// Insert a new element \a x at the end of the array (increase size by 1)
    typename ArrayTraits<VarArgArray<Var> >::ArgsType&
    operator <<(const Var& x);
    /// Append \a x to the end of the array
    typename ArrayTraits<VarArgArray<Var> >::ArgsType&
    operator <<(const VarArgArray<Var>& x);
    //@}

    /// Test if all variables are assigned
    bool assigned(void) const;

    friend typename ArrayTraits<VarArgArray<Var> >::ArgsType
    operator + <>(const VarArgArray<Var>& x, const VarArgArray<Var>& y);
    friend typename ArrayTraits<VarArgArray<Var> >::ArgsType
    operator + <>(const VarArgArray<Var>& x, const Var& y);
    friend
    typename ArrayTraits<VarArgArray<Var> >::ArgsType
    operator + <>(const Var& x, const VarArgArray<Var>& y);

    /// \name Variable equality
    //@{
    /**
     * \brief Test whether array contains same variable multiply
     *
     * Note that assigned variables are ignored.
     */
    bool same(const Space& home) const;
    /**
     * \brief Test whether array contains variable \a y
     *
     * Note that assigned variables are ignored.
     */
    bool same(const Space& home, const Var& y) const;
    /**
     * \brief Test whether all elements from array and \a y contains same variable multiply
     *
     * Note that assigned variables are ignored.
     */
    bool same(const Space& home, const VarArgArray<Var>& y) const;
    //@}
  };

  /**
   * \brief Print array elements enclosed in curly brackets
   * \relates VarArray
   */
  template<class Char, class Traits, class Var>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
             const VarArray<Var>& x);

  /**
   * \brief Print array elements enclosed in curly brackets
   * \relates ViewArray
   */
  template<class Char, class Traits, class View>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ViewArray<View>& x);

  /**
   * \brief Print array elements enclosed in curly brackets
   * \relates ArgArrayBase
   */
  template<class Char, class Traits, class T>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ArgArrayBase<T>& x);


  /*
   * Implementation
   *
   */

  /*
   * Variable arrays
   *
   * These arrays are allocated in the space.
   *
   */

  template<class Var>
  forceinline
  VarArray<Var>::VarArray(void) : n(0), x(NULL) {}

  template<class Var>
  forceinline
  VarArray<Var>::VarArray(Space& home, int n0)
    : n(n0) {
    // Allocate from space
    x = (n>0) ? home.alloc<Var>(n) : NULL;
  }

  template<class Var>
  forceinline
  VarArray<Var>::VarArray(const VarArray<Var>& a) {
    n = a.n; x = a.x;
  }

  template<class Var>
  inline const VarArray<Var>&
  VarArray<Var>::operator =(const VarArray<Var>& a) {
    n = a.n; x = a.x;
    return *this;
  }

  template<class Var>
  forceinline int
  VarArray<Var>::size(void) const {
    return n;
  }

  template<class Var>
  forceinline Var&
  VarArray<Var>::operator [](int i) {
    assert((i >= 0) && (i < size()));
    return x[i];
  }

  template<class Var>
  forceinline const Var&
  VarArray<Var>::operator [](int i) const {
    assert((i >= 0) && (i < size()));
    return x[i];
  }

  template<class Var>
  typename ArrayTraits<VarArgArray<Var> >::ArgsType
  VarArray<Var>::slice(int start, int inc, int maxN) {
    assert(n==0 || start < n);
    if (n==0 || maxN<0)
      maxN = n;
    int s;
    if (inc == 0)
      s = n-start;
    else if (inc > 0)
      s = (n-start)/inc + ((n-start) % inc == 0 ? 0 : 1);
    else
      s = (start+1)/-inc + ((start+1) % -inc == 0 ? 0 : 1);
    typename ArrayTraits<VarArgArray<Var> >::ArgsType r(std::min(maxN,s));
    for (int i=0; i<r.size(); i++, start+=inc)
      r[i] = x[start];
    return r;
  }

  template<class Var>
  forceinline typename VarArray<Var>::iterator
  VarArray<Var>::begin(void) {
    return x;
  }

  template<class Var>
  forceinline typename VarArray<Var>::const_iterator
  VarArray<Var>::begin(void) const {
    return x;
  }

  template<class Var>
  forceinline typename VarArray<Var>::iterator
  VarArray<Var>::end(void) {
    return x+n;
  }

  template<class Var>
  forceinline typename VarArray<Var>::const_iterator
  VarArray<Var>::end(void) const {
    return x+n;
  }

  template<class Var>
  forceinline typename VarArray<Var>::reverse_iterator
  VarArray<Var>::rbegin(void) {
    return reverse_iterator(x+n);
  }

  template<class Var>
  forceinline typename VarArray<Var>::const_reverse_iterator
  VarArray<Var>::rbegin(void) const {
    return const_reverse_iterator(x+n);
  }

  template<class Var>
  forceinline typename VarArray<Var>::reverse_iterator
  VarArray<Var>::rend(void) {
    return reverse_iterator(x);
  }

  template<class Var>
  forceinline typename VarArray<Var>::const_reverse_iterator
  VarArray<Var>::rend(void) const {
    return const_reverse_iterator(x);
  }

  template<class Var>
  forceinline void
  VarArray<Var>::update(Space& home, bool share, VarArray<Var>& a) {
    n = a.n;
    if (n > 0) {
      x = home.alloc<Var>(n);
      for (int i = n; i--;)
        x[i].update(home, share, a.x[i]);
    } else {
      x = NULL;
    }
  }

  template<class Var>
  forceinline bool
  VarArray<Var>::assigned(void) const {
    for (int i = n; i--;)
      if (!x[i].assigned())
        return false;
    return true;
  }
  
  template<class Var>
  forceinline void*
  VarArray<Var>::operator new(size_t) {
    return NULL;
  }

  template<class Var>
  forceinline void
  VarArray<Var>::operator delete(void*,size_t) {
  }

  template<class Var>
  typename ArrayTraits<VarArray<Var> >::ArgsType
  operator +(const VarArray<Var>& x, const VarArray<Var>& y) {
    typename ArrayTraits<VarArray<Var> >::ArgsType r(x.size()+y.size());
    for (int i=x.size(); i--;)
      r[i] = x[i];
    for (int i=y.size(); i--;)
      r[x.size()+i] = y[i];
    return r;
  }

  template<class Var>
  typename ArrayTraits<VarArray<Var> >::ArgsType
  operator +(const VarArray<Var>& x, const VarArgArray<Var>& y) {
    typename ArrayTraits<VarArray<Var> >::ArgsType r(x.size()+y.size());
    for (int i=x.size(); i--;)
      r[i] = x[i];
    for (int i=y.size(); i--;)
      r[x.size()+i] = y[i];
    return r;
  }

  template<class Var>
  typename ArrayTraits<VarArray<Var> >::ArgsType
  operator +(const VarArgArray<Var>& x, const VarArray<Var>& y) {
    typename ArrayTraits<VarArray<Var> >::ArgsType r(x.size()+y.size());
    for (int i=x.size(); i--;)
      r[i] = x[i];
    for (int i=y.size(); i--;)
      r[x.size()+i] = y[i];
    return r;
  }

  template<class Var>
  typename ArrayTraits<VarArray<Var> >::ArgsType
  operator +(const VarArray<Var>& x, const Var& y) {
    typename ArrayTraits<VarArray<Var> >::ArgsType r(x.size()+1);
    for (int i=x.size(); i--;)
      r[i] = x[i];
    r[x.size()] = y;
    return r;    
  }

  template<class Var>
  typename ArrayTraits<VarArray<Var> >::ArgsType
  operator +(const Var& x, const VarArray<Var>& y) {
    typename ArrayTraits<VarArray<Var> >::ArgsType r(y.size()+1);
    r[0] = x;
    for (int i=y.size(); i--;)
      r[1+i] = y[i];
    return r;
  }

  /*
   * View arrays
   *
   */

  template<class View>
  forceinline
  ViewArray<View>::ViewArray(void) : n(0), x(NULL) {}

  template<class View>
  forceinline
  ViewArray<View>::ViewArray(Space& home, int n0)
    : n(n0) {
    x = (n>0) ? home.alloc<View>(n) : NULL;
  }
  template<class View>
  forceinline
  ViewArray<View>::ViewArray(Region& r, int n0)
    : n(n0) {
    x = (n>0) ? r.alloc<View>(n) : NULL;
  }

  template<class View>
  ViewArray<View>::ViewArray(Space& home, const ViewArray<View>& a)
    : n(a.size()) {
    if (n>0) {
      x = home.alloc<View>(n);
      for (int i = n; i--; )
        x[i] = a[i];
    } else {
      x = NULL;
    }
  }
  template<class View>
  ViewArray<View>::ViewArray(Region& r, const ViewArray<View>& a)
    : n(a.size()) {
    if (n>0) {
      x = r.alloc<View>(n);
      for (int i = n; i--; )
        x[i] = a[i];
    } else {
      x = NULL;
    }
  }

  template<class View>
  forceinline
  ViewArray<View>::ViewArray(const ViewArray<View>& a)
    : n(a.n), x(a.x) {}

  template<class View>
  forceinline const ViewArray<View>&
  ViewArray<View>::operator =(const ViewArray<View>& a) {
    n = a.n; x = a.x;
    return *this;
  }

  template<class View>
  forceinline int
  ViewArray<View>::size(void) const {
    return n;
  }

  template<class View>
  forceinline void
  ViewArray<View>::size(int n0) {
    n = n0;
  }

  template<class View>
  forceinline View&
  ViewArray<View>::operator [](int i) {
    assert((i >= 0) && (i < size()));
    return x[i];
  }

  template<class View>
  forceinline const View&
  ViewArray<View>::operator [](int i) const {
    assert((i >= 0) && (i < size()));
    return x[i];
  }

  template<class View>
  forceinline typename ViewArray<View>::iterator
  ViewArray<View>::begin(void) {
    return x;
  }

  template<class View>
  forceinline typename ViewArray<View>::const_iterator
  ViewArray<View>::begin(void) const {
    return x;
  }

  template<class View>
  forceinline typename ViewArray<View>::iterator
  ViewArray<View>::end(void) {
    return x+n;
  }

  template<class View>
  forceinline typename ViewArray<View>::const_iterator
  ViewArray<View>::end(void) const {
    return x+n;
  }

  template<class View>
  forceinline typename ViewArray<View>::reverse_iterator
  ViewArray<View>::rbegin(void) {
    return reverse_iterator(x+n);
  }

  template<class View>
  forceinline typename ViewArray<View>::const_reverse_iterator
  ViewArray<View>::rbegin(void) const {
    return const_reverse_iterator(x+n);
  }

  template<class View>
  forceinline typename ViewArray<View>::reverse_iterator
  ViewArray<View>::rend(void) {
    return reverse_iterator(x);
  }

  template<class View>
  forceinline typename ViewArray<View>::const_reverse_iterator
  ViewArray<View>::rend(void) const {
    return const_reverse_iterator(x);
  }

  template<class View>
  forceinline void
  ViewArray<View>::move_fst(int i) {
    x[i]=x[0]; x++; n--;
  }

  template<class View>
  forceinline void
  ViewArray<View>::move_lst(int i) {
    n--; x[i]=x[n];
  }

  template<class View>
  forceinline void
  ViewArray<View>::drop_fst(int i) {
    assert(i>=0);
    x += i; n -= i;
  }

  template<class View>
  forceinline void
  ViewArray<View>::drop_lst(int i) {
    assert(i<n);
    n = i+1;
  }

  template<class View>
  forceinline void
  ViewArray<View>::move_fst(int i, Space& home, Propagator& p, PropCond pc) {
    // Move x[0] to x[i]
    x[i].cancel(home,p,pc);
    x[i]=x[0]; x++; n--;
  }

  template<class View>
  forceinline void
  ViewArray<View>::move_lst(int i, Space& home, Propagator& p, PropCond pc) {
    // Move x[n-1] to x[i]
    x[i].cancel(home,p,pc);
    n--; x[i]=x[n];
  }

  template<class View>
  void
  ViewArray<View>::drop_fst(int i, Space& home, Propagator& p, PropCond pc) {
    // Drop elements from 0..i-1
    assert(i>=0);
    for (int j=i; j--; )
      x[j].cancel(home,p,pc);
    x += i; n -= i;
  }

  template<class View>
  void
  ViewArray<View>::drop_lst(int i, Space& home, Propagator& p, PropCond pc) {
    // Drop elements from i+1..n-1
    assert(i<n);
    for (int j=i+1; j<n; j++)
      x[j].cancel(home,p,pc);
    n = i+1;
  }

  template<class View>
  forceinline void
  ViewArray<View>::move_fst(int i, Space& home, Advisor& a) {
    // Move x[0] to x[i]
    x[i].cancel(home,a);
    x[i]=x[0]; x++; n--;
  }

  template<class View>
  forceinline void
  ViewArray<View>::move_lst(int i, Space& home, Advisor& a) {
    // Move x[n-1] to x[i]
    x[i].cancel(home,a);
    n--; x[i]=x[n];
  }

  template<class View>
  void
  ViewArray<View>::drop_fst(int i, Space& home, Advisor& a) {
    // Drop elements from 0..i-1
    assert(i>=0);
    for (int j=i; j--; )
      x[j].cancel(home,a);
    x += i; n -= i;
  }

  template<class View>
  void
  ViewArray<View>::drop_lst(int i, Space& home, Advisor& a) {
    // Drop elements from i+1..n-1
    assert(i<n);
    for (int j=i+1; j<n; j++)
      x[j].cancel(home,a);
    n = i+1;
  }

  template<class View>
  void
  ViewArray<View>::update(Space& home, bool share, ViewArray<View>& y) {
    n = y.n;
    if (n > 0) {
      x = home.alloc<View>(n);
      for (int i = n; i--; )
        x[i].update(home, share, y.x[i]);
    } else {
      x = NULL;
    }
  }

  template<class View>
  void
  ViewArray<View>::subscribe(Space& home, Propagator& p, PropCond pc,
                             bool process) {
    for (int i = n; i--; )
      x[i].subscribe(home,p,pc,process);
  }

  template<class View>
  void
  ViewArray<View>::cancel(Space& home, Propagator& p, PropCond pc) {
    for (int i = n; i--; )
      x[i].cancel(home,p,pc);
  }

  template<class View>
  void
  ViewArray<View>::subscribe(Space& home, Advisor& a) {
    for (int i = n; i--; )
      x[i].subscribe(home,a);
  }

  template<class View>
  void
  ViewArray<View>::cancel(Space& home, Advisor& a) {
    for (int i = n; i--; )
      x[i].cancel(home,a);
  }

  template<class View>
  forceinline bool
  ViewArray<View>::assigned(void) const {
    for (int i = n; i--;)
      if (!x[i].assigned())
        return false;
    return true;
  }

  template<class View>
  forceinline bool
  __before(const View& x, const View& y) {
    return before(x,y);
  }

  template<class View> template<class X>
  forceinline bool
  ViewArray<View>::ViewLess<X>::operator ()(const X& a, const X& b) {
    return __before(a,b);
  }

  template<class View>
  void
  ViewArray<View>::sort(View* y, int m) {
    ViewLess<View> vl;
    Support::quicksort<View,ViewLess<View> >(y,m,vl);
  }

  template<class X, class Y>
  forceinline bool
  __same(const X& x, const Y& y) {
    return same(x,y);
  }
  template<class X, class Y>
  forceinline bool
  __shared(const X& x, const Y& y) {
    return shared(x,y);
  }

  template<class View>
  bool
  ViewArray<View>::same(const Space& home) const {
    if (n < 2)
      return false;
    Region r(home);
    View* y = r.alloc<View>(n);
    for (int i = n; i--; )
      y[i] = x[i];
    sort(y,n);
    for (int i = n-1; i--; )
      if (!y[i].assigned() && __same(y[i+1],y[i])) {
        r.free<View>(y,n);
        return true;
      }
    r.free<View>(y,n);
    return false;
  }

  template<class View>
  bool
  ViewArray<View>::same(const Space&, const View& y) const {
    if (y.assigned())
      return false;
    for (int i = n; i--; )
      if (__same(x[i],y))
        return true;
    return false;
  }

  template<class View>
  void
  ViewArray<View>::unique(const Space&) {
    if (n < 2)
      return;
    sort(x,n);
    int j = 0;
    for (int i = 1; i<n; i++)
      if (!__same(x[j],x[i]))
        x[++j] = x[i];
    n = j+1;
  }

  template<class View>
  bool
  ViewArray<View>::shared(const Space& home) const {
    if (n < 2)
      return false;
    Region r(home);
    View* y = r.alloc<View>(n);
    for (int i = n; i--; )
      y[i] = x[i];
    sort(y,n);
    for (int i = n-1; i--; )
      if (!y[i].assigned() && __shared(y[i+1],y[i])) {
        r.free<View>(y,n);
        return true;
      }
    r.free<View>(y,n);
    return false;
  }

  template<class View> template<class ViewY>
  bool
  ViewArray<View>::shared(const Space&, const ViewY& y) const {
    if (y.assigned())
      return false;
    for (int i = n; i--; )
      if (!x[i].assigned() && __shared(x[i],y))
        return true;
    return false;
  }

  template<class View> template<class ViewY>
  bool
  ViewArray<View>::shared(const Space& home, const ViewArray<ViewY>& y) const {
    if ((size() < 1) || (y.size() < 1))
      return false;
    Region r(home);
    View* xs = r.alloc<View>(size());
    for (int i=size(); i--; )
      xs[i] = x[i];
    ViewLess<View> xvl;
    Support::quicksort<View,ViewLess<View> >(xs,size(),xvl);
    ViewY* ys = r.alloc<ViewY>(y.size());
    for (int j=y.size(); j--; )
      ys[j] = y[j];
    ViewLess<ViewY> yvl;
    Support::quicksort<ViewY,ViewLess<ViewY> >(ys,y.size(),yvl);
    {
      int i=0, j=0;
      while ((i < size()) && (j < y.size()))
        if (!x[i].assigned() && __shared(x[i],y[j])) {
          r.free<View>(xs,size());
          r.free<ViewY>(ys,y.size());
          return true;
        } else if (before(x[i],y[j])) {
          i++;
        } else {
          j++;
        }
    }
    r.free<View>(xs,size());
    r.free<ViewY>(ys,y.size());
    return false;
  }

  template<class View>
  forceinline void*
  ViewArray<View>::operator new(size_t) {
    return NULL;
  }

  template<class View>
  forceinline void
  ViewArray<View>::operator delete(void*,size_t) {
  }


  /*
   * Argument arrays: base class
   *
   */

  template<class T>
  forceinline T*
  ArgArrayBase<T>::allocate(int n) {
    return (n > onstack_size) ?
      heap.alloc<T>(static_cast<unsigned int>(n)) : &onstack[0];
  }

  template<class T>
  forceinline void
  ArgArrayBase<T>::resize(int i) {
    if (n+i >= capacity) {
      assert(n+i >= onstack_size);
      int newCapacity = (3*capacity)/2;
      if (newCapacity <= n+i)
        newCapacity = n+i;
      T* newA = allocate(newCapacity);
      heap.copy<T>(newA,a,n);
      if (capacity > onstack_size)
        heap.free(a,capacity);
      capacity = newCapacity;
      a = newA;
    }
  }

  template<class T>
  forceinline
  ArgArrayBase<T>::ArgArrayBase(void)
    : n(0), capacity(onstack_size), a(allocate(0)) {}

  template<class T>
  forceinline
  ArgArrayBase<T>::ArgArrayBase(int n0)
    : n(n0), capacity(n < onstack_size ? onstack_size : n), a(allocate(n)) {}

  template<class T>
  inline
  ArgArrayBase<T>::ArgArrayBase(const ArgArrayBase<T>& aa)
    : n(aa.n), capacity(n < onstack_size ? onstack_size : n), a(allocate(n)) {
    heap.copy<T>(a,aa.a,n);
  }

  template<class T>
  inline
  ArgArrayBase<T>::ArgArrayBase(const std::vector<T>& aa)
    : n(static_cast<int>(aa.size())),
      capacity(n < onstack_size ? onstack_size : n), a(allocate(n)) {
    heap.copy<T>(a,&aa[0],n);
  }

  template<class T>
  forceinline
  ArgArrayBase<T>::~ArgArrayBase(void) {
    if (capacity > onstack_size)
      heap.free(a,capacity);
  }

  template<class T>
  forceinline const ArgArrayBase<T>&
  ArgArrayBase<T>::operator =(const ArgArrayBase<T>& aa) {
    if (&aa != this) {
      if (capacity > onstack_size)
        heap.free(a,capacity);
      n = aa.n;
      capacity = (n < onstack_size ? onstack_size : n);
      a = allocate(aa.n);
      heap.copy<T>(a,aa.a,n);
    }
    return *this;
  }

  template<class T>
  forceinline int
  ArgArrayBase<T>::size(void) const {
    return n;
  }

  template<class T>
  forceinline T&
  ArgArrayBase<T>::operator [](int i) {
    assert((i>=0) && (i < n));
    return a[i];
  }

  template<class T>
  forceinline const T&
  ArgArrayBase<T>::operator [](int i) const {
    assert((i>=0) && (i < n));
    return a[i];
  }

  template<class T>
  forceinline typename ArgArrayBase<T>::iterator
  ArgArrayBase<T>::begin(void) {
    return a;
  }

  template<class T>
  forceinline typename ArgArrayBase<T>::const_iterator
  ArgArrayBase<T>::begin(void) const {
    return a;
  }

  template<class T>
  forceinline typename ArgArrayBase<T>::iterator
  ArgArrayBase<T>::end(void) {
    return a+n;
  }

  template<class T>
  forceinline typename ArgArrayBase<T>::const_iterator
  ArgArrayBase<T>::end(void) const {
    return a+n;
  }

  template<class T>
  forceinline typename ArgArrayBase<T>::reverse_iterator
  ArgArrayBase<T>::rbegin(void) {
    return reverse_iterator(a+n);
  }

  template<class T>
  forceinline typename ArgArrayBase<T>::const_reverse_iterator
  ArgArrayBase<T>::rbegin(void) const {
    return const_reverse_iterator(a+n);
  }

  template<class T>
  forceinline typename ArgArrayBase<T>::reverse_iterator
  ArgArrayBase<T>::rend(void) {
    return reverse_iterator(a);
  }

  template<class T>
  forceinline typename ArgArrayBase<T>::const_reverse_iterator
  ArgArrayBase<T>::rend(void) const {
    return const_reverse_iterator(a);
  }

  template<class T> template<class A>
  A
  ArgArrayBase<T>::slice(int start, int inc, int maxN) {
    assert(n==0 || start < n);
    if (n==0 || maxN<0)
      maxN = n;
    int s;
    if (inc == 0)
      s = n-start;
    else if (inc > 0)
      s = (n-start)/inc + ((n-start) % inc == 0 ? 0 : 1);
    else
      s = (start+1)/-inc + ((start+1) % -inc == 0 ? 0 : 1);
    A r(std::min(maxN,s));
    for (int i=0; i<r.size(); i++, start+=inc)
      new (&r[i]) T(a[start]);
    return r;
  }

  template<class T> template<class A>
  inline A&
  ArgArrayBase<T>::append(const T& x) {
    resize(1);
    new (&a[n++]) T(x);
    return static_cast<A&>(*this);
  }

  template<class T>
  template<class InputIterator>
  inline
  ArgArrayBase<T>::ArgArrayBase(InputIterator first, InputIterator last)
  : n(0), capacity(onstack_size), a(allocate(0)) {
    while (first != last) {
      (void) append<ArgArrayBase<T> >(*first);
      ++first;
    }
  }


  template<class T> template<class A>
  inline A&
  ArgArrayBase<T>::append(const ArgArrayBase<T>& x) {
    resize(x.size());
    for (int i=0; i<x.size(); i++)
      new (&a[n++]) T(x[i]);
    return static_cast<A&>(*this);
  }

  template<class T> template<class A>
  inline A
  ArgArrayBase<T>::concat(const ArgArrayBase<T>& x) const {
    A r(n+x.n);
    for (int i=n; i--;)
      new (&r[i]) T(a[i]);
    for (int i=x.n; i--;)
      new (&r[n+i]) T(x.a[i]);
    return r;
  }

  template<class T> template<class A>
  inline A
  ArgArrayBase<T>::concat(const T& x) const {
    A r(n+1);
    for (int i=n; i--;)
      new (&r[i]) T(a[i]);
    new (&r[n]) T(x);
    return r;
  }

  template<class T>
  forceinline void*
  ArgArrayBase<T>::operator new(size_t) {
    return NULL;
  }

  template<class T>
  forceinline void
  ArgArrayBase<T>::operator delete(void*,size_t) {
  }

  /*
   * Argument arrays for primitive types
   *
   */

  template<class T>
  forceinline
  PrimArgArray<T>::PrimArgArray(void) {}

  template<class T>
  forceinline
  PrimArgArray<T>::PrimArgArray(int n) : ArgArrayBase<T>(n) {}

  template<class T>
  PrimArgArray<T>::PrimArgArray(int n, T a0, ...)
    : ArgArrayBase<T>(n) {
    va_list args;
    va_start(args, a0);
    a[0] = a0;
    for (int i = 1; i < n; i++)
      a[i] = va_arg(args,T);
    va_end(args);
  }

  template<class T>
  PrimArgArray<T>::PrimArgArray(int n, const T* a0)
    : ArgArrayBase<T>(n) {
    for (int i=n; i--; )
      a[i] = a0[i];
  }

  template<class T>
  forceinline
  PrimArgArray<T>::PrimArgArray(const PrimArgArray<T>& aa)
    : ArgArrayBase<T>(aa) {}

  template<class T>
  forceinline
  PrimArgArray<T>::PrimArgArray(const std::vector<T>& aa)
    : ArgArrayBase<T>(aa) {}

  template<class T>
  template<class InputIterator>
  forceinline
  PrimArgArray<T>::PrimArgArray(InputIterator first, InputIterator last)
    : ArgArrayBase<T>(first,last) {}

  template<class T>
  forceinline typename ArrayTraits<PrimArgArray<T> >::ArgsType
  PrimArgArray<T>::slice(int start, int inc, int maxN) {
    return ArgArrayBase<T>::template slice
      <typename ArrayTraits<PrimArgArray<T> >::ArgsType>
        (start,inc,maxN);
  }

  template<class T>
  forceinline typename ArrayTraits<PrimArgArray<T> >::ArgsType&
  PrimArgArray<T>::operator <<(const T& x) {
    return
      ArgArrayBase<T>::template append
        <typename ArrayTraits<PrimArgArray<T> >::ArgsType>(x);
  }

  template<class T>
  forceinline typename ArrayTraits<PrimArgArray<T> >::ArgsType&
  PrimArgArray<T>::operator <<(const PrimArgArray<T>& x) {
    return
      ArgArrayBase<T>::template append
        <typename ArrayTraits<PrimArgArray<T> >::ArgsType>(x);
  }

  template<class T>
  typename ArrayTraits<PrimArgArray<T> >::ArgsType
  operator +(const PrimArgArray<T>& x, const PrimArgArray<T>& y) {
    return x.template concat
      <typename ArrayTraits<PrimArgArray<T> >::ArgsType>(y);
  }

  template<class T>
  typename ArrayTraits<PrimArgArray<T> >::ArgsType
  operator +(const PrimArgArray<T>& x, const T& y) {
    return x.template concat
      <typename ArrayTraits<PrimArgArray<T> >::ArgsType>(y);    
  }

  template<class T>
  typename ArrayTraits<PrimArgArray<T> >::ArgsType
  operator +(const T& x, const PrimArgArray<T>& y) {
    return PrimArgArray<T>(1,x).template concat
      <typename ArrayTraits<PrimArgArray<T> >::ArgsType>(y);    
  }


  /*
   * Argument arrays for non-primitive types
   *
   */

  template<class T>
  forceinline
  ArgArray<T>::ArgArray(void) {}

  template<class T>
  forceinline
  ArgArray<T>::ArgArray(int n) : ArgArrayBase<T>(n) {}

  template<class T>
  ArgArray<T>::ArgArray(int n, const T* a0)
    : ArgArrayBase<T>(n) {
    for (int i=n; i--; )
      a[i] = a0[i];
  }

  template<class T>
  forceinline
  ArgArray<T>::ArgArray(const ArgArray<T>& aa)
    : ArgArrayBase<T>(aa) {}

  template<class T>
  forceinline
  ArgArray<T>::ArgArray(const std::vector<T>& aa)
    : ArgArrayBase<T>(aa) {}

  template<class T>
  template<class InputIterator>
  forceinline
  ArgArray<T>::ArgArray(InputIterator first, InputIterator last)
    : ArgArrayBase<T>(first,last) {}

  template<class T>
  forceinline typename ArrayTraits<ArgArray<T> >::ArgsType
  ArgArray<T>::slice(int start, int inc, int maxN) {
    return ArgArrayBase<T>::template slice
      <typename ArrayTraits<ArgArray<T> >::ArgsType>
      (start,inc,maxN);
  }

  template<class T>
  forceinline typename ArrayTraits<ArgArray<T> >::ArgsType&
  ArgArray<T>::operator <<(const T& x) {
    return
      ArgArrayBase<T>::template append
        <typename ArrayTraits<ArgArray<T> >::ArgsType>(x);
  }

  template<class T>
  forceinline typename ArrayTraits<ArgArray<T> >::ArgsType&
  ArgArray<T>::operator <<(const ArgArray<T>& x) {
    return
      ArgArrayBase<T>::template append
        <typename ArrayTraits<ArgArray<T> >::ArgsType>(x);
  }

  template<class T>
  typename ArrayTraits<ArgArray<T> >::ArgsType
  operator +(const ArgArray<T>& x, const ArgArray<T>& y) {
    return x.template concat
      <typename ArrayTraits<ArgArray<T> >::ArgsType>(y);
  }

  template<class T>
  typename ArrayTraits<ArgArray<T> >::ArgsType
  operator +(const ArgArray<T>& x, const T& y) {
    return x.template concat
      <typename ArrayTraits<ArgArray<T> >::ArgsType>(y);    
  }

  template<class T>
  typename ArrayTraits<ArgArray<T> >::ArgsType
  operator +(const T& x, const ArgArray<T>& y) {
    ArgArray<T> xa(1);
    xa[0] = x;
    return xa.template concat
      <typename ArrayTraits<ArgArray<T> >::ArgsType>(y);    
  }

  /*
   * Argument arrays for variables
   *
   */

  template<class Var>
  forceinline
  VarArgArray<Var>::VarArgArray(void) {}

  template<class Var>
  forceinline
  VarArgArray<Var>::VarArgArray(int n) : ArgArrayBase<Var>(n) {}

  template<class Var>
  forceinline
  VarArgArray<Var>::VarArgArray(const VarArgArray<Var>& aa)
    : ArgArrayBase<Var>(aa) {}

  template<class Var>
  forceinline
  VarArgArray<Var>::VarArgArray(const std::vector<Var>& aa)
    : ArgArrayBase<Var>(aa) {}

  template<class Var>
  template<class InputIterator>
  forceinline
  VarArgArray<Var>::VarArgArray(InputIterator first, InputIterator last)
    : ArgArrayBase<Var>(first,last) {}

  template<class Var>
  inline
  VarArgArray<Var>::VarArgArray(const VarArray<Var>& x)
    : ArgArrayBase<Var>(x.size()) {
    for (int i=x.size(); i--; )
      a[i]=x[i];
  }

  template<class Var>
  forceinline typename ArrayTraits<VarArgArray<Var> >::ArgsType
  VarArgArray<Var>::slice(int start, int inc, int maxN) {
    return ArgArrayBase<Var>::template slice
      <typename ArrayTraits<VarArgArray<Var> >::ArgsType>
        (start,inc,maxN);
  }

  template<class Var>
  forceinline typename ArrayTraits<VarArgArray<Var> >::ArgsType&
  VarArgArray<Var>::operator <<(const Var& x) {
    return
      ArgArrayBase<Var>::template append
        <typename ArrayTraits<VarArgArray<Var> >::ArgsType>(x);
  }

  template<class Var>
  forceinline typename ArrayTraits<VarArgArray<Var> >::ArgsType&
  VarArgArray<Var>::operator <<(const VarArgArray<Var>& x) {
    return
      ArgArrayBase<Var>::template append
        <typename ArrayTraits<VarArgArray<Var> >::ArgsType>(x);
  }

  template<class Var>
  typename ArrayTraits<VarArgArray<Var> >::ArgsType
  operator +(const VarArgArray<Var>& x, const VarArgArray<Var>& y) {
    return x.template concat
      <typename ArrayTraits<VarArgArray<Var> >::ArgsType>(y);
  }

  template<class Var>
  typename ArrayTraits<VarArgArray<Var> >::ArgsType
  operator +(const VarArgArray<Var>& x, const Var& y) {
    return x.template concat
      <typename ArrayTraits<VarArgArray<Var> >::ArgsType>(y);    
  }

  template<class Var>
  typename ArrayTraits<VarArgArray<Var> >::ArgsType
  operator +(const Var& x, const VarArgArray<Var>& y) {
    VarArgArray<Var> xa(1);
    xa[0] = x;
    return xa.template concat
      <typename ArrayTraits<VarArgArray<Var> >::ArgsType>(y);    
  }

  template<class Var>
  forceinline bool
  VarArgArray<Var>::VarLess::operator ()(const Var& a, const Var& b) {
    return a.varimp() < b.varimp();
  }

  template<class Var>
  forceinline bool
  VarArgArray<Var>::assigned(void) const {
    for (int i = n; i--;)
      if (!a[i].assigned())
        return false;
    return true;
  }

  template<class Var>
  bool
  VarArgArray<Var>::same(const Space& home) const {
    if (n < 2)
      return false;
    Region r(home);
    Var* y = r.alloc<Var>(n);
    for (int i = n; i--; )
      y[i] = a[i];
    VarLess vl;
    Support::quicksort<Var,VarLess>(y,n,vl);
    for (int i = n-1; i--; )
      if (!y[i].assigned() && (y[i+1].varimp() == y[i].varimp())) {
        r.free<Var>(y,n);
        return true;
      }
    r.free<Var>(y,n);
    return false;
  }

  template<class Var>
  bool
  VarArgArray<Var>::same(const Space& home, const VarArgArray<Var>& y) const {
    int m = n + y.n;
    if (m < 2)
      return false;
    Region r(home);
    Var* z = r.alloc<Var>(m);
    for (int i = n; i--; )
      z[i] = a[i];
    for (int i = y.n; i--; )
      z[i+n] = y.a[i];
    VarLess vl;
    Support::quicksort<Var,VarLess>(z,m,vl);
    for (int i = m-1; i--; )
      if (!z[i].assigned() && (z[i+1].varimp() == z[i].varimp())) {
        r.free<Var>(z,m);
        return true;
      }
    r.free<Var>(z,m);
    return false;
  }

  template<class Var>
  bool
  VarArgArray<Var>::same(const Space&, const Var& y) const {
    if (y.assigned())
      return false;
    for (int i = n; i--; )
      if (a[i].varimp() == y.varimp())
        return true;
    return false;
  }






  /*
   * Interdependent code
   *
   */

  template<class Var>
  inline
  VarArray<Var>::VarArray(Space& home, const VarArgArray<Var>& a)
    : n(a.size()) {
    if (n>0) {
      x = home.alloc<Var>(n);
      for (int i=n; i--;)
        x[i] = a[i];
    } else {
      x = NULL;
    }
  }


  /*
   * Printing of arrays
   *
   */
  template<class Char, class Traits, class Var>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
              const VarArray<Var>& x) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << '{';
    if (x.size() > 0) {
      s << x[0];
      for (int i=1; i<x.size(); i++)
        s << ", " << x[i];
    }
    s << '}';
    return os << s.str();
  }

  template<class Char, class Traits, class View>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
             const ViewArray<View>& x) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << '{';
    if (x.size() > 0) {
      s << x[0];
      for (int i=1; i<x.size(); i++)
        s << ", " << x[i];
    }
    s << '}';
    return os << s.str();
  }

  template<class Char, class Traits, class T>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
             const ArgArrayBase<T>& x) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << '{';
    if (x.size() > 0) {
      s << x[0];
      for (int i=1; i<x.size(); i++)
        s << ", " << x[i];
    }
    s << '}';
    return os << s.str();
  }

}

// STATISTICS: kernel-other
