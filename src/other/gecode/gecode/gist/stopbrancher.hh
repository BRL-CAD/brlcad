/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2006
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <gecode/kernel.hh>
#include <gecode/gist.hh>

namespace Gecode { namespace Gist {
  
  /// %Choice for %StopBrancher
  class GECODE_GIST_EXPORT StopChoice : public Choice {
  public:
    /// Initialize choice for brancher \a b
    StopChoice(const Brancher& b);
    /// Report size occupied
    virtual size_t size(void) const;
    /// Archive into \a e
    virtual void archive(Archive& e) const;
  };
  
  /// %Brancher that stops exploration in %Gist
  class StopBrancher : public Brancher {
  protected:
    /// Flag whether brancher has been executed
    bool done;
    /// Construct brancher
    StopBrancher(Home home);
    /// Copy constructor
    StopBrancher(Space& home, bool share, StopBrancher& b);
  public:
    /// Check status of brancher, return true if alternatives left
    virtual bool status(const Space&) const;
    /// Return choice
    virtual Choice* choice(Space&);
    /// Return choice
    virtual Choice* choice(const Space& home, Archive&);
    /// Perform commit for choice \a _c and alternative \a a
    virtual ExecStatus commit(Space&, const Choice&, unsigned int);
    /// Print explanation
    virtual void print(const Space& home, const Gecode::Choice& c, 
                       unsigned int,
                       std::ostream& o) const;
    /// Copy brancher
    virtual Actor* copy(Space& home, bool share);
    /// Post brancher
    static void post(Home home);
    /// Delete brancher and return its size
    virtual size_t dispose(Space&);
  };
  
}}

// STATISTICS: gist-any
