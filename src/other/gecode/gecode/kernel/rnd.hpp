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

#include <ctime>

namespace Gecode {

  /**
   * \brief Random number generator
   * \ingroup TaskModel
   */
  class Rnd : public SharedHandle {
  private:
    /// Implementation of generator
    class IMP : public SharedHandle::Object {
    public:
      /// The actual generator
      Support::RandomGenerator rg;
      /// Initialize generator with seed \a s
      IMP(unsigned int s);
      /// Delete implemenentation
      virtual ~IMP(void);
      /// Create a copy
      GECODE_KERNEL_EXPORT
      virtual SharedHandle::Object* copy(void) const;
    };
  public:
    /// Default constructor that does not initialize the generator
    Rnd(void);
    /// Initialize with seed \a s
    GECODE_KERNEL_EXPORT Rnd(unsigned int s);
    /// Initialize from generator \a r
    Rnd(const Rnd& r);
    /// Set the current seed to \a s (initializes if needed)
    GECODE_KERNEL_EXPORT
    void seed(unsigned int s);
    /// Set current seed based on time (initializes if needed)
    void time(void);
    /// Set current seed to hardware-based random number (initializes if needed)
    void hw(void);
    /// Return current seed
    unsigned int seed(void) const;
    /// Return a random integer from the interval [0..n)
    unsigned int operator ()(unsigned int n);
    /// Test whether generator has been properly initialized
    bool initialized(void) const;
  };

  forceinline
  Rnd::IMP::IMP(unsigned int s)
    : rg(s) {}

  forceinline
  Rnd::IMP::~IMP(void) {}

  forceinline
  Rnd::Rnd(void) {}
  forceinline
  Rnd::Rnd(const Rnd& r)
    : SharedHandle(r) {}
  inline void
  Rnd::time(void) {
    seed(static_cast<unsigned int>(::time(NULL)));
  }
  inline void
  Rnd::hw(void) {
    seed(Support::hwrnd());
  }
  forceinline unsigned int
  Rnd::seed(void) const {
    const IMP* i = static_cast<const IMP*>(object());
    return i->rg.seed();
  }
  forceinline unsigned int
  Rnd::operator ()(unsigned int n) {
    IMP* i = static_cast<IMP*>(object());
    return i->rg(n);
  }
  forceinline bool
  Rnd::initialized(void) const {
    return object() != NULL;
  }

}

// STATISTICS: kernel-other
