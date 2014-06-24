/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
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

#include <gecode/int/cumulatives.hh>

namespace Gecode {

  using namespace Int;

  namespace {
    ViewArray<IntView>
    make_view_array(Space& home, const IntVarArgs& in) {
      return ViewArray<Int::IntView>(home, in);
    }

    ViewArray<ConstIntView>
    make_view_array(Space& home, const IntArgs& in) {
      ViewArray<Int::ConstIntView> res(home, in.size());
      for (int i = in.size(); i--; ) {
        Int::Limits::check(in[i],"Int::cumulatives");
        res[i] = Int::ConstIntView(in[i]);
      }

      return res;
    }

    template<class In> class ViewType;

    template<>
    class ViewType<IntArgs> {
    public:
      typedef Int::ConstIntView Result;
    };

    template<>
    class ViewType<IntVarArgs> {
    public:
      typedef Int::IntView Result;
    };

    template<class Machine, class Processing, class Usage>
    void
    post_cumulatives(Home home, const Machine& m,
                     const IntVarArgs& s, const Processing& p,
                     const IntVarArgs& e, const Usage& u,
                     const IntArgs& c, bool at_most,
                     IntConLevel) {
      if (m.size() != s.size()  ||
          s.size() != p.size() ||
          p.size() != e.size()   ||
          e.size() != u.size())
        throw Int::ArgumentSizeMismatch("Int::cumulatives");
      if (home.failed()) return;

      ViewArray<typename ViewType<Machine>::Result>
        vm  = make_view_array(home,  m);
      ViewArray<typename ViewType<Processing>::Result>
        vp = make_view_array(home, p);
      ViewArray<typename ViewType<Usage>::Result>
        vu   = make_view_array(home, u);
      ViewArray<IntView>
        vs    = make_view_array(home, s),
        ve    = make_view_array(home, e);

      SharedArray<int> c_s(c.size());
      for (int i=c.size(); i--;)
        c_s[i] = c[i];

      // There is only the value-consistent propagator for this constraint
      GECODE_ES_FAIL((Int::Cumulatives::Val<
                           typename ViewType<Machine>::Result,
                           typename ViewType<Processing>::Result,
                           typename ViewType<Usage>::Result,
                           IntView>::post(home, vm,vs,vp,ve,vu,c_s,at_most)));

    }
  }

  void
  cumulatives(Home home, const IntVarArgs& m,
              const IntVarArgs& s, const IntVarArgs& p,
              const IntVarArgs& e, const IntVarArgs& u,
              const IntArgs& c, bool at_most,
              IntConLevel cl) {
    post_cumulatives(home, m, s, p, e, u, c, at_most, cl);
  }

  void
  cumulatives(Home home, const IntArgs& m,
              const IntVarArgs& s, const IntVarArgs& p,
              const IntVarArgs& e, const IntVarArgs& u,
              const IntArgs& c, bool at_most,
              IntConLevel cl) {
    post_cumulatives(home, m, s, p, e, u, c, at_most, cl);
  }

  void
  cumulatives(Home home, const IntVarArgs& m,
              const IntVarArgs& s, const IntArgs& p,
              const IntVarArgs& e, const IntVarArgs& u,
              const IntArgs& c, bool at_most,
              IntConLevel cl) {
    post_cumulatives(home, m, s, p, e, u, c, at_most, cl);
  }

  void
  cumulatives(Home home, const IntArgs& m,
              const IntVarArgs& s, const IntArgs& p,
              const IntVarArgs& e, const IntVarArgs& u,
              const IntArgs& c, bool at_most,
              IntConLevel cl) {
    post_cumulatives(home, m, s, p, e, u, c, at_most, cl);
  }

  void
  cumulatives(Home home, const IntVarArgs& m,
              const IntVarArgs& s, const IntVarArgs& p,
              const IntVarArgs& e, const IntArgs& u,
              const IntArgs& c, bool at_most,
              IntConLevel cl) {
    post_cumulatives(home, m, s, p, e, u, c, at_most, cl);
  }

  void
  cumulatives(Home home, const IntArgs& m,
              const IntVarArgs& s, const IntVarArgs& p,
              const IntVarArgs& e, const IntArgs& u,
              const IntArgs& c, bool at_most,
              IntConLevel cl) {
    post_cumulatives(home, m, s, p, e, u, c, at_most, cl);
  }

  void
  cumulatives(Home home, const IntVarArgs& m,
              const IntVarArgs& s, const IntArgs& p,
              const IntVarArgs& e, const IntArgs& u,
              const IntArgs& c, bool at_most,
              IntConLevel cl) {
    post_cumulatives(home, m, s, p, e, u, c, at_most, cl);
  }

  void
  cumulatives(Home home, const IntArgs& m,
              const IntVarArgs& s, const IntArgs& p,
              const IntVarArgs& e, const IntArgs& u,
              const IntArgs& c, bool at_most,
              IntConLevel cl) {
    post_cumulatives(home, m, s, p, e, u, c, at_most, cl);
  }

}

// STATISTICS: int-post
