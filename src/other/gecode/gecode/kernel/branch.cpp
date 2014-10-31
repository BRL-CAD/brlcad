/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
 *     Mikael Lagerkvist, 2008
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

#include <gecode/kernel.hh>

namespace Gecode {

  /// %Brancher for calling a function
  class GECODE_KERNEL_EXPORT FunctionBranch : public Brancher {
  protected:
    /// Minimal brancher description storing no information
    class GECODE_KERNEL_EXPORT Description : public Choice {
    public:
      /// Initialize description for brancher \a b, number of alternatives \a a.
      Description(const Brancher& b, unsigned int a) : Choice(b,a) {}
      /// Report size occupied
      virtual size_t size(void) const { return sizeof(Description); }
      /// Archive into \a e
      virtual void archive(Archive& e) const {
        Choice::archive(e);
      }
    };
    /// Function to call
    void (*f)(Space&);
    /// Call function just once
    bool done;
    /// Construct brancher
    FunctionBranch(Home home, void (*f0)(Space&))
      : Brancher(home), f(f0), done(false) {}
    /// Copy constructor
    FunctionBranch(Space& home, bool share, FunctionBranch& b)
      : Brancher(home,share,b), f(b.f), done(b.done) {}
  public:
    /// Check status of brancher, return true if alternatives left
    virtual bool status(const Space&) const {
      return !done;
    }
    /// Return choice
    virtual const Choice* choice(Space&) {
      assert(!done);
      return new Description(*this,1);
    }
    /// Return choice
    virtual const Choice* choice(const Space&, Archive&) {
      return new Description(*this,1);
    }
    /// Perform commit
    virtual ExecStatus 
    commit(Space& home, const Choice&, unsigned int) {
      done = true;
      f(home);
      return home.failed() ? ES_FAILED : ES_OK;
    }
    /// Print explanation
    virtual void
    print(const Space&, const Choice&, unsigned int, 
          std::ostream& o) const {
      o << "FunctionBranch(" << f << ")";
    }
    /// Copy brancher
    virtual Actor* copy(Space& home, bool share) {
      return new (home) FunctionBranch(home,share,*this);
    }
    /// Post brancher
    static BrancherHandle post(Home home, void (*f)(Space&)) {
      return *new (home) FunctionBranch(home,f);
    }
  };


  BrancherHandle
  branch(Home home, void (*f)(Space& home)) {
    if (home.failed())
      return BrancherHandle();
    return FunctionBranch::post(home,f);
  }

}

// STATISTICS: kernel-branch
