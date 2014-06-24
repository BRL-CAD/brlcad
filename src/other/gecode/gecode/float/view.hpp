/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2005
 *     Vincent Barichard, 2012
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

namespace Gecode { namespace Float {

  /**
   * \defgroup TaskActorFloatView Float views
   *
   * Float propagators and branchers compute with float views.
   * Float views provide views on float variable implementations.
   * \ingroup TaskActorFloat
   */

  /**
   * \brief Float view for float variables
   * \ingroup TaskActorFloatView
   */
  class FloatView : public VarImpView<FloatVar> {
  protected:
    using VarImpView<FloatVar>::x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    FloatView(void);
    /// Initialize from float variable \a y
    FloatView(const FloatVar& y);
    /// Initialize from float variable \a y
    FloatView(FloatVarImp* y);
    //@}

    /// \name Value access
    //@{
    /// Return domain
    FloatVal domain(void) const;
    /// Return minimum of domain
    FloatNum min(void) const;
    /// Return maximum of domain
    FloatNum max(void) const;
    /// Return median of domain (closest representation)
    FloatNum med(void) const;
    /**
     * \brief Return assigned value
     *
     * Throws an exception of type Float::ValOfUnassignedVar if variable
     * is not yet assigned.
     *
     */
    FloatVal val(void) const;

