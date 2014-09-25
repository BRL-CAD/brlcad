/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2013
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

namespace Gecode { namespace Int { namespace Branch {

  template<class View>
  forceinline
  EqNGL<View>::EqNGL(Space& home, View x, int n)
    : ViewValNGL<View,int,PC_INT_VAL>(home,x,n) {}
  template<class View>
  forceinline
  EqNGL<View>::EqNGL(Space& home, bool share, EqNGL& ngl)
    : ViewValNGL<View,int,PC_INT_VAL>(home,share,ngl) {}
  template<class View>
  NGL*
  EqNGL<View>::copy(Space& home, bool share) {
    return new (home) EqNGL<View>(home,share,*this);
  }
  template<class View>
  NGL::Status
  EqNGL<View>::status(const Space&) const {
    if (x.assigned())
      return (x.val() == n) ? NGL::SUBSUMED : NGL::FAILED;
    else 
      return x.in(n) ? NGL::NONE : NGL::FAILED;
  }
  template<class View>
  ExecStatus
  EqNGL<View>::prune(Space& home) {
    return me_failed(x.nq(home,n)) ? ES_FAILED : ES_OK;
  }


  template<class View>
  forceinline
  NqNGL<View>::NqNGL(Space& home, View x, int n)
    : ViewValNGL<View,int,PC_INT_DOM>(home,x,n) {}
  template<class View>
  forceinline
  NqNGL<View>::NqNGL(Space& home, bool share, NqNGL& ngl)
    : ViewValNGL<View,int,PC_INT_DOM>(home,share,ngl) {}
  template<class View>
  NGL*
  NqNGL<View>::copy(Space& home, bool share) {
    return new (home) NqNGL<View>(home,share,*this);
  }
  template<class View>
  NGL::Status
  NqNGL<View>::status(const Space&) const {
    if (x.assigned())
      return (x.val() == n) ? NGL::FAILED : NGL::SUBSUMED;
    else 
      return x.in(n) ? NGL::NONE : NGL::SUBSUMED;
  }
  template<class View>
  ExecStatus
  NqNGL<View>::prune(Space& home) {
    return me_failed(x.eq(home,n)) ? ES_FAILED : ES_OK;
  }


  template<class View>
  forceinline
  LqNGL<View>::LqNGL(Space& home, View x, int n)
    : ViewValNGL<View,int,PC_INT_BND>(home,x,n) {}
  template<class View>
  forceinline
  LqNGL<View>::LqNGL(Space& home, bool share, LqNGL& ngl)
    : ViewValNGL<View,int,PC_INT_BND>(home,share,ngl) {}
  template<class View>
  NGL*
  LqNGL<View>::copy(Space& home, bool share) {
    return new (home) LqNGL<View>(home,share,*this);
  }
  template<class View>
  NGL::Status
  LqNGL<View>::status(const Space&) const {
    if (x.max() <= n)
      return NGL::SUBSUMED;
    else if (x.min() > n)
      return NGL::FAILED;
    else
      return NGL::NONE;
  }
  template<class View>
  ExecStatus
  LqNGL<View>::prune(Space& home) {
    return me_failed(x.gr(home,n)) ? ES_FAILED : ES_OK;
  }


  template<class View>
  forceinline
  GqNGL<View>::GqNGL(Space& home, View x, int n)
    : ViewValNGL<View,int,PC_INT_BND>(home,x,n) {}
  template<class View>
  forceinline
  GqNGL<View>::GqNGL(Space& home, bool share, GqNGL& ngl)
    : ViewValNGL<View,int,PC_INT_BND>(home,share,ngl) {}
  template<class View>
  NGL*
  GqNGL<View>::copy(Space& home, bool share) {
    return new (home) GqNGL<View>(home,share,*this);
  }
  template<class View>
  NGL::Status
  GqNGL<View>::status(const Space&) const {
    if (x.min() >= n)
      return NGL::SUBSUMED;
    else if (x.max() < n)
      return NGL::FAILED;
    else
      return NGL::NONE;
  }
  template<class View>
  ExecStatus
  GqNGL<View>::prune(Space& home) {
    return me_failed(x.le(home,n)) ? ES_FAILED : ES_OK;
  }

}}}

// STATISTICS: int-branch
