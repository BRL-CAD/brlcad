/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
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

#include <climits>

namespace Gecode { namespace Support {

  /// Description of integer types
  enum IntType {
    IT_CHAR = 0, ///< char integer type
    IT_SHRT = 1, ///< short integer type
    IT_INT  = 2  ///< integer type
  };

  /// Return type required to represent \a n
  IntType u_type(unsigned int n);
  /// Return type required to represent \a n
  IntType s_type(signed int n);

  /// Traits to for information about integer types
  template<class IntType>
  class IntTypeTraits {};

  /// Traits for signed char
  template<>
  class IntTypeTraits<signed char> {
  public:
    /// Corresponding unsigned type
    typedef unsigned char utype;
    /// Corresponding signed type
    typedef signed char stype;
    /// Minimal value
    static const signed char min = SCHAR_MIN;
    /// Maximal value
    static const signed char max = SCHAR_MAX;
    /// Description of type
    static const IntType description = IT_CHAR;
  };
  /// Traits for unsigned char
  template<>
  class IntTypeTraits<unsigned char> {
  public:
    /// Corresponding unsigned type
    typedef unsigned char utype;
    /// Corresponding signed type
    typedef signed char stype;
    /// Minimal value
    static const unsigned char min = 0;
    /// Maximal value
    static const unsigned char max = UCHAR_MAX;
    /// Description of type
    static const IntType description = IT_CHAR;
  };
  /// Traits for signed short int
  template<>
  class IntTypeTraits<signed short int> {
  public:
    /// Corresponding unsigned type
    typedef unsigned short int utype;
    /// Corresponding signed type
    typedef signed short int stype;
    /// Minimal value
    static const signed short int min = SHRT_MIN;
    /// Maximal value
    static const signed short int max = SHRT_MAX;
    /// Description of type
    static const IntType description = IT_SHRT;
  };
  /// Traits for unsigned short int
  template<>
  class IntTypeTraits<unsigned short int> {
  public:
    /// Corresponding unsigned type
    typedef unsigned short int utype;
    /// Corresponding signed type
    typedef signed short int stype;
    /// Minimal value
    static const unsigned short int min = 0;
    /// Maximal value
    static const unsigned short int max = USHRT_MAX;
    /// Description of type
    static const IntType description = IT_SHRT;
  };
  /// Traits for signed integer
  template<>
  class IntTypeTraits<signed int> {
  public:
    /// Corresponding unsigned type
    typedef unsigned int utype;
    /// Corresponding signed type
    typedef signed int stype;
    /// Minimal value
    static const signed int min = INT_MIN;
    /// Maximal value
    static const signed int max = INT_MAX;
    /// Description of type
    static const IntType description = IT_INT;
  };
  /// Traits for unsigned integer
  template<>
  class IntTypeTraits<unsigned int> {
  public:
    /// Corresponding unsigned type
    typedef unsigned int utype;
    /// Corresponding signed type
    typedef signed int stype;
    /// Minimal value
    static const unsigned int min = 0;
    /// Maximal value
    static const unsigned int max = UINT_MAX;
    /// Description of type
    static const IntType description = IT_INT;
  };


  forceinline IntType
  u_type(unsigned int n) {
    if (n < UCHAR_MAX)
      return IT_CHAR;
    else if (n < USHRT_MAX)
      return IT_SHRT;
    else
      return IT_INT;
  }

  forceinline IntType
  s_type(int n) {
    if ((n > SCHAR_MIN) && (n < SCHAR_MAX))
      return IT_CHAR;
    else if ((n > SHRT_MIN) && (n < SHRT_MAX))
      return IT_SHRT;
    else
      return IT_INT;
  }

}}

// STATISTICS: support-any
