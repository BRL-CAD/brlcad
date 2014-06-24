/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004, 2005
 *     Guido Tack, 2005
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

namespace Gecode { namespace Set {

  /**
   * \defgroup FuncThrowSet Set exceptions
   * \ingroup FuncThrow
   */

  /// %Exception: Value out of limits
  class GECODE_VTABLE_EXPORT OutOfLimits : public Exception {
  public:
    /// Initialize with location \a l
    OutOfLimits(const char* l);
  };

  //@{
  /// %Exception: %Variable created with empty domain
  class GECODE_VTABLE_EXPORT VariableEmptyDomain : public Exception {
  public:
    /// Initialize with location \a l
    VariableEmptyDomain(const char* l);
  };

  /// %Exception: No arguments available in argument array
  class GECODE_VTABLE_EXPORT TooFewArguments : public Exception {
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

  /// %Exception: Unknown value or variable selection passed as argument
  class GECODE_VTABLE_EXPORT UnknownBranching : public Exception {
  public:
    /// Initialize with location \a l
    UnknownBranching(const char* l);
  };

  /// %Exception: Unknown relation type passed as argument
  class GECODE_VTABLE_EXPORT UnknownRelation : public Exception {
  public:
    /// Initialize with location \a l
    UnknownRelation(const char* l);
  };

  /// %Exception: Unknown operation type passed as argument
  class GECODE_VTABLE_EXPORT UnknownOperation : public Exception {
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

  //@}

  /**
   * Classes for exceptions raised by set module
   *
   */

  inline
  OutOfLimits::OutOfLimits(const char* l)
    : Exception(l,"Number out of limits") {}

  inline
  VariableEmptyDomain::VariableEmptyDomain(const char* l)
    : Exception(l,"Attempt to create variable with empty domain") {}

  inline
  TooFewArguments::TooFewArguments(const char* l)
    : Exception(l,"Passed argument array has too few elements") {}

  inline
  ArgumentSizeMismatch::ArgumentSizeMismatch(const char* l)
    : Exception(l,"Sizes of argument arrays mismatch") {}

  inline
  IllegalOperation::IllegalOperation(const char* l)
    : Exception(l,"Illegal operation type") {}

  inline
  UnknownBranching::UnknownBranching(const char* l)
    : Exception(l,"Unknown branching type") {}

  inline
  UnknownRelation::UnknownRelation(const char* l)
    : Exception(l,"Unknown relation type") {}

  inline
  UnknownOperation::UnknownOperation(const char* l)
    : Exception(l,"Unknown operation type") {}

}}

// STATISTICS: set-other

