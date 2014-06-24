/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
 *     Guido Tack, 2010
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

#include <gecode/int/unary.hh>
#include <gecode/int/distinct.hh>

#include <algorithm>

namespace Gecode {

  void
  unary(Home home, const IntVarArgs& s, const IntArgs& p, IntConLevel icl) {
    using namespace Gecode::Int;
    using namespace Gecode::Int::Unary;
    if (s.same(home))
      throw Int::ArgumentSame("Int::unary");
    if (s.size() != p.size())
      throw Int::ArgumentSizeMismatch("Int::unary");
    for (int i=p.size(); i--; ) {
      Int::Limits::nonnegative(p[i],"Int::unary");
      Int::Limits::check(static_cast<long long int>(s[i].max()) + p[i],
                         "Int::unary");
    }
    if (home.failed()) return;
    bool allOne = true;
    for (int i=p.size(); i--;) {
      if (p[i] != 1) {
        allOne = false;
        break;
      }
    }
    if (allOne) {
      ViewArray<IntView> xv(home,s);
      switch (icl) {
      case ICL_BND:
        GECODE_ES_FAIL(Distinct::Bnd<IntView>::post(home,xv));
        break;
      case ICL_DOM:
        GECODE_ES_FAIL(Distinct::Dom<IntView>::post(home,xv));
        break;
      default:
        GECODE_ES_FAIL(Distinct::Val<IntView>::post(home,xv));
      }
    } else {
      TaskArray<ManFixPTask> t(home,s.size());
      for (int i=s.size(); i--; )
        t[i].init(s[i],p[i]);
      GECODE_ES_FAIL(ManProp<ManFixPTask>::post(home,t));
    }
  }

  void
  unary(Home home, const TaskTypeArgs& t,
        const IntVarArgs& flex, const IntArgs& fix, IntConLevel icl) {
    using namespace Gecode::Int;
    using namespace Gecode::Int::Unary;
    if ((flex.size() != fix.size()) || (flex.size() != t.size()))
      throw Int::ArgumentSizeMismatch("Int::unary");
    for (int i=fix.size(); i--; ) {
      if (t[i] == TT_FIXP)
        Int::Limits::nonnegative(fix[i],"Int::unary");
      else
        Int::Limits::check(fix[i],"Int::unary");
      Int::Limits::check(static_cast<long long int>(flex[i].max()) + fix[i],
                         "Int::unary");
    }
    if (home.failed()) return;
    bool fixp = true;
    for (int i=t.size(); i--;)
      if (t[i] != TT_FIXP) {
        fixp = false; break;
      }
    if (fixp) {
      unary(home, flex, fix, icl);
    } else {
      TaskArray<ManFixPSETask> tasks(home,flex.size());
      for (int i=flex.size(); i--;)
        tasks[i].init(t[i],flex[i],fix[i]);
      GECODE_ES_FAIL(ManProp<ManFixPSETask>::post(home,tasks));
    }
  }

  void
  unary(Home home, const IntVarArgs& s, const IntArgs& p, 
        const BoolVarArgs& m, IntConLevel icl) {
    using namespace Gecode::Int;
    using namespace Gecode::Int::Unary;
    if (s.same(home))
      throw Int::ArgumentSame("Int::unary");
    if ((s.size() != p.size()) || (s.size() != m.size()))
      throw Int::ArgumentSizeMismatch("Int::unary");
    for (int i=p.size(); i--; ) {
      Int::Limits::nonnegative(p[i],"Int::unary");
      Int::Limits::check(static_cast<long long int>(s[i].max()) + p[i],
                         "Int::unary");
    }
    bool allMandatory = true;
    for (int i=m.size(); i--;) {
      if (!m[i].one()) {
        allMandatory = false;
        break;
      }
    }
    if (allMandatory) {
      unary(home,s,p,icl);
    } else {
      if (home.failed()) return;
      TaskArray<OptFixPTask> t(home,s.size());
      for (int i=s.size(); i--; )
        t[i].init(s[i],p[i],m[i]);
      GECODE_ES_FAIL(OptProp<OptFixPTask>::post(home,t));
    }
  }

