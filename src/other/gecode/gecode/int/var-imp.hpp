/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Guido Tack <tack@gecode.org>
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

#include <cmath>

namespace Gecode { namespace Int {

  class IntVarImp;
  class BoolVarImp;

  /**
   * \brief Integer delta information for advisors
   *
   * Note that the same delta information is used for both
   * integer and Boolean variables and views.
   */
  class IntDelta : public Delta {
    friend class IntVarImp;
    friend class BoolVarImp;
  private:
    int _min; ///< Minimum value just pruned
    int _max; ///< Largest value just pruned
  public:
    /// Create integer delta as providing no information
    IntDelta(void);
    /// Create integer delta with \a min and \a max
    IntDelta(int min, int max);
    /// Create integer delta with \a min
    IntDelta(int min);
  private:
    /// Return minimum
    int min(void) const;
    /// Return maximum
    int max(void) const;
    /// Test whether any domain change has happened
    bool any(void) const;
  };

}}

#include <gecode/int/var-imp/delta.hpp>

namespace Gecode { namespace Int {

  class IntVarImpFwd;
  class IntVarImpBwd;

  /**
   * \brief Integer variable implementation
   *
   * \ingroup Other
   */
  class IntVarImp : public IntVarImpBase {
    friend class IntVarImpFwd;
    friend class IntVarImpBwd;
  protected:
    /**
     * \brief Lists of ranges (intervals)
     *
     * Range lists are doubly-linked storing the pointer to both
     * the next and the previous element in a single pointer.
     * That means that the next element is only available when
     * the previous element is supplied as additional information.
     * The same holds true for access to the previous element.
     */
    class RangeList : public FreeList {
    protected:
      /// Minimum of range
      int _min;
      /// Maximum of range
      int _max;
    public:
      /// \name Constructors
      //@{
      /// Default constructor (noop)
      RangeList(void);
      /// Initialize with minimum \a min and maximum \a max
      RangeList(int min, int max);
      /// Initialize with minimum \a min and maximum \a max and predecessor \a p and successor \a n
      RangeList(int min, int max, RangeList* p, RangeList* n);
      //@}

      /// \name Access
      //@{
      /// Return minimum
      int min(void) const;
      /// Return maximum
      int max(void) const;
      /// Return width (distance between maximum and minimum)
      unsigned int width(void) const;

      /// Return next element (from previous \a p)
      RangeList* next(const RangeList* p) const;
      /// Return previous element (from next \a n)
      RangeList* prev(const RangeList* n) const;
      //@}

      /// \name Update
      //@{
      /// Set minimum to \a n
      void min(int n);
      /// Set maximum to \a n
      void max(int n);

      /// Set previous element to \a p and next element to \a n
      void prevnext(RangeList* p, RangeList* n);
      /// Set next element from \a o to \a n
      void next(RangeList* o, RangeList* n);
      /// Set previous element from \a o to \a n
      void prev(RangeList* o, RangeList* n);
      /// Restore simple link to next element (so that it becomes a true free list)
      void fix(RangeList* n);
      //@}

      /// \name Memory management
      //@{
      /**
       * \brief Free memory for all elements between this and \a l (inclusive)
       *
       * \a p must be the pointer to the previous element of \c this.
       */
      void dispose(Space& home, RangeList* p, RangeList* l);
      /**
       * \brief Free memory for all elements between this and \a l (inclusive)
       *
       * This routine assumes that the list has already been fixed.
       */
      void dispose(Space& home, RangeList* l);
      /// Free memory for this element
      void dispose(Space& home);

      /// Allocate memory from space
      static void* operator new(size_t s, Space& home);
      /// Placement new
      static void* operator new(size_t s, void* p);
      /// No-op (for exceptions)
      static void  operator delete(void*);
      /// No-op (use dispose instead)
      static void  operator delete(void*, Space&);
      /// No-op (use dispose instead)
      static void  operator delete(void*, void*);
      //@}
    };

