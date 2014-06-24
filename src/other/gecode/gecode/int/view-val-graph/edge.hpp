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

  template<class View>
  forceinline
  Edge<View>::Edge(ValNode<View>* s, ViewNode<View>* d)
    : sd(s,d) {
    s->add(this);
  }
  template<class View>
  forceinline
  Edge<View>::Edge(ValNode<View>* s, ViewNode<View>* d, Edge<View>* n)
    : _next_edge(n), sd(s,d) {
    s->add(this);
  }

  template<class View>
  forceinline Node<View>*
  Edge<View>::dst(Node<View>* s) const {
    return sd.ptr(s);
  }

  template<class View>
  forceinline void
  Edge<View>::revert(Node<View>* d) {
    unlink();
    d->add(this);
  }

  template<class View>
  forceinline ViewNode<View>*
  Edge<View>::view(ValNode<View>* n) const {
    return static_cast<ViewNode<View>*>(sd.ptr(n));
  }
  template<class View>
  forceinline ValNode<View>*
  Edge<View>::val(ViewNode<View>* x) const {
    return static_cast<ValNode<View>*>(sd.ptr(x));
  }

  template<class View>
  forceinline bool
  Edge<View>::used(Node<View>* v) const {
    return sd.is_set() || (v->comp == sd.ptr(v)->comp);
  }
  template<class View>
  forceinline void
  Edge<View>::use(void) {
    sd.set();
  }
  template<class View>
  forceinline void
  Edge<View>::free(void) {
    sd.unset();
  }

  template<class View>
  forceinline Edge<View>*
  Edge<View>::next_edge(void) const {
    return _next_edge;
  }
  template<class View>
  forceinline Edge<View>**
  Edge<View>::next_edge_ref(void) {
    return &_next_edge;
  }
  template<class View>
  forceinline Edge<View>*
  Edge<View>::next(void) const {
    return static_cast<Edge<View>*>(BiLink::next());
  }

  template<class View>
  forceinline void
  Edge<View>::operator delete(void*, size_t) {}
  template<class View>
  forceinline void
  Edge<View>::operator delete(void*,Space&) {}
  template<class View>
  forceinline void*
  Edge<View>::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

}}}

// STATISTICS: int-prop

