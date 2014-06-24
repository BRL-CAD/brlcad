/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Christian Schulte <schulte@gecode.org>
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


#include <gecode/set.hh>

namespace Gecode {

  SetVar::SetVar(Space& home)
    : VarImpVar<Set::SetVarImp>(new (home) Set::SetVarImp(home)) {}

  SetVar::SetVar(Space& home,int lbMin,int lbMax,int ubMin,int ubMax,
                 unsigned int minCard, unsigned int maxCard)
    : VarImpVar<Set::SetVarImp>(new (home) Set::SetVarImp(home,lbMin,lbMax,
                                                          ubMin,ubMax,
                                                          minCard,maxCard)) {
    Set::Limits::check(lbMin,"SetVar::SetVar");
    Set::Limits::check(lbMax,"SetVar::SetVar");
    Set::Limits::check(ubMin,"SetVar::SetVar");
    Set::Limits::check(ubMax,"SetVar::SetVar");
    Set::Limits::check(maxCard,"SetVar::SetVar");
    if (minCard > maxCard || minCard > lubSize() || maxCard < glbSize() ||
        lbMin < ubMin || lbMax > ubMax)
      throw Set::VariableEmptyDomain("SetVar::SetVar");
  }

  SetVar::SetVar(Space& home, const IntSet& glb,int ubMin,int ubMax,
                 unsigned int minCard, unsigned int maxCard)
    : VarImpVar<Set::SetVarImp>(new (home) Set::SetVarImp(home,glb,ubMin,ubMax,
                                                          minCard,maxCard)) {
    Set::Limits::check(glb,"SetVar::SetVar");
    Set::Limits::check(ubMin,"SetVar::SetVar");
    Set::Limits::check(ubMax,"SetVar::SetVar");
    Set::Limits::check(maxCard,"SetVar::SetVar");
    if (minCard > maxCard || minCard > lubSize() || maxCard < glbSize() ||
        glb.min() < ubMin || glb.max() > ubMax)
      throw Set::VariableEmptyDomain("SetVar::SetVar");
  }

  SetVar::SetVar(Space& home,int lbMin,int lbMax,const IntSet& lub,
                 unsigned int minCard, unsigned int maxCard)
    : VarImpVar<Set::SetVarImp>(new (home) Set::SetVarImp(home,lbMin,lbMax,lub,
                                                          minCard,maxCard)) {
    Set::Limits::check(lbMin,"SetVar::SetVar");
    Set::Limits::check(lbMax,"SetVar::SetVar");
    Set::Limits::check(lub,"SetVar::SetVar");
    Set::Limits::check(maxCard,"SetVar::SetVar");
    Iter::Ranges::Singleton glbr(lbMin,lbMax);
    IntSetRanges lubr(lub);
    if (minCard > maxCard || minCard > lubSize() || maxCard < glbSize() ||
        !Iter::Ranges::subset(glbr,lubr))
      throw Set::VariableEmptyDomain("SetVar::SetVar");
  }

  SetVar::SetVar(Space& home,
                 const IntSet& glb, const IntSet& lub,
                 unsigned int minCard, unsigned int maxCard)
    : VarImpVar<Set::SetVarImp>(new (home) Set::SetVarImp(home,glb,lub,minCard,
                                                          maxCard)) {
    Set::Limits::check(glb,"SetVar::SetVar");
    Set::Limits::check(lub,"SetVar::SetVar");
    Set::Limits::check(maxCard,"SetVar::SetVar");
    IntSetRanges glbr(glb);
    IntSetRanges lubr(lub);
    if (minCard > maxCard || minCard > lubSize() || maxCard < glbSize() ||
        !Iter::Ranges::subset(glbr,lubr))
      throw Set::VariableEmptyDomain("SetVar::SetVar");
  }

}

// STATISTICS: set-var

