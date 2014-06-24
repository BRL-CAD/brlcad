/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

#include <algorithm>
#include <climits>

namespace Gecode { namespace Support {

  /// Exchange elements according to order
  template<class Type, class Less>
  forceinline void
  exchange(Type &a, Type &b, Less &less) {
    if (less(b,a)) std::swap(a,b);
  }

  /// Perform quicksort only for more elements
  int const QuickSortCutoff = 20;

  /// Static stack for quicksort
  template<class Type>
  class QuickSortStack {
  private:
    /// Maximal stacksize quicksort ever needs
    static const int maxsize = sizeof(int) * CHAR_BIT;
    /// Top of stack
    Type** tos;
    /// Stack entries (terminated by NULL entry)
    Type*  stack[2*maxsize+1];
  public:
    /// Initialize stack as empty
    QuickSortStack(void);
    /// Test whether stack is empty
    bool empty(void) const;
    /// Push two positions \a l and \a r
    void push(Type* l, Type* r);
    /// Pop two positions \a l and \a r
    void pop(Type*& l, Type*& r);
  };

  template<class Type>
  forceinline
  QuickSortStack<Type>::QuickSortStack(void) : tos(&stack[0]) {
    *(tos++) = NULL;
  }

  template<class Type>
  forceinline bool
  QuickSortStack<Type>::empty(void) const {
    return *(tos-1) == NULL;
  }

  template<class Type>
  forceinline void
  QuickSortStack<Type>::push(Type* l, Type* r) {
    *(tos++) = l; *(tos++) = r;
  }

  template<class Type>
  forceinline void
  QuickSortStack<Type>::pop(Type*& l, Type*& r) {
    r = *(--tos); l = *(--tos);
  }

  /// Standard insertion sort
  template<class Type, class Less>
  forceinline void
  insertion(Type* l, Type* r, Less &less) {
    for (Type* i = r; i > l; i--)
      exchange(*(i-1),*i,less);
    for (Type* i = l+2; i <= r; i++) {
      Type* j = i;
      Type v = *i;
      while (less(v,*(j-1))) {
        *j = *(j-1); j--;
      }
      *j = v;
    }
  }

  /// Standard partioning
  template<class Type, class Less>
  forceinline Type*
  partition(Type* l, Type* r, Less &less) {
    Type* i = l-1;
    Type* j = r;
    Type v = *r;
    while (true) {
      while (less(*(++i),v)) {}
      while (less(v,*(--j))) if (j == l) break;
        if (i >= j) break;
        std::swap(*i,*j);
    }
    std::swap(*i,*r);
    return i;
  }

  /// Standard quick sort
  template<class Type, class Less>
  inline void
  quicksort(Type* l, Type* r, Less &less) {
    QuickSortStack<Type> s;
    while (true) {
      std::swap(*(l+((r-l) >> 1)),*(r-1));
      exchange(*l,*(r-1),less);
      exchange(*l,*r,less);
      exchange(*(r-1),*r,less);
      Type* i = partition(l+1,r-1,less);
      if (i-l > r-i) {
        if (r-i > QuickSortCutoff) {
          s.push(l,i-1); l=i+1; continue;
        }
        if (i-l > QuickSortCutoff) {
          r=i-1; continue;
        }
      } else {
        if (i-l > QuickSortCutoff) {
          s.push(i+1,r); r=i-1; continue;
        }
        if (r-i > QuickSortCutoff) {
          l=i+1; continue;
        }
      }
      if (s.empty())
        break;
      s.pop(l,r);
    }
  }

  /// Comparison class for sorting using \a <
  template<class Type>
  class Less {
  public:
    bool operator ()(const Type& lhs, const Type& rhs) {
      return lhs < rhs;
    }
  };

  /**
   * \brief Insertion sort
   *
   * Sorts by insertion the \a n first elements of array \a x according
   * to the order \a l as instance of class \a Less. The class
   * \a Less must implement the single member function
   * \code bool operator ()(const Type&, const Type&) \endcode
   * for comparing elements. Note that the order must be strict, that
   * is: comparing an element with itself must return false.
   *
   * The algorithm is largely based on the following book:
   * Robert Sedgewick, Algorithms in C++, 3rd edition, 1998, Addison Wesley.
   *
   * \ingroup FuncSupport
   */
  template<class Type, class Less>
  forceinline void
  insertion(Type* x, int n, Less &l) {
    if (n < 2)
      return;
    assert(!l(x[0],x[0]));
    insertion(x,x+n-1,l);
  }

  /**
   * \brief Insertion sort
   *
   * Sorts by insertion the \a n first elements of array \a x according
   * to the order \a <.
   *
   * The algorithm is largely based on the following book:
   * Robert Sedgewick, Algorithms in C++, 3rd edition, 1998, Addison Wesley.
   *
   * \ingroup FuncSupport
   */
  template<class Type>
  forceinline void
  insertion(Type* x, int n) {
    if (n < 2)
      return;
    Less<Type> l;
    assert(!l(x[0],x[0]));
    insertion(x,x+n-1,l);
  }

  /**
   * \brief Quicksort
   *
   * Sorts with quicksort the \a n first elements of array \a x according
   * to the order \a l as instance of class \a Less. The class
   * \a Less must implement the single member function
   * \code bool operator ()(const Type&, const Type&) \endcode
   * for comparing elements. Note that the order must be strict, that
   * is: comparing an element with itself must return false.
   *
   * The algorithm is largely based on the following book:
   * Robert Sedgewick, Algorithms in C++, 3rd edition, 1998, Addison Wesley.
   *
   * \ingroup FuncSupport
   */
  template<class Type, class Less>
  forceinline void
  quicksort(Type* x, int n, Less &l) {
    if (n < 2)
      return;
    assert(!l(x[0],x[0]));
    if (n > QuickSortCutoff)
      quicksort(x,x+n-1,l);
    insertion(x,x+n-1,l);
  }

  /**
   * \brief Quicksort
   *
   * Sorts with quicksort the \a n first elements of array \a x according
   * to the order \a <.
   *
   * The algorithm is largely based on the following book:
   * Robert Sedgewick, Algorithms in C++, 3rd edition, 1998, Addison Wesley.
   *
   * \ingroup FuncSupport
   */
  template<class Type>
  forceinline void
  quicksort(Type* x, int n) {
    if (n < 2)
      return;
    Less<Type> l;
    assert(!l(x[0],x[0]));
    if (n > QuickSortCutoff)
      quicksort(x,x+n-1,l);
    insertion(x,x+n-1,l);
  }

}}

// STATISTICS: support-any
