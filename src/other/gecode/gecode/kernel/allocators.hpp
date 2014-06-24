/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Filip Konvicka <filip.konvicka@logis.cz>
 *
 *  Copyright:
 *     LOGIS, s.r.o., 2009
 *
 *  Bugfixes provided by:
 *     Gustavo Gutierrez
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
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
 */

#include <limits>

namespace Gecode {

  template<class T> struct space_allocator;

/**
 
\defgroup FuncMemAllocator Using allocators with Gecode
\ingroup FuncMem

%Gecode provides two allocator classes that can be used with
generic data structures such as those of the STL
(e.g. <tt>std::set</tt>). Memory can be allocated from the space heap
(see Space) or from a region (see Region).
 
\section FuncMemAllocatorA Using allocators with dynamic data structures

There are two possible scenarios for allocator usage. One is to
let the dynamic data structure allocate memory from the space or
region:

\code
struct MySpace : public Space {
  typedef std::set<int, std::less<int>, Gecode::space_allocator<int> > S;
  S safe_set;

  MySpace(void)
    : safe_set(S::key_compare(), S::allocator_type(*this))
  {}

  MySpace(bool share, MySpace& other)
    : Space(share, other), 
      safe_set(other.safe_set.begin(), other.safe_set.end(), 
               S::key_compare(), S::allocator_type(*this))
  {}

  ...
};
\endcode

In this example, \a S is a set that allocates its nodes from the
space heap. Note that we pass an instance of space_allocator
bound to this space to the constructor of the set. A similar
thing must be done in the copying constructor, where we must be
sure to pass an allocator that allocates memory from the
destination ("this") space, not "other".  Note that the set
itself is a member of \a MySpace, so it is destroyed within
<i>MySpace::~MySpace</i> as usual.  The set destructor destroys
all contained items and deallocates all nodes in its destructors.

\section FuncMemAllocatorB Preventing unnecessary destruction overhead

In the above example, we know that the value type in \a S is a
builtin type and does not have a destructor.  So what happens
during \a safe_set destruction is that it just deallocates all
nodes. However, we know that all nodes were allocated from the
space heap, which is going to disappear with the space anyway.
If we prevent calling \a safe_set destructor, we may save a
significant amount of time during space destruction.  A safe way
of doing this is to allocate the set object itself on the space
heap, and keep only a reference to it as a member of the
space. We can use the convenience helpers Space::construct for
the construction.

\code
struct MySpace : public Space {
  typedef std::set<int, std::less<int>, Gecode::space_allocator<int> > S;
  S& fast_set;

  MySpace(void)
    : fast_set(construct<S>(S::key_compare(), S::allocator_type(*this)))
  {}

  MySpace(bool share, MySpace& other)
    : Space(share, other), 
	fast_set(construct<S>(other.safe_set.begin(), other.safe_set.end(), 
	S::key_compare(), S::allocator_type(*this)))
  {}

  ...
};
\endcode

\section FuncMemAllocatorC Region example

The above examples were using a space_allocator. A region_allocator
works similarly, one just should keep in mind that regions never
really release any memory. Similar to Space, Region provides
helper functions Region::construct to make non-stack allocation
easy.

\code
Space& home = ...;
typedef std::set<int, std::less<int>, Gecode::region_allocator<int> > SR;
// Create a set with the region allocator. Note that the set destructor is still quite costly...
{
  Region r(home);
  SR r_safe_set(SR::key_compare(), (SR::allocator_type(r)));
  for(int i=0; i<10000; ++i)
    r_safe_set.insert(i*75321%10000);
}
// Create a set directly in the region (not on the stack). No destructors will be called.
{
  Region r(*this);
  SR& r_fast_set=r.construct<SR>(SR::key_compare(), SR::allocator_type(r));
  for(int i=0; i<10000; ++i)
    r_fast_set.insert(i*75321%10000);
}
\endcode

*/


  /**
   * \brief %Space allocator - specialization for \c void.
   *
   * The specialization is needed as the default instantiation fails 
   * for \c void.
   */
  template<> 
  struct space_allocator<void> {
    typedef void*       pointer;
    typedef const void* const_pointer;
    typedef void        value_type;
    /// Rebinding helper (returns the type of a similar allocator for type \a U)
    template<class U> struct rebind {
      typedef space_allocator<U> other;
    };
  };