    /**
     * \brief Domain information
     *
     * Provides fast access to minimum and maximum of the
     * entire domain and links to the first element
     * of a RangeList defining the domain.
     */
    RangeList dom;
    /// Link the last element
    RangeList* _lst;
    /// Return first element of rangelist
    RangeList* fst(void) const;
    /// Set first element of rangelist
    void fst(RangeList* f);
    /// Return last element of rangelist
    RangeList* lst(void) const;
    /// Set last element of rangelist
    void lst(RangeList* l);
    /// Size of holes in the domain
    unsigned int holes;

  protected:
    /// Constructor for cloning \a x
    IntVarImp(Space& home, bool share, IntVarImp& x);
  public:
    /// Initialize with range domain
    IntVarImp(Space& home, int min, int max);
    /// Initialize with domain specified by \a d
    IntVarImp(Space& home, const IntSet& d);

    /// \name Value access
    //@{
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;
    /// Return median of domain (greatest element not greater than the median)
    GECODE_INT_EXPORT int med(void) const;

    /// Return size (cardinality) of domain
    unsigned int size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    unsigned int width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    unsigned int regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    unsigned int regret_max(void) const;
    //@}

  private:
    /// Test whether \a n is contained in domain (full domain)
    GECODE_INT_EXPORT bool in_full(int n) const;

  public:
    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
    /// Test whether variable is assigned
    bool assigned(void) const;

    /// Test whether \a n is contained in domain
    bool in(int n) const;
    /// Test whether \a n is contained in domain
    bool in(long long int n) const;
    //@}

  protected:
    /// \name Range list access for iteration
    //@{
    /// Return range list for forward iteration
    const RangeList* ranges_fwd(void) const;
    /// Return range list for backward iteration
    const RangeList* ranges_bwd(void) const;
    //@}

  private:
    /// Test whether \a n is closer to the minimum or maximum
    bool closer_min(int b) const;
    /// \name Domain update by value (full domain)
    //@{
    /// Restrict domain values to be less or equal than \a n
    GECODE_INT_EXPORT ModEvent lq_full(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    GECODE_INT_EXPORT ModEvent gq_full(Space& home, int n);
    /// Restrict domain values to be equal to current minimum of domain
    GECODE_INT_EXPORT ModEvent eq_full(Space& home, int n);
    /// Restrict domain values to be different from \a n
    GECODE_INT_EXPORT ModEvent nq_full(Space& home, int n);
    //@}
  public:
    /// \name Domain update by value
    //@{
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, int n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, long long int n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, long long int n);

    /// Restrict domain values to be different from \a n
    ModEvent nq(Space& home, int n);
    /// Restrict domain values to be different from \a n
    ModEvent nq(Space& home, long long int n);

    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, int n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, long long int n);
    //@}

    /**
     * \name Domain update by iterator
     *
     * Variable domains can be both updated by range and value iterators.
     * Value iterators do not need to be strict in that the same value
     * is allowed to occur more than once in the iterated sequence.
     *
     * The argument \a depends must be true, if the iterator
     * passed as argument depends on the variable implementation
     * on which the operation is invoked. In this case, the variable
     * implementation is only updated after the iterator has been
     * consumed. Otherwise, the domain might be updated concurrently
     * while following the iterator.
     *
     */
    //@{
    /// Replace domain by ranges described by \a i
    template<class I>
    ModEvent narrow_r(Space& home, I& i, bool depends=true);
    /// Intersect domain with ranges described by \a i
    template<class I>
    ModEvent inter_r(Space& home, I& i, bool depends=true);
    /// Remove from domain the ranges described by \a i
    template<class I>
    ModEvent minus_r(Space& home, I& i, bool depends=true);
    /// Replace domain by values described by \a i
    template<class I>
    ModEvent narrow_v(Space& home, I& i, bool depends=true);
    /// Intersect domain with values described by \a i
    template<class I>
    ModEvent inter_v(Space& home, I& i, bool depends=true);
    /// Remove from domain the values described by \a i
    template<class I>
    ModEvent minus_v(Space& home, I& i, bool depends=true);
    //@}

    /// \name Dependencies
    //@{
    /**
     * \brief Subscribe propagator \a p with propagation condition \a pc to variable
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     *
     */
    void subscribe(Space& home, Propagator& p, PropCond pc, bool schedule=true);
    /// Cancel subscription of propagator \a p with propagation condition \a pc
    void cancel(Space& home, Propagator& p, PropCond pc);
    /// Subscribe advisor \a a to variable
    void subscribe(Space& home, Advisor& a);
    /// Cancel subscription of advisor \a a
    void cancel(Space& home, Advisor& a);
    //@}

