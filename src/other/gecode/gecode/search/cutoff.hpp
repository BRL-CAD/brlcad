/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2013
 *     Guido Tack, 2013
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

namespace Gecode { namespace Search {

  /// Cutoff generator for constant sequence
  class CutoffConstant : public Cutoff {
    friend class Cutoff;
  private:
    /// Constant
    unsigned long int c;
    /// Constructor
    CutoffConstant(unsigned long int c);
  public:
    /// Return next cutoff value
    virtual unsigned long int operator() (void);
  };

  /// Cutoff generator for linear sequence
  class CutoffLinear : public Cutoff {
    friend class Cutoff;
  private:
    /// Scale factor
    unsigned long int scale;
    /// Next number in sequence
    unsigned long int n;
    /// Constructor
    CutoffLinear(unsigned long int scale);
  public:
    /// Return next cutoff value
    virtual unsigned long int operator() (void);
  };

  /// Cutoff generator for the Luby sequence
  class CutoffLuby : public Cutoff {
    friend class Cutoff;
  private:
    /// Iteration number
    unsigned long int i;
    /// Scale factor
    unsigned long int scale;
    /// Number of pre-computed luby values
    static const unsigned long int n_start = 63U;
    /// Precomputed luby-values
    static unsigned long int start[n_start];
    /// Compute binary logarithm of \a i
    static unsigned long int log(unsigned long int i);
    /// Compute Luby number for step \a i
    static unsigned long int luby(unsigned long int i);
    /// Constructor
    CutoffLuby(unsigned long int scale);
  public:
    /// Return next cutoff value
    virtual unsigned long int operator() (void);
  };

  /// Cutoff generator for the geometric sequence
  class CutoffGeometric : public Cutoff {
    friend class Cutoff;
  private:
    /// Current cutoff value
    double n;
    /// Base
    double base;
    /// Constructor
    CutoffGeometric(unsigned long int scale, double base);
  public:
    /// Return next cutoff value
    virtual unsigned long int operator ()(void);
  };
  
  /// Cutoff generator for the random sequence
  class CutoffRandom : public Cutoff {
    friend class Cutoff;
  private:
    /// Random number generator
    Support::RandomGenerator rnd;
    /// Minimum cutoff value
    unsigned long int min;
    /// Random values
    unsigned long int n;
    /// Step size
    unsigned long int step;
    /// Constructor
    CutoffRandom(unsigned int seed, 
                 unsigned long int min, unsigned long int max, 
                 unsigned long int n);
  public:
    /// Return next cutoff value
    virtual unsigned long int operator ()(void);
  };
  
  /// Cutoff generator appending two cutoff generators
  class CutoffAppend : public Cutoff {
    friend class Cutoff;
  private:
    /// First cutoff generators
    Cutoff* c1;
    /// Second cutoff generators
    Cutoff* c2;
    /// How many number to take from the first
    unsigned long int n;
    /// Constructor
    CutoffAppend(Cutoff* c1, unsigned long int n, Cutoff* c2);
  public:
    /// Return next cutoff value
    virtual unsigned long int operator ()(void);
    /// Destructor
    virtual ~CutoffAppend(void);
  };

  /// Cutoff generator that repeats a cutoff from another cutoff generator
  class CutoffRepeat : public Cutoff {
    friend class Cutoff;
  private:
    /// Actual cutoff generator
    Cutoff* c;
    // Current cutoff
    unsigned int cutoff;
    // Iteration
    unsigned long int i;
    // Number of repetitions
    unsigned long int n;
    /// Constructor
    CutoffRepeat(Cutoff* c, unsigned long int n);
  public:
    /// Return next cutoff value
    virtual unsigned long int operator ()(void);
    /// Destructor
    virtual ~CutoffRepeat(void);
  };
  
  forceinline
  Cutoff::Cutoff(void) {}
  forceinline
  Cutoff::~Cutoff(void) {}
  forceinline void*
  Cutoff::operator new(size_t s) {
    return heap.ralloc(s);
  }
  forceinline void
  Cutoff::operator delete(void* p) {
    heap.rfree(p);
  }

}}

// STATISTICS: search-other
