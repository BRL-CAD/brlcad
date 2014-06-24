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

#ifndef __GECODE_INT_SUPPORT_VALUES_HH__
#define __GECODE_INT_SUPPORT_VALUES_HH__

#include <gecode/int.hh>

namespace Gecode { namespace Int {

  /**
   * \brief %Support value iterator and recorder
   *
   * Requires \code #include <gecode/int/support-values.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class View, class A>
  class SupportValues {
  private:
    /// Memory allocator
    A& a;
    /// Range and position information
    class RangePos {
    public:
      int min;          ///< Minimum of range
      unsigned int pos; ///< Starting position of range
    };
    /// Value iterator for unsupported values
    class Unsupported {
    private:
      /// Current range
      RangePos* rp;
      /// Current position
      unsigned int p;
      /// Access to other information
      SupportValues& sv;
      /// Find next unsupported value
      void find(void);
    public:
      /// \name Constructors and initialization
      //@{
      /// Initialize with values from \a sv
      Unsupported(SupportValues& sv0);
      //@}

      /// \name Iteration control
      //@{
      /// Test whether iterator is still at a value or done
      bool operator ()(void) const;
      /// Move iterator to next value (if possible)
      void operator ++(void);
      //@}

      /// \name Value access
      //@{
      /// Return current value
      int val(void) const;
      //@}
    };

    /// The view
    View x;
    /// Bit set
    Gecode::Support::BitSetBase bs;
    /// Start of range and position information
    RangePos* rp_fst;
    /// End of range and position information
    RangePos* rp_lst;
    /// Current range
    RangePos* rp;
    /// Current value
    int v;
    /// Current maximum of range
    int max;

    /// Mark \a n as supported and return whether value can be supported
    bool _support(int n);
  public:
    /// Initialize for view \a x
    SupportValues(A& a, View x);
    /// Destructor
    ~SupportValues(void);

    /// \name Iteration control
    //@{
    /// Reset iterator
    void reset(void);
    /// Test whether iterator is still at a value or done
    bool operator ()(void) const;
    /// Move iterator to next value (if possible)
    void operator ++(void);
    //@}

    /// \name Value access
    //@{
    /// Return current value
    int val(void) const;
    //@}

    /// \name Support control
    //@{
    /// Mark current (iterator) value as supported
    void support(void);
    /// Mark \a n as supported if possible
    bool support(int n);
    /// Mark \a n as supported if possible
    bool support(long long int n);
    /// Remove all unsupported values
    ModEvent tell(Space& home);
    //@}
  };

}}

#include <gecode/int/support-values.hpp>

#endif

// STATISTICS: int-prop