    /// \name Variable implementation-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}


  private:
    /// Return copy of not-yet copied variable
    GECODE_INT_EXPORT IntVarImp* perform_copy(Space& home, bool share);
  public:
    /// \name Cloning
    //@{
    /// Return copy of this variable
    IntVarImp* copy(Space& home, bool share);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    static int min(const Delta& d);
    /// Return maximum value just pruned
    static int max(const Delta& d);
    /// Test whether arbitrary values got pruned
    static bool any(const Delta& d);
    //@}
  };


  /**
   * \brief Range iterator for ranges of integer variable implementation
   *
   */
  class IntVarImpFwd {
  private:
    /// Previous range
    const IntVarImp::RangeList* p;
    /// Current range
    const IntVarImp::RangeList* c;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    IntVarImpFwd(void);
    /// Initialize with ranges from variable implementation \a x
    IntVarImpFwd(const IntVarImp* x);
    /// Initialize with ranges from variable implementation \a x
    void init(const IntVarImp* x);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
    /// Move iterator to next range (if possible)
    void operator ++(void);
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
  };

  /**
   * \brief Backward iterator for ranges of integer variable implementations
   *
   * Note that this iterator is not a range iterator as the ranges
   * are not iterated in increasing but in decreasing order.
   *
   */
  class IntVarImpBwd {
  private:
    /// Next range
    const IntVarImp::RangeList* n;
    /// Current range
    const IntVarImp::RangeList* c;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    IntVarImpBwd(void);
    /// Initialize with ranges from variable implementation \a x
    IntVarImpBwd(const IntVarImp* x);
    /// Initialize with ranges from variable implementation \a x
    void init(const IntVarImp* x);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
    /// Move iterator to previous range (if possible)
    void operator ++(void);
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
  };

}}

#include <gecode/int/var-imp/int.hpp>

namespace Gecode {

  class IntVar;
  class BoolVar;
}

namespace Gecode { namespace Int {

  /// Type for status of a Boolean variable
  typedef unsigned int BoolStatus;

  /**
   * \brief Boolean variable implementation
   *
   * \ingroup Other
   */
  class BoolVarImp : public BoolVarImpBase {
    friend class ::Gecode::BoolVar;
  private:
    /**
     * \brief Domain information
     *
     * - The bit at position 0 corresponds to the minimum
     * - The bit at position 1 corresponds to the maximum
     * - Interpreted as an unsigned, that is: 0 represents
     *   a variable assigned to 0, 3 represents a variable
     *   assigned to 1, and 2 represents an unassigned variable.
     * - No other values are possible.
     */

    GECODE_INT_EXPORT static BoolVarImp s_one;
    GECODE_INT_EXPORT static BoolVarImp s_zero;

    /// Constructor for cloning \a x
    BoolVarImp(Space& home, bool share, BoolVarImp& x);
    /// Initialize static instance assigned to \a n
    BoolVarImp(int n);
  public:
    /// Initialize with range domain
    BoolVarImp(Space& home, int min, int max);

    /// \name Domain status access
    //@{
    /// How many bits does the status have
    static const int BITS = 2;
    /// Status of domain assigned to zero
    static const BoolStatus ZERO = 0;
    /// Status of domain assigned to one
    static const BoolStatus ONE  = 3;
    /// Status of domain not yet assigned
    static const BoolStatus NONE = 2;
    /// Return current domain status
    BoolStatus status(void) const;
    //@}

    /// \name Value access
    //@{
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;
    /// Return median of domain (greatest element not greater than the median)
    int med(void) const;

    /// Return size (cardinality) of domain
    unsigned int size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    unsigned int width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    unsigned int regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    unsigned int regret_max(void) const;
    //@}

    /// \name Boolean domain tests
    //@{
    /// Test whether variable is assigned to zero
    bool zero(void) const;
    /// Test whether variable is assigned to one
    bool one(void) const;
    /// Test whether variable is not yet assigned
    bool none(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
    /// Test whether variable is assigned
    bool assigned(void) const;

    /// Test whether \a n is contained in domain
    bool in(int n) const;
    /// Test whether \a n is contained in domain
    bool in(long long int n) const;
    //@}

    /// \name Domain update by value
    //@{
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, int n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, long long int n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, long long int n);

