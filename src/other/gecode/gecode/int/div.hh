/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2013
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

#ifndef __GECODE_INT_DIV_HH__
#define __GECODE_INT_DIV_HH__

#include <gecode/int.hh>

namespace Gecode { namespace Int {

  /// Compute \f$\lceil x/y\rceil\f$ where \a x and \a y are non-negative
  template<class IntType>
  IntType ceil_div_pp(IntType x, IntType y);
  /// Compute \f$\lfloor x/y\rfloor\f$ where \a x and \a y are non-negative
  template<class IntType>
  IntType floor_div_pp(IntType x, IntType y);
  /// Compute \f$\lceil x/y\rceil\f$ where \a x is non-negative
  template<class IntType>
  IntType ceil_div_px(IntType x, IntType y);
  /// Compute \f$\lfloor x/y\rfloor\f$ where \a x is non-negative
  template<class IntType>
  IntType floor_div_px(IntType x, IntType y);
  /// Compute \f$\lceil x/y\rceil\f$ where \a y is non-negative
  template<class IntType>
  IntType ceil_div_xp(IntType x, IntType y);
  /// Compute \f$\lfloor x/y\rfloor\f$ where \a y is non-negative
  template<class IntType>
  IntType floor_div_xp(IntType x, IntType y);
  /// Compute \f$\lceil x/y\rceil\f$
  template<class IntType>
  IntType ceil_div_xx(IntType x, IntType y);
  /// Compute \f$\lfloor x/y\rfloor\f$
  template<class IntType>
  IntType floor_div_xx(IntType x, IntType y);

}}

#include <gecode/int/div.hpp>

#endif

// STATISTICS: int-prop

