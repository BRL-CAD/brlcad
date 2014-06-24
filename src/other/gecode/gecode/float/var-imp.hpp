/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Filip Konvicka <filip.konvicka@logis.cz>
 *     Lubomir Moric <lubomir.moric@logis.cz>
 *     Vincent Barichard <Vincent.Barichard@univ-angers.fr>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     LOGIS, s.r.o., 2008
 *     Christian Schulte, 2010
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

#include <cmath>

namespace Gecode { namespace Float {

  class FloatVarImp;

  /// Float delta information for advisors
  class FloatDelta : public Delta {
    friend class FloatVarImp;
  private:
    FloatNum _min; ///< Minimum value just pruned
    FloatNum _max; ///< Largest value just pruned
  public:
    /// Create float delta as providing no information
    FloatDelta(void);
    /// Create float delta with \a min and \a max
    FloatDelta(FloatNum min, FloatNum max);
  private:
    /// Return minimum
    FloatNum min(void) const;
    /// Return maximum
    FloatNum max(void) const;
  };

}}

#include <gecode/float/var-imp/delta.hpp>

namespace Gecode { namespace Float {

  /**
   * \brief Float variable implementation
   *
   * \ingroup Other
   */
  class FloatVarImp : public FloatVarImpBase {
  protected:
    /// Domain information
    FloatVal dom;
    /// Constructor for cloning \a x
    FloatVarImp(Space& home, bool share, FloatVarImp& x);
  public:
    /// Initialize with interval \a d
    FloatVarImp(Space& home, const FloatVal& d);

    /// \name Value access
    //@{
    /// Return domain
    FloatVal domain(void) const;
    /// Return minimum of domain
    FloatNum min(void) const;
    /// Return maximum of domain
    FloatNum max(void) const;
    /// Return value of domain (only if assigned)
    FloatVal val(void) const;
    /// Return median of domain (closest representation)
    FloatNum med(void) const;

    /// Return width of domain (distance between maximum and minimum)
    FloatNum size(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether variable is assigned
    bool assigned(void) const;

    /// Test whether 0 is contained in domain
    bool zero_in(void) const;
    /// Test whether \a n is contained in domain
    bool in(FloatNum n) const;
    /// Test whether \a n is contained in domain
    bool in(const FloatVal& n) const;
    //@}

    /// \name Domain update by value
    //@{
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, FloatNum n);
    /// Restrict domain values to be equal to \a n
    ModEvent eq(Space& home, const FloatVal& n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, FloatNum n);
    /// Restrict domain values to be less or equal than \a n
    ModEvent lq(Space& home, const FloatVal& n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, FloatNum n);
    /// Restrict domain values to be greater or equal than \a n
    ModEvent gq(Space& home, const FloatVal& n);
    //@}

    /// \name Dependencies
    //@{
    /**
     * \brief Subscribe propagator \a p with propagation condition \a pc to variable
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     *
     */
    void subscribe(Space& home, Propagator& p, PropCond pc, bool schedule=true);
    /// Cancel subscription of propagator \a p with propagation condition \a pc
    void cancel(Space& home, Propagator& p, PropCond pc);
    /// Subscribe advisor \a a to variable
    void subscribe(Space& home, Advisor& a);
    /// Cancel subscription of advisor \a a
    void cancel(Space& home, Advisor& a);
    //@}

    /// \name Variable implementation-dependent propagator support
    //@{
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}


  private:
    /// Return copy of not-yet copied variable
    GECODE_FLOAT_EXPORT FloatVarImp* perform_copy(Space& home, bool share);
  public:
    /// \name Cloning
    //@{
    /// Return copy of this variable
    FloatVarImp* copy(Space& home, bool share);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned
    static FloatNum min(const Delta& d);
    /// Return maximum value just pruned
    static FloatNum max(const Delta& d);
    //@}
  };

}}

#include <gecode/float/var-imp/float.hpp>

// STATISTICS: float-var