  /**
   * \brief Allocator that allocates memory from a space heap
   *
   * Note that this allocator may be used to construct dynamic 
   * data structures that allocate memory from the space heap,
   * or even reside in the space heap as a whole.
   *
   * \ingroup FuncMemAllocator
   */
  template<class T> 
  struct space_allocator {
    /// Type of objects the allocator creates. This is identical to \a T.
    typedef T value_type;
    /// Type that can represent the size of the largest object.
    typedef size_t size_type;
    /// Type that can represent the difference between any two pointers.
    typedef ptrdiff_t difference_type;
    /// Type of pointers returned by the allocator.
    typedef T* pointer;
    /// Const version of pointer.
    typedef T const* const_pointer;
    /// Non-const reference to \a T.
    typedef T& reference;
    /// Const reference to \a T.
    typedef T const&  const_reference;
    /// Rebinding helper (returns the type of a similar allocator for type \a U).
    template<class U> struct rebind { 
      /// The allocator type for \a U
      typedef space_allocator<U> other;
    };

    /// The space that we allocate objects from
    Space& space;

    /**
     * \brief Construction
     * @param space The space whose heap to allocate objects from.
     */
    space_allocator(Space& space) throw() : space(space) {}
    /**
     * \brief Copy construction
     * @param al The allocator to copy.
     */
    space_allocator(space_allocator const& al) throw() : space(al.space) {}
    /**
     * \brief Assignment operator
     * @param al The allocator to assign.
     */
    space_allocator& operator =(space_allocator const& al) {
      (void) al;
      assert(&space == &al.space);
      return *this;
    }
    /**
     * \brief Copy from other instantiation
     * @param al The source allocator.
     */
    template<class U> 
    space_allocator(space_allocator<U> const& al) throw() : space(al.space) {}

    /// Convert a reference \a x to a pointer
    pointer address(reference x) const { return &x; }
    /// Convert a const reference \a x to a const pointer
    const_pointer address(const_reference x) const { return &x; }
    /// Returns the largest size for which a call to allocate might succeed.
    size_type max_size(void) const throw() {
      return std::numeric_limits<size_type>::max() / 
        (sizeof(T)>0 ? sizeof(T) : 1);
    }
    /**
     * \brief Allocates storage 
     *
     * Returns a pointer to the first element in a block of storage
     * <tt>count*sizeof(T)</tt> bytes in size. The block is aligned 
     * appropriately for objects of type \a T. Throws the exception 
     * \a bad_alloc if the storage is unavailable.
     */
    pointer allocate(size_type count) {
      return static_cast<pointer>(space.ralloc(sizeof(T)*count));
    }

    /**
     * \brief Allocates storage 
     *
     * Returns a pointer to the first element in a block of storage
     * <tt>count*sizeof(T)</tt> bytes in size. The block is aligned 
     * appropriately for objects of type \a T. Throws the exception 
     * \a bad_alloc if the storage is unavailable.
     * The (unused) parameter could be used as an allocation hint, 
     * but this allocator ignores it.
     */
    pointer allocate(size_type count, const void * const hint) {
      (void) hint;
      return allocate(count);
    }

    /// Deallocates the storage obtained by a call to allocate() with arguments \a count and \a p.
    void deallocate(pointer p, size_type count) {
      space.rfree(static_cast<void*>(p), count);
    }

    /*
     * \brief Constructs an object 
     *
     * Constructs an object of type \a T with the initial value of \a t 
     * at the location specified by \a element. This function calls 
     * the <i>placement new()</i> operator.
     */
    void construct(pointer element, const_reference t) {
      new (element) T(t);
    }

    /// Calls the destructor on the object pointed to by \a element.
    void destroy(pointer element) {
      element->~T();
    }
  };

  /**
   * \brief Tests two space allocators for equality
   *
   * Two allocators are equal when each can release storage allocated 
   * from the other.
   */
  template<class T1, class T2>
  bool operator==(space_allocator<T1> const& al1, 
                  space_allocator<T2> const& al2) throw() {
    return &al1.space == &al2.space;
  }

  /**
   * \brief Tests two space allocators for inequality
   *
   * Two allocators are equal when each can release storage allocated 
   * from the other.
   */
  template<class T1, class T2>
  bool operator!=(space_allocator<T1> const& al1, 
                  space_allocator<T2> const& al2) throw() {
    return &al1.space != &al2.space;
  }


  template<class T> struct region_allocator;

  /**
   * \brief %Region allocator - specialization for \c void.
   *
   * The specialization is needed as the default instantiation fails 
   * for \c void.
   */
  template<> 
  struct region_allocator<void> {
    typedef void* pointer;
    typedef const void* const_pointer;
    typedef void value_type;
    /// Rebinding helper (returns the type of a similar allocator for type \a U)
    template<class U> struct rebind {
      typedef region_allocator<U> other;
    };
  };

