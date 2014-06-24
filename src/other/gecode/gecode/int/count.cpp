/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
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

#include <gecode/int/count.hh>
#include <gecode/int/rel.hh>

namespace Gecode {

  void
  count(Home home, const IntVarArgs& x, int n,
        IntRelType irt, int m, IntConLevel) {
    using namespace Int;
    Limits::check(n,"Int::count");
    Limits::check(m,"Int::count");

    if (home.failed()) return;

    ViewArray<IntView> xv(home,x);
    ConstIntView y(n);

    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL((Count::EqInt<IntView,ConstIntView>
                      ::post(home,xv,y,m)));
      break;
    case IRT_NQ:
      {
        IntVar z(home,0,x.size());
        GECODE_ME_FAIL(IntView(z).nq(home,m));
        GECODE_ES_FAIL((Count::EqView<IntView,ConstIntView,IntView,true,false>
                        ::post(home,xv,y,z,0)));
      }
      break;
    case IRT_LE:
      m--; // FALL THROUGH
    case IRT_LQ:
      GECODE_ES_FAIL((Count::LqInt<IntView,ConstIntView>
                      ::post(home,xv,y,m)));
      break;
    case IRT_GR:
      m++; // FALL THROUGH
    case IRT_GQ:
      GECODE_ES_FAIL((Count::GqInt<IntView,ConstIntView>
                      ::post(home,xv,y,m)));
      break;
    default:
      throw UnknownRelation("Int::count");
    }
  }

  void
  count(Home home, const IntVarArgs& x, IntVar y,
        IntRelType irt, int m, IntConLevel icl) {
    using namespace Int;
    Limits::check(m,"Int::count");
    if (home.failed()) return;
    ViewArray<IntView> xv(home,x);

    switch (irt) {
    case IRT_EQ:
      {
        ConstIntView z(m);
        if ((icl == ICL_DOM) || (icl == ICL_DEF))
          GECODE_ES_FAIL((Count::EqView<IntView,IntView,ConstIntView,true,true>
                          ::post(home,xv,y,z,0)));
        else
          GECODE_ES_FAIL((Count::EqView<IntView,IntView,ConstIntView,true,false>
                          ::post(home,xv,y,z,0)));
      }
      break;
    case IRT_NQ:
      {
        IntVar z(home,0,x.size());
        GECODE_ME_FAIL(IntView(z).nq(home,m));
        GECODE_ES_FAIL((Count::EqView<IntView,IntView,IntView,true,false>
                        ::post(home,xv,y,z,0)));
      }
      break;
    case IRT_LE:
      m--; // FALL THROUGH
    case IRT_LQ:
      GECODE_ES_FAIL((Count::LqInt<IntView,IntView>
                      ::post(home,xv,y,m)));
      break;
    case IRT_GR:
      m++; // FALL THROUGH
    case IRT_GQ:
      {
        ConstIntView z(m);
        if ((icl == ICL_DOM) || (icl == ICL_DEF))
          GECODE_ES_FAIL((Count::GqView<IntView,IntView,ConstIntView,true,true>
                          ::post(home,xv,y,z,0)));
        else
          GECODE_ES_FAIL((Count::GqView<IntView,IntView,ConstIntView,true,false>
                          ::post(home,xv,y,z,0)));
      }
      break;
    default:
      throw UnknownRelation("Int::count");
    }
  }

  void
  count(Home home, const IntVarArgs& x, const IntSet& y,
        IntRelType irt, int m, IntConLevel) {
    using namespace Int;

    if (y.size() == 1) {
      count(home,x,y.min(),irt,m);
      return;
    }
      
    Limits::check(y.min(),"Int::count");
    Limits::check(y.max(),"Int::count");
    Limits::check(m,"Int::count");

    if (home.failed()) return;

    ViewArray<IntView> xv(home,x);
    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL((Count::EqInt<IntView,IntSet>::post(home,xv,y,m)));
      break;
    case IRT_NQ:
      {
        IntVar z(home,0,x.size());
        GECODE_ME_FAIL(IntView(z).nq(home,m));
        GECODE_ES_FAIL((Count::EqView<IntView,IntSet,IntView,true,false>
                        ::post(home,xv,y,z,0)));
      }
      break;
    case IRT_LE:
      m--; // FALL THROUGH
    case IRT_LQ:
      GECODE_ES_FAIL((Count::LqInt<IntView,IntSet>::post(home,xv,y,m)));
      break;
    case IRT_GR:
      m++; // FALL THROUGH
    case IRT_GQ:
      GECODE_ES_FAIL((Count::GqInt<IntView,IntSet>::post(home,xv,y,m)));
      break;
    default:
      throw UnknownRelation("Int::count");
    }
  }

  void
  count(Home home, const IntVarArgs& x, const IntArgs& y,
        IntRelType irt, int m, IntConLevel) {
    using namespace Int;
    if (x.size() != y.size())
      throw ArgumentSizeMismatch("Int::count");
    Limits::check(m,"Int::count");
    if (home.failed()) return;

    ViewArray<OffsetView> xy(home,x.size());
    for (int i=x.size(); i--; )
      xy[i] = OffsetView(x[i],-y[i]);

    ZeroIntView zero;
    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL((Count::EqInt<OffsetView,ZeroIntView>
                      ::post(home,xy,zero,m)));
      break;
    case IRT_NQ:
      {
        IntVar z(home,0,x.size());
        GECODE_ME_FAIL(IntView(z).nq(home,m));
        GECODE_ES_FAIL((Count::EqView<OffsetView,ZeroIntView,IntView,true,false>
                        ::post(home,xy,zero,z,0)));
      }
      break;
    case IRT_LE:
      m--; // FALL THROUGH
    case IRT_LQ:
      GECODE_ES_FAIL((Count::LqInt<OffsetView,ZeroIntView>
                      ::post(home,xy,zero,m)));
      break;
    case IRT_GR:
      m++; // FALL THROUGH
    case IRT_GQ:
      GECODE_ES_FAIL((Count::GqInt<OffsetView,ZeroIntView>
                      ::post(home,xy,zero,m)));
      break;
    default:
      throw UnknownRelation("Int::count");
    }
  }

  void
  count(Home home, const IntVarArgs& x, int n,
        IntRelType irt, IntVar z, IntConLevel) {
    using namespace Int;
    Limits::check(n,"Int::count");
    if (home.failed()) return;
    ViewArray<IntView> xv(home,x);
    ConstIntView yv(n);
    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL((Count::EqView<IntView,ConstIntView,IntView,true,false>
                      ::post(home,xv,yv,z,0)));
      break;
    case IRT_NQ:
      {
        IntVar nz(home,0,x.size());
        GECODE_ES_FAIL(Rel::Nq<IntView>::post(home,z,nz));
        GECODE_ES_FAIL((Count::EqView<IntView,ConstIntView,IntView,true,false>
                        ::post(home,xv,yv,nz,0)));
      }
      break;
    case IRT_LE:
      GECODE_ES_FAIL((Count::LqView<IntView,ConstIntView,IntView,true>
                           ::post(home,xv,yv,z,-1)));
      break;
    case IRT_LQ:
      GECODE_ES_FAIL((Count::LqView<IntView,ConstIntView,IntView,true>
                           ::post(home,xv,yv,z,0)));
      break;
    case IRT_GR:
      GECODE_ES_FAIL((Count::GqView<IntView,ConstIntView,IntView,true,false>
                      ::post(home,xv,yv,z,1)));
      break;
    case IRT_GQ:
      GECODE_ES_FAIL((Count::GqView<IntView,ConstIntView,IntView,true,false>
                      ::post(home,xv,yv,z,0)));
      break;
    default:
      throw UnknownRelation("Int::count");
    }
  }

  void
  count(Home home, const IntVarArgs& x, IntVar y,
        IntRelType irt, IntVar z, IntConLevel icl) {
    using namespace Int;
    if (home.failed()) return;
    ViewArray<IntView> xv(home,x);
    switch (irt) {
    case IRT_EQ:
      if ((icl == ICL_DOM) || (icl == ICL_DEF))
        GECODE_ES_FAIL((Count::EqView<IntView,IntView,IntView,true,true>
                        ::post(home,xv,y,z,0)));
      else
        GECODE_ES_FAIL((Count::EqView<IntView,IntView,IntView,true,false>
                        ::post(home,xv,y,z,0)));
      break;
    case IRT_NQ:
      {
        IntVar nz(home,0,x.size());
        GECODE_ES_FAIL(Rel::Nq<IntView>::post(home,z,nz));
        GECODE_ES_FAIL((Count::EqView<IntView,IntView,IntView,true,false>
                        ::post(home,xv,y,nz,0)));
      }
      break;
    case IRT_LE:
      GECODE_ES_FAIL((Count::LqView<IntView,IntView,IntView,true>
                      ::post(home,xv,y,z,-1)));
      break;
    case IRT_LQ:
      GECODE_ES_FAIL((Count::LqView<IntView,IntView,IntView,true>
                      ::post(home,xv,y,z,0)));
      break;
    case IRT_GR:
      if ((icl == ICL_DOM) || (icl == ICL_DEF))
        GECODE_ES_FAIL((Count::GqView<IntView,IntView,IntView,true,true>
                        ::post(home,xv,y,z,1)));
      else
        GECODE_ES_FAIL((Count::GqView<IntView,IntView,IntView,true,false>
                        ::post(home,xv,y,z,0)));
      break;
    case IRT_GQ:
      if ((icl == ICL_DOM) || (icl == ICL_DEF))
        GECODE_ES_FAIL((Count::GqView<IntView,IntView,IntView,true,true>
                        ::post(home,xv,y,z,0)));
      else
        GECODE_ES_FAIL((Count::GqView<IntView,IntView,IntView,true,false>
                        ::post(home,xv,y,z,0)));
      break;
    default:
      throw UnknownRelation("Int::count");
    }
  }

  void
  count(Home home, const IntVarArgs& x, const IntSet& y,
        IntRelType irt, IntVar z, IntConLevel) {
    using namespace Int;

    if (y.size() == 1) {
      count(home,x,y.min(),irt,z);
      return;
    }
      
    Limits::check(y.min(),"Int::count");
    Limits::check(y.max(),"Int::count");

    if (home.failed()) return;
    ViewArray<IntView> xv(home,x);
    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL((Count::EqView<IntView,IntSet,IntView,true,false>
                      ::post(home,xv,y,z,0)));
      break;
    case IRT_NQ:
      {
        IntVar nz(home,0,x.size());
        GECODE_ES_FAIL(Rel::Nq<IntView>::post(home,z,nz));
        GECODE_ES_FAIL((Count::EqView<IntView,IntSet,IntView,true,false>
                        ::post(home,xv,y,nz,0)));
      }
      break;
    case IRT_LE:
      GECODE_ES_FAIL((Count::LqView<IntView,IntSet,IntView,true>
                      ::post(home,xv,y,z,-1)));
      break;
    case IRT_LQ:
      GECODE_ES_FAIL((Count::LqView<IntView,IntSet,IntView,true>
                      ::post(home,xv,y,z,0)));
      break;
    case IRT_GR:
      GECODE_ES_FAIL((Count::GqView<IntView,IntSet,IntView,true,false>
                      ::post(home,xv,y,z,1)));
      break;
    case IRT_GQ:
      GECODE_ES_FAIL((Count::GqView<IntView,IntSet,IntView,true,false>
                      ::post(home,xv,y,z,0)));
      break;
    default:
      throw UnknownRelation("Int::count");
    }
  }

  void
  count(Home home, const IntVarArgs& x, const IntArgs& y,
        IntRelType irt, IntVar z, IntConLevel) {
    using namespace Int;
    if (x.size() != y.size())
      throw ArgumentSizeMismatch("Int::count");
    if (home.failed()) return;

    ViewArray<OffsetView> xy(home,x.size());
    for (int i=x.size(); i--; )
      xy[i] = OffsetView(x[i],-y[i]);

    ZeroIntView u;
    switch (irt) {
    case IRT_EQ:
      GECODE_ES_FAIL((Count::EqView<OffsetView,ZeroIntView,IntView,true,false>
                      ::post(home,xy,u,z,0)));
      break;
    case IRT_NQ:
      {
        IntVar nz(home,0,x.size());
        GECODE_ES_FAIL(Rel::Nq<IntView>::post(home,z,nz));
        GECODE_ES_FAIL((Count::EqView<OffsetView,ZeroIntView,IntView,true,false>
                        ::post(home,xy,u,nz,0)));
      }
      break;
    case IRT_LE:
      GECODE_ES_FAIL((Count::LqView<OffsetView,ZeroIntView,IntView,true>
                      ::post(home,xy,u,z,-1)));
      break;
    case IRT_LQ:
      GECODE_ES_FAIL((Count::LqView<OffsetView,ZeroIntView,IntView,true>
                      ::post(home,xy,u,z,0)));
      break;
    case IRT_GR:
      GECODE_ES_FAIL((Count::GqView<OffsetView,ZeroIntView,IntView,true,false>
                      ::post(home,xy,u,z,1)));
      break;
    case IRT_GQ:
      GECODE_ES_FAIL((Count::GqView<OffsetView,ZeroIntView,IntView,true,false>
                      ::post(home,xy,u,z,0)));
      break;
    default:
      throw UnknownRelation("Int::count");
    }
  }

}

// STATISTICS: int-post
