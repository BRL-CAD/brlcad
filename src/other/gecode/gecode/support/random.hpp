/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2005
 *     Mikael Lagerkvist, 2005
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

namespace Gecode { namespace Support {

  /** \brief Template for linear congruential generators
   *
   * This class template defines a simple class for linear
   * congruential generators.
   *
   * \ingroup FuncSupport
   */
  template<unsigned int m, unsigned int a, unsigned int q, unsigned int r>
  class LinearCongruentialGenerator {
  private:
    /// The maximum size of random numbers generated.
    static const unsigned int max = 1UL<<31;
    /// Current seed value
    unsigned int s;
    /// Generate next number in series
    unsigned int next(void);
  public:
    /// Set the current seed to \a s
    void seed(unsigned int s);
    /// Construct the generator instance with seed \a s
    LinearCongruentialGenerator(unsigned int s = 1);
    /// Return current seed
    unsigned int seed(void) const;
    /// Returns a random integer from the interval [0..n)
    unsigned int operator ()(unsigned int n);
    /// Report size occupied
    size_t size(void) const;
  };

  template<unsigned int m, unsigned int a, unsigned int q, unsigned int r>
  forceinline unsigned int
  LinearCongruentialGenerator<m,a,q,r>::next(void) {
    s = a*(s%q) - r*(s/q);
    unsigned int res = s;
    if (s==0) s = 1;
    return res;
  }
  template<unsigned int m, unsigned int a, unsigned int q, unsigned int r>
  forceinline void
  LinearCongruentialGenerator<m,a,q,r>::seed(unsigned int _s) {
    s = _s % m;
    if (s == 0) s = 1;
  }
  template<unsigned int m, unsigned int a, unsigned int q, unsigned int r>
  forceinline
  LinearCongruentialGenerator<m,a,q,r>::
  LinearCongruentialGenerator(unsigned int _s) {
    seed(_s);
  }
  template<unsigned int m, unsigned int a, unsigned int q, unsigned int r>
  forceinline unsigned int
  LinearCongruentialGenerator<m,a,q,r>::seed(void) const {
    return s;
  }
  template<unsigned int m, unsigned int a, unsigned int q, unsigned int r>
  forceinline unsigned int
  LinearCongruentialGenerator<m,a,q,r>::operator ()(unsigned int n) {
    unsigned int x1 = next() & ((1<<16)-1);
    unsigned int x2 = next() & ((1<<16)-1);
    if (n < 2) return 0;
    double d = static_cast<double>(((x1<<16) | x2) % max) / max;
    unsigned int val = static_cast<unsigned int>(n * d);
    return (val < n) ? val : (n-1);
  }
  template<unsigned int m, unsigned int a, unsigned int q, unsigned int r>
  forceinline size_t
  LinearCongruentialGenerator<m,a,q,r>::size(void) const {
    return sizeof(LinearCongruentialGenerator<m,a,q,r>);
  }


  /** \brief Default values for linear congruential generator
   *
   * While this pseudo-random number generator is not a good source of
   * randomness, it is still an acceptable choice for many
   * applications. The choice of values is taken from D. E. Knuth,
   * The Art of Computer Programming, Vol 2, Seminumerical Algorithms,
   * 3rd edition.
   *
   * \ingroup FuncSupport
   */
  typedef LinearCongruentialGenerator<2147483647, 48271, 44488, 3399>
  RandomGenerator;

}}

// STATISTICS: support-any
