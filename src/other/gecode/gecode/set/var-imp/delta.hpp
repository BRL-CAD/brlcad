/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2007
 *     Guido Tack, 2007
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

namespace Gecode { namespace Set {

  forceinline
  SetDelta::SetDelta(void) : _glbMin(1), _glbMax(0), _lubMin(1), _lubMax(0) {}

  forceinline
  SetDelta::SetDelta(int glbMin, int glbMax,
                     int lubMin, int lubMax)
    : _glbMin(glbMin), _glbMax(glbMax),
      _lubMin(lubMin), _lubMax(lubMax) {}

  forceinline int
  SetDelta::glbMin(void) const {
    return _glbMin;
  }
  forceinline int
  SetDelta::glbMax(void) const {
    return _glbMax;
  }
  forceinline int
  SetDelta::lubMin(void) const {
    return _lubMin;
  }
  forceinline int
  SetDelta::lubMax(void) const {
    return _lubMax;
  }
  forceinline bool
  SetDelta::glbAny(void) const {
    return _glbMin > _glbMax;
  }
  forceinline bool
  SetDelta::lubAny(void) const {
    return _lubMin > _lubMax;
  }

}}

// STATISTICS: set-var
