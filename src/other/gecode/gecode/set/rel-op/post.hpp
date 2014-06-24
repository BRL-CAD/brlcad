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

namespace Gecode { namespace Set { namespace RelOp {

  template<class View0, class View1, class Res>
  forceinline void
  rel_eq(Home home, View0 x0, SetOpType op, View1 x1, Res x2) {
    switch(op) {
    case SOT_DUNION:
      {
        EmptyView emptyset;
        GECODE_ES_FAIL((SuperOfInter<View0,View1,EmptyView>
                             ::post(home, x0, x1, emptyset)));
        // fall through to SOT_UNION
      }
    case SOT_UNION:
      {
        GECODE_ES_FAIL(
                       (Union<View0,View1,Res>
                        ::post(home, x0, x1, x2)));
      }
      break;
    case SOT_INTER:
      {
        GECODE_ES_FAIL((Intersection<View0,View1,Res>
                             ::post(home, x0,x1,x2)));
      }
      break;
    case SOT_MINUS:
      {
        ComplementView<View1> cx1(x1);
        GECODE_ES_FAIL(
                       (Intersection<View0,
                        ComplementView<View1>,Res>
                        ::post(home,x0,cx1,x2)));
      }
      break;
    }
  }

  template<class View0, class View1, class View2>
  forceinline void
  rel_sub(Home home, View0 x0, SetOpType op, View1 x1, View2 x2) {
    switch(op) {
    case SOT_DUNION:
      {
        EmptyView emptyset;
        GECODE_ES_FAIL((SuperOfInter<View0,View1,EmptyView>
                             ::post(home, x0, x1, emptyset)));
        // fall through to SOT_UNION
      }
    case SOT_UNION:
      {
        SetVar tmp(home);
        GECODE_ES_FAIL(
                       (Rel::Subset<SetView,View2>::post(home,tmp,x2)));

        GECODE_ES_FAIL(
                       (Union<View0,View1,SetView>
                        ::post(home, x0, x1, tmp)));
      }
      break;
    case SOT_INTER:
      {
        GECODE_ES_FAIL((SuperOfInter<View0,View1,View2>
                             ::post(home, x0,x1,x2)));
      }
      break;
    case SOT_MINUS:
      {
        ComplementView<View1> cx1(x1);
        GECODE_ES_FAIL(
                       (SuperOfInter<View0,
                        ComplementView<View1>,View2>
                        ::post(home,x0,cx1,x2)));
      }
      break;
    }

  }

  template<class View0, class View1, class View2>
  forceinline void
  rel_sup(Home home, View0 x0, SetOpType op, View1 x1, View2 x2) {
    switch(op) {
    case SOT_DUNION:
      {
        EmptyView emptyset;
        GECODE_ES_FAIL((SuperOfInter<View0,View1,EmptyView>
                             ::post(home, x0, x1, emptyset)));
        // fall through to SOT_UNION
      }
    case SOT_UNION:
      {
        GECODE_ES_FAIL(
                       (SubOfUnion<View0,View1,View2>
                        ::post(home, x0, x1, x2)));
      }
      break;
    case SOT_INTER:
      {
        SetVar tmp(home);
        GECODE_ES_FAIL(
                       (Rel::Subset<View2,SetView>::post(home,x2,tmp)));

        GECODE_ES_FAIL((Intersection<View0,View1,SetView>
                             ::post(home, x0,x1,tmp)));
      }
      break;
    case SOT_MINUS:
      {
        SetVar tmp(home);
        GECODE_ES_FAIL(
                       (Rel::Subset<View2,SetView>::post(home,x2,tmp)));

        ComplementView<View1> cx1(x1);
        GECODE_ES_FAIL(
                       (Intersection<View0,
                        ComplementView<View1>,SetView>
                        ::post(home,x0,cx1,tmp)));
      }
      break;
    }

  }

  template<class View>
  forceinline void
  rel_op_post_lex(Home home, SetView x0, SetRelType r, View x1) {
    switch (r) {
    case SRT_LQ:
      GECODE_ES_FAIL((Rel::Lq<SetView,View,false>::post(home,x0,x1)));
      break;
    case SRT_LE:
      GECODE_ES_FAIL((Rel::Lq<SetView,View,true>::post(home,x0,x1)));
      break;
    case SRT_GQ:
      GECODE_ES_FAIL((Rel::Lq<View,SetView,false>::post(home,x1,x0)));
      break;
    case SRT_GR:
      GECODE_ES_FAIL((Rel::Lq<View,SetView,true>::post(home,x1,x0)));
      break;
    default:
      throw UnknownRelation("Set::rel");
    }
  }

  template<class View0, class View1, class View2>
  forceinline void
  rel_op_post_nocompl(Home home, View0 x, SetOpType op, View1 y,
                      SetRelType r, View2 z) {
    if (home.failed()) return;
    switch(r) {
    case SRT_EQ:
      rel_eq<View0,View1,View2>(home, x, op, y, z);
      break;
    case SRT_LQ: case SRT_LE: case SRT_GQ: case SRT_GR:
      {
        SetVar tmp(home,IntSet::empty,Set::Limits::min,Set::Limits::max);
        rel_eq<View0,View1,SetView>(home, x, op, y, tmp);
        rel_op_post_lex<View2>(home,tmp,r,z);
      }
      break;
    case SRT_NQ:
      {
        SetVar tmp(home);
        GECODE_ES_FAIL(
                       (Rel::Distinct<SetView,View2>
                        ::post(home,tmp,z)));
        rel_eq<View0,View1,SetView>(home, x, op, y, tmp);
      }
      break;
    case SRT_SUB:
      rel_sub<View0,View1,View2>(home, x, op, y, z);
      break;
    case SRT_SUP:
      rel_sup<View0,View1,View2>(home, x, op, y, z);
      break;
    case SRT_DISJ:
      {
        SetVar tmp(home);
        EmptyView emptyset;
        GECODE_ES_FAIL((SuperOfInter<View2,SetView,EmptyView>
                             ::post(home, z, tmp, emptyset)));
        rel_eq<View0,View1,SetView>(home, x, op, y, tmp);
      }
      break;
    default:
      GECODE_NEVER;
    }

  }

  GECODE_SET_EXPORT void
  post_nocompl(Home home, SetView x, SetOpType op, SetView y,
               SetRelType r, SetView z);
  GECODE_SET_EXPORT void
  post_nocompl(Home home, ConstSetView x, SetOpType op, SetView y,
               SetRelType r, SetView z);

  GECODE_SET_EXPORT void
  post_nocompl(Home home, SetView x, SetOpType op, SetView y,
               SetRelType r, ConstSetView z);

  GECODE_SET_EXPORT void
  post_nocompl(Home home, ConstSetView x, SetOpType op, SetView y,
               SetRelType r, ConstSetView z);

  GECODE_SET_EXPORT void
  post_compl(Home home, SetView x, SetOpType op, SetView y, SetView z);

  GECODE_SET_EXPORT void
  post_compl(Home home, ConstSetView x, SetOpType op, SetView y, SetView z);

  GECODE_SET_EXPORT void
  post_compl(Home home, SetView x, SetOpType op, SetView y, ConstSetView z);

  GECODE_SET_EXPORT void
  post_compl(Home home, ConstSetView x, SetOpType op, SetView y,
             ConstSetView z);

}}}

// STATISTICS: set-prop
