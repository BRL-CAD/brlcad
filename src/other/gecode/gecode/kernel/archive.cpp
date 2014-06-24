/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2011
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

#include <gecode/kernel.hh>

namespace Gecode {

  void
  Archive::resize(int n) {
    int m = std::max(n+1, (3*_size)/2);
    _a = heap.realloc<unsigned int>(_a,_n,m);
    _size = m;
  }
  
  Archive::Archive(const Archive& e) : _size(e._n), _n(e._n), _pos(e._pos) {
    _a = heap.alloc<unsigned int>(_n);
    heap.copy<unsigned int>(_a,e._a,_n);
  }
  
  Archive&
  Archive::operator =(const Archive& e) {
    if (this!=&e) {
      _a = heap.realloc<unsigned int>(_a, _size, e._n);
      heap.copy<unsigned int>(_a,e._a,e._n);
      _size = _n = e._n;
    }
    return *this;
  }
  
  Archive::~Archive(void) {
    heap.free<unsigned int>(_a,_size);
  }

}

// STATISTICS: kernel-branch