  /**
   * \brief Allocator that allocates memory from a region
   *
   * Note that this allocator may be used to construct dynamic data structures
   * that allocate memory from the region, or even reside in the region as a whole.
   *
   * \ingroup FuncMemAllocator
   */
  template<class T> 
  struct region_allocator {
    /// Type of objects the allocator creates. This is identical to \a T.
    typedef T value_type;
    /// Type that can represent the size of the largest object.
    typedef size_t size_type;
    /// Type that can represent the difference between any two pointers.
    typedef ptrdiff_t difference_type;
    /// Type of pointers returned by the allocator.
    typedef T* pointer;
    /// Const version of pointer.
    typedef T const*  const_pointer;
    /// Non-const reference to \a T.
    typedef T&  reference;
    /// Const reference to \a T.
    typedef T const& const_reference;

    /// Rebinding helper (returns the type of a similar allocator for type \a U).
    template<class U> struct rebind { 
      /// The allocator type for \a U
     typedef region_allocator<U> other;
    };

    /// The region that we allocate objects from
    Region& region;

    /**
     * \brief Construction
     * @param region The region to allocate objects from.
     */
    region_allocator(Region& region) throw() 
      : region(region) {}
    /**
     * \brief Copy construction
     * @param al The allocator to copy.
     */
    region_allocator(region_allocator const& al) throw() 
      : region(al.region) {}
    /**
     * \brief Copy from other instantiation.
     * @param al The source allocator.
     */
    template<class U> 
    region_allocator(region_allocator<U> const& al) throw() 
      : region(al.region) {}

    /// Convert a reference \a x to a pointer
    pointer address(reference x) const { return &x; }
    /// Convert a const reference \a x to a const pointer
    const_pointer address(const_reference x) const { return &x; }
    /// Returns the largest size for which a call to allocate might succeed.
    size_type max_size(void) const throw() {
      return std::numeric_limits<size_type>::max() 
        / (sizeof(T)>0 ? sizeof(T) : 1);
    }

     /**
      * \brief Allocates storage 
      *
      * Returns a pointer to the first element in a block of storage
      * <tt>count*sizeof(T)</tt> bytes in size. The block is aligned 
      * appropriately for objects of type \a T. Throws the exception 
      * \a bad_alloc if the storage is unavailable.
      */
    pointer allocate(size_type count) {
      return static_cast<pointer>(region.ralloc(sizeof(T)*count));
    }

     /**
      * \brief Allocates storage 
      *
      * Returns a pointer to the first element in a block of storage
      * <tt>count*sizeof(T)</tt> bytes in size. The block is aligned 
      * appropriately for objects of type \a T. Throws the exception 
      * \a bad_alloc if the storage is unavailable.
      *
      * The (unused) parameter could be used as an allocation hint, 
      * but this allocator ignores it.
      */
    pointer allocate(size_type count, const void * const hint) {
      (void) hint;
      return allocate(count);
    }
    
    /**
     * \brief Deallocates storage
     *
     * Deallocates storage obtained by a call to allocate() with 
     * arguments \a count and \a p. Note that region allocator never 
     * actually deallocates memory (so this function does nothing);
     * the memory is released when the region is destroyed.
     */
    void deallocate(pointer* p, size_type count) {
      region.rfree(static_cast<void*>(p), count);
    }

    /**
     * \brief Constructs an object
     *
     * Constructs an object of type \a T with the initial value 
     * of \a t at the location specified by \a element. This function 
     * calls the <i>placement new()</i> operator.
     */
     void construct(pointer element, const_reference t) {
      new (element) T(t);
    }

    /// Calls the destructor on the object pointed to by \a element.
    void destroy(pointer element) {
      element->~T();
    }
  };

  /*
   * \brief Tests two region allocators for equality
   *
   * Two allocators are equal when each can release storage allocated 
   * from the other.
   */
  template<class T1, class T2>
  bool operator==(region_allocator<T1> const& al1, 
                  region_allocator<T2> const& al2) throw() {
    return &al1.region == &al2.region;
  }

  /*
   * \brief Tests two region allocators for inequality
   *
   * Two allocators are equal when each can release storage allocated
   * from the other.
   */
  template<class T1, class T2>
  bool operator!=(region_allocator<T1> const& al1, 
                  region_allocator<T2> const& al2) throw() {
    return &al1.region != &al2.region;
  }

}

// STATISTICS: kernel-memory
