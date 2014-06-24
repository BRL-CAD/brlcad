/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
 *     Gabor Szokoli, 2004
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

#define GECODE_SET_ME_CHECK_VAL(p,f) {                                \
    ModEvent __me__ ## __LINE__ = (p);                                \
    if (me_failed(__me__ ## __LINE__)) return ES_FAILED;        \
    if (ME_GEN_ASSIGNED==(__me__ ## __LINE__))f=true; }

#define GECODE_SET_ME_CHECK_VAL_B(modified, tell, f)        \
  {                                                        \
    ModEvent me = (tell);                                \
    modified |= me_modified(me);                        \
    if (ME_GEN_ASSIGNED==(me))f=true;                        \
    GECODE_ME_CHECK(me);                                \
  }

namespace Gecode { namespace Set { namespace Rel {

  forceinline
  bool subsumesME(ModEvent me0, ModEvent me1, ModEvent me2, ModEvent me) {
    ModEvent cme = SetVarImp::me_combine(me0,SetVarImp::me_combine(me1, me2));
    return SetVarImp::me_combine(cme, me)==cme;
  }
  forceinline
  bool subsumesME(ModEvent me0, ModEvent me1, ModEvent me) {
    ModEvent cme = SetVarImp::me_combine(me0, me1);
    return SetVarImp::me_combine(cme, me)==cme;
  }
  forceinline
  bool subsumesME(ModEvent me0, ModEvent me) {
    return SetVarImp::me_combine(me0, me)==me0;
  }

  forceinline
  bool testSetEventLB(ModEvent me0, ModEvent me1, ModEvent me2) {
    return subsumesME(me0, me1, me2, ME_SET_GLB);
  }
  forceinline
  bool testSetEventUB(ModEvent me0, ModEvent me1, ModEvent me2) {
    return subsumesME(me0, me1, me2, ME_SET_LUB);
  }
  forceinline
  bool testSetEventAnyB(ModEvent me0, ModEvent me1, ModEvent me2) {
    return ( me0!=ME_SET_CARD || me1!=ME_SET_CARD || me2!=ME_SET_CARD );
  }
  forceinline
  bool testSetEventCard(ModEvent me0, ModEvent me1, ModEvent me2) {
    return subsumesME(me0, me1, me2, ME_SET_CARD);
  }
  forceinline
  bool testSetEventLB(ModEvent me0, ModEvent me1) {
    return subsumesME(me0, me1, ME_SET_GLB);
  }
  forceinline
  bool testSetEventUB(ModEvent me0, ModEvent me1) {
    return subsumesME(me0, me1, ME_SET_LUB);
  }
  forceinline
  bool testSetEventAnyB(ModEvent me0, ModEvent me1) {
    return ( me0!=ME_SET_CARD || me1!=ME_SET_CARD );
  }
  forceinline
  bool testSetEventCard(ModEvent me0, ModEvent me1) {
    return subsumesME(me0, me1, ME_SET_CARD);
  }
  forceinline
  bool testSetEventLB(ModEvent me0) {
    return subsumesME(me0, ME_SET_GLB);
  }
  forceinline
  bool testSetEventUB(ModEvent me0) {
    return subsumesME(me0, ME_SET_LUB);
  }
  forceinline
  bool testSetEventAnyB(ModEvent me0) {
    return ( me0!=ME_SET_CARD );
  }
  forceinline
  bool testSetEventCard(ModEvent me0) {
    return subsumesME(me0, ME_SET_CARD);
  }

}}}

// STATISTICS: set-prop