    /// Restrict domain values to be different from \a n
    ModEvent nq(Space& home, int n);
    /// Restrict domain values to be different from \a n
    ModEvent nq(Space& home, long long int n);

    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, int n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, long long int n);
    //@}

    /**
     * \name Domain update by iterator
     *
     * Variable domains can be both updated by range and value iterators.
     * Value iterators do not need to be strict in that the same value
     * is allowed to occur more than once in the iterated sequence.
     *
     * The argument \a depends must be true, if the iterator
     * passed as argument depends on the variable implementation
     * on which the operation is invoked. In this case, the variable
     * implementation is only updated after the iterator has been
     * consumed. Otherwise, the domain might be updated concurrently
     * while following the iterator.
     *
     */
    //@{
    /// Replace domain by ranges described by \a i
    template<class I>
    ModEvent narrow_r(Space& home, I& i, bool depends=true);
    /// Intersect domain with ranges described by \a i
    template<class I>
    ModEvent inter_r(Space& home, I& i, bool depends=true);
    /// Remove from domain the ranges described by \a i
    template<class I>
    ModEvent minus_r(Space& home, I& i, bool depends=true);
    /// Replace domain by values described by \a i
    template<class I>
    ModEvent narrow_v(Space& home, I& i, bool depends=true);
    /// Intersect domain with values described by \a i
    template<class I>
    ModEvent inter_v(Space& home, I& i, bool depends=true);
    /// Remove from domain the values described by \a i
    template<class I>
    ModEvent minus_v(Space& home, I& i, bool depends=true);
    //@}

    /// \name Boolean update operations
    //@{
    /// Assign variable to zero
    ModEvent zero(Space& home);
    /// Assign variable to one
    ModEvent one(Space& home);
    /// Assign unassigned variable to zero
    GECODE_INT_EXPORT ModEvent zero_none(Space& home);
    /// Assign unassigned variable to one
    GECODE_INT_EXPORT ModEvent one_none(Space& home);
    //@}

  public:
    /// \name Dependencies
    //@{
    /**
     * \brief Subscribe propagator \a p to variable with propagation condition \a pc
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     *
     * The propagation condition \a pc can be a propagation condition
     * for integer variables, which will be mapped to PC_BOOL_VAL.
     */
    void subscribe(Space& home, Propagator& p, PropCond pc, bool schedule=true);
    /**
     * \brief Cancel subscription of propagator \a p with propagation condition \a pc
     *
     * The propagation condition \a pc can be a propagation condition
     * for integer variables, which will be mapped to PC_BOOL_VAL.
     */
    void cancel(Space& home, Propagator& p, PropCond pc);
    /// Subscribe advisor \a a to variable
    void subscribe(Space& home, Advisor& a);
    /// Cancel subscription of advisor \a a
    void cancel(Space& home, Advisor& a);
    //@}

    /// \name Variable implementation-dependent propagator support
    //@{
    /**
     * \brief Schedule propagator \a p with modification event \a me
     *
     * The modfication event \a me can be a modification event for
     * integer variables, however \a me will be ignored if it is not
     * ME_INT_VAL (or ME_BOOL_VAL).
     */
    static void schedule(Space& home, Propagator& p, ModEvent me);
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}

    /// \name Support for delta information for advisors
    //@{
    /// Return modification event
    static ModEvent modevent(const Delta& d);
    /// Return minimum value just pruned
    static int min(const Delta& d);
    /// Return maximum value just pruned
    static int max(const Delta& d);
    /// Test whether arbitrary values got pruned
    static bool any(const Delta& d);
    /// Test whether a variable has been assigned to zero
    static bool zero(const Delta& d);
    /// Test whether a variable has been assigned to one
    static bool one(const Delta& d);
    //@}

    /// \name Cloning
    //@{
    /// Return copy of this variable
    BoolVarImp* copy(Space& home, bool share);
    //@}

  };

}}

#include <gecode/int/var-imp/bool.hpp>

// STATISTICS: int-var

