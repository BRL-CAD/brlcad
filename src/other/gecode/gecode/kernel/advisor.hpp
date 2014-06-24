/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Mikael Lagerkvist, 2006
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

namespace Gecode {

  /**
   * \brief %Advisor storing a single view
   *
   */
  template<class View>
  class ViewAdvisor : public Advisor {
  protected:
    /// The single view
    View x;
  public:
    /// Constructor for creation
    template<class A>
    ViewAdvisor(Space& home, Propagator& p, Council<A>& c, View x0);
    /// Constructor for cloning \a a
    ViewAdvisor(Space& home, bool share, ViewAdvisor<View>& a);
    /// Access view
    View view(void) const;
    /// Replace view (also replaces subscription to view)
    void view(Space& home, View y);
    /// Delete advisor
    template<class A>
    void dispose(Space& home, Council<A>& c);
  };


  template<class View>
  template<class A>
  forceinline
  ViewAdvisor<View>::ViewAdvisor(Space& home, Propagator& p,
                                 Council<A>& c, View x0)
    : Advisor(home,p,c), x(x0) {
    x.subscribe(home,*this);
  }
  template<class View>
  forceinline
  ViewAdvisor<View>::ViewAdvisor(Space& home, bool share,
                                 ViewAdvisor<View>& a)
    : Advisor(home,share,a) {
    x.update(home,share,a.x);
  }
  template<class View>
  forceinline View
  ViewAdvisor<View>::view(void) const {
    return x;
  }
  template<class View>
  forceinline void
  ViewAdvisor<View>::view(Space& home, View y) {
    x.cancel(home,*this); x=y; x.subscribe(home,*this);
  }
  template<class View>
  template<class A>
  forceinline void
  ViewAdvisor<View>::dispose(Space& home, Council<A>& c) {
    x.cancel(home,*this);
    Advisor::dispose(home,c);
  }

}

// STATISTICS: kernel-other
