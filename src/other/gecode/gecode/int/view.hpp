/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2005
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

#include <iostream>

namespace Gecode { namespace Int {

  /**
   * \brief Range iterator for integer views
   *
   * This class provides (by specialization) a range iterator
   * for all integer views.
   *
   * Note that this template class serves only as a specification
   * of the interface of the various specializations.
   *
   * \ingroup TaskActorInt
   */
  template<class View>
  class ViewRanges {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewRanges(void);
    /// Initialize with ranges for view \a x
    ViewRanges(const View& x);
    /// Initialize with ranges for view \a x
    void init(const View& x);
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
   * \brief Value iterator for integer views
   *
   * This class provides a value iterator for all
   * integer views.
   *
   * \ingroup TaskActorInt
   */
  template<class View>
  class ViewValues : public Iter::Ranges::ToValues<ViewRanges<View> > {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewValues(void);
    /// Initialize with values for \a x
    ViewValues(const View& x);
    /// Initialize with values \a x
    void init(const View& x);
    //@}
  };

}}

#include <gecode/int/view/iter.hpp>

namespace Gecode { namespace Int {

  /**
   * \defgroup TaskActorIntView Integer views
   *
   * Integer propagators and branchers compute with integer views.
   * Integer views provide views on integer variable implementations,
   * integer constants, and also allow to scale, translate, and negate
   * variables. Additionally, a special Boolean view is provided that
   * offers convenient and efficient operations for Boolean (0/1)
   * views.
   * \ingroup TaskActorInt
   */
  
  /**
   * \brief Integer view for integer variables
   * \ingroup TaskActorIntView
   */
  class IntView : public VarImpView<IntVar> {
  protected:
    using VarImpView<IntVar>::x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    IntView(void);
    /// Initialize from integer variable \a y
    IntView(const IntVar& y);
    /// Initialize from integer variable \a y
    IntView(IntVarImp* y);
    //@}
    
