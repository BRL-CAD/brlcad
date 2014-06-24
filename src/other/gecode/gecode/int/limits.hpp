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

namespace Gecode { namespace Int {

  inline bool
  Limits::valid(int n) {
    return ((n >= min) && (n <= max));
  }
  inline bool
  Limits::valid(long long int n) {
    return ((n >= min) && (n <= max));
  }
  
  inline void
  Limits::check(int n, const char* l) {
    if ((n < min) || (n > max))
      throw OutOfLimits(l);
  }
  inline void
  Limits::check(long long int n, const char* l) {
    if ((n < min) || (n > max))
      throw OutOfLimits(l);
  }
  
  inline void
  Limits::positive(int n, const char* l) {
    if ((n <= 0) || (n > max))
      throw OutOfLimits(l);
  }
  inline void
  Limits::positive(long long int n, const char* l) {
    if ((n <= 0.0) || (n > max))
      throw OutOfLimits(l);
  }
  
  inline void
  Limits::nonnegative(int n, const char* l) {
    if ((n < 0) || (n > max))
      throw OutOfLimits(l);
  }
  inline void
  Limits::nonnegative(long long int n, const char* l) {
    if ((n < 0.0) || (n > max))
      throw OutOfLimits(l);
  }
  
  forceinline bool
  Limits::overflow_add(int n, int m) {
    long long int nm = 
      static_cast<long long int>(n) + static_cast<long long int>(m);
    return (nm > INT_MAX) || (nm < INT_MIN+1);
  }
  forceinline bool
  Limits::overflow_add(long long int n, long long int m) {
    if (m < 0)
      return n < LLONG_MIN + 1 - m;
    else
      return n > LLONG_MAX - m;
  }

  forceinline bool
  Limits::overflow_sub(int n, int m) {
    long long int nm = 
      static_cast<long long int>(n) - static_cast<long long int>(m);
    return (nm > INT_MAX) || (nm < INT_MIN+1);
  }
  forceinline bool
  Limits::overflow_sub(long long int n, long long int m) {
    if (m < 0)
      return n > LLONG_MAX + m;
    else
      return n < LLONG_MIN + 1 + m;
  }

  inline bool
  Limits::overflow_mul(int n, int m) {
    long long int nm = 
      static_cast<long long int>(n) * static_cast<long long int>(m);
    return (nm > INT_MAX) || (nm < INT_MIN+1);
  }

  inline bool
  Limits::overflow_mul(long long int n, long long int m) {
    // Check whether we can at least change the sign
    if ((n == LLONG_MIN) || (m == LLONG_MIN))
      return false;

    unsigned long long int un = 
      static_cast<unsigned long long int>(n < 0 ? -n : n);
    unsigned long long int um = 
      static_cast<unsigned long long int>(m < 0 ? -m : m);

    const unsigned int k = CHAR_BIT * sizeof(int);

    unsigned long long int un_hi = un >> k; 
    unsigned long long int un_lo = un & ((1ULL << k) - 1ULL);
    unsigned long long int um_hi = um >> k; 
    unsigned long long int um_lo = um & ((1ULL << k) - 1ULL);

    // This would mean that there is something larger than 2^64
    if ((un_hi != 0ULL) && (um_hi != 0ULL))
      return true;

    unsigned long long int unm_hi = 0ULL;

    // Now, either un_hi or m_hi can be different from zero
    if (un_hi != 0ULL)
      unm_hi = un_hi * um_lo;
    else if (um_hi != 0ULL)
      unm_hi = um_hi * un_lo;
    else
      return false;
    
    // Again, something larger than 2^64
    if ((unm_hi >> k) != 0ULL)
      return true;
    unm_hi <<= k;

    unsigned long long int unm_lo = un_lo * um_lo;

    return unm_hi > static_cast<unsigned long long int>(LLONG_MAX) - unm_lo;
  }

}}

// STATISTICS: int-var
