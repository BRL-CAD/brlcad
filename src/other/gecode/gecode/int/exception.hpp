/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
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

namespace Gecode { namespace Int {

  /**
   * \defgroup FuncThrowInt Integer exceptions
   * \ingroup FuncThrow
   */

  //@{

  /// %Exception: Value out of limits
  class GECODE_VTABLE_EXPORT OutOfLimits : public Exception {
  public:
    /// Initialize with location \a l
    OutOfLimits(const char* l);
  };

  /// %Exception: Not 0/1 integer
  class GECODE_VTABLE_EXPORT NotZeroOne : public Exception {
  public:
    /// Initialize with location \a l
    NotZeroOne(const char* l);
  };


  /// %Exception: %Variable created with empty domain
  class GECODE_VTABLE_EXPORT VariableEmptyDomain : public Exception {
  public:
    /// Initialize with location \a l
    VariableEmptyDomain(const char* l);
  };

  /// %Exception: Too few arguments available in argument array
  class GECODE_VTABLE_EXPORT TooFewArguments : public Exception  {
  public:
    /// Initialize with location \a l
    TooFewArguments(const char* l);
  };

  /// %Exception: Arguments are of different size
  class GECODE_VTABLE_EXPORT ArgumentSizeMismatch : public Exception  {
  public:
    /// Initialize with location \a l
    ArgumentSizeMismatch(const char* l);
  };

  /// %Exception: Arguments contain same variable multiply
  class GECODE_VTABLE_EXPORT ArgumentSame : public Exception  {
  public:
    /// Initialize with location \a l
    ArgumentSame(const char* l);
  };

  /// %Exception: Unknown relation passed as argument
  class GECODE_VTABLE_EXPORT UnknownRelation : public Exception  {
  public:
    /// Initialize with location \a l
    UnknownRelation(const char* l);
  };

  /// %Exception: Unknown operation passed as argument
  class GECODE_VTABLE_EXPORT UnknownOperation : public Exception  {
  public:
    /// Initialize with location \a l
    UnknownOperation(const char* l);
  };

  /// %Exception: Illegal operation passed as argument
  class GECODE_VTABLE_EXPORT IllegalOperation : public Exception  {
  public:
    /// Initialize with location \a l
    IllegalOperation(const char* l);
  };

  /// %Exception: Unknown value or variable selection passed as argument
  class GECODE_VTABLE_EXPORT UnknownBranching : public Exception {
  public:
    /// Initialize with location \a l
    UnknownBranching(const char* l);
  };

  /// %Exception: Unknown reification mode passed as argument
  class GECODE_VTABLE_EXPORT UnknownReifyMode : public Exception {
  public:
    /// Initialize with location \a l
    UnknownReifyMode(const char* l);
  };

  /// %Exception: Attempt to access value of unassigned variable
  class GECODE_VTABLE_EXPORT ValOfUnassignedVar : public Exception {
  public:
    /// Initialize with location \a l
    ValOfUnassignedVar(const char* l);
  };

  /// %Exception: Tuple set not yet finalized
  class GECODE_VTABLE_EXPORT NotYetFinalized : public Exception {
  public:
    /// Initialize with location \a l
    NotYetFinalized(const char* l);
  };

  /// %Exception: Variable in symmetry not branched on
  class GECODE_VTABLE_EXPORT LDSBUnbranchedVariable : public Exception {
  public:
    /// Initialize with location \a l
    LDSBUnbranchedVariable(const char* l);
  };

  /// %Exception: Value selection incompatible with LDSB
  class GECODE_VTABLE_EXPORT LDSBBadValueSelection : public Exception {
  public:
    /// Initialize with location \a l
    LDSBBadValueSelection(const char* l);
  };

  //@}





  /*
   * Classes for exceptions raised by integer module
   *
   */

  inline
  OutOfLimits::OutOfLimits(const char* l)
    : Exception(l,"Number out of limits") {}

  inline
  VariableEmptyDomain::VariableEmptyDomain(const char* l)
    : Exception(l,"Attempt to create variable with empty domain") {}

  inline
  NotZeroOne::NotZeroOne(const char* l)
    : Exception(l,"Not a zero/one integer value") {}


  inline
  TooFewArguments::TooFewArguments(const char* l)
    : Exception(l,"Passed argument array has too few elements") {}

  inline
  ArgumentSizeMismatch::ArgumentSizeMismatch(const char* l)
    : Exception(l,"Sizes of argument arrays mismatch") {}

  inline
  ArgumentSame::ArgumentSame(const char* l)
    : Exception(l,"Argument array contains same variable multiply") {}


  inline
  UnknownRelation::UnknownRelation(const char* l)
    : Exception(l,"Unknown relation type") {}

  inline
  UnknownOperation::UnknownOperation(const char* l)
    : Exception(l,"Unknown operation type") {}

  inline
  IllegalOperation::IllegalOperation(const char* l)
    : Exception(l,"Illegal operation type") {}

  inline
  UnknownBranching::UnknownBranching(const char* l)
    : Exception(l,"Unknown branching type") {}

  inline
  UnknownReifyMode::UnknownReifyMode(const char* l)
    : Exception(l,"Unknown reification mode") {}

  inline
  ValOfUnassignedVar::ValOfUnassignedVar(const char* l)
    : Exception(l,"Attempt to access value of unassigned variable") {}

  inline
  NotYetFinalized::NotYetFinalized(const char* l)
    : Exception(l,"Tuple set not yet finalized") {}

  inline
  LDSBUnbranchedVariable::LDSBUnbranchedVariable(const char* l)
    : Exception(l,"Variable in symmetry not branched on") {}

  inline
  LDSBBadValueSelection::LDSBBadValueSelection(const char* l)
    : Exception(l,"Value selection incomatible with LDSB") {}

}}

// STATISTICS: int-other

