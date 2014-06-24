/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christopher Mears <chris.mears@monash.edu>
 *
 *  Copyright:
 *     Christopher Mears, 2012
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

#include <gecode/int/ldsb.hh>

namespace Gecode {
  SymmetryHandle::SymmetryHandle(void)
    : ref(NULL) {}
  SymmetryHandle::SymmetryHandle(Int::LDSB::SymmetryObject* o)
    : ref(o) {}
  SymmetryHandle::SymmetryHandle(const SymmetryHandle& h)
    : ref(h.ref) {
    if (ref != NULL)
      increment();
  }
  const SymmetryHandle&
  SymmetryHandle::operator=(const SymmetryHandle& h) {
    if (h.ref == ref)
      return *this;
    if (ref != NULL)
      decrement();
    ref = h.ref;
    if (ref != NULL)
      increment();
    return *this;
  }
  SymmetryHandle::~SymmetryHandle(void) {
    if (ref != NULL)
      decrement();
  }
  void
  SymmetryHandle::increment(void) {
    (ref->nrefs)++;
  }
  void
  SymmetryHandle::decrement(void) {
    (ref->nrefs)--;
    if (ref->nrefs == 0)
      delete ref;
    ref = NULL;
  }
}

namespace Gecode { namespace Int { namespace LDSB {

  SymmetryObject::SymmetryObject(void)
    : nrefs(1) {}
  SymmetryObject::~SymmetryObject(void) {}

  VariableSymmetryObject::VariableSymmetryObject(ArgArray<VarImpBase*> vars) {
    nxs = vars.size();
    xs = new VarImpBase*[nxs];
    for (int i = 0 ; i < nxs ; i++)
      xs[i] = vars[i];
  }
  VariableSymmetryObject::~VariableSymmetryObject(void) {
    delete [] xs;
  }

  ValueSymmetryObject::ValueSymmetryObject(IntSet vs)
    : values(vs) {}

  VariableSequenceSymmetryObject::
  VariableSequenceSymmetryObject(ArgArray<VarImpBase*> x, int ss)
    : seq_size(ss) {
    nxs = x.size();
    xs = new VarImpBase*[nxs];
    for (int i = 0 ; i < nxs ; i++)
      xs[i] = x[i];
  }
  VariableSequenceSymmetryObject::~VariableSequenceSymmetryObject(void) {
    delete [] xs;
  }

  ValueSequenceSymmetryObject::ValueSequenceSymmetryObject(IntArgs vs, int ss)
      : values(vs), seq_size(ss) {}

}}}

// STATISTICS: int-branch