    /// \name Value access
    //@{
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return median of domain (greatest element not greater than the median)
    int med(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;

    /// Return size (cardinality) of domain
    unsigned int size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    unsigned int width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    unsigned int regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    unsigned int regret_max(void) const;
    //@}
    
    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
    
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
    
    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, int n);
    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, long long int n);
    
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, long long int n);
    
    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, int n);
    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, long long int n);

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
     * Views can be both updated by range and value iterators.
     * Value iterators do not need to be strict in that the same value
     * is allowed to occur more than once in the iterated sequence.
     *
     * The argument \a depends must be true, if the iterator
     * passed as argument depends on the view on which the operation
     * is invoked. In this case, the view is only updated after the
     * iterator has been consumed. Otherwise, the domain might be updated
     * concurrently while following the iterator.
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

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    int min(const Delta& d) const;
    /// Return maximum value just pruned
    int max(const Delta& d) const;
    /// Test whether arbitrary values got pruned
    bool any(const Delta& d) const;
    //@}

    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}
  };

  /**
   * \brief Print integer variable view
   * \relates Gecode::Int::IntView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const IntView& x);
  

  /**
   * \brief Minus integer view
   *
   * A minus integer view \f$m\f$ for an integer view \f$x\f$ provides
   * operations such that \f$m\f$ behaves as \f$-x\f$.
   * \ingroup TaskActorIntView
   */
  class MinusView : public DerivedView<IntView> {
  protected:
    using DerivedView<IntView>::x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    MinusView(void);
    /// Initialize with integer view \a y
    explicit MinusView(const IntView& y);
    //@}
    
    /// \name Value access
    //@{
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return median of domain
    int med(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;
    
    /// Return size (cardinality) of domain
    unsigned int size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    unsigned int width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    unsigned int regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    unsigned int regret_max(void) const;
    //@}
    
    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
    
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

    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, int n);
    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, long long int n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, long long int n);

    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, int n);
    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, long long int n);

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
     * Views can be both updated by range and value iterators.
     * Value iterators do not need to be strict in that the same value
     * is allowed to occur more than once in the iterated sequence.
     *
     * The argument \a depends must be true, if the iterator
     * passed as argument depends on the view on which the operation
     * is invoked. In this case, the view is only updated after the
     * iterator has been consumed. Otherwise, the domain might be updated
     * concurrently while following the iterator.
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
    
    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}
    
    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    int min(const Delta& d) const;
    /// Return maximum value just pruned
    int max(const Delta& d) const;
    /// Test whether arbitrary values got pruned
    bool any(const Delta& d) const;
    //@}
  };

  /**
   * \brief Print integer minus view
   * \relates Gecode::Int::MinusView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const MinusView& x);


  /**
   * \brief Offset integer view
   *
   * An offset integer view \f$o\f$ for an integer view \f$x\f$ and
   * an integer \f$c\f$ provides operations such that \f$o\f$
   * behaves as \f$x+c\f$.
   * \ingroup TaskActorIntView
   */
  class OffsetView : public DerivedView<IntView> {
  protected:
    /// Offset
    int c;
    using DerivedView<IntView>::x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    OffsetView(void);
    /// Initialize with integer view \a y and offset \a c
    OffsetView(const IntView& y, int c);
    //@}
    
    /// \name Value access
    //@{
    /// Return offset
    int offset(void) const;
    /// Change offset to \a n
    void offset(int n);
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return median of domain (greatest element not greater than the median)
    int med(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;
    
    /// Return size (cardinality) of domain
    unsigned int size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    unsigned int width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    unsigned int regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    unsigned int regret_max(void) const;
    //@}
    
    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
    
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

    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, int n);
    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, long long int n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, long long int n);

    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, int n);
    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, long long int n);

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
     * Views can be both updated by range and value iterators.
     * Value iterators do not need to be strict in that the same value
     * is allowed to occur more than once in the iterated sequence.
     *
     * The argument \a depends must be true, if the iterator
     * passed as argument depends on the view on which the operation
     * is invoked. In this case, the view is only updated after the
     * iterator has been consumed. Otherwise, the domain might be updated
     * concurrently while following the iterator.
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

    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    int min(const Delta& d) const;
    /// Return maximum value just pruned
    int max(const Delta& d) const;
    /// Test whether arbitrary values got pruned
    bool any(const Delta& d) const;
    //@}
    
    /// \name Cloning
    //@{
    /// Update this view to be a clone of view \a y
    void update(Space& home, bool share, OffsetView& y);
    //@}
  };

  /**
   * \brief Print integer offset view
   * \relates Gecode::Int::OffsetView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OffsetView& x);
  
  /** \name View comparison
   *  \relates Gecode::Int::OffsetView
   */
  //@{
  /// Test whether views \a x and \a y are the same
  bool same(const OffsetView& x, const OffsetView& y);
  /// Test whether view \a x comes before \a y (arbitrary order)
  bool before(const OffsetView& x, const OffsetView& y);
  //@}

  /**
   * \brief Converter without offsets
   */
  template<class View>
  class NoOffset {
  public:
    /// The view type
    typedef View ViewType;
    /// Pass through \a x
    View& operator ()(View& x);
    /// Update during cloning
    void update(const NoOffset&);
    /// Access offset
    int offset(void) const;
  };

  template<class View>
  forceinline View&
  NoOffset<View>::operator ()(View& x) {
    return x;
  }

  template<class View>
  forceinline void
  NoOffset<View>::update(const NoOffset&) {}

  template<class View>
  forceinline int
  NoOffset<View>::offset(void) const {
    return 0;
  }
  

  /**
   * \brief Converter with fixed offset
   */
  class Offset {
  public:
    /// The view type
    typedef OffsetView ViewType;
    /// The offset
    int c;
    /// Constructor with offset \a off
    Offset(int off = 0);
    /// Return OffsetRefView for \a x
    OffsetView operator()(IntView& x);
    /// Update during cloning
    void update(const Offset& o);
    /// Access offset
    int offset(void) const;
  };

  forceinline
  Offset::Offset(int off) : c(off) {}

  forceinline void
  Offset::update(const Offset& o) { c = o.c; }

  forceinline int
  Offset::offset(void) const { return c; }

  forceinline OffsetView
  Offset::operator ()(IntView& x) {
      return OffsetView(x,c);
  }

  /**
   * \brief Scale integer view (template)
   *
   * A scale integer view \f$s\f$ for an integer view \f$x\f$ and
   * a non-negative integer \f$a\f$ provides operations such that \f$s\f$
   * behaves as \f$a\cdot x\f$.
   *
   * The precision of a scale integer view is defined by the value types
   * \a Val and \a UnsVal. \a Val can be either \c int or \c long \c long 
   * \c int where \a UnsVal must be the unsigned variant of \a Val. The 
   * range which is allowed for the two types is defined by the values in
   * Gecode::Limits.
   *
   * Note that scale integer views currently do not provide operations
   * for updating domains by range iterators.
   *
   * The template is not to be used directly (as it is very clumsy). Use
   * the following instead:
   *  - IntScaleView for scale views with integer precision
   *  - LLongScaleView for scale views with long long precision
   *
   * \ingroup TaskActorIntView
   */
  template<class Val, class UnsVal>
  class ScaleView : public DerivedView<IntView> {
  protected:
    using DerivedView<IntView>::x;
    /// Scale factor
    int a;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ScaleView(void);
    /// Initialize as \f$b\cdot y\f$
    ScaleView(int b, const IntView& y);
    //@}
    
    /// \name Value access
    //@{
    /// Return scale factor of scale view
    int scale(void) const;
    /// Return minimum of domain
    Val min(void) const;
    /// Return maximum of domain
    Val max(void) const;
    /// Return median of domain (greatest element not greater than the median)
    Val med(void) const;
    /// Return assigned value (only if assigned)
    Val val(void) const;
    
    /// Return size (cardinality) of domain
    UnsVal size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    UnsVal width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    UnsVal regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    UnsVal regret_max(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
    /// Test whether \a n is contained in domain
    bool in(Val n) const;
    //@}
    
    /// \name Domain update by value
    //@{
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, Val n);
    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, Val n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, Val n);
    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, Val n);
    /// Restrict domain values to be different from \a n
    ModEvent nq(Space& home, Val n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, Val n);
    //@}
    
    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    Val min(const Delta& d) const;
    /// Return maximum value just pruned
    Val max(const Delta& d) const;
    /// Test whether arbitrary values got pruned
    bool any(const Delta& d) const;
    //@}
    
    /// \name Cloning
    //@{
    /// Update this view to be a clone of view \a y
    void update(Space& home, bool share, ScaleView<Val,UnsVal>& y);
    //@}
  };

  /**
   * \brief Integer-precision integer scale view
   * \ingroup TaskActorIntView
   */
  typedef ScaleView<int,unsigned int> IntScaleView;

  /**
   * \brief Long long-precision integer scale view
   * \ingroup TaskActorIntView
   */
  typedef ScaleView<long long int,unsigned long long int> LLongScaleView;
  
  /**
   * \brief Print integer-precision integer scale view
   * \relates Gecode::Int::ScaleView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const IntScaleView& x);
  
  /**
   * \brief Print long long-precision integer scale view
   * \relates Gecode::Int::ScaleView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const LLongScaleView& x);

  /** \name View comparison
   *  \relates Gecode::Int::ScaleView
   */
  //@{
  /// Test whether views \a x and \a y are the same
  template<class Val, class UnsVal>
  bool same(const ScaleView<Val,UnsVal>& x, const ScaleView<Val,UnsVal>& y);
  /// Test whether view \a x comes before \a y (arbitrary order)
  template<class Val, class UnsVal>
  bool before(const ScaleView<Val,UnsVal>& x, const ScaleView<Val,UnsVal>& y);
  //@}



  /**
   * \brief Constant integer view
   *
   * A constant integer view \f$x\f$ for an integer \f$c\f$ provides
   * operations such that \f$x\f$ behaves as a view assigned to \f$c\f$.
   * \ingroup TaskActorIntView
   */
  class ConstIntView : public ConstView<IntView> {
  protected:
    int x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ConstIntView(void);
    /// Initialize with integer value \a n
    ConstIntView(int n);
    //@}
    
    /// \name Value access
    //@{
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return median of domain (greatest element not greater than the median)
    int med(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;
    
    /// Return size (cardinality) of domain
    unsigned int size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    unsigned int width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    unsigned int regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    unsigned int regret_max(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
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

    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, int n);
    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, long long int n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, long long int n);

    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, int n);
    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, long long int n);

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
     * Views can be both updated by range and value iterators.
     * Value iterators do not need to be strict in that the same value
     * is allowed to occur more than once in the iterated sequence.
     *
     * The argument \a depends must be true, if the iterator
     * passed as argument depends on the view on which the operation
     * is invoked. In this case, the view is only updated after the
     * iterator has been consumed. Otherwise, the domain might be updated
     * concurrently while following the iterator.
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

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    int min(const Delta& d) const;
    /// Return maximum value just pruned
    int max(const Delta& d) const;
    /// Test whether arbitrary values got pruned
    bool any(const Delta& d) const;
    //@}

    /// \name Cloning
    //@{
    /// Update this view to be a clone of view \a y
    void update(Space& home, bool share, ConstIntView& y);
    //@}
  };

  /**
   * \brief Print integer constant integer view
   * \relates Gecode::Int::ConstIntView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ConstIntView& x);

  /**
   * \name View comparison
   * \relates Gecode::Int::ConstIntView
   */
  //@{
  /// Test whether views \a x and \a y are the same
  bool same(const ConstIntView& x, const ConstIntView& y);
  /// Test whether view \a x is before \a y (arbitrary order)
  bool before(const ConstIntView& x, const ConstIntView& y);
  //@}


  /**
   * \brief Zero integer view
   *
   * A zero integer view \f$x\f$ for provides
   * operations such that \f$x\f$ behaves as a view assigned to \f$0\f$.
   * \ingroup TaskActorIntView
   */
  class ZeroIntView : public ConstView<IntView> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ZeroIntView(void);
    //@}
    
    /// \name Value access
    //@{
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return median of domain (greatest element not greater than the median)
    int med(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;
    
    /// Return size (cardinality) of domain
    unsigned int size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    unsigned int width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    unsigned int regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    unsigned int regret_max(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
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

    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, int n);
    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, long long int n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, long long int n);

    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, int n);
    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, long long int n);

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
     * Views can be both updated by range and value iterators.
     * Value iterators do not need to be strict in that the same value
     * is allowed to occur more than once in the iterated sequence.
     *
     * The argument \a depends must be true, if the iterator
     * passed as argument depends on the view on which the operation
     * is invoked. In this case, the view is only updated after the
     * iterator has been consumed. Otherwise, the domain might be updated
     * concurrently while following the iterator.
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

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    int min(const Delta& d) const;
    /// Return maximum value just pruned
    int max(const Delta& d) const;
    /// Test whether arbitrary values got pruned
    bool any(const Delta& d) const;
    //@}
  };

  /**
   * \brief Print integer zero view
   * \relates Gecode::Int::ZeroView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ZeroIntView& x);

  /**
   * \name View comparison
   * \relates Gecode::Int::ZeroIntView
   */
  //@{
  /// Test whether views \a x and \a y are the same
  bool same(const ZeroIntView& x, const ZeroIntView& y);
  //@}

  template<class View> class ViewDiffRanges;

  /**
   * \brief Cached integer view
   *
   * A cached integer view \f$c\f$ for an integer view \f$x\f$ adds operations
   * for cacheing the current domain of \f$x\f$ and comparing the current
   * domain to the cached domain. Cached views make it easy to implement
   * incremental propagation algorithms.
   *
   * \ingroup TaskActorIntView
   */
  template<class View>
  class CachedView : public DerivedView<View> {
    friend class ViewDiffRanges<View>;
  protected:
    using DerivedView<View>::x;
    /// First cached range
    RangeList* _firstRange;
    /// Last cached range
    RangeList* _lastRange;
    /// Size of cached domain
    unsigned int _size;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    CachedView(void);
    /// Initialize with integer view \a y
    explicit CachedView(const View& y);
    //@}
    
    /// \name Value access
    //@{
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return median of domain (greatest element not greater than the median)
    int med(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;
    
    /// Return size (cardinality) of domain
    unsigned int size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    unsigned int width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    unsigned int regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    unsigned int regret_max(void) const;
    //@}
    
    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
    
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

    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, int n);
    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, long long int n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, long long int n);

    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, int n);
    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, long long int n);

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
     * Views can be both updated by range and value iterators.
     * Value iterators do not need to be strict in that the same value
     * is allowed to occur more than once in the iterated sequence.
     *
     * The argument \a depends must be true, if the iterator
     * passed as argument depends on the view on which the operation
     * is invoked. In this case, the view is only updated after the
     * iterator has been consumed. Otherwise, the domain might be updated
     * concurrently while following the iterator.
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

    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    int min(const Delta& d) const;
    /// Return maximum value just pruned
    int max(const Delta& d) const;
    /// Test whether arbitrary values got pruned
    bool any(const Delta& d) const;
    //@}

    /// \name Domain cache operations
    //@{
    /// Initialize cache to set \a s
    void initCache(Space& home, const IntSet& s);
    /// Update cache to current domain
    void cache(Space& home);
    /// Check whether cache differs from current domain
    bool modified(void) const;
    //@}
    
    /// \name Cloning
    //@{
    /// Update this view to be a clone of view \a y
    void update(Space& home, bool share, CachedView<View>& y);
    //@}
  };

  /**
   * \brief Print integer offset view
   * \relates Gecode::Int::CachedView
   */
  template<class Char, class Traits, class View>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const CachedView<View>& x);
  
  /** \name View comparison
   *  \relates Gecode::Int::CachedView
   */
  //@{
  /// Test whether views \a x and \a y are the same
  template<class View>
  bool same(const CachedView<View>& x, const CachedView<View>& y);
  /// Test whether view \a x comes before \a y (arbitrary order)
  template<class View>
  bool before(const CachedView<View>& x, const CachedView<View>& y);
  //@}

  /**
   * \brief %Range iterator for cached integer views
   *
   * This iterator iterates the difference between the cached domain
   * and the current domain of an integer view.
   *
   * \ingroup TaskActorInt
   */
  template<class View>
  class ViewDiffRanges
    : public Iter::Ranges::Diff<Iter::Ranges::RangeList,ViewRanges<View> > {
    typedef Iter::Ranges::Diff<Iter::Ranges::RangeList,ViewRanges<View> > 
      Super;
  protected:
    /// Cached domain iterator
    Iter::Ranges::RangeList cr;
    /// Current domain iterator
    ViewRanges<View> dr;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ViewDiffRanges(void);
    /// Initialize with ranges for view \a x
    ViewDiffRanges(const CachedView<View>& x);
    /// Initialize with ranges for view \a x
    void init(const CachedView<View>& x);
    //@}
  };

  /**
   * \brief Boolean view for Boolean variables
   *
   * Provides convenient and efficient operations for Boolean views.
   * \ingroup TaskActorIntView
   */
  class BoolView : public VarImpView<BoolVar> {
  protected:
    using VarImpView<BoolVar>::x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    BoolView(void);
    /// Initialize from Boolean variable \a y
    BoolView(const BoolVar& y);
    /// Initialize from Boolean variable implementation \a y
    BoolView(BoolVarImp* y);
    //@}
    
    /// \name Domain status access
    //@{
    /// How many bits does the status have
    static const int BITS = BoolVarImp::BITS;
    /// Status of domain assigned to zero
    static const BoolStatus ZERO = BoolVarImp::ZERO;
    /// Status of domain assigned to one
    static const BoolStatus ONE  = BoolVarImp::ONE;
    /// Status of domain not yet assigned
    static const BoolStatus NONE = BoolVarImp::NONE;
    /// Return current domain status
    BoolStatus status(void) const;
    //@}
    
    /// \name Value access
    //@{
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return median of domain (greatest element not greater than the median)
    int med(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;
    
    /// Return size (cardinality) of domain
    unsigned int size(void) const;
    /// Return width of domain (distance between maximum and minimum)
    unsigned int width(void) const;
    /// Return regret of domain minimum (distance to next larger value)
    unsigned int regret_min(void) const;
    /// Return regret of domain maximum (distance to next smaller value)
    unsigned int regret_max(void) const;
    //@}
    
    /// \name Domain tests
    //@{
    /// Test whether domain is a range
    bool range(void) const;
    /// Test whether \a n is contained in domain
    bool in(int n) const;
    /// Test whether \a n is contained in domain
    bool in(long long int n) const;
    //@}

    /// \name Boolean domain tests
    //@{
    /// Test whether view is assigned to be zero
    bool zero(void) const;
    /// Test whether view is assigned to be one
    bool one(void) const;
    /// Test whether view is not yet assigned
    bool none(void) const;
    //@}
    
    /// \name Boolean assignment operations
    //@{
    /// Try to assign view to one
    ModEvent one(Space& home);
    /// Try to assign view to zero
    ModEvent zero(Space& home);
    /// Assign not yet assigned view to one
    ModEvent one_none(Space& home);
    /// Assign not yet assigned view to zero
    ModEvent zero_none(Space& home);
    //@}

    /// \name Domain update by value
    //@{
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, int n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, long long int n);

    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, int n);
    /// Restrict domain values to be less than \a n
    ModEvent le(Space& home, long long int n);
    
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, long long int n);

    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, int n);
    /// Restrict domain values to be greater than \a n
    ModEvent gr(Space& home, long long int n);
    
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
     * Views can be both updated by range and value iterators.
     * Value iterators do not need to be strict in that the same value
     * is allowed to occur more than once in the iterated sequence.
     *
     * The argument \a depends must be true, if the iterator
     * passed as argument depends on the view on which the operation
     * is invoked. In this case, the view is only updated after the
     * iterator has been consumed. Otherwise, the domain might be updated
     * concurrently while following the iterator.
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

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    int min(const Delta& d) const;
    /// Return maximum value just pruned
    int max(const Delta& d) const;
    /// Test whether arbitrary values got pruned
    bool any(const Delta& d) const;
    /// Test whether a view has been assigned to zero
    static bool zero(const Delta& d);
    /// Test whether a view has been assigned to one
    static bool one(const Delta& d);
    //@}
    
    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}
  };

  /**
   * \brief Print Boolean view
   * \relates Gecode::Int::BoolView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const BoolView& x);
  


  /**
   * \brief Negated Boolean view
   *
   * A negated Boolean view \f$n\f$ for a Boolean view \f$b\f$
   * provides operations such that \f$n\f$
   * behaves as \f$\neg b\f$.
   * \ingroup TaskActorIntView
   */
  class NegBoolView : public DerivedView<BoolView> {
  protected:
    using DerivedView<BoolView>::x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    NegBoolView(void);
    /// Initialize with Boolean view \a y
    explicit NegBoolView(const BoolView& y);
    //@}
    
    /// \name Domain status access
    //@{
    /// How many bits does the status have
    static const int BITS = BoolView::BITS;
    /// Status of domain assigned to zero
    static const BoolStatus ZERO = BoolView::ONE;
    /// Status of domain assigned to one
    static const BoolStatus ONE  = BoolView::ZERO;
    /// Status of domain not yet assigned
    static const BoolStatus NONE = BoolView::NONE;
    /// Return current domain status
    BoolStatus status(void) const;
    //@}
    
    /// \name Boolean domain tests
    //@{
    /// Test whether view is assigned to be zero
    bool zero(void) const;
    /// Test whether view is assigned to be one
    bool one(void) const;
    /// Test whether view is not yet assigned
    bool none(void) const;
    //@}
    
    /// \name Boolean assignment operations
    //@{
    /// Try to assign view to one
    ModEvent one(Space& home);
    /// Try to assign view to zero
    ModEvent zero(Space& home);
    /// Assign not yet assigned view to one
    ModEvent one_none(Space& home);
    /// Assign not yet assigned view to zero
    ModEvent zero_none(Space& home);
    //@}
    
    /// \name Value access
    //@{
    /// Return minimum of domain
    int min(void) const;
    /// Return maximum of domain
    int max(void) const;
    /// Return assigned value (only if assigned)
    int val(void) const;
    //@}
    
    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    int min(const Delta& d) const;
    /// Return maximum value just pruned
    int max(const Delta& d) const;
    /// Test whether arbitrary values got pruned
    bool any(const Delta& d) const;
    /// Test whether a view has been assigned to zero
    static bool zero(const Delta& d);
    /// Test whether a view has been assigned to one
    static bool one(const Delta& d);
    //@}
  };

  /**
   * \brief Print negated Boolean view
   * \relates Gecode::Int::NegBoolView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const NegBoolView& x);

}}

#include <gecode/int/var/int.hpp>
#include <gecode/int/var/bool.hpp>

#include <gecode/int/view/int.hpp>

#include <gecode/int/view/constint.hpp>
#include <gecode/int/view/zero.hpp>
#include <gecode/int/view/minus.hpp>
#include <gecode/int/view/offset.hpp>
#include <gecode/int/view/scale.hpp>
#include <gecode/int/view/cached.hpp>

#include <gecode/int/view/bool.hpp>

#include <gecode/int/view/neg-bool.hpp>

#include <gecode/int/view/print.hpp>
#include <gecode/int/var/print.hpp>

namespace Gecode { namespace Int {

  /**
   * \defgroup TaskActorIntTest Testing relations between integer views
   * \ingroup TaskActorInt
   */

  //@{
  /// Result of testing relation
  enum RelTest {
    RT_FALSE = 0, ///< Relation does not hold
    RT_MAYBE = 1, ///< Relation may hold or not
    RT_TRUE  = 2  ///< Relation does hold
  };
  
  /// Test whether views \a x and \a y are equal (use bounds information)
  template<class View> RelTest rtest_eq_bnd(View x, View y);
  /// Test whether views \a x and \a y are equal (use full domain information)
  template<class View> RelTest rtest_eq_dom(View x, View y);
  /// Test whether view \a x and integer \a n are equal (use bounds information)
  template<class View> RelTest rtest_eq_bnd(View x, int n);
  /// Test whether view \a x and integer \a n are equal (use full domain information)
  template<class View> RelTest rtest_eq_dom(View x, int n);
  
  /// Test whether views \a x and \a y are different (use bounds information)
  template<class View> RelTest rtest_nq_bnd(View x, View y);
  /// Test whether views \a x and \a y are different (use full domain information)
  template<class View> RelTest rtest_nq_dom(View x, View y);
  /// Test whether view \a x and integer \a n are different (use bounds information)
  template<class View> RelTest rtest_nq_bnd(View x, int n);
  /// Test whether view \a x and integer \a n are different (use full domain information)
  template<class View> RelTest rtest_nq_dom(View x, int n);

  /// Test whether view \a x is less or equal than view \a y
  template<class View> RelTest rtest_lq(View x, View y);
  /// Test whether view \a x is less or equal than integer \a n
  template<class View> RelTest rtest_lq(View x, int n);
  
  /// Test whether view \a x is less than view \a y
  template<class View> RelTest rtest_le(View x, View y);
  /// Test whether view \a x is less than integer \a n
  template<class View> RelTest rtest_le(View x, int n);
  
  /// Test whether view \a x is greater or equal than view \a y
  template<class View> RelTest rtest_gq(View x, View y);
  /// Test whether view \a x is greater or equal than integer \a n
  template<class View> RelTest rtest_gq(View x, int n);
  
  /// Test whether view \a x is greater than view \a y
  template<class View> RelTest rtest_gr(View x, View y);
  /// Test whether view \a x is greater than integer \a n
  template<class View> RelTest rtest_gr(View x, int n);
  //@}


  /**
   * \brief Boolean tests
   *
   */
  enum BoolTest {
    BT_NONE, ///< No sharing
    BT_SAME, ///< Same variable
    BT_COMP  ///< Same variable but complement
  };
  
  /**
   * \name Test sharing between Boolean and negated Boolean views
   * \relates BoolView NegBoolView
     */
  //@{
  /// Test whether views \a b0 and \a b1 are the same
  BoolTest bool_test(const BoolView& b0, const BoolView& b1);
  /// Test whether views \a b0 and \a b1 are complementary
  BoolTest bool_test(const BoolView& b0, const NegBoolView& b1);
  /// Test whether views \a b0 and \a b1 are complementary
  BoolTest bool_test(const NegBoolView& b0, const BoolView& b1);
  /// Test whether views \a b0 and \a b1 are the same
  BoolTest bool_test(const NegBoolView& b0, const NegBoolView& b1);
  //@}
  
}}

#include <gecode/int/view/rel-test.hpp>
#include <gecode/int/view/bool-test.hpp>

// STATISTICS: int-var
