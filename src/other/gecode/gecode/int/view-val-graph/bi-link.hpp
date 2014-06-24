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

namespace Gecode { namespace Int { namespace ViewValGraph {

  forceinline
  BiLink::BiLink(void) {
    _prev = this; _next = this;
  }

  forceinline BiLink*
  BiLink::prev(void) const {
    return _prev;
  }
  forceinline BiLink*
  BiLink::next(void) const {
    return _next;
  }
  forceinline void
  BiLink::prev(BiLink* l) {
    _prev = l;
  }
  forceinline void
  BiLink::next(BiLink* l) {
    _next = l;
  }

  forceinline void
  BiLink::add(BiLink* l) {
    l->_prev = this; l->_next = _next;
    _next->_prev = l; _next = l;
  }
  forceinline void
  BiLink::unlink(void) {
    BiLink* p = _prev; BiLink* n = _next;
    p->_next = n; n->_prev = p;
  }

  forceinline void
  BiLink::mark(void) {
    _next = NULL;
  }
  forceinline bool
  BiLink::marked(void) const {
    return _next == NULL;
  }
  forceinline bool
  BiLink::empty(void) const {
    return _prev == this;
  }

}}}

// STATISTICS: int-prop

