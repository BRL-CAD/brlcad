/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

namespace Gecode {

  namespace Int { namespace Linear {

    /**
     * \brief No view serves as filler for empty view arrays
     *
     */
    class NoView : public ConstView<IntView> {
    public:
      /// \name Constructors and initialization
      //@{
      /// Default constructor
      NoView(void) {}
      //@}

      /// \name Value access
      //@{
      /// Return minimum of domain
      int min(void) const { return 0; }
      /// Return maximum of domain
      int max(void) const { return 0; }
      /// Return median of domain (greatest element not greater than the median)
      int med(void) const { return 0; }
      /// Return assigned value (only if assigned)
      int val(void) const { return 0; }

      /// Return size (cardinality) of domain
      unsigned int size(void) const { return 1; }
      /// Return width of domain (distance between maximum and minimum)
      unsigned int width(void) const { return 1; }
      /// Return regret of domain minimum (distance to next larger value)
      unsigned int regret_min(void) const { return 0; }
      /// Return regret of domain maximum (distance to next smaller value)
      unsigned int regret_max(void) const { return 0; }
      //@}

      /// \name Domain tests
      //@{
      /// Test whether domain is a range
      bool range(void) const { return true; }
      /// Test whether view is assigned
      bool assigned(void) const { return true; }

      /// Test whether \a n is contained in domain
      bool in(int n) const { (void) n; return false; }
      /// Test whether \a n is contained in domain
      bool in(long long int n) const { (void) n; return false; }
      //@}

      /// \name Domain update by value
      //@{
      /// Restrict domain values to be less or equal than \a n
      ModEvent lq(Space& home, int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be less or equal than \a n
      ModEvent lq(Space& home, long long int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be less than \a n
      ModEvent le(Space& home, int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be less than \a n
      ModEvent le(Space& home, long long int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be greater or equal than \a n
      ModEvent gq(Space& home, int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be greater or equal than \a n
      ModEvent gq(Space& home, long long int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be greater than \a n
      ModEvent gr(Space& home, int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be greater than \a n
      ModEvent gr(Space& home, long long int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be different from \a n
      ModEvent nq(Space& home, int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be different from \a n
      ModEvent nq(Space& home, long long int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be equal to \a n
      ModEvent eq(Space& home, int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      /// Restrict domain values to be equal to \a n
      ModEvent eq(Space& home, long long int n) {
        (void) home; (void) n;
        return ME_INT_NONE;
      }
      //@}
    };

    /**
     * \brief Print integer variable view
     * \relates Gecode::Int::Linear::NoView
     */
    template<class Char, class Traits>
    std::basic_ostream<Char,Traits>&
    operator <<(std::basic_ostream<Char,Traits>& os, const NoView&) { return os; }

  }}


  /**
   * \brief View array for no view (empty)
   *
   */
  template<>
  class ViewArray<Int::Linear::NoView> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor (array of size 0)
    ViewArray(void) {}
    /// Allocate array with \a m variables
    ViewArray(Space& home, int m) { (void) home; (void) m; }
    /// Initialize
    ViewArray(const ViewArray<Int::Linear::NoView>&) {}
    /// Initialize
    ViewArray(Space&, const ViewArray<Int::Linear::NoView>&) {}
    /// Initialize
    const ViewArray<Int::Linear::NoView>& operator =(const ViewArray<Int::Linear::NoView>&) { return *this; }
    //@}

    /// \name Array size
    //@{
    /// Return size of array (number of elements)
    int size(void) const { return 0; }
    /// Decrease size of array (number of elements)
    void size(int n) { (void) n; }
    //@}

    /// \name Array elements
    //@{
    /// Return view at position \a i
    Int::Linear::NoView operator [](int i) {
      (void)  i;
      Int::Linear::NoView n;
      return n;
    }
    /// Return view at position \a i
    const Int::Linear::NoView operator [](int i) const {
      (void)  i;
      Int::Linear::NoView n;
      return n;
    }
    //@}

    /// \name Dependencies
    //@{
    /// Subscribe propagator \a p with propagation condition \a pc to all views
    void subscribe(Space&, Propagator& p, PropCond pc, bool process=true) {
      (void) p; (void) pc; (void) process;
    }
    /// Cancel subscription of propagator \a p with propagation condition \a pc to all views
    void cancel(Space& home, Propagator& p, PropCond pc) {
      (void) home; (void) p; (void) pc;
    }
    //@}

    /// \name Cloning
    //@{
    /**
     * \brief Update array to be a clone of array \a a
     *
     * If \a share is true, sharing is retained for all shared
     * data structures. Otherwise, for each of them an independent
     * copy is created.
     */
    void update(Space&, bool share, ViewArray<Int::Linear::NoView>& a) {
      (void) share; (void) a;
    }
    //@}

    /// \name Moving elements
    //@{
    /// Move assigned view from position 0 to position \a i (shift elements to the left)
    void move_fst(int i) { (void) i; }
    /// Move assigned view from position \c size()-1 to position \a i (truncate array by one)
    void move_lst(int i) { (void) i; }
    //@}
  private:
    static void* operator new(size_t);
    static void  operator delete(void*,size_t);
  };

}


// STATISTICS: int-prop

