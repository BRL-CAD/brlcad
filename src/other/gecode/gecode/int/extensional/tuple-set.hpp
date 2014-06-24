/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
 *     Mikael Lagerkvist, 2007
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

#include <sstream>

namespace Gecode {

  forceinline bool
  TupleSet::TupleSetI::finalized(void) const {
    assert(((excess == -1) && (domsize > 0)) ||
           ((excess != -1) && (domsize == 0)));
    return excess == -1;
  }

  forceinline
  TupleSet::TupleSetI::TupleSetI(void)
    : arity(-1),
      size(0),
      tuples(NULL),
      tuple_data(NULL),
      data(NULL),
      excess(0),
      min(Int::Limits::max),
      max(Int::Limits::min),
      domsize(0),
      last(NULL),
      nullpointer(NULL)
  {}


  template<class T>
  void
  TupleSet::TupleSetI::add(T t) {
    assert(arity  != -1); // Arity has been set
    assert(excess != -1); // Tuples may still be added
    if (excess == 0) resize();
    assert(excess >= 0);
    --excess;
    int end = size*arity;
    for (int i = arity; i--; ) {
      data[end+i] = t[i];
      if (t[i] < min) min = t[i];
      if (t[i] > max) max = t[i];
    }
    ++size;
  }

  forceinline
  TupleSet::TupleSet(void) {
  }

  forceinline
  TupleSet::TupleSet(const TupleSet& ts)
    : SharedHandle(ts) {}

  forceinline TupleSet::TupleSetI*
  TupleSet::implementation(void) {
    TupleSetI* imp = static_cast<TupleSetI*>(object());
    assert(imp);
    return imp;
  }

  inline void
  TupleSet::add(const IntArgs& tuple) {
    TupleSetI* imp = static_cast<TupleSetI*>(object());
    if (imp == NULL) {
      imp = new TupleSetI;
      object(imp);
    }
    assert(imp->arity == -1 ||
           imp->arity == tuple.size());
    imp->arity = tuple.size();
    imp->add(tuple);
  }

  forceinline void
  TupleSet::finalize(void) {
    TupleSetI* imp = static_cast<TupleSetI*>(object());
    if (imp == NULL) {
      imp = new TupleSetI;
      imp->arity = 0;
      imp->excess = -1;
      imp->domsize = 1;
      imp->size = 1;
      object(imp);
    }
    if (!imp->finalized()) {
      imp->finalize();
    }
  }

  forceinline bool
  TupleSet::finalized(void) const {
    TupleSetI* imp = static_cast<TupleSetI*>(object());
    assert(imp);
    return imp->finalized();
  }

  forceinline int
  TupleSet::arity(void) const {
    TupleSetI* imp = static_cast<TupleSetI*>(object());
    assert(imp);
    assert(imp->arity != -1);
    return imp->arity;
  }
  forceinline int
  TupleSet::tuples(void) const {
    TupleSetI* imp = static_cast<TupleSetI*>(object());
    assert(imp);
    assert(imp->finalized());
    return imp->size-1;
  }
  forceinline TupleSet::Tuple
  TupleSet::operator [](int i) const {
    TupleSetI* imp = static_cast<TupleSetI*>(object());
    assert(imp);
    assert(imp->finalized());
    return imp->data + i*imp->arity;
  }
  forceinline int
  TupleSet::min(void) const {
    TupleSetI* imp = static_cast<TupleSetI*>(object());
    assert(imp);
    assert(imp->finalized());
    return imp->min;
  }
  forceinline int
  TupleSet::max(void) const {
    TupleSetI* imp = static_cast<TupleSetI*>(object());
    assert(imp);
    assert(imp->finalized());
    return imp->max;
  }


  template<class Char, class Traits, class T>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const TupleSet& ts) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << "Number of tuples: " << ts.tuples() << std::endl
      << "Tuples:" << std::endl;
    for (int i = 0; i < ts.tuples(); ++i) {
      s << '\t';
      for (int j = 0; j < ts.arity(); ++j) {
        s.width(3);
        s << " " << ts[i][j];
      }
      s << std::endl;
    }
    return os << s.str();
  }

}

// STATISTICS: int-prop

