/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

namespace Gecode { namespace Int { namespace Count {

  template<class VX, class VY, class VZ, bool shr, bool dom>
  forceinline
  EqView<VX,VY,VZ,shr,dom>::EqView(Home home,
                               ViewArray<VX>& x, VY y, VZ z, int c)
    : ViewBase<VX,VY,VZ>(home,x,y,z,c) {}

  template<class VX, class VY, class VZ, bool shr, bool dom>
  ExecStatus
  EqView<VX,VY,VZ,shr,dom>::post(Home home,
                             ViewArray<VX>& x, VY y, VZ z, int c) {
    GECODE_ME_CHECK(z.gq(home,-c));
    GECODE_ME_CHECK(z.lq(home,x.size()-c));
    if ((vtd(y) != VTD_VARVIEW) && z.assigned())
      return EqInt<VX,VY>::post(home,x,y,z.val()+c);
    if (sharing(x,y,z))
      (void) new (home) EqView<VX,VY,VZ,true,dom>(home,x,y,z,c);
    else
      (void) new (home) EqView<VX,VY,VZ,false,dom>(home,x,y,z,c);
    return ES_OK;
  }

  template<class VX, class VY, class VZ, bool shr, bool dom>
  forceinline
  EqView<VX,VY,VZ,shr,dom>::EqView(Space& home, bool share,
                               EqView<VX,VY,VZ,shr,dom>& p)
    : ViewBase<VX,VY,VZ>(home,share,p) {}

  template<class VX, class VY, class VZ, bool shr, bool dom>
  Actor*
  EqView<VX,VY,VZ,shr,dom>::copy(Space& home, bool share) {
    return new (home) EqView<VX,VY,VZ,shr,dom>(home,share,*this);
  }

  template<class VX, class VY, class VZ, bool shr, bool dom>
  ExecStatus
  EqView<VX,VY,VZ,shr,dom>::propagate(Space& home, const ModEventDelta&) {
    count(home);

    GECODE_ME_CHECK(z.gq(home,atleast()));
    GECODE_ME_CHECK(z.lq(home,atmost()));

    if (z.assigned()) {
      if (z.val() == atleast()) {
        GECODE_ES_CHECK(post_false(home,x,y));
        return home.ES_SUBSUMED(*this);
      }
      if (z.val() == atmost()) {
        GECODE_ES_CHECK(post_true(home,x,y));
        return home.ES_SUBSUMED(*this);
      }
      if (!dom || (vtd(y) != VTD_VARVIEW)) {
        VY yc(y);
        GECODE_REWRITE(*this,(EqInt<VX,VY>
                              ::post(home(*this),x,yc,z.val()+c)));    
      }
    }


    if (dom && (vtd(y) == VTD_VARVIEW) && (z.min() > 0)) {
      /* 
       * Only if the propagator is at fixpoint here, continue
       * when things are shared: the reason is that prune
       * requires that the views in x overlap with y!
       */
      if (shr && (VX::me(Propagator::modeventdelta()) != ME_INT_NONE))
        return ES_NOFIX;

      GECODE_ES_CHECK(prune(home,x,y));

      return ES_NOFIX;
    }
    
    return shr ? ES_NOFIX : ES_FIX;
  }

}}}

// STATISTICS: int-prop
