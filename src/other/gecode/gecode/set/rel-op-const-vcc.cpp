/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004, 2005
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

#include <gecode/set.hh>
#include <gecode/set/rel.hh>
#include <gecode/set/rel-op.hh>

namespace Gecode {
  using namespace Gecode::Set;
  using namespace Gecode::Set::Rel;
  using namespace Gecode::Set::RelOp;

  void
  rel(Home home, SetVar x, SetOpType op, const IntSet& y, SetRelType r,
      const IntSet& z) {
    Set::Limits::check(y, "Set::rel");
    Set::Limits::check(z, "Set::rel");
    ConstSetView yv(home, y);
    ConstSetView zv(home, z);

    if (op==SOT_MINUS) {
      switch (r) {
      case SRT_EQ:
        {
          GlbRanges<ConstSetView> yr(yv);
          RangesCompl<GlbRanges<ConstSetView> > yrc(yr);
          IntSet yc(yrc);
          ConstSetView cy(home, yc);
          GECODE_ES_FAIL(
                         (Intersection<ConstSetView,
                          SetView,ConstSetView>
                          ::post(home,cy,x,zv)));
        }
        break;
      case SRT_LQ: case SRT_LE: case SRT_GQ: case SRT_GR:
        {
          GlbRanges<ConstSetView> yr(yv);
          RangesCompl<GlbRanges<ConstSetView> > yrc(yr);
          IntSet yc(yrc);
          ConstSetView cy(home, yc);
          SetVar tmp(home,IntSet::empty,Set::Limits::min,Set::Limits::max);
          GECODE_ES_FAIL(
                         (Intersection<ConstSetView,
                          SetView,SetView>
                          ::post(home,cy,x,tmp)));
          dom(home,tmp,r,z);
        }
        break;
      case SRT_NQ:
        {
          SetVar tmp(home);
          GECODE_ES_FAIL(
                         (Distinct<SetView,ConstSetView>
                          ::post(home,tmp,zv)));
          GlbRanges<ConstSetView> yr(yv);
          RangesCompl<GlbRanges<ConstSetView> > yrc(yr);
          IntSet yc(yrc);
          ConstSetView cy(home, yc);
          GECODE_ES_FAIL(
                         (Intersection<ConstSetView,
                          SetView,SetView>
                          ::post(home,cy,x,tmp)));
        }
        break;
      case SRT_SUB:
        {
          GlbRanges<ConstSetView> yr(yv);
          RangesCompl<GlbRanges<ConstSetView> > yrc(yr);
          IntSet yc(yrc);
          ConstSetView cy(home, yc);
          GECODE_ES_FAIL(
                         (SuperOfInter<ConstSetView,SetView,ConstSetView>
                          ::post(home,cy,x,zv)));

        }
        break;
      case SRT_SUP:
        {
          // z <= tmp
          SetVar tmp(home,z,Limits::min, Limits::max);
          SetView xv(x);

          GlbRanges<ConstSetView> yr(yv);
          RangesCompl<GlbRanges<ConstSetView> > yrc(yr);
          IntSet yc(yrc);
          ConstSetView cy(home, yc);

          GECODE_ES_FAIL(
                         (Intersection<ConstSetView,
                          SetView,SetView>
                          ::post(home,cy,xv,tmp)));
        }
        break;
      case SRT_DISJ:
        {
          SetVar tmp(home);
          SetView tmpv(tmp);
          IntSetRanges zi(z);
          GECODE_ME_FAIL( tmpv.excludeI(home, zi));

          GlbRanges<ConstSetView> yr(yv);
          RangesCompl<GlbRanges<ConstSetView> > yrc(yr);
          IntSet yc(yrc);
          ConstSetView cy(home, yc);
          GECODE_ES_FAIL(
                         (Intersection<ConstSetView,
                          SetView,SetView>
                          ::post(home,cy,x,tmp)));
        }
        break;
      case SRT_CMPL:
        {
          SetView xv(x);
          ComplementView<SetView> cx(xv);
          GECODE_ES_FAIL(
                         (Union<ConstSetView,
                          ComplementView<SetView>,
                          ConstSetView>::post(home, yv, cx, zv)));
        }
        break;
      default:
        throw UnknownRelation("Set::rel");
      }
    } else {
      rel(home, y, op, x, r, z);
    }
  }

}

// STATISTICS: set-post
