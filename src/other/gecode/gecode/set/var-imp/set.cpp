/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
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


namespace Gecode { namespace Set {

  /*
   * "Standard" tell operations
   *
   */
  ModEvent
  SetVarImp::cardMin_full(Space& home) {
    ModEvent me = ME_SET_CARD;
    if (cardMin() == lub.size()) {
      glb.become(home, lub);
      me = ME_SET_VAL;
    }
    SetDelta d;
    return notify(home, me, d);
  }

  ModEvent
  SetVarImp::cardMax_full(Space& home) {
    ModEvent me = ME_SET_CARD;
    if (cardMax() == glb.size()) {
      lub.become(home, glb);
      me = ME_SET_VAL;
    }
    SetDelta d;
    return notify(home, me, d);
  }

  ModEvent
  SetVarImp::processLubChange(Space& home, SetDelta& d) {
    ModEvent me = ME_SET_LUB;
    if (cardMax() > lub.size()) {
      lub.card(lub.size());
      if (cardMin() > cardMax()) {
        glb.become(home, lub);
        glb.card(glb.size());
        lub.card(glb.size());
        return ME_SET_FAILED;
      }
      me = ME_SET_CLUB;
    }
    if (cardMax() == lub.size() && cardMin() == cardMax()) {
      glb.become(home, lub);
      me = ME_SET_VAL;
      assert(d.glbMin() == 1);
      assert(d.glbMax() == 0);
    }
    return notify(home, me, d);
  }

  ModEvent
  SetVarImp::processGlbChange(Space& home, SetDelta& d) {
    ModEvent me = ME_SET_GLB;
    if (cardMin() < glb.size()) {
      glb.card(glb.size());
      if (cardMin() > cardMax()) {
        glb.become(home, lub);
        glb.card(glb.size());
        lub.card(glb.size());
        return ME_SET_FAILED;
      }
      me = ME_SET_CGLB;
    }
    if (cardMin() == glb.size() && cardMin() == cardMax()) {
      lub.become(home, glb);
      me = ME_SET_VAL;
      assert(d.lubMin() == 1);
      assert(d.lubMax() == 0);
    }
    return notify(home, me, d);
  }

  /*
   * Copying variables
   *
   */

  forceinline
  SetVarImp::SetVarImp(Space& home, bool share, SetVarImp& x)
    : SetVarImpBase(home,share,x) {
    lub.update(home, x.lub);
    glb.card(x.cardMin());
    lub.card(x.cardMax());
    if (x.assigned()) {
      glb.become(home,lub);
    } else {
      glb.update(home,x.glb);
    }
  }


  SetVarImp*
  SetVarImp::perform_copy(Space& home, bool share) {
    return new (home) SetVarImp(home,share,*this);
  }

}}

// STATISTICS: set-var

