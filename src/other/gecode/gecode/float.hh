/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Guido Tack, 2004
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

#ifndef __GECODE_FLOAT_HH__
#define __GECODE_FLOAT_HH__

#include <climits>
#include <cfloat>
#include <iostream>

#include <gecode/kernel.hh>
#include <gecode/int.hh>

/*
 * Configure linking
 *
 */
#if !defined(GECODE_STATIC_LIBS) && \
    (defined(__CYGWIN__) || defined(__MINGW32__) || defined(_MSC_VER))

#ifdef GECODE_BUILD_FLOAT
#define GECODE_FLOAT_EXPORT __declspec( dllexport )
#else
#define GECODE_FLOAT_EXPORT __declspec( dllimport )
#endif

#else

#ifdef GECODE_GCC_HAS_CLASS_VISIBILITY
#define GECODE_FLOAT_EXPORT __attribute__ ((visibility("default")))
#else
#define GECODE_FLOAT_EXPORT
#endif

#endif

// Configure auto-linking
#ifndef GECODE_BUILD_FLOAT
#define GECODE_LIBRARY_NAME "Float"
#include <gecode/support/auto-link.hpp>
#endif

// Include interval implementation
#include <gecode/third-party/boost/numeric/interval.hpp>

/**
 * \namespace Gecode::Float
 * \brief Floating point numbers
 *
 * The Gecode::Float namespace contains all functionality required
 * to program propagators and branchers for floating point numbers.
 * In addition, all propagators and branchers for floating point
 * numbers provided by %Gecode are contained as nested namespaces.
 *
 */

#include <gecode/float/exception.hpp>

#include <gecode/float/nextafter.hpp>

namespace Gecode {

  /**
   * \brief Floating point number base type
   *
   * This type defines the interval bounds used for representing floating
   * point values.
   * \ingroup TaskModelFloatVars
   */
  typedef double FloatNum;

  /// Return lower bound of \f$\pi/2\f$
  FloatNum pi_half_lower(void);
  /// Return upper bound of \f$\pi/2\f$
  FloatNum pi_half_upper(void);
  /// Return lower bound of \f$\pi\f$
  FloatNum pi_lower(void);
  /// Return upper bound of \f$\pi\f$
  FloatNum pi_upper(void);
  /// Return lower bound of \f$2\pi\f$
  FloatNum pi_twice_lower(void);
  /// Return upper bound of \f$2\pi\f$
  FloatNum pi_twice_upper(void);

  // Forward declaration
  class FloatVal;

}

#include <gecode/float/num.hpp>

namespace Gecode { namespace Float {

  /**
   * \brief Floating point rounding policy
   *
   * \ingroup TaskModelFloatVars
   */
  class Rounding : 
    public boost::numeric::interval_lib::rounded_arith_opp<FloatNum> {
  protected:
    /// Base class
    typedef boost::numeric::interval_lib::rounded_arith_opp<FloatNum> Base;
  public:
    /// \name Constructor and destructor
    //@{
    /// Default constructor (configures full rounding mode)
    Rounding(void);
    /// Destructor (restores previous rounding mode)
    ~Rounding(void);
    //@}
  
    /// \name Arithmetic operations
    //@{
    /// Return lower bound of \a x plus \a y (domain: \f$ [-\infty;+\infty][-\infty;+\infty]\f$)
    FloatNum add_down(FloatNum x, FloatNum y);
    /// Return upper bound of \a x plus \a y (domain: \f$ [-\infty;+\infty] [-\infty;+\infty]\f$)
    FloatNum add_up  (FloatNum x, FloatNum y);
    /// Return lower bound of \a x minus \a y (domain: \f$ [-\infty;+\infty] [-\infty;+\infty]\f$)
    FloatNum sub_down(FloatNum x, FloatNum y);
    /// Return upper bound of \a x minus \a y (domain: \f$ [-\infty;+\infty] [-\infty;+\infty]\f$)
    FloatNum sub_up  (FloatNum x, FloatNum y);
    /// Return lower bound of \a x times \a y (domain: \f$ [-\infty;+\infty] [-\infty;+\infty]\f$)
    FloatNum mul_down(FloatNum x, FloatNum y);
    /// Return upper bound of \a x times \a y (domain: \f$ [-\infty;+\infty] [-\infty;+\infty]\f$)
    FloatNum mul_up  (FloatNum x, FloatNum y);
    /// Return lower bound of \a x divided by \a y (domain: \f$ [-\infty;+\infty] ([-\infty;+\infty]-{0})   \f$)
    FloatNum div_down(FloatNum x, FloatNum y);
    /// Return upper bound of \a x divided \a y (domain: \f$ [-\infty;+\infty] ([-\infty;+\infty]-{0})\f$)
    FloatNum div_up  (FloatNum x, FloatNum y);
    /// Return lower bound of square root of \a x (domain: \f$ ]0;+\infty]   \f$)
    FloatNum sqrt_down(FloatNum x);
    /// Return upper bound of square root of \a x (domain: \f$ ]0;+\infty]\f$)
    FloatNum sqrt_up  (FloatNum x);
    //@}

    /// \name Miscellaneous operations
    //@{
    /// Return median of \a x and \a y (domain: \f$ [-\infty;+\infty][-\infty;+\infty]\f$)
    FloatNum median(FloatNum x, FloatNum y);
    /// Return next downward-rounded integer of \a x (domain: \f$ [-\infty;+\infty]\f$)
    FloatNum int_down(FloatNum x);
    /// Return next upward-rounded integer of \a x (domain: \f$ [-\infty;+\infty] \f$)
    FloatNum int_up  (FloatNum x);
    //@}

#ifdef GECODE_HAS_MPFR
    /// \name Exponential functions
    //@{
    /// Return lower bound of exponential of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum exp_down(FloatNum x);
    /// Return upper bound of exponential of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum exp_up  (FloatNum x);
    /// Return lower bound of logarithm of \a x (domain: \f$ ]0;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum log_down(FloatNum x);
    /// Return upper bound of logarithm of \a x (domain: \f$ ]0;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum log_up  (FloatNum x);
    //@}

    /// \name Trigonometric functions
    //@{
    /// Return lower bound of sine of \a x (domain: \f$ [0;2\pi]\f$)
    GECODE_FLOAT_EXPORT FloatNum sin_down(FloatNum x);
    /// Return upper bound of sine of \a x (domain: \f$ [0;2\pi]\f$)
    GECODE_FLOAT_EXPORT FloatNum sin_up  (FloatNum x);
    /// Return lower bound of cosine of \a x (domain: \f$ [0;2\pi]\f$)
    GECODE_FLOAT_EXPORT FloatNum cos_down(FloatNum x);
    /// Return upper bound of cosine of \a x (domain: \f$ [0;2\pi]\f$)
    GECODE_FLOAT_EXPORT FloatNum cos_up  (FloatNum x);
    /// Return lower bound of tangent of \a x (domain: \f$ ]-\pi/2;\pi/2[\f$)
    GECODE_FLOAT_EXPORT FloatNum tan_down(FloatNum x);
    /// Return upper bound of tangent of \a x (domain: \f$ ]-\pi/2;\pi/2[\f$)
    GECODE_FLOAT_EXPORT FloatNum tan_up  (FloatNum x);
    //@}

    /// \name Inverse trigonometric functions
    //@{
    /// Return lower bound of arcsine of \a x (domain: \f$ [-1;1]\f$)
    GECODE_FLOAT_EXPORT FloatNum asin_down(FloatNum x);
    /// Return upper bound of arcsine of \a x (domain: \f$ [-1;1]\f$)
    GECODE_FLOAT_EXPORT FloatNum asin_up  (FloatNum x);
    /// Return lower bound of arccosine of \a x (domain: \f$ [-1;1]\f$)
    GECODE_FLOAT_EXPORT FloatNum acos_down(FloatNum x);
    /// Return upper bound of arccossine of \a x (domain: \f$ [-1;1]\f$)
    GECODE_FLOAT_EXPORT FloatNum acos_up  (FloatNum x);
    /// Return lower bound of arctangent of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum atan_down(FloatNum x);
    /// Return upper bound of arctangent of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum atan_up  (FloatNum x);
    //@}

    /// \name Hyperbolic functions
    //@{
    /// Return lower bound of hyperbolic sine of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum sinh_down(FloatNum x);
    /// Return upper bound of hyperbolic sine of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum sinh_up  (FloatNum x);
    /// Return lower bound of hyperbolic cosine of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum cosh_down(FloatNum x);
    /// Return upper bound of hyperbolic cosine of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum cosh_up  (FloatNum x);
    /// Return lower bound of hyperbolic tangent of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum tanh_down(FloatNum x);
    /// Return upper bound of hyperbolic tangent of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum tanh_up  (FloatNum x);
    //@}

    /// \name Inverse hyperbolic functions
    //@{
    /// Return lower bound of hyperbolic arcsine of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum asinh_down(FloatNum x);
    /// Return upper bound of hyperbolic arcsine of \a x (domain: \f$ [-\infty;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum asinh_up  (FloatNum x);
    /// Return lower bound of hyperbolic arccosine of \a x (domain: \f$ [1;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum acosh_down(FloatNum x);
    /// Return upper bound of hyperbolic arccosine of \a x (domain: \f$ [1;+\infty]\f$)
    GECODE_FLOAT_EXPORT FloatNum acosh_up  (FloatNum x);
    /// Return lower bound of hyperbolic arctangent of \a x (domain: \f$ [-1;1]\f$)
    GECODE_FLOAT_EXPORT FloatNum atanh_down(FloatNum x);
    /// Return upper bound of hyperbolic arctangent of \a x (domain: \f$ [-1;1]\f$)
    GECODE_FLOAT_EXPORT FloatNum atanh_up  (FloatNum x);
    //@}
#endif
  };

}}

