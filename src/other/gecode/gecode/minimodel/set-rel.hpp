/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2005
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

#ifdef GECODE_HAS_SET_VARS

namespace Gecode {

  /*
   * Operations for set relations
   *
   */
  forceinline
  SetCmpRel::SetCmpRel(const SetExpr& l0, SetRelType srt0, const SetExpr& r0)
    : l(l0), r(r0), srt(srt0) {}

  forceinline
  SetRel::SetRel(void) {}
   
  forceinline
  SetRel::SetRel(const SetExpr& e0, SetRelType srt, const SetExpr& e1)
    : _e0(e0), _srt(srt), _e1(e1) {}

  forceinline
  SetRel::SetRel(const SetCmpRel& r)
    : _e0(r.l), _srt(r.srt), _e1(r.r) {}

  forceinline void
  SetRel::post(Home home, bool t) const {
    if (t)
      _e0.post(home,_srt,_e1);
    else if (_srt == SRT_EQ)
      _e0.post(home,SRT_NQ,_e1);
    else if (_srt == SRT_NQ)
      _e0.post(home,SRT_EQ,_e1);
    else {
      BoolVar b(home,0,0);
      _e0.post(home,b,true,_srt,_e1);
    }
  }

  forceinline void
  SetRel::post(Home home, BoolVar b, bool t) const {
    _e0.post(home,b,t,_srt,_e1);
  }

}

#endif

// STATISTICS: minimodel-any
