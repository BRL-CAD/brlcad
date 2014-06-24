/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
 *     Mikael Lagerkvist, 2005
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

namespace Gecode { namespace MiniModel {

  /**
   * \defgroup FuncThrowMiniModel MiniModel exceptions
   * \ingroup FuncThrow
   */

  //@{
  /// %Exception: Too few arguments available in argument array
  class GECODE_VTABLE_EXPORT TooFewArguments : public Exception  {
  public:
    /// Initialize with location \a l
    TooFewArguments(const char* l);
  };

  /// %Exception: Sizes of arguments does not match
  class GECODE_VTABLE_EXPORT ArgumentSizeMismatch : public Exception  {
  public:
    /// Initialize with location \a l
    ArgumentSizeMismatch(const char* l);
  };

  /// %Exception: Argument out of range
  class GECODE_VTABLE_EXPORT ArgumentOutOfRange : public Exception  {
  public:
    /// Initialize with location \a l
    ArgumentOutOfRange(const char* l);
  };
  //@}

  inline
  TooFewArguments::TooFewArguments(const char* l)
    : Exception(l,"Passed argument array has too few elements") {}

  inline
  ArgumentSizeMismatch::ArgumentSizeMismatch(const char* l)
    : Exception(l,"Sizes of argument arrays mismatch") {}

  inline
  ArgumentOutOfRange::ArgumentOutOfRange(const char* l)
    : Exception(l,"Argument out of range") {}

}}

// STATISTICS: minimodel-any
