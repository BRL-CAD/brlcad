/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2007
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

namespace Gecode { namespace Iter { namespace Values {

  /**
   * \brief Value iterator for single value
   *
   * \ingroup FuncIterValues
   */

  class Singleton {
  protected:
    /// Whether the iterator is done
    bool done;
    /// The single value
    int v;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Singleton(void);
    /// Initialize with value \a v
    Singleton(int v);
    /// Initialize with value \a v
    void init(int v);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a value or done
    bool operator ()(void) const;
    /// Move iterator to next value (if possible)
    void operator ++(void);
    //@}

    /// \name %Value access
    //@{
    /// Return current value
    int val(void) const;
    //@}
  };


  forceinline
  Singleton::Singleton(void) {}

  forceinline
  Singleton::Singleton(int v0)
    : done(false), v(v0) {}

  forceinline void
  Singleton::init(int v0) {
    done=false; v=v0;
  }

  forceinline void
  Singleton::operator ++(void) {
    done=true;
  }
  forceinline bool
  Singleton::operator ()(void) const {
    return !done;
  }

  forceinline int
  Singleton::val(void) const {
    return v;
  }


}}}

// STATISTICS: iter-any

