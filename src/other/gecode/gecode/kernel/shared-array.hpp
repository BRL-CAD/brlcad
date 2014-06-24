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
#include <sstream>

namespace Gecode {

  /**
   * \brief Shared array with arbitrary number of elements
   *
   * Sharing is implemented by reference counting: the same elements
   * are shared among several objects.
   *
   */
  template<class T>
  class SharedArray : public SharedHandle {
  protected:
    /// Implementation of object for shared arrays
    class SAO : public SharedHandle::Object {
    private:
      /// Elements
      T*  a;
      /// Number of elements
      int n;
    public:
      /// Allocate for \a n elements
      SAO(int n);
      /// Create copy of elements
      virtual SharedHandle::Object* copy(void) const;
      /// Delete object
      virtual ~SAO(void);

      /// Access element at position \a i
      T& operator [](int i);
      /// Access element at position \a i
      const T& operator [](int i) const;

      /// Return number of elements
      int size(void) const;
      
      /// Return beginning of array (for iterators)
      T* begin(void);
      /// Return beginning of array (for iterators)
      const T* begin(void) const;
      /// Return end of array (for iterators)
      T* end(void);
      /// Return end of array (for iterators)
      const T* end(void) const;
    };
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

    /**
     * \brief Construct as not yet intialized
     *
     * The only member functions that can be used on a constructed but not
     * yet initialized shared array is init and the assignment operator .
     *
     */
    SharedArray(void);
    /// Initialize as array with \a n elements
    SharedArray(int n);
    /**
     * \brief Initialize as array with \a n elements
     *
     * This member function can only be used once and only if the shared
     * array has been constructed with the default constructor.
     *
     */
    void init(int n);
    /// Initialize from shared array \a a (share elements)
    SharedArray(const SharedArray& a);
    /// Initialize from argument array \a a
    SharedArray(const ArgArrayBase<T>& a);

    /// Access element at position \a i
    T& operator [](int i);
    /// Access element at position \a i
    const T& operator [](int i) const;

    /// Return number of elements
    int size(void) const;

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
  };

  /**
   * \brief Print array elements enclosed in curly brackets
   * \relates SharedArray
   */
  template<class Char, class Traits, class T>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
             const SharedArray<T>& x);


  /*
   * Implementation
   *
   */

  /*
   * Shared arrays
   *
   */
  template<class T>
  forceinline
  SharedArray<T>::SAO::SAO(int n0) : n(n0) {
    a = (n>0) ? heap.alloc<T>(n) : NULL;
  }

  template<class T>
  SharedHandle::Object*
  SharedArray<T>::SAO::copy(void) const {
    SAO* o = new SAO(n);
    for (int i=n; i--;)
      o->a[i] = a[i];
    return o;
  }

  template<class T>
  SharedArray<T>::SAO::~SAO(void) {
    if (n>0) {
      heap.free<T>(a,n);
    }
  }

  template<class T>
  forceinline T&
  SharedArray<T>::SAO::operator [](int i) {
    assert((i>=0) && (i<n));
    return a[i];
  }

  template<class T>
  forceinline const T&
  SharedArray<T>::SAO::operator [](int i) const {
    assert((i>=0) && (i<n));
    return a[i];
  }

  template<class T>
  forceinline int
  SharedArray<T>::SAO::size(void) const {
    return n;
  }

  template<class T>
  forceinline T*
  SharedArray<T>::SAO::begin(void) {
    return a;
  }

  template<class T>
  forceinline const T*
  SharedArray<T>::SAO::begin(void) const {
    return a;
  }

  template<class T>
  forceinline T*
  SharedArray<T>::SAO::end(void) {
    return a+n;
  }

  template<class T>
  forceinline const T*
  SharedArray<T>::SAO::end(void) const {
    return a+n;
  }


  template<class T>
  forceinline
  SharedArray<T>::SharedArray(void) {}

  template<class T>
  forceinline
  SharedArray<T>::SharedArray(int n)
    : SharedHandle(new SAO(n)) {}

  template<class T>
  forceinline
  SharedArray<T>::SharedArray(const SharedArray<T>& sa)
    : SharedHandle(sa) {}

  template<class T>
  forceinline void
  SharedArray<T>::init(int n) {
    assert(object() == NULL);
    object(new SAO(n));
  }

  template<class T>
  forceinline T&
  SharedArray<T>::operator [](int i) {
    assert(object() != NULL);
    return (*static_cast<SAO*>(object()))[i];
  }

  template<class T>
  forceinline const T&
  SharedArray<T>::operator [](int i) const {
    assert(object() != NULL);
    return (*static_cast<SAO*>(object()))[i];
  }

  template<class T>
  forceinline
  SharedArray<T>::SharedArray(const ArgArrayBase<T>& a)
    : SharedHandle(new SAO(a.size())) {
    for (int i=a.size(); i--; )
      operator [](i)=a[i];
  }

  template<class T>
  forceinline int
  SharedArray<T>::size(void) const {
    assert(object() != NULL);
    return static_cast<SAO*>(object())->size();
  }

  template<class T>
  forceinline typename SharedArray<T>::iterator
  SharedArray<T>::begin(void) {
    assert(object() != NULL);
    return static_cast<SAO*>(object())->begin();
  }

  template<class T>
  forceinline typename SharedArray<T>::const_iterator
  SharedArray<T>::begin(void) const {
    assert(object() != NULL);
    return static_cast<SAO*>(object())->begin();
  }

  template<class T>
  forceinline typename SharedArray<T>::iterator
  SharedArray<T>::end(void) {
    assert(object() != NULL);
    return static_cast<SAO*>(object())->end();
  }

  template<class T>
  forceinline typename SharedArray<T>::const_iterator
  SharedArray<T>::end(void) const {
    assert(object() != NULL);
    return static_cast<SAO*>(object())->end();
  }

  template<class T>
  forceinline typename SharedArray<T>::reverse_iterator
  SharedArray<T>::rbegin(void) {
    assert(object() != NULL);
    return reverse_iterator(static_cast<SAO*>(object())->end());
  }

  template<class T>
  forceinline typename SharedArray<T>::const_reverse_iterator
  SharedArray<T>::rbegin(void) const {
    assert(object() != NULL);
    return const_reverse_iterator(static_cast<SAO*>(object())->end());
  }

  template<class T>
  forceinline typename SharedArray<T>::reverse_iterator
  SharedArray<T>::rend(void) {
    assert(object() != NULL);
    return reverse_iterator(static_cast<SAO*>(object())->begin());
  }

  template<class T>
  forceinline typename SharedArray<T>::const_reverse_iterator
  SharedArray<T>::rend(void) const {
    assert(object() != NULL);
    return const_reverse_iterator(static_cast<SAO*>(object())->begin());
  }

  template<class Char, class Traits, class T>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
             const SharedArray<T>& x) {
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
