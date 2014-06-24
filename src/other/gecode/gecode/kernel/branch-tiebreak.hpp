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

  /// Combine variable selection criteria for tie-breaking
  template<class VarBranch>
  class TieBreak {
  public:
    /// Branching criteria to try in order
    VarBranch a, b, c, d;
    /// Initialize with variable selection criteria
    TieBreak(VarBranch a0 = VarBranch(),
             VarBranch b0 = VarBranch(),
             VarBranch c0 = VarBranch(),
             VarBranch d0 = VarBranch());
  };

  /** 
   * \defgroup TaskModelBranchTieBreak Tie-breaking for variable selection
   * 
   * \ingroup TaskModelBranch
   */
  //@{
  /// Combine variable selection criteria \a a and \a b for tie-breaking
  template<class VarBranch>
  TieBreak<VarBranch>
  tiebreak(VarBranch a, VarBranch b);
  /// Combine variable selection criteria \a a, \a b, and \a c for tie-breaking
  template<class VarBranch>
  TieBreak<VarBranch>
  tiebreak(VarBranch a, VarBranch b, VarBranch c);
  /// Combine variable selection criteria \a a, \a b, \a c, and \a d for tie-breaking
  template<class VarBranch>
  TieBreak<VarBranch>
  tiebreak(VarBranch a, VarBranch b, VarBranch c, VarBranch d);
  //@}


  template<class VarBranch>
  forceinline
  TieBreak<VarBranch>::TieBreak(VarBranch a0,
                                VarBranch b0,
                                VarBranch c0,
                                VarBranch d0)
    : a(a0), b(b0), c(c0), d(d0) {}

  template<class VarBranch>
  forceinline TieBreak<VarBranch>
  tiebreak(VarBranch a, VarBranch b) {
    TieBreak<VarBranch> ab(a,b);
    return ab;
  }

  template<class VarBranch>
  forceinline TieBreak<VarBranch>
  tiebreak(VarBranch a, VarBranch b, VarBranch c) {
    TieBreak<VarBranch> abc(a,b,c);
    return abc;
  }

  template<class VarBranch>
  forceinline TieBreak<VarBranch>
  tiebreak(VarBranch a, VarBranch b, VarBranch c, VarBranch d) {
    TieBreak<VarBranch> abcd(a,b,c,d);
    return abcd;
  }

}

// STATISTICS: kernel-branch