  void
  unary(Home home, const TaskTypeArgs& t,
        const IntVarArgs& flex, const IntArgs& fix, const BoolVarArgs& m, 
        IntConLevel icl) {
    using namespace Gecode::Int;
    using namespace Gecode::Int::Unary;
    if ((flex.size() != fix.size()) || (flex.size() != t.size()) ||
        (flex.size() != m.size()))
      throw Int::ArgumentSizeMismatch("Int::unary");
    bool fixp = true;
    for (int i=fix.size(); i--; ) {
      if (t[i] == TT_FIXP) {
        Int::Limits::nonnegative(fix[i],"Int::unary");
      } else {
        fixp = false;
        Int::Limits::check(fix[i],"Int::unary");
      }
      Int::Limits::check(static_cast<long long int>(flex[i].max()) + fix[i],
                         "Int::unary");
    }
    if (home.failed()) return;
    bool allMandatory = true;
    for (int i=m.size(); i--;) {
      if (!m[i].one()) {
        allMandatory = false;
        break;
      }
    }
    if (allMandatory) {
      unary(home,t,flex,fix,icl);
    } else {
      if (fixp) {
        TaskArray<OptFixPTask> tasks(home,flex.size());
        for (int i=flex.size(); i--; )
          tasks[i].init(flex[i],fix[i],m[i]);
        GECODE_ES_FAIL(OptProp<OptFixPTask>::post(home,tasks));
      } else {
        TaskArray<OptFixPSETask> tasks(home,flex.size());
        for (int i=flex.size(); i--;)
          tasks[i].init(t[i],flex[i],fix[i],m[i]);
        GECODE_ES_FAIL(OptProp<OptFixPSETask>::post(home,tasks));
      }
    }
  }

  void
  unary(Home home, const IntVarArgs& s, const IntVarArgs& p,
        const IntVarArgs& e, IntConLevel icl) {
    using namespace Gecode::Int;
    using namespace Gecode::Int::Unary;
    if ((s.size() != p.size()) || (s.size() != e.size()))
      throw Int::ArgumentSizeMismatch("Int::unary");
    if (home.failed()) return;
    for (int i=p.size(); i--; ) {
      rel(home, p[i], IRT_GQ, 0);
    }
    bool fixP = true;
    for (int i=p.size(); i--;) {
      if (!p[i].assigned()) {
        fixP = false;
        break;
      }
    }
    if (fixP) {
      IntArgs pp(p.size());
      for (int i=p.size(); i--;)
        pp[i] = p[i].val();
      unary(home,s,pp,icl);
    } else {
      TaskArray<ManFlexTask> t(home,s.size());
      for (int i=s.size(); i--; )
        t[i].init(s[i],p[i],e[i]);
      GECODE_ES_FAIL(ManProp<ManFlexTask>::post(home,t));
    }
  }

  void
  unary(Home home, const IntVarArgs& s, const IntVarArgs& p, 
        const IntVarArgs& e, const BoolVarArgs& m, IntConLevel icl) {
    using namespace Gecode::Int;
    using namespace Gecode::Int::Unary;
    if ((s.size() != p.size()) || (s.size() != m.size()) ||
        (s.size() != e.size()))
      throw Int::ArgumentSizeMismatch("Int::unary");
    if (home.failed()) return;
    for (int i=p.size(); i--; ) {
      rel(home, p[i], IRT_GQ, 0);
    }
    bool allMandatory = true;
    for (int i=m.size(); i--;) {
      if (!m[i].one()) {
        allMandatory = false;
        break;
      }
    }
    if (allMandatory) {
      unary(home,s,p,e,icl);
    } else {
      TaskArray<OptFlexTask> t(home,s.size());
      for (int i=s.size(); i--; )
        t[i].init(s[i],p[i],e[i],m[i]);
      GECODE_ES_FAIL(OptProp<OptFlexTask>::post(home,t));
    }
  }

}

// STATISTICS: int-post
