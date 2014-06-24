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

namespace Gecode { namespace Int { namespace Branch {

  // Minimum merit
  template<class View>
  forceinline
  MeritMin<View>::MeritMin(Space& home, const VarBranch& vb)
    : MeritBase<View,int>(home,vb) {}
  template<class View>
  forceinline
  MeritMin<View>::MeritMin(Space& home, bool shared, MeritMin& m)
    : MeritBase<View,int>(home,shared,m) {}
  template<class View>
  forceinline int
  MeritMin<View>::operator ()(const Space&, View x, int) {
    return x.min();
  }

  // Maximum merit
  template<class View>
  forceinline
  MeritMax<View>::MeritMax(Space& home, const VarBranch& vb)
    : MeritBase<View,int>(home,vb) {}
  template<class View>
  forceinline
  MeritMax<View>::MeritMax(Space& home, bool shared, MeritMax& m)
    : MeritBase<View,int>(home,shared,m) {}
  template<class View>
  forceinline int
  MeritMax<View>::operator ()(const Space&, View x, int) {
    return x.max();
  }

  // Size merit
  template<class View>
  forceinline
  MeritSize<View>::MeritSize(Space& home, const VarBranch& vb)
    : MeritBase<View,unsigned int>(home,vb) {}
  template<class View>
  forceinline
  MeritSize<View>::MeritSize(Space& home, bool shared, MeritSize& m)
    : MeritBase<View,unsigned int>(home,shared,m) {}
  template<class View>
  forceinline unsigned int
  MeritSize<View>::operator ()(const Space&, View x, int) {
    return x.size();
  }

  // Degree over size merit
  template<class View>
  forceinline
  MeritDegreeSize<View>::MeritDegreeSize(Space& home, const VarBranch& vb)
    : MeritBase<View,double>(home,vb) {}
  template<class View>
  forceinline
  MeritDegreeSize<View>::MeritDegreeSize(Space& home, bool shared, 
                                         MeritDegreeSize& m)
    : MeritBase<View,double>(home,shared,m) {}
  template<class View>
  forceinline double
  MeritDegreeSize<View>::operator ()(const Space&, View x, int) {
    return static_cast<double>(x.degree()) / static_cast<double>(x.size());
  }

  // AFC over size merit
  template<class View>
  forceinline
  MeritAFCSize<View>::MeritAFCSize(Space& home, const VarBranch& vb)
    : MeritBase<View,double>(home,vb), afc(vb.afc()) {}
  template<class View>
  forceinline
  MeritAFCSize<View>::MeritAFCSize(Space& home, bool shared, MeritAFCSize& m)
    : MeritBase<View,double>(home,shared,m) {
    afc.update(home,shared,m.afc);
  }
  template<class View>
  forceinline double
  MeritAFCSize<View>::operator ()(const Space& home, View x, int) {
    return x.afc(home) / static_cast<double>(x.size());
  }
  template<class View>
  forceinline bool
  MeritAFCSize<View>::notice(void) const {
    return true;
  }
  template<class View>
  forceinline void
  MeritAFCSize<View>::dispose(Space&) {
    afc.~AFC();
  }

  // Activity over size merit
  template<class View>
  forceinline
  MeritActivitySize<View>::MeritActivitySize(Space& home,
                                             const VarBranch& vb)
    : MeritBase<View,double>(home,vb), activity(vb.activity()) {}
  template<class View>
  forceinline
  MeritActivitySize<View>::MeritActivitySize(Space& home, bool shared, 
                                             MeritActivitySize& m)
    : MeritBase<View,double>(home,shared,m) {
    activity.update(home, shared, m.activity);
  }
  template<class View>
  forceinline double
  MeritActivitySize<View>::operator ()(const Space&, View x, int i) {
    return activity[i] / static_cast<double>(x.size());
  }
  template<class View>
  forceinline bool
  MeritActivitySize<View>::notice(void) const {
    return true;
  }
  template<class View>
  forceinline void
  MeritActivitySize<View>::dispose(Space&) {
    activity.~Activity();
  }

  // Minimum regret merit
  template<class View>
  forceinline
  MeritRegretMin<View>::MeritRegretMin(Space& home, const VarBranch& vb)
    : MeritBase<View,unsigned int>(home,vb) {}
  template<class View>
  forceinline
  MeritRegretMin<View>::MeritRegretMin(Space& home, bool shared, MeritRegretMin& m)
    : MeritBase<View,unsigned int>(home,shared,m) {}
  template<class View>
  forceinline unsigned int
  MeritRegretMin<View>::operator ()(const Space&, View x, int) {
    return x.regret_min();
  }

  // Maximum regret merit
  template<class View>
  forceinline
  MeritRegretMax<View>::MeritRegretMax(Space& home, const VarBranch& vb)
    : MeritBase<View,unsigned int>(home,vb) {}
  template<class View>
  forceinline
  MeritRegretMax<View>::MeritRegretMax(Space& home, bool shared, MeritRegretMax& m)
    : MeritBase<View,unsigned int>(home,shared,m) {}
  template<class View>
  forceinline unsigned int
  MeritRegretMax<View>::operator ()(const Space&, View x, int) {
    return x.regret_max();
  }

}}}

// STATISTICS: int-branch