    /// Return size of domain (distance between maximum and minimum)
    FloatNum size(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether 0 is contained in domain
    bool zero_in(void) const;
    /// Test whether \a n is contained in domain
    bool in(FloatNum n) const;
    /// Test whether \a n is contained in domain
    bool in(const FloatVal& n) const;
    //@}

    /// \name Domain update by value
    //@{
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, int n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, FloatNum n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, FloatVal n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, FloatNum n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, FloatVal n);

    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, int n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, FloatNum n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, const FloatVal& n);

    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    FloatNum min(const Delta& d) const;
    /// Return maximum value just pruned
    FloatNum max(const Delta& d) const;
    //@}

    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}
  };

  /**
   * \brief Print float variable view
   * \relates Gecode::Float::FloatView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const FloatView& x);

  /**
   * \brief Minus float view
   *
   * A minus float view \f$m\f$ for an float view \f$x\f$ provides
   * operations such that \f$m\f$ behaves as \f$-x\f$.
   * \ingroup TaskActorFloatView
   */
  class MinusView : public DerivedView<FloatView> {
  protected:
    using DerivedView<FloatView>::x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    MinusView(void);
    /// Initialize with float view \a y
    explicit MinusView(const FloatView& y);
    //@}

    /// \name Value access
    //@{
    /// Return domain
    FloatVal domain(void) const;
    /// Return minimum of domain
    FloatNum min(void) const;
    /// Return maximum of domain
    FloatNum max(void) const;
    /// Return median of domain (closest representation)
    FloatNum med(void) const;
    /**
     * \brief Return assigned value
     *
     * Throws an exception of type Float::ValOfUnassignedVar if variable
     * is not yet assigned.
     *
     */
    FloatVal val(void) const;

    /// Return size of domain (distance between maximum and minimum)
    FloatNum size(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether 0 is contained in domain
    bool zero_in(void) const;
    /// Test whether \a n is contained in domain
    bool in(FloatNum n) const;
    /// Test whether \a n is contained in domain
    bool in(const FloatVal& n) const;
    //@}

    /// \name Domain update by value
    //@{
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, int n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, FloatNum n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, FloatVal n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, FloatNum n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, FloatVal n);

    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, int n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, FloatNum n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, const FloatVal& n);

    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    FloatNum min(const Delta& d) const;
    /// Return maximum value just pruned
    FloatNum max(const Delta& d) const;
    //@}

    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}
  };

  /**
   * \brief Print float minus view
   * \relates Gecode::Float::MinusView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const MinusView& x);


  /**
   * \brief OffsetView float view
   *
   * An offset float view \f$o\f$ for an float view \f$x\f$ and
   * an float \f$c\f$ provides operations such that \f$o\f$
   * behaves as \f$x+c\f$.
   * \ingroup TaskActorFloatView
   */
  class OffsetView : public DerivedView<FloatView> {
  protected:
    /// Offset
    FloatNum c;
    using DerivedView<FloatView>::x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    OffsetView(void);
    /// Initialize with float view \a y and an offset c
    explicit OffsetView(const FloatView& y, FloatNum c);
    //@}

    /// \name Value access
    //@{
    /// Return offset
    FloatNum offset(void) const;
    /// Change offset to be \a n
    void offset(FloatNum n);
    /// Return domain
    FloatVal domain(void) const;
    /// Return minimum of domain
    FloatNum min(void) const;
    /// Return maximum of domain
    FloatNum max(void) const;
    /// Return median of domain (closest representation)
    FloatNum med(void) const;
    /**
     * \brief Return assigned value
     *
     * Throws an exception of type Float::ValOfUnassignedVar if variable
     * is not yet assigned.
     *
     */
    FloatVal val(void) const;

    /// Return size of domain (distance between maximum and minimum)
    FloatNum size(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether 0 is contained in domain
    bool zero_in(void) const;
    /// Test whether \a n is contained in domain
    bool in(FloatNum n) const;
    /// Test whether \a n is contained in domain
    bool in(const FloatVal& n) const;
    //@}

    /// \name Domain update by value
    //@{
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, int n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, FloatNum n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, FloatVal n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, FloatNum n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, FloatVal n);

    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, int n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, FloatNum n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, const FloatVal& n);

    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    FloatNum min(const Delta& d) const;
    /// Return maximum value just pruned
    FloatNum max(const Delta& d) const;
    //@}

    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}

    /// \name Cloning
    //@{
    void update(Space& home, bool share, OffsetView& y);
    //@}
  };

  /**
   * \brief Print float offset view
   * \relates Gecode::Float::OffsetView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OffsetView& x);

  /**
   * \brief Scale float view
   *
   * A scale float view \f$s\f$ for a float view \f$x\f$ and
   * a non-negative float \f$a\f$ provides operations such that \f$s\f$
   * behaves as \f$a\cdot x\f$.
   *
   * \ingroup TaskActorFloatView
   */
  class ScaleView : public DerivedView<FloatView> {
  protected:
    using DerivedView<FloatView>::x;
    /// Scale factor
    FloatVal a;

  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ScaleView(void);
    /// Initialize as \f$b\cdot y\f$
    ScaleView(FloatVal b, const FloatView& y);
    //@}
    
    /// \name Value access
    //@{
    /// Return domain
    FloatVal domain(void) const;
    /// Return scale factor of scale view
    FloatVal scale(void) const;
    /// Return minimum of domain
    FloatNum min(void) const;
    /// Return maximum of domain
    FloatNum max(void) const;
    /// Return median of domain (closest representation)
    FloatNum med(void) const;

    /**
     * \brief Return assigned value
     *
     * Throws an exception of type Float::ValOfUnassignedVar if variable
     * is not yet assigned.
     *
     */
    FloatVal val(void) const;

    /// Return size of domain (distance between maximum and minimum)
    FloatNum size(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether 0 is contained in domain
    bool zero_in(void) const;
    /// Test whether \a n is contained in domain
    bool in(FloatNum n) const;
    /// Test whether \a n is contained in domain
    bool in(const FloatVal& n) const;
    //@}

    /// \name Domain update by value
    //@{
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, int n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, FloatNum n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, FloatVal n);

    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, int n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, FloatNum n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, FloatVal n);

    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, int n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, FloatNum n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, const FloatVal& n);

    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    FloatNum min(const Delta& d) const;
    /// Return maximum value just pruned
    FloatNum max(const Delta& d) const;
    //@}

    /// \name View-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}

    /// \name Cloning
    //@{
    void update(Space& home, bool share, ScaleView& y);
    //@}
  };

  /**
   * \brief Print scale view
   * \relates Gecode::Float::ScaleView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ScaleView& x);

} }

#include <gecode/float/var/float.hpp>
#include <gecode/float/view/float.hpp>
#include <gecode/float/view/minus.hpp>
#include <gecode/float/view/offset.hpp>
#include <gecode/float/view/scale.hpp>
#include <gecode/float/view/print.hpp>
#include <gecode/float/var/print.hpp>

namespace Gecode { namespace Float {
  /**
   * \defgroup TaskActorFloatTest Testing relations between float views
   * \ingroup TaskActorFloat
   */

  //@{
  /// Result of testing relation
  enum RelTest {
    RT_FALSE = 0, ///< Relation does not hold
    RT_MAYBE = 1, ///< Relation may hold or not
    RT_TRUE  = 2  ///< Relation does hold
  };
  
  /// Test whether views \a x and \a y are equal
  template<class View> RelTest rtest_eq(View x, View y);
  /// Test whether view \a x and Float \a n are equal
  template<class View> RelTest rtest_eq(View x, FloatVal n);
  
  /// Test whether view \a x is less or equal than view \a y
  template<class View> RelTest rtest_lq(View x, View y);
  /// Test whether view \a x is less or equal than float \a n
  template<class View> RelTest rtest_lq(View x, FloatVal n);
  
  /// Test whether view \a x is less than view \a y
  template<class View> RelTest rtest_le(View x, View y);
  /// Test whether view \a x is less or equal than float \a n
  template<class View> RelTest rtest_le(View x, FloatVal n);
  
  //@}

}}

#include <gecode/float/view/rel-test.hpp>

// STATISTICS: float-var
