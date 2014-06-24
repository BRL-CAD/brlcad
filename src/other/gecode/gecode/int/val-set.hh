/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2011
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

#ifndef __GECODE_INT_VALSET_HH__
#define __GECODE_INT_VALSET_HH__

#include <gecode/int.hh>

namespace Gecode { namespace Int {

  /**
   * \brief Class for storing values of already assigned views
   */
  class ValSet {
  protected:
    /// First element of range list
    RangeList* fst;
    /// Last element of range list
    RangeList* lst;
    /// Number of stored values (integer precision is sufficient)
    int n;
  public:
    /// Initialize
    ValSet(void);
    /// Add value \a v to value set
    void add(Space& home, int v);
    /// Return size (number of values)
    int size(void) const;
    /// Test whether set is empty
    bool empty(void) const;
    /// Return smallest value (provided the set is not empty)
    int min(void) const;
    /// Return largest value (provided the set is not empty)
    int max(void) const;
    /// Compare view \a x with value set
    template<class View>
    Iter::Ranges::CompareStatus compare(View x) const;
    /// Whether all values of \a x are included in the value set
    template<class View>
    bool subset(View x) const;
    /// Update value set during cloning
    void update(Space& home, bool share, ValSet& vs);
    /// Flush entries
    void flush(void);
    /// Dispose value set
    void dispose(Space& home);
    /// Iterator over ranges
    class Ranges {
      /// Current position in range list
      const RangeList* c;
    public:
      /// \name Constructors and initialization
      //@{
      /// Initialize
      Ranges(const ValSet& vs);
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
  };

}}

#include <gecode/int/val-set.hpp>

#endif

// STATISTICS: int-other
