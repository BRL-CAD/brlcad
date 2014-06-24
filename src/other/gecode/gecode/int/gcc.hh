/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Patrick Pekczynski <pekczynski@ps.uni-sb.de>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Patrick Pekczynski, 2004/2005
 *     Christian Schulte, 2009
 *     Guido Tack, 2009
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

#ifndef __GECODE_INT_GCC_HH__
#define __GECODE_INT_GCC_HH__

#include <gecode/int.hh>

/**
 * \namespace Gecode::Int::GCC
 * \brief Global cardinality propagators (Counting)
 */

#include <gecode/int/gcc/view.hpp>
#include <gecode/int/gcc/bnd-sup.hpp>
#include <gecode/int/gcc/dom-sup.hpp>

namespace Gecode { namespace Int { namespace GCC {

  /**
   * \brief Value consistent global cardinality propagator
   *
   * Requires \code #include <gecode/int/gcc.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Card>
  class Val : public Propagator {
  protected:
    /// Views on which to perform value-propagation
    ViewArray<IntView> x;
    /// Array containing either fixed cardinalities or CardViews
    ViewArray<Card> k;
    /// Constructor for posting
    Val(Home home, ViewArray<IntView>& x, ViewArray<Card>& k);
    /// Constructor for cloning \a p
    Val(Space& home, bool share, Val<Card>& p);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost funtion returning high linear
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Destructor
    virtual size_t dispose(Space& home);
    /// Post propagator for views \a x and cardinalities \a k
    static ExecStatus post(Home home,
                           ViewArray<IntView>& x, ViewArray<Card>& k);
  };

  /**
   * \brief Bounds consistent global cardinality propagator
   * 
   * The algorithm is taken from:
   *    Claude-Guy Quimper, Peter van Beek, Alejandro López-Ortiz,
   *    Alexander Golynski, and Sayyed Bashir Sadjad. An Efficient 
   *    Bounds Consistency Algorithm for the Global Cardinality 
   *    Constraint, CP 2003, pages 600-614.
   *
   * This implementation uses the code that is provided
   * by Peter Van Beek:
   *   http://ai.uwaterloo.ca/~vanbeek/software/software.html
   * The code here has only been slightly modified to fit Gecode
   * (taking idempotent/non-idempotent propagation into account)
   * and uses a more efficient layout of datastructures (keeping the
   * number of different arrays small).
   *
   * The Bnd class is used to post the propagator and BndImp
   * is the actual implementation taking shared variables into account.
   *
   * Requires \code #include <gecode/int/gcc.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Card>
  class Bnd : public Propagator {
  protected:
    /// Views on which to perform bounds-propagation
    ViewArray<IntView> x;
    /// Views on which to perform value-propagation (subset of \c x)
    ViewArray<IntView> y;
    /// Array containing either fixed cardinalities or CardViews
    ViewArray<Card> k;
    /**
     * \brief  Data structure storing the sum of the views lower bounds
     * Necessary for reasoning about the interval capacities in the
     * propagation algorithm.
     */
    PartialSum<Card> lps;
    /// Data structure storing the sum of the views upper bounds
    PartialSum<Card> ups;
    /**
     * \brief Stores whether cardinalities are all assigned
     *
     * If all cardinalities are assigned the propagation algorithm
     * only has to perform propagation for the upper bounds.
     */
    bool card_fixed;
    /**
     * \brief Stores whether the minium required occurences of
     *        the cardinalities are all zero. If so, we do not need
     *        to perform lower bounds propagation.
     */
    bool skip_lbc;
    /// Constructor for cloning \a p
    Bnd(Space& home, bool share, Bnd<Card>& p);

    /// Prune cardinality variables with 0 maximum occurrence
    ExecStatus pruneCards(Space& home);