#include <gecode/float/rounding.hpp>

namespace Gecode { namespace Float {

  /**
   * \brief Test whether \a x is a subset of \a y
   * \relates Gecode::FloatVal
   */
  bool subset(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Test whether \a x is a proper subset of \a y
   * \relates Gecode::FloatVal
   */
  bool proper_subset(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Test whether \a x and \a y overlap
   * \relates Gecode::FloatVal
   */
  bool overlap(const FloatVal& x, const FloatVal& y);

  /**
   * \brief Return intersection of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal intersect(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Return hull of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal hull(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Return hull of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal hull(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Return hull of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal hull(const FloatNum& x, const FloatVal& y);
  /**
   * \brief Return hull of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal hull(const FloatNum& x, const FloatNum& y);

}}

namespace Gecode {

  /**
   * \brief Float value type
   *
   * \ingroup TaskModelFloatVars
   */
  class FloatVal {
    friend FloatVal operator +(const FloatVal& x);
    friend FloatVal operator -(const FloatVal& x);
    friend FloatVal operator +(const FloatVal& x, const FloatVal& y);
    friend FloatVal operator +(const FloatVal& x, const FloatNum& y);
    friend FloatVal operator +(const FloatNum& x, const FloatVal& y);
    friend FloatVal operator -(const FloatVal& x, const FloatVal& y);
    friend FloatVal operator -(const FloatVal& x, const FloatNum& y);
    friend FloatVal operator -(const FloatNum& x, const FloatVal& y);
    friend FloatVal operator *(const FloatVal& x, const FloatVal& y);
    friend FloatVal operator *(const FloatVal& x, const FloatNum& y);
    friend FloatVal operator *(const FloatNum& x, const FloatVal& y);
    friend FloatVal operator /(const FloatVal& x, const FloatVal& y);
    friend FloatVal operator /(const FloatVal& x, const FloatNum& y);
    friend FloatVal operator /(const FloatNum& x, const FloatVal& y);

    friend bool operator <(const FloatVal& x, const FloatVal& y);
    friend bool operator <(const FloatVal& x, const FloatNum& y);
    friend bool operator <(const FloatNum& x, const FloatVal& y);
    friend bool operator <=(const FloatVal& x, const FloatVal& y);
    friend bool operator <=(const FloatVal& x, const FloatNum& y);
    friend bool operator <=(const FloatNum& x, const FloatVal& y);
    friend bool operator >(const FloatVal& x, const FloatVal& y);
    friend bool operator >(const FloatVal& x, const FloatNum& y);
    friend bool operator >(const FloatNum& x, const FloatVal& y);
    friend bool operator >=(const FloatVal& x, const FloatVal& y);
    friend bool operator >=(const FloatVal& x, const FloatNum& y);
    friend bool operator >=(const FloatNum& x, const FloatVal& y);
    friend bool operator ==(const FloatVal& x, const FloatVal& y);
    friend bool operator ==(const FloatVal& x, const FloatNum& y);
    friend bool operator ==(const FloatNum& x, const FloatVal& y);
    friend bool operator !=(const FloatVal& x, const FloatVal& y);
    friend bool operator !=(const FloatVal& x, const FloatNum& y);
    friend bool operator !=(const FloatNum& x, const FloatVal& y);

    template<class Char, class Traits>
    friend std::basic_ostream<Char,Traits>&
    operator <<(std::basic_ostream<Char,Traits>& os, const FloatVal& x);

    friend FloatVal abs(const FloatVal& x);
    friend FloatVal sqrt(const FloatVal& x);
    friend FloatVal sqr(const FloatVal& x);
    friend FloatVal pow(const FloatVal& x, int n);
    friend FloatVal nroot(const FloatVal& x, int n);

    friend FloatVal max(const FloatVal& x, const FloatVal& y);
    friend FloatVal max(const FloatVal& x, const FloatNum& y);
    friend FloatVal max(const FloatNum& x, const FloatVal& y);
    friend FloatVal min(const FloatVal& x, const FloatVal& y);
    friend FloatVal min(const FloatVal& x, const FloatNum& y);
    friend FloatVal min(const FloatNum& x, const FloatVal& y);

#ifdef GECODE_HAS_MPFR
    friend FloatVal exp(const FloatVal& x);
    friend FloatVal log(const FloatVal& x);
    friend FloatVal fmod(const FloatVal& x, const FloatVal& y);
    friend FloatVal fmod(const FloatVal& x, const FloatNum& y);
    friend FloatVal fmod(const FloatNum& x, const FloatVal& y);
    friend FloatVal sin(const FloatVal& x);
    friend FloatVal cos(const FloatVal& x);
    friend FloatVal tan(const FloatVal& x);
    friend FloatVal asin(const FloatVal& x);
    friend FloatVal acos(const FloatVal& x);
    friend FloatVal atan(const FloatVal& x);
    friend FloatVal sinh(const FloatVal& x);
    friend FloatVal cosh(const FloatVal& x);
    friend FloatVal tanh(const FloatVal& x);
    friend FloatVal asinh(const FloatVal& x);
    friend FloatVal acosh(const FloatVal& x);
    friend FloatVal atanh(const FloatVal& x);
#endif

    friend bool Float::subset(const FloatVal& x, const FloatVal& y);
    friend bool Float::proper_subset(const FloatVal& x, const FloatVal& y);
    friend bool Float::overlap(const FloatVal& x, const FloatVal& y);
    friend FloatVal Float::intersect(const FloatVal& x, const FloatVal& y);
    friend FloatVal Float::hull(const FloatVal& x, const FloatVal& y);
    friend FloatVal Float::hull(const FloatVal& x, const FloatNum& y);
    friend FloatVal Float::hull(const FloatNum& x, const FloatVal& y);
    friend FloatVal Float::hull(const FloatNum& x, const FloatNum& y);
  protected:
    /// Used rounding policies
    typedef boost::numeric::interval_lib::save_state<Float::Rounding> R;
    /// Used checking policy
    typedef boost::numeric::interval_lib::checking_strict<FloatNum> P;
    /// Implementation type for float value
    typedef boost::numeric::interval
      <FloatNum,
       boost::numeric::interval_lib::policies<R, P> >
    FloatValImpType;
    /// Implementation of float value
    FloatValImpType x;
    /// Initialize from implementation \a i
    explicit FloatVal(const FloatValImpType& i);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    FloatVal(void);
    /// Initialize with float number \a n
    FloatVal(const FloatNum& n);
    /// Initialize with lower bound \a l and upper bound \a u
    FloatVal(const FloatNum& l, const FloatNum& u);
    /// Copy constructor
    FloatVal(const FloatVal& v);

    /// Assignment operator
    FloatVal& operator =(const FloatNum& n);
    /// Assignment operator
    FloatVal& operator =(const FloatVal& v);
    
    /// Assign lower bound \a l and upper bound \a u
    void assign(FloatNum const &l, FloatNum const &u);
    //@}

    /// \name Value access
    //@{
    /// Return lower bound
    FloatNum min(void) const;
    /// Return upper bound
    FloatNum max(void) const;
    /// Return size of float value (distance between maximum and minimum)
    FloatNum size(void) const;
    /// Return median of float value
    FloatNum med(void) const;
    //@}

    /// \name Value tests
    //@{
    /// Test whether float is tight
    bool tight(void) const;
    /// Test whether float is a singleton
    bool singleton(void) const;
    /// Test whether \a n is included
    bool in(FloatNum n) const;
    /// Test whether zero is included
    bool zero_in(void) const;
    //@}
    
    /// \name Float value construction
    //@{
    /// Return hull of \a x and \a y
    static FloatVal hull(FloatNum x, FloatNum y);
    /// Return \f$\pi/2\f$
    static FloatVal pi_half(void);
    /// Return lower bound of \f$\pi\f$
    static FloatVal pi(void);
    /// Return \f$2\pi\f$
    static FloatVal pi_twice(void);
    //@}
    
    /// \name Update operators
    //@{
    /// Increment by \a n
    FloatVal& operator +=(const FloatNum& n);
    /// Subtract by \a n
    FloatVal& operator -=(const FloatNum& n);
    /// Multiply by \a n
    FloatVal& operator *=(const FloatNum& n);
    /// Divide by \a n
    FloatVal& operator /=(const FloatNum& n);
    /// Increment by \a v
    FloatVal& operator +=(const FloatVal& v);
    /// Subtract by \a v
    FloatVal& operator -=(const FloatVal& v);
    /// Multiply by \a v
    FloatVal& operator *=(const FloatVal& v);
    /// Divide by \a v
    FloatVal& operator /=(const FloatVal& v);
    //@}
  };

  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator +(const FloatVal& x);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator -(const FloatVal& x);

  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator +(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator +(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator +(const FloatNum& x, const FloatVal& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator -(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator -(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator -(const FloatNum& x, const FloatVal& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator *(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator *(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator *(const FloatNum& x, const FloatVal& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator /(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator /(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Arithmetic operator
   * \relates Gecode::FloatVal
   */
  FloatVal operator /(const FloatNum& r, const FloatVal& x);

  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator <(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator <(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator <(const FloatNum& x, const FloatVal& y);

  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator <=(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator <=(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator <=(const FloatNum& x, const FloatVal& y);

  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator >(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator >(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator >(const FloatNum& x, const FloatVal& y);

  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator >=(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator >=(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator >=(const FloatNum& x, const FloatVal& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator ==(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator ==(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator ==(const FloatNum& x, const FloatVal& y);

  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator !=(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator !=(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Comparison operator
   * \relates Gecode::FloatVal
   */
  bool operator !=(const FloatNum& x, const FloatVal& y);

  /**
   * \brief Print float value \a x
   * \relates Gecode::FloatVal
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const FloatVal& x);

  /**
   * \brief Return absolute value of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal abs(const FloatVal& x);
  /**
   * \brief Return square root of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal sqrt(const FloatVal& x);
  /**
   * \brief Return square of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal sqr(const FloatVal& x);
  /**
   * \brief Return \a n -th power of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal pow(const FloatVal& x, int n);
  /**
   * \brief Return \a n -th root of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal nroot(const FloatVal& x, int n);

  /**
   * \brief Return maximum of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal max(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Return maximum of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal max(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Return maximum of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal max(const FloatNum& x, const FloatVal& y);
  /**
   * \brief Return minimum of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal min(const FloatVal& x, const FloatVal& y);
  /**
   * \brief Return minimum of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal min(const FloatVal& x, const FloatNum& y);
  /**
   * \brief Return minimum of \a x and \a y
   * \relates Gecode::FloatVal
   */
  FloatVal min(const FloatNum& x, const FloatVal& y);

#ifdef GECODE_HAS_MPFR
  /* transcendental functions: exp, log */
  /**
   * \brief Return exponential of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal exp(const FloatVal& x);
  /**
   * \brief Return logarithm of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal log(const FloatVal& x);

  /**
   * \brief Trigonometric function argument reduction
   * \relates Gecode::FloatVal
   */
  FloatVal fmod(const FloatVal& x, const FloatVal& y);
  /**
   * \brief  Trigonometric function argument reduction
   * \relates Gecode::FloatVal
   */
  FloatVal fmod(const FloatVal& x, const FloatNum& y);
  /**
   * \brief  Trigonometric function argument reduction
   * \relates Gecode::FloatVal
   */
  FloatVal fmod(const FloatNum& x, const FloatVal& y);

  /**
   * \brief Return sine of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal sin(const FloatVal& x);
  /**
   * \brief  Return cosine of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal cos(const FloatVal& x);
  /**
   * \brief  Return tangent of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal tan(const FloatVal& x);
  /**
   * \brief  Return arcsine of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal asin(const FloatVal& x);
  /**
   * \brief  Return arccosine of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal acos(const FloatVal& x);
  /**
   * \brief  Return arctangent of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal atan(const FloatVal& x);

  /**
   * \brief  Return hyperbolic of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal sinh(const FloatVal& x);
  /**
   * \brief  Return hyperbolic of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal cosh(const FloatVal& x);
  /**
   * \brief  Return hyperbolic of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal tanh(const FloatVal& x);
  /**
   * \brief  Return hyperbolic of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal asinh(const FloatVal& x);
  /**
   * \brief  Return hyperbolic of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal acosh(const FloatVal& x);
  /**
   * \brief  Return hyperbolic of \a x
   * \relates Gecode::FloatVal
   */
  FloatVal atanh(const FloatVal& x);

#endif

}

#include <gecode/float/val.hpp>

namespace Gecode { namespace Float {

  /**
   * \brief Numerical limits for floating point variables
   *
   * \ingroup TaskModelFloatVars
   */
  namespace Limits {
    /// Largest allowed float value
    const FloatNum max = std::numeric_limits<FloatNum>::max();
    /// Smallest allowed float value
    const FloatNum min = -max;
    /// Return whether float \a n is a valid number
    bool valid(const FloatVal& n);
    /// Check whether float \a n is a valid number, otherwise throw out of limits exception with information \a l
    void check(const FloatVal& n, const char* l);
  }

}}

#include <gecode/float/limits.hpp>

#include <gecode/float/var-imp.hpp>

namespace Gecode {

  namespace Float {
    class FloatView;
  }

  /**
   * \brief Float variables
   *
   * \ingroup TaskModelFloatVars
   */
  class FloatVar : public VarImpVar<Float::FloatVarImp> {
    friend class FloatVarArray;
    friend class FloatVarArgs;
  private:
    using VarImpVar<Float::FloatVarImp>::x;
    /**
     * \brief Initialize variable with range domain
     *
     * The variable is created with a domain ranging from \a min
     * to \a max. No exceptions are thrown.
     */
    void _init(Space& home, FloatNum min, FloatNum max);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    FloatVar(void);
    /// Initialize from float variable \a y
    FloatVar(const FloatVar& y);
    /// Initialize from float view \a y
    FloatVar(const Float::FloatView& y);
    /**
     * \brief Initialize variable with range domain
     *
     * The variable is created with a domain ranging from \a min
     * to \a max. The following exceptions might be thrown:
     *  - If \a min is greater than \a max, an exception of type
     *    Gecode::Float::VariableEmptyDomain is thrown.
     *  - If \a min or \a max exceed the limits for floats as defined
     *    in Gecode::Float::Limits, an exception of type
     *    Gecode::Float::OutOfLimits is thrown.
     */
    GECODE_FLOAT_EXPORT FloatVar(Space& home, FloatNum min, FloatNum max);
    //@}

    /// \name Value access
    //@{
    /// Return domain
    FloatVal domain(void) const;
    /// Return minimum of domain
    FloatNum min(void) const;
    /// Return maximum of domain
    FloatNum max(void) const;
    /// Return median of domain
    FloatNum med(void) const;
    /// Return size of domain (distance between maximum and minimum)
    FloatNum size(void) const;
    /**
     * \brief Return assigned value
     *
     * Throws an exception of type Float::ValOfUnassignedVar if variable
     * is not yet assigned.
     *
     */
    FloatVal val(void) const;

    //@}

    /// \name Domain tests
    //@{
    /// Test whether \a n is contained in domain
    bool in(const FloatVal& n) const;
    //@}
  };

  /**
   * \brief Print float variable \a x
   * \relates Gecode::FloatVar
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const FloatVar& x);
}

#include <gecode/float/view.hpp>
#include <gecode/float/array-traits.hpp>

namespace Gecode {

  /// Passing float arguments
  class FloatValArgs : public PrimArgArray<FloatVal> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Allocate empty array
    FloatValArgs(void);
    /// Allocate array with \a n elements
    explicit FloatValArgs(int n);
    /// Allocate array and copy elements from \a x
    FloatValArgs(const SharedArray<FloatVal>& x);
    /// Allocate array and copy elements from \a x
    FloatValArgs(const std::vector<FloatVal>& x);
    /// Allocate array and copy elements from \a first to \a last
    template<class InputIterator>
    FloatValArgs(InputIterator first, InputIterator last);
    /// Allocate array with \a n elements and initialize with \a e0, ...
    GECODE_FLOAT_EXPORT
    FloatValArgs(int n, int e0, ...);
    /// Allocate array with \a n elements and initialize with elements from array \a e
    FloatValArgs(int n, const FloatVal* e);
    /// Initialize from primitive argument array \a a (copy elements)
    FloatValArgs(const PrimArgArray<FloatVal>& a);

    /// Allocate array with \a n elements such that for all \f$0\leq i<n: x_i=\text{start}+i\cdot\text{inc}\f$
    static FloatValArgs create(int n, FloatVal start, int inc=1);
    //@}
  };

  /// \brief Passing float variables
  class FloatVarArgs : public VarArgArray<FloatVar> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Allocate empty array
    FloatVarArgs(void) {}
    /// Allocate array with \a n elements
    explicit FloatVarArgs(int n) : VarArgArray<FloatVar>(n) {}
    /// Initialize from variable argument array \a a (copy elements)
    FloatVarArgs(const FloatVarArgs& a) : VarArgArray<FloatVar>(a) {}
    /// Initialize from variable array \a a (copy elements)
    FloatVarArgs(const VarArray<FloatVar>& a) : VarArgArray<FloatVar>(a) {}
    /// Initialize from vector \a a
    FloatVarArgs(const std::vector<FloatVar>& a) : VarArgArray<FloatVar>(a) {}
    /// Initialize from InputIterator \a first and \a last
    template<class InputIterator>
    FloatVarArgs(InputIterator first, InputIterator last)
    : VarArgArray<FloatVar>(first,last) {}
    /**
     * \brief Initialize array with \a n new variables
     *
     * The variables are created with a domain ranging from \a min
     * to \a max. The following execptions might be thrown:
     *  - If \a min is greater than \a max, an exception of type
     *    Gecode::Float::VariableEmptyDomain is thrown.
     *  - If \a min or \a max exceed the limits for floats as defined
     *    in Gecode::Float::Limits, an exception of type
     *    Gecode::Float::OutOfLimits is thrown.
     */
    GECODE_FLOAT_EXPORT
    FloatVarArgs(Space& home, int n, FloatNum min, FloatNum max);
    //@}
  };
  //@}

  /**
   * \defgroup TaskModelFloatVarArrays Variable arrays
   *
   * Variable arrays can store variables. They are typically used
   * for storing the variables being part of a solution (script). However,
   * they can also be used for temporary purposes (even though
   * memory is not reclaimed until the space it is created for
   * is deleted).
   * \ingroup TaskModelFloat
   */

  /**
   * \brief Float variable array
   * \ingroup TaskModelFloatVarArrays
   */
  class FloatVarArray : public VarArray<FloatVar> {
  public:
    /// \name Creation and initialization
    //@{
    /// Default constructor (array of size 0)
    FloatVarArray(void);
    /// Allocate array for \a n float variables (variables are uninitialized)
    FloatVarArray(Space& home, int n);
    /// Initialize from float variable array \a a (share elements)
    FloatVarArray(const FloatVarArray& a);
    /// Initialize from float variable argument array \a a (copy elements)
    FloatVarArray(Space& home, const FloatVarArgs& a);
    /**
     * \brief Initialize array with \a n new variables
     *
     * The variables are created with a domain ranging from \a min
     * to \a max. The following execptions might be thrown:
     *  - If \a min is greater than \a max, an exception of type
     *    Gecode::Float::VariableEmptyDomain is thrown.
     *  - If \a min or \a max exceed the limits for floats as defined
     *    in Gecode::Float::Limits, an exception of type
     *    Gecode::Float::OutOfLimits is thrown.
     */
    GECODE_FLOAT_EXPORT
    FloatVarArray(Space& home, int n, FloatNum min, FloatNum max);
    //@}
  };

}

#include <gecode/float/array.hpp>

namespace Gecode {

  /**
   * \brief Relation types for floats
   * \ingroup TaskModelFloat
   */
  enum FloatRelType {
    FRT_EQ, ///< Equality (\f$=\f$)
    FRT_NQ, ///< Disequality (\f$\neq\f$)
    FRT_LQ, ///< Less or equal (\f$\leq\f$)
    FRT_LE, ///< Less (\f$<\f$)
    FRT_GQ, ///< Greater or equal (\f$\geq\f$)
    FRT_GR ///< Greater (\f$>\f$)
  };

  /**
   * \defgroup TaskModelFloatDomain Domain constraints
   * \ingroup TaskModelFloat
   *
   */

  //@{
  /// Propagates \f$x=n\f$
  GECODE_FLOAT_EXPORT void
  dom(Home home, FloatVar x, FloatVal n);
  /// Propagates \f$ x_i=n\f$ for all \f$0\leq i<|x|\f$
  GECODE_FLOAT_EXPORT void
  dom(Home home, const FloatVarArgs& x, FloatVal n);
  /// Propagates \f$ l\leq x\leq u\f$
  GECODE_FLOAT_EXPORT void
  dom(Home home, FloatVar x, FloatNum l, FloatNum m);
  /// Propagates \f$ l\leq x_i\leq u\f$ for all \f$0\leq i<|x|\f$
  GECODE_FLOAT_EXPORT void
  dom(Home home, const FloatVarArgs& x, FloatNum l, FloatNum u);
  /// Post domain consistent propagator for \f$ (x=n) \equiv r\f$
  GECODE_FLOAT_EXPORT void
  dom(Home home, FloatVar x, FloatVal n, Reify r);
  /// Post domain consistent propagator for \f$ (l\leq x \leq u) \equiv r\f$
  GECODE_FLOAT_EXPORT void
  dom(Home home, FloatVar x, FloatNum l, FloatNum u, Reify r);
  /// Constrain domain of \a x according to domain of \a d
  GECODE_FLOAT_EXPORT void
  dom(Home home, FloatVar x, FloatVar d);
  /// Constrain domain of \f$ x_i \f$ according to domain of \f$ d_i \f$ for all \f$0\leq i<|x|\f$
  GECODE_FLOAT_EXPORT void
  dom(Home home, const FloatVarArgs& x, const FloatVarArgs& d);
  //@}

  /**
   * \defgroup TaskModelFloatRelFloat Simple relation constraints over float variables
   * \ingroup TaskModelFloat
   */
  /** \brief Post propagator for \f$ x_0 \sim_{frt} x_1\f$
   *
   * \ingroup TaskModelFloatRelFloat
   */
  GECODE_FLOAT_EXPORT void
  rel(Home home, FloatVar x0, FloatRelType frt, FloatVar x1);
  /** \brief Propagates \f$ x \sim_{frt} c\f$
   * \ingroup TaskModelFloatRelFloat
   */
  GECODE_FLOAT_EXPORT void
  rel(Home home, FloatVar x, FloatRelType frt, FloatVal c);
  /** \brief Post propagator for \f$(x \sim_{frt} c)\equiv r\f$
   * \ingroup TaskModelFloatRelFloat
   */
  GECODE_FLOAT_EXPORT void
  rel(Home home, FloatVar x, FloatRelType frt, FloatVal c, Reify r);
  /** \brief Post propagator for \f$(x_0 \sim_{frt} x_1)\equiv r\f$
   * \ingroup TaskModelFloatRelFloat
   */
  GECODE_FLOAT_EXPORT void
  rel(Home home, FloatVar x0, FloatRelType frt, FloatVar x1, Reify r);
  /** \brief Propagates \f$ x_i \sim_{frt} c \f$ for all \f$0\leq i<|x|\f$
   * \ingroup TaskModelFloatRelFloat
   */
  GECODE_FLOAT_EXPORT void
  rel(Home home, const FloatVarArgs& x, FloatRelType frt, FloatVal c);
  /** \brief Propagates \f$ x_i \sim_{frt} y \f$ for all \f$0\leq i<|x|\f$
   * \ingroup TaskModelFloatRelFloat
   */
  GECODE_FLOAT_EXPORT void
  rel(Home home, const FloatVarArgs& x, FloatRelType frt, FloatVar y);

}


namespace Gecode {

  /**
   * \defgroup TaskModelFloatArith Arithmetic constraints
   * \ingroup TaskModelFloat
   */

  //@{
  /** \brief Post propagator for \f$ \min\{x_0,x_1\}=x_2\f$
   */
  GECODE_FLOAT_EXPORT void
  min(Home home, FloatVar x0, FloatVar x1, FloatVar x2);
  /** \brief Post propagator for \f$ \min x=y\f$
   * If \a x is empty, an exception of type Float::TooFewArguments is thrown.
   */
  GECODE_FLOAT_EXPORT void
  min(Home home, const FloatVarArgs& x, FloatVar y);
  /** \brief Post propagator for \f$ \max\{x_0,x_1\}=x_2\f$
   */
  GECODE_FLOAT_EXPORT void
  max(Home home, FloatVar x0, FloatVar x1, FloatVar x2);
  /** \brief Post propagator for \f$ \max x=y\f$
   * If \a x is empty, an exception of type Float::TooFewArguments is thrown.
   */
  GECODE_FLOAT_EXPORT void
  max(Home home, const FloatVarArgs& x, FloatVar y);

  /** \brief Post propagator for \f$ |x_0|=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  abs(Home home, FloatVar x0, FloatVar x1);

  /** \brief Post propagator for \f$x_0\cdot x_1=x_2\f$
   */
  GECODE_FLOAT_EXPORT void
  mult(Home home, FloatVar x0, FloatVar x1, FloatVar x2);

  /** \brief Post propagator for \f$x_0\cdot x_0=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  sqr(Home home, FloatVar x0, FloatVar x1);

  /** \brief Post propagator for \f$\sqrt{x_0}=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  sqrt(Home home, FloatVar x0, FloatVar x1);

  /** \brief Post propagator for \f${x_0}^{n}=x_1\f$ for $n\geq 0$
   */
  GECODE_FLOAT_EXPORT void
  pow(Home home, FloatVar x0, int n, FloatVar x1);

  /** \brief Post propagator for \f${x_0}^{1/n}=x_1\f$ for $n\geq 0$
   */
  GECODE_FLOAT_EXPORT void
  nroot(Home home, FloatVar x0, int n, FloatVar x1);

  /** \brief Post propagator for \f$x_0\ \mathrm{div}\ x_1=x_2\f$
   */
  GECODE_FLOAT_EXPORT void
  div(Home home, FloatVar x0, FloatVar x1, FloatVar x2);
#ifdef GECODE_HAS_MPFR
  /** \brief Post propagator for \f$ \mathrm{exp}(x_0)=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  exp(Home home, FloatVar x0, FloatVar x1);
  /** \brief Post propagator for \f$ \mathrm{log}_e(x_0)=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  log(Home home, FloatVar x0, FloatVar x1);
  /** \brief Post propagator for \f$ \mathit{base}^{x_0}=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  pow(Home home, FloatNum base, FloatVar x0, FloatVar x1);
  /** \brief Post propagator for \f$ \mathrm{log}_{\mathit{base}}(x_0)=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  log(Home home, FloatNum base, FloatVar x0, FloatVar x1);
  /** \brief Post propagator for \f$ \mathrm{asin}(x_0)=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  asin(Home home, FloatVar x0, FloatVar x1);
  /** \brief Post propagator for \f$ \mathrm{sin}(x_0)=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  sin(Home home, FloatVar x0, FloatVar x1);
  /** \brief Post propagator for \f$ \mathrm{acos}(x_0)=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  acos(Home home, FloatVar x0, FloatVar x1);
  /** \brief Post propagator for \f$ \mathrm{cos}(x_0)=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  cos(Home home, FloatVar x0, FloatVar x1);
  /** \brief Post propagator for \f$ \mathrm{atan}(x_0)=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  atan(Home home, FloatVar x0, FloatVar x1);
  /** \brief Post propagator for \f$ \mathrm{tan}(x_0)=x_1\f$
   */
  GECODE_FLOAT_EXPORT void
  tan(Home home, FloatVar x0, FloatVar x1);
  //@}
#endif

  /**
   * \defgroup TaskModelFloatLI Linear constraints over float variables
   * \ingroup TaskModelFloat
   */
  /** \brief Post propagator for \f$\sum_{i=0}^{|x|-1}x_i\sim_{frt} c\f$
   * \ingroup TaskModelFloatLI
   */
  GECODE_FLOAT_EXPORT void
  linear(Home home, const FloatVarArgs& x,
         FloatRelType frt, FloatNum c);
  /** \brief Post propagator for \f$\sum_{i=0}^{|x|-1}x_i\sim_{frt} y\f$
   * \ingroup TaskModelFloatLI
   */
  GECODE_FLOAT_EXPORT void
  linear(Home home, const FloatVarArgs& x,
         FloatRelType frt, FloatVar y);
  /** \brief Post propagator for \f$\left(\sum_{i=0}^{|x|-1}x_i\sim_{frt} c\right)\equiv r\f$
   * \ingroup TaskModelFloatLI
   */
  GECODE_FLOAT_EXPORT void
  linear(Home home, const FloatVarArgs& x,
         FloatRelType frt, FloatNum c, Reify r);
  /** \brief Post propagator for \f$\left(\sum_{i=0}^{|x|-1}x_i\sim_{frt} y\right)\equiv r\f$
   * \ingroup TaskModelFloatLI
   */
  GECODE_FLOAT_EXPORT void
  linear(Home home, const FloatVarArgs& x,
         FloatRelType frt, FloatVar y, Reify r);
  /** \brief Post propagator for \f$\sum_{i=0}^{|x|-1}a_i\cdot x_i\sim_{frt} c\f$
   *
   *  Throws an exception of type Float::ArgumentSizeMismatch, if
   *  \a a and \a x are of different size.
   * \ingroup TaskModelFloatLI
   */
  GECODE_FLOAT_EXPORT void
  linear(Home home, const FloatValArgs& a, const FloatVarArgs& x,
         FloatRelType frt, FloatNum c);
  /** \brief Post propagator for \f$\sum_{i=0}^{|x|-1}a_i\cdot x_i\sim_{frt} y\f$
   *
   *  Throws an exception of type Float::ArgumentSizeMismatch, if
   *  \a a and \a x are of different size.
   * \ingroup TaskModelFloatLI
   */
  GECODE_FLOAT_EXPORT void
  linear(Home home, const FloatValArgs& a, const FloatVarArgs& x,
         FloatRelType frt, FloatVar y);
  /** \brief Post propagator for \f$\left(\sum_{i=0}^{|x|-1}a_i\cdot x_i\sim_{frt} c\right)\equiv r\f$
   *
   *  Throws an exception of type Float::ArgumentSizeMismatch, if
   *  \a a and \a x are of different size.
   * \ingroup TaskModelFloatLI
   */
  GECODE_FLOAT_EXPORT void
  linear(Home home, const FloatValArgs& a, const FloatVarArgs& x,
         FloatRelType frt, FloatNum c, Reify r);
  /** \brief Post propagator for \f$\left(\sum_{i=0}^{|x|-1}a_i\cdot x_i\sim_{frt} y\right)\equiv r\f$
   *
   *  Throws an exception of type Float::ArgumentSizeMismatch, if
   *  \a a and \a x are of different size.
   * \ingroup TaskModelFloatLI
   */
  GECODE_FLOAT_EXPORT void
  linear(Home home, const FloatValArgs& a, const FloatVarArgs& x,
         FloatRelType frt, FloatVar y, Reify r);


  /**
   * \defgroup TaskModelFloatChannel Channel constraints
   * \ingroup TaskModelFloat
   */
  //@{
  /// Post propagator for channeling a float and an integer variable \f$ x_0 = x_1\f$
  GECODE_FLOAT_EXPORT void
  channel(Home home, FloatVar x0, IntVar x1);
  /// Post propagator for channeling a float and an integer variable \f$ x_0 = x_1\f$
  GECODE_FLOAT_EXPORT void
  channel(Home home, IntVar x0, FloatVar x1);
  //@}


  /**
   * \defgroup TaskModelFloatExec Synchronized execution
   * \ingroup TaskModelFloat
   *
   * Synchronized execution executes a function or a static member function
   * when a certain event happends.
   */
  //@{
  /// Execute \a c when \a x becomes assigned
  GECODE_FLOAT_EXPORT void
  wait(Home home, FloatVar x, void (*c)(Space& home));
  /// Execute \a c when all variables in \a x become assigned
  GECODE_FLOAT_EXPORT void
  wait(Home home, const FloatVarArgs& x, void (*c)(Space& home));
  //@}

}

namespace Gecode {

  /**
   * \defgroup TaskModelFloatBranch Branching on float variables
   * \ingroup TaskModelFloat
   */

  /**
   * \brief Branch filter function type for float variables
   *
   * The variable \a x is considered for selection and \a i refers to the
   * variable's position in the original array passed to the brancher.
   *
   * \ingroup TaskModelFloatBranch
   */
  typedef bool (*FloatBranchFilter)(const Space& home, FloatVar x, int i);

  /**
   * \brief Branch merit function type for float variables
   *
   * The function must return a merit value for the variable
   * \a x.
   * The value \a i refers to the variable's position in the original array
   * passed to the brancher.
   *
   * \ingroup TaskModelFloatBranch
   */
  typedef double (*FloatBranchMerit)(const Space& home, FloatVar x, int i);

  /**
   * \brief Value description class for branching
   *
   * \ingroup TaskModelFloatBranch
   */
  class FloatNumBranch {
  public:
    /// The middle value for branching
    FloatNum n;
    /// Whether to try the lower or upper half first
    bool l;
  };

  /**
   * \brief Branch value function type for float variables
   *
   * Returns a value for the variable \a x that is to be used in the
   * corresponding branch commit function. The integer \a i refers 
   * to the variable's position in the original array passed to the 
   * brancher.
   *
   * \ingroup TaskModelFloatBranch
   */
  typedef FloatNumBranch (*FloatBranchVal)(const Space& home, FloatVar x, int i);

  /**
   * \brief Branch commit function type for float variables
   *
   * The function must post a constraint on the variable \a x which
   * corresponds to the alternative \a a.  The integer \a i refers 
   * to the variable's position in the original array passed to the 
   * brancher. The value \a nl is the value description
   * computed by the corresponding branch value function.
   *
   * \ingroup TaskModelFloatBranch
   */
  typedef void (*FloatBranchCommit)(Space& home, unsigned int a,
                                    FloatVar x, int i, FloatNumBranch nl);

}

#include <gecode/float/branch/traits.hpp>

namespace Gecode {

  /**
   * \brief Recording AFC information for float variables
   *
   * \ingroup TaskModelFloatBranch
   */
  class FloatAFC : public AFC {
  public:
    /**
     * \brief Construct as not yet initialized
     *
     * The only member functions that can be used on a constructed but not
     * yet initialized AFC storage is init or the assignment operator.
     *
     */
    FloatAFC(void);
    /// Copy constructor
    FloatAFC(const FloatAFC& a);
    /// Assignment operator
    FloatAFC& operator =(const FloatAFC& a);      
    /// Initialize for float variables \a x with decay factor \a d
    FloatAFC(Home home, const FloatVarArgs& x, double d=1.0);
    /**
     * \brief Initialize for float variables \a x with decay factor \a d
     *
     * This member function can only be used once and only if the
     * AFC storage has been constructed with the default constructor.
     *
     */
    void init(Home, const FloatVarArgs& x, double d=1.0);
  };

}

#include <gecode/float/branch/afc.hpp>

namespace Gecode {

  /**
   * \brief Recording activities for float variables
   *
   * \ingroup TaskModelFloatBranch
   */
  class FloatActivity : public Activity {
  public:
    /**
     * \brief Construct as not yet initialized
     *
     * The only member functions that can be used on a constructed but not
     * yet initialized activity storage is init or the assignment operator.
     *
     */
    FloatActivity(void);
    /// Copy constructor
    FloatActivity(const FloatActivity& a);
    /// Assignment operator
    FloatActivity& operator =(const FloatActivity& a);      
    /**
     * \brief Initialize for float variables \a x with decay factor \a d
     *
     * If the branch merit function \a bm is different from NULL, the
     * activity for each variable is initialized with the merit returned
     * by \a bm.
     *
     */
    GECODE_FLOAT_EXPORT 
    FloatActivity(Home home, const FloatVarArgs& x, double d=1.0,
                  FloatBranchMerit bm=NULL);
    /**
     * \brief Initialize for float variables \a x with decay factor \a d
     *
     * If the branch merit function \a bm is different from NULL, the
     * activity for each variable is initialized with the merit returned
     * by \a bm.
     *
     * This member function can only be used once and only if the
     * activity storage has been constructed with the default constructor.
     *
     */
    GECODE_FLOAT_EXPORT void
    init(Home, const FloatVarArgs& x, double d=1.0,
         FloatBranchMerit bm=NULL);
  };

}

#include <gecode/float/branch/activity.hpp>

namespace Gecode {

  /// Function type for explaining branching alternatives for set variables
  typedef void (*FloatVarValPrint)(const Space &home, const BrancherHandle& bh,
                                   unsigned int a,
                                   FloatVar x, int i, const FloatNumBranch& n,
                                   std::ostream& o);

}

namespace Gecode {

  /**
   * \brief Which variable to select for branching
   *
   * \ingroup TaskModelFloatBranch
   */
  class FloatVarBranch : public VarBranch {
  public:
    /// Which variable selection
    enum Select {
      SEL_NONE = 0,        ///< First unassigned
      SEL_RND,             ///< Random (uniform, for tie breaking)
      SEL_MERIT_MIN,       ///< With least merit
      SEL_MERIT_MAX,       ///< With highest merit
      SEL_DEGREE_MIN,      ///< With smallest degree
      SEL_DEGREE_MAX,      ///< With largest degree
      SEL_AFC_MIN,         ///< With smallest accumulated failure count
      SEL_AFC_MAX,         ///< With largest accumulated failure count
      SEL_ACTIVITY_MIN,    ///< With lowest activity
      SEL_ACTIVITY_MAX,    ///< With highest activity
      SEL_MIN_MIN,         ///< With smallest min
      SEL_MIN_MAX,         ///< With largest min
      SEL_MAX_MIN,         ///< With smallest max
      SEL_MAX_MAX,         ///< With largest max
      SEL_SIZE_MIN,        ///< With smallest domain size
      SEL_SIZE_MAX,        ///< With largest domain size
      SEL_DEGREE_SIZE_MIN, ///< With smallest degree divided by domain size
      SEL_DEGREE_SIZE_MAX, ///< With largest degree divided by domain size
      SEL_AFC_SIZE_MIN,    ///< With smallest accumulated failure count divided by domain size
      SEL_AFC_SIZE_MAX,    ///< With largest accumulated failure count divided by domain size
      SEL_ACTIVITY_SIZE_MIN, ///< With smallest activity divided by domain size
      SEL_ACTIVITY_SIZE_MAX  ///< With largest activity divided by domain size
    };
  protected:
    /// Which variable to select
    Select s;
  public:
    /// Initialize with strategy SEL_NONE
    FloatVarBranch(void);
    /// Initialize with random number generator \a r
    FloatVarBranch(Rnd r);
    /// Initialize with selection strategy \a s and tie-break limit function \a t
    FloatVarBranch(Select s, BranchTbl t);
    /// Initialize with selection strategy \a s, decay factor \a d, and tie-break limit function \a t
    FloatVarBranch(Select s, double, BranchTbl t);
    /// Initialize with selection strategy \a s, AFC \a a, and tie-break limit function \a t
    FloatVarBranch(Select s, AFC a, BranchTbl t);
    /// Initialize with selection strategy \a s, activity \a a, and tie-break limit function \a t
    FloatVarBranch(Select s, Activity a, BranchTbl t);
    /// Initialize with selection strategy \a s, branch merit function \a mf, and tie-break limit function \a t
    FloatVarBranch(Select s, VoidFunction mf, BranchTbl t);
    /// Return selection strategy
    Select select(void) const;
    /// Expand decay factor into AFC or activity
    void expand(Home home, const FloatVarArgs& x);
  };

  
  /**
   * \defgroup TaskModelFloatBranchVar Variable selection for float variables
   * \ingroup TaskModelFloatBranch
   */
  //@{
  /// Select first unassigned variable
  FloatVarBranch FLOAT_VAR_NONE(void);
  /// Select random variable (uniform distribution, for tie breaking)
  FloatVarBranch FLOAT_VAR_RND(Rnd r);
  /// Select variable with least merit according to branch merit function \a bm
  FloatVarBranch FLOAT_VAR_MERIT_MIN(FloatBranchMerit bm, BranchTbl tbl=NULL);
  /// Select variable with highest merit according to branch merit function \a bm
  FloatVarBranch FLOAT_VAR_MERIT_MAX(FloatBranchMerit bm, BranchTbl tbl=NULL);
  /// Select variable with smallest degree
  FloatVarBranch FLOAT_VAR_DEGREE_MIN(BranchTbl tbl=NULL);
  /// Select variable with largest degree
  FloatVarBranch FLOAT_VAR_DEGREE_MAX(BranchTbl tbl=NULL);
  /// Select variable with smallest accumulated failure count with decay factor \a d
  FloatVarBranch FLOAT_VAR_AFC_MIN(double d=1.0, BranchTbl tbl=NULL);
  /// Select variable with smallest accumulated failure count
  FloatVarBranch FLOAT_VAR_AFC_MIN(FloatAFC a, BranchTbl tbl=NULL);
  /// Select variable with largest accumulated failure count with decay factor \a d
  FloatVarBranch FLOAT_VAR_AFC_MAX(double d=1.0, BranchTbl tbl=NULL);
  /// Select variable with largest accumulated failure count    
  FloatVarBranch FLOAT_VAR_AFC_MAX(FloatAFC a, BranchTbl tbl=NULL);
  /// Select variable with lowest activity with decay factor \a d
  FloatVarBranch FLOAT_VAR_ACTIVITY_MIN(double d=1.0, BranchTbl tbl=NULL);
  /// Select variable with lowest activity
  FloatVarBranch FLOAT_VAR_ACTIVITY_MIN(FloatActivity a, BranchTbl tbl=NULL);
  /// Select variable with highest activity with decay factor \a d
  FloatVarBranch FLOAT_VAR_ACTIVITY_MAX(double d=1.0, BranchTbl tbl=NULL);
  /// Select variable with highest activity
  FloatVarBranch FLOAT_VAR_ACTIVITY_MAX(FloatActivity a, BranchTbl tbl=NULL);
  /// Select variable with smallest min
  FloatVarBranch FLOAT_VAR_MIN_MIN(BranchTbl tbl=NULL);         
  /// Select variable with largest min
  FloatVarBranch FLOAT_VAR_MIN_MAX(BranchTbl tbl=NULL);
  /// Select variable with smallest max
  FloatVarBranch FLOAT_VAR_MAX_MIN(BranchTbl tbl=NULL); 
  /// Select variable with largest max
  FloatVarBranch FLOAT_VAR_MAX_MAX(BranchTbl tbl=NULL);
  /// Select variable with smallest domain size
  FloatVarBranch FLOAT_VAR_SIZE_MIN(BranchTbl tbl=NULL);
  /// Select variable with largest domain size
  FloatVarBranch FLOAT_VAR_SIZE_MAX(BranchTbl tbl=NULL);
  /// Select variable with smallest degree divided by domain size
  FloatVarBranch FLOAT_VAR_DEGREE_SIZE_MIN(BranchTbl tbl=NULL);
  /// Select variable with largest degree divided by domain size
  FloatVarBranch FLOAT_VAR_DEGREE_SIZE_MAX(BranchTbl tbl=NULL);
  /// Select variable with smalllest accumulated failure count  divided by domain size with decay factor \a d
  FloatVarBranch FLOAT_VAR_AFC_SIZE_MIN(double d=1.0, BranchTbl tbl=NULL);
  /// Select variable with smallest accumulated failure count divided by domain size
  FloatVarBranch FLOAT_VAR_AFC_SIZE_MIN(FloatAFC a, BranchTbl tbl=NULL);
  /// Select variable with largest accumulated failure count  divided by domain size with decay factor \a d
  FloatVarBranch FLOAT_VAR_AFC_SIZE_MAX(double d=1.0, BranchTbl tbl=NULL);
  /// Select variable with largest accumulated failure count divided by domain size
  FloatVarBranch FLOAT_VAR_AFC_SIZE_MAX(FloatAFC a, BranchTbl tbl=NULL);
  /// Select variable with smallest activity divided by domain size with decay factor \a d
  FloatVarBranch FLOAT_VAR_ACTIVITY_SIZE_MIN(double d=1.0, BranchTbl tbl=NULL);
  /// Select variable with smallest activity divided by domain size
  FloatVarBranch FLOAT_VAR_ACTIVITY_SIZE_MIN(FloatActivity a, BranchTbl tbl=NULL);
  /// Select variable with largest activity divided by domain size with decay factor \a d
  FloatVarBranch FLOAT_VAR_ACTIVITY_SIZE_MAX(double d=1.0, BranchTbl tbl=NULL);
  /// Select variable with largest activity divided by domain size
  FloatVarBranch FLOAT_VAR_ACTIVITY_SIZE_MAX(FloatActivity a, BranchTbl tbl=NULL);
  //@}

}

#include <gecode/float/branch/var.hpp>

namespace Gecode {

  /**
   * \brief Which values to select for branching first
   *
   * \ingroup TaskModelFloatBranch
   */
  class FloatValBranch : public ValBranch {
  public:
    /// Which value selection
    enum Select {
      SEL_SPLIT_MIN, ///< Select values not greater than mean of smallest and largest value
      SEL_SPLIT_MAX, ///< Select values greater than mean of smallest and largest value
      SEL_SPLIT_RND,  ///< Select values randomly which are not greater or not smaller than mean of largest and smallest value
      SEL_VAL_COMMIT  ///< Select value according to user-defined functions
    };
  protected:
    /// Which value to select
    Select s;
  public:
    /// Initialize with selection strategy \a s
    FloatValBranch(Select s = SEL_SPLIT_MIN);
    /// Initialize with random number generator \a r
    FloatValBranch(Rnd r);
    /// Initialize with value function \a f and commit function \a c
    FloatValBranch(VoidFunction v, VoidFunction c);
    /// Return selection strategy
    Select select(void) const;
  };

  /**
   * \defgroup TaskModelFloatBranchVal Value selection for float variables
   * \ingroup TaskModelFloatBranch
   */
  //@{
  /// Select values not greater than mean of smallest and largest value
  FloatValBranch FLOAT_VAL_SPLIT_MIN(void);
  /// Select values greater than mean of smallest and largest value
  FloatValBranch FLOAT_VAL_SPLIT_MAX(void);
  /// Select values randomly which are not greater or not smaller than mean of largest and smallest value
  FloatValBranch FLOAT_VAL_SPLIT_RND(Rnd r);
  /**
   * Select value as defined by the value function \a v and commit function \a c
   * The default commit function posts the constraint that the float variable
   * \a x must be less or equal than the value \a n for the first
   * alternative and that \a x must be greater or equal than \a n otherwise.
   */
  FloatValBranch FLOAT_VAL(FloatBranchVal v, FloatBranchCommit c=NULL);
  //@}

}

#include <gecode/float/branch/val.hpp>

namespace Gecode {

  /**
   * \brief Which values to select for assignment
   *
   * \ingroup TaskModelFloatBranch
   */
  class FloatAssign : public ValBranch {
  public:
    /// Which value selection
    enum Select {
      SEL_MIN,       ///< Select median value of the lower part
      SEL_MAX,       ///< Select median value of the upper part
      SEL_RND,       ///< Select median value of a randomly chosen part
      SEL_VAL_COMMIT ///< Select value according to user-defined functions
    };
  protected:
    /// Which value to select
    Select s;
  public:
    /// Initialize with selection strategy \a s
    FloatAssign(Select s = SEL_MIN);
    /// Initialize with random number generator \a r
    FloatAssign(Rnd r);
    /// Initialize with value function \a f and commit function \a c
    FloatAssign(VoidFunction v, VoidFunction c);
    /// Return selection strategy
    Select select(void) const;
  };

  /**
   * \defgroup TaskModelFloatBranchAssign Value selection for assigning float variables
   * \ingroup TaskModelFloatBranch
   */
  //@{
  /// Select median value of the lower part
  FloatAssign FLOAT_ASSIGN_MIN(void);
  /// Select median value of the upper part
  FloatAssign FLOAT_ASSIGN_MAX(void);
  /// Select median value of a randomly chosen part
  FloatAssign FLOAT_ASSIGN_RND(Rnd r);
  /**
   * Select value as defined by the value function \a v and commit function \a c
   * The default commit function posts the constraint that the float variable
   * \a x must be less or equal than the value \a n.
   */
  FloatAssign FLOAT_ASSIGN(FloatBranchVal v, FloatBranchCommit c=NULL);
  //@}

}

#include <gecode/float/branch/assign.hpp>

namespace Gecode {

  /**
   * \brief Branch over \a x with variable selection \a vars and value selection \a vals
   *
   * \ingroup TaskModelFloatBranch
   */
  GECODE_FLOAT_EXPORT BrancherHandle
  branch(Home home, const FloatVarArgs& x,
         FloatVarBranch vars, FloatValBranch vals, 
         FloatBranchFilter bf=NULL,
         FloatVarValPrint vvp=NULL);
  /**
   * \brief Branch over \a x with tie-breaking variable selection \a vars and value selection \a vals
   *
   * \ingroup TaskModelFloatBranch
   */
  GECODE_FLOAT_EXPORT BrancherHandle
  branch(Home home, const FloatVarArgs& x,
         TieBreak<FloatVarBranch> vars, FloatValBranch vals,
         FloatBranchFilter bf=NULL,
         FloatVarValPrint vvp=NULL);
  /**
   * \brief Branch over \a x with value selection \a vals
   *
   * \ingroup TaskModelFloatBranch
   */
  GECODE_FLOAT_EXPORT BrancherHandle
  branch(Home home, FloatVar x, FloatValBranch vals,
         FloatVarValPrint vvp=NULL);

  /**
   * \brief Assign all \a x with value selection \a vals
   *
   * \ingroup TaskModelFloatBranch
   */
  GECODE_FLOAT_EXPORT BrancherHandle
  assign(Home home, const FloatVarArgs& x, FloatAssign vals,
         FloatBranchFilter fbf=NULL,
         FloatVarValPrint vvp=NULL);
  /**
   * \brief Assign \a x with value selection \a vals
   *
   * \ingroup TaskModelFloatBranch
   */
  GECODE_FLOAT_EXPORT BrancherHandle
  assign(Home home, FloatVar x, FloatAssign vals,
         FloatVarValPrint vvp=NULL);
  //@}

}

#endif

// IFDEF: GECODE_HAS_FLOAT_VARS
// STATISTICS: float-post

