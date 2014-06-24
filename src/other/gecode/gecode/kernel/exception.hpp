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

namespace Gecode {

  /**
   * \defgroup FuncThrowKernel Kernel exceptions
   * \ingroup FuncThrow
   */

  //@{

  /// %Exception: Operation on failed space invoked
  class GECODE_VTABLE_EXPORT SpaceFailed : public Exception {
  public:
    /// Initialize with location \a l
    SpaceFailed(const char* l);
  };

  /// %Exception: Operation on not stable space invoked
  class GECODE_VTABLE_EXPORT SpaceNotStable : public Exception {
  public:
    /// Initialize with location \a l
    SpaceNotStable(const char* l);
  };

  /// %Exception: Copy constructor did not call base class copy constructor
  class GECODE_VTABLE_EXPORT SpaceNotCloned : public Exception {
  public:
    /// Initialize with location \a l
    SpaceNotCloned(const char* l);
  };

  /// %Exception: Commit when no brancher present
  class GECODE_VTABLE_EXPORT SpaceNoBrancher : public Exception {
  public:
    /// Initialize with location \a l
    SpaceNoBrancher(const char* l);
  };

  /// %Exception: Commit with illegal alternative
  class GECODE_VTABLE_EXPORT SpaceIllegalAlternative : public Exception {
  public:
    /// Initialize with location \a l
    SpaceIllegalAlternative(const char* l);
  };

  /// %Exception: too many branchers
  class GECODE_VTABLE_EXPORT TooManyBranchers : public Exception {
  public:
    /// Initialize with location \a l
    TooManyBranchers(const char* l);
  };

  /// %Exception: illegal decay factor
  class GECODE_VTABLE_EXPORT IllegalDecay : public Exception {
  public:
    /// Initialize with location \a l
    IllegalDecay(const char* l);
  };

  /// %Exception: uninitialized AFC
  class GECODE_VTABLE_EXPORT UninitializedAFC : public Exception {
  public:
    /// Initialize with location \a l
    UninitializedAFC(const char* l);
  };

  /// %Exception: uninitialized activity
  class GECODE_VTABLE_EXPORT UninitializedActivity : public Exception {
  public:
    /// Initialize with location \a l
    UninitializedActivity(const char* l);
  };

  /// %Exception: uninitialized random number generator
  class GECODE_VTABLE_EXPORT UninitializedRnd : public Exception {
  public:
    /// Initialize with location \a l
    UninitializedRnd(const char* l);
  };

  /// %Exception: AFC has wrong arity
  class GECODE_VTABLE_EXPORT AFCWrongArity : public Exception {
  public:
    /// Initialize with location \a l
    AFCWrongArity(const char* l);
  };

  /// %Exception: activity has wrong arity
  class GECODE_VTABLE_EXPORT ActivityWrongArity : public Exception {
  public:
    /// Initialize with location \a l
    ActivityWrongArity(const char* l);
  };

  //@}

  /*
   * Classes for exceptions raised by kernel
   *
   */
  inline
  SpaceFailed::SpaceFailed(const char* l)
    : Exception(l,"Attempt to invoke operation on failed space") {}

  inline
  SpaceNotStable::SpaceNotStable(const char* l)
    : Exception(l,"Attempt to invoke operation on not stable space") {}

  inline
  SpaceNotCloned::SpaceNotCloned(const char* l)
    : Exception(l,"Copy constructor of space did not call base class copy constructor") {}

  inline
  SpaceNoBrancher::SpaceNoBrancher(const char* l)
    : Exception(l,"Attempt to commit with no brancher") {}

  inline
  SpaceIllegalAlternative::SpaceIllegalAlternative(const char* l)
    : Exception(l,"Attempt to commit with illegal alternative") {}

  inline
  TooManyBranchers::TooManyBranchers(const char* l)
    : Exception(l,"Too many branchers created") {}

  inline
  UninitializedRnd::UninitializedRnd(const char* l)
    : Exception(l,"Uninitialized random generator for branching") {}

  inline
  IllegalDecay::IllegalDecay(const char* l)
    : Exception(l,"Illegal decay factor") {}

  inline
  UninitializedAFC::UninitializedAFC(const char* l)
    : Exception(l,"Uninitialized AFC information for branching") {}

  inline
  UninitializedActivity::UninitializedActivity(const char* l)
    : Exception(l,"Uninitialized activity information for branching") {}

  inline
  AFCWrongArity::AFCWrongArity(const char* l)
    : Exception(l,"AFC has wrong number of variables") {}

  inline
  ActivityWrongArity::ActivityWrongArity(const char* l)
    : Exception(l,"Activity has wrong number of variables") {}

}

// STATISTICS: kernel-other