    /**
     * \brief Lower Bounds constraint (LBC) stating
     * \f$ \forall j \in \{0, \dots, |k|-1\}:
     * \#\{i\in\{0, \dots, |x| - 1\} | x_i = card(k_j)\} \geq min(k_j)\f$
     * Hence the lbc constraints the variables such that every value occurs
     * at least as often as specified by its lower cardinality bound.
     * \param home current space
     * \param nb denotes number of unique bounds
     * \param hall contains information about the hall structure of the problem
     *        (cf. HallInfo)
     * \param rank ranking information about the variable bounds (cf. Rank)
     * \param mu permutation \f$ \mu \f$ such that
     *        \f$ \forall i\in \{0, \dots, |x|-2\}:
     *        max(x_{\mu(i)}) \leq max(x_{\mu(i+1)})\f$
     * \param nu permutation \f$ \nu \f$ such that
     *        \f$ \forall i\in \{0, \dots, |x|-2\}:
     *        min(x_{\mu(i)}) \leq min(x_{\mu(i+1)})\f$
     */
    ExecStatus lbc(Space& home, int& nb, HallInfo hall[], Rank rank[],
                   int mu[], int nu[]);

    /**
     * \brief Upper Bounds constraint (UBC) stating
     * \f$ \forall j \in \{0, \dots, |k|-1\}:
     * \#\{i\in\{0, \dots, |x| - 1\} | x_i = card(k_j)\} \leq max(k_j)\f$
     * Hence the ubc constraints the variables such that no value occurs
     * more often than specified by its upper cardinality bound.
     * \param home current space
     * \param nb denotes number of unique bounds
     * \param hall contains information about the hall structure of the problem
     *        (cf. HallInfo)
     * \param rank ranking information about the variable bounds (cf. Rank)
     * \param mu permutation \f$ \mu \f$ such that
     *        \f$ \forall i\in \{0, \dots, |x|-2\}:
     *        max(x_{\mu(i)}) \leq max(x_{\mu(i+1)})\f$
     * \param nu permutation \f$ \nu \f$ such that
     *        \f$ \forall i\in \{0, \dots, |x|-2\}:
     *        min(x_{\mu(i)}) \leq min(x_{\mu(i+1)})\f$
     */
    ExecStatus ubc(Space& home, int& nb, HallInfo hall[], Rank rank[],
                   int mu[], int nu[]);
    /// Constructor for posting
    Bnd(Home home, ViewArray<IntView>&, ViewArray<Card>&, bool, bool);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost funtion
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Destructor
    virtual size_t dispose(Space& home);
    /// Post propagator for views \a x and cardinalities \a k
    static ExecStatus post(Home home,
                           ViewArray<IntView>& x, ViewArray<Card>& k);
  };

  /**
   * \brief Domain consistent global cardinality propagator
   * 
   * The algorithm is taken from:
   *   Claude-Guy Quimper, Peter van Beek, Alejandro López-Ortiz,
   *   and Alexander Golynski. Improved Algorithms for the
   *   Global Cardinality Constraint, CP 2004, pages 542-556.
   *
   * Requires \code #include <gecode/int/gcc.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Card>
  class Dom : public Propagator {
  protected:
    /// Views on which to perform domain-propagation
    ViewArray<IntView> x;
    /**
     * \brief Views used to channel information between \c x and \c k
     * (\f$ x \subseteq y \f$).
     */
    ViewArray<IntView> y;
    /// Array containing either fixed cardinalities or CardViews
    ViewArray<Card> k;
    /// Propagation is performed on a variable-value graph (used as cache)
    VarValGraph<Card>* vvg;
    /**
     * \brief Stores whether cardinalities are all assigned
     *
     * If all cardinalities are assigned the propagation algorithm
     * only has to perform propagation for the upper bounds.
     */
    bool card_fixed;
    /// Constructor for cloning \a p
    Dom(Space& home, bool share, Dom<Card>& p);
    /// Constructor for posting
    Dom(Home home, ViewArray<IntView>&, ViewArray<Card>&, bool);
  public:
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost function
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Destructor
    virtual size_t dispose(Space& home);
    /// Post propagator for views \a x and cardinalities \a k
    static ExecStatus post(Home home,
                           ViewArray<IntView>& x, ViewArray<Card>& k);
  };

}}}

#include <gecode/int/gcc/post.hpp>
#include <gecode/int/gcc/val.hpp>
#include <gecode/int/gcc/bnd.hpp>
#include <gecode/int/gcc/dom.hpp>

#endif


// STATISTICS: int-prop

