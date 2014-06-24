/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2012
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

  /**
   * \brief Tie-break limit function
   * 
   * Here the value \a w is the worst and \b is the best merit value
   * found. The function must return the merit value that is considered
   * the limit for breaking ties.
   *
   * \ingroup TaskModelBranch
   */
  typedef double (*BranchTbl)(const Space& home, double w, double b);

  /**
   * \brief Variable branching information
   * \ingroup TaskModelBranch
   */
  class VarBranch {
  protected:
    /// Tie-breaking limit function
    BranchTbl _tbl;
    /// Random number generator
    Rnd _rnd;
    /// Decay information for AFC and activity
    double _decay;
    /// AFC information
    AFC _afc;
    /// Activity information
    Activity _act;
    /// Merit function (generic function pointer)
    VoidFunction _mf;
  public:
    /// Initialize with tie-break limit function \a t
    VarBranch(BranchTbl t);
    /// Initialize with random number generator \a r
    VarBranch(Rnd r);
    /// Initialize with decay factor \a d and tie-break limit function \a t
    VarBranch(double d, BranchTbl t);
    /// Initialize with AFC \a a and tie-break limit function \a t
    VarBranch(AFC a, BranchTbl t);
    /// Initialize with activity \a a and tie-break limit function \a t
    VarBranch(Activity a, BranchTbl t);
    /// Initialize with merit function \a f and tie-break limit function \a t
    VarBranch(void (*f)(void), BranchTbl t);
    /// Return tie-break limit function
    BranchTbl tbl(void) const;
    /// Return random number generator
    Rnd rnd(void) const;
    /// Return decay factor
    double decay(void) const;
    /// Return AFC
    AFC afc(void) const;
    /// Set AFC to \a a
    void afc(AFC a);
    /// Return activity
    Activity activity(void) const;
    /// Set activity to \a a
    void activity(Activity a);
    /// Return merit function
    VoidFunction merit(void) const;
  };

  // Variable branching
  forceinline 
  VarBranch::VarBranch(BranchTbl t)
    : _tbl(t), _decay(1.0) {}

  forceinline 
  VarBranch::VarBranch(double d, BranchTbl t)
    : _tbl(t), _decay(d) {}

  forceinline 
  VarBranch::VarBranch(AFC a, BranchTbl t)
    : _tbl(t), _decay(1.0), _afc(a) {
    if (!_afc.initialized())
      throw UninitializedAFC("VarBranch::VarBranch");
  }

  forceinline 
  VarBranch::VarBranch(Activity a, BranchTbl t)
    : _tbl(t), _decay(1.0), _act(a) {
    if (!_act.initialized())
      throw UninitializedActivity("VarBranch::VarBranch");
  }

  forceinline 
  VarBranch::VarBranch(Rnd r)
    : _tbl(NULL), _rnd(r), _decay(1.0) {
    if (!_rnd.initialized())
      throw UninitializedRnd("VarBranch::VarBranch");
  }

  forceinline 
  VarBranch::VarBranch(VoidFunction f, BranchTbl t)
    : _tbl(t), _decay(1.0), _mf(f) {}

  forceinline BranchTbl
  VarBranch::tbl(void) const {
    return _tbl;
  }

  inline Rnd
  VarBranch::rnd(void) const {
    return _rnd;
  }

  inline double
  VarBranch::decay(void) const {
    return _decay;
  }

  inline AFC
  VarBranch::afc(void) const {
    return _afc;
  }

  inline void
  VarBranch::afc(AFC a) {
    _afc=a;
  }

  inline Activity
  VarBranch::activity(void) const {
    return _act;
  }

  inline void
  VarBranch::activity(Activity a) {
    _act=a;
  }

  forceinline VoidFunction
  VarBranch::merit(void) const {
    return _mf;
  }

}

// STATISTICS: kernel-branch
