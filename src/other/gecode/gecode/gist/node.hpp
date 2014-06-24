/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2006
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

namespace Gecode { namespace Gist {

  template<class T>
  void
  NodeAllocatorBase<T>::allocate(void) {
    cur_b++;
    cur_t = 0;
    if (cur_b==n) {
      int oldn = n;
      n = static_cast<int>(n*1.5+1.0);
      b = heap.realloc<Block*>(b,oldn,n);
    }
    b[cur_b] = static_cast<Block*>(heap.ralloc(sizeof(Block)));
  }

  template<class T>
  NodeAllocatorBase<T>::NodeAllocatorBase(bool bab)
    : _bab(bab) {
    b = heap.alloc<Block*>(10);
    n = 10;
    cur_b = -1;
    cur_t = NodeBlockSize-1;
  }

  template<class T>
  NodeAllocatorBase<T>::~NodeAllocatorBase(void) {
    for (int i=cur_b+1; i--;)
      heap.rfree(b[i]);
    heap.free<Block*>(b,n);
  }

  template<class T>
  forceinline int
  NodeAllocatorBase<T>::allocate(int p) {
    cur_t++;
    if (cur_t==NodeBlockSize)
      allocate();
    new (&b[cur_b]->b[cur_t]) T(p);
    b[cur_b]->best[cur_t] = -1;
    return cur_b*NodeBlockSize+cur_t;
  }

  template<class T>
  forceinline int
  NodeAllocatorBase<T>::allocate(Space* root) {
    cur_t++;
    if (cur_t==NodeBlockSize)
      allocate();
    new (&b[cur_b]->b[cur_t]) T(root);
    b[cur_b]->best[cur_t] = -1;
    return cur_b*NodeBlockSize+cur_t;
  }

  template<class T>
  forceinline T*
  NodeAllocatorBase<T>::operator [](int i) const {
    assert(i/NodeBlockSize < n);
    assert(i/NodeBlockSize < cur_b || i%NodeBlockSize <= cur_t);
    return &(b[i/NodeBlockSize]->b[i%NodeBlockSize]);
  }

  template<class T>
  forceinline T*
  NodeAllocatorBase<T>::best(int i) const {
    assert(i/NodeBlockSize < n);
    assert(i/NodeBlockSize < cur_b || i%NodeBlockSize <= cur_t);
    int bi = b[i/NodeBlockSize]->best[i%NodeBlockSize];
    return bi == -1 ? NULL : (*this)[bi];
  }

  template<class T>
  forceinline void
  NodeAllocatorBase<T>::setBest(int i, int best) {
    assert(i/NodeBlockSize < n);
    assert(i/NodeBlockSize < cur_b || i%NodeBlockSize <= cur_t);
    b[i/NodeBlockSize]->best[i%NodeBlockSize] = best;
  }
  
  template<class T>
  forceinline bool
  NodeAllocatorBase<T>::bab(void) const {
    return _bab;
  }

  template<class T>
  forceinline bool
  NodeAllocatorBase<T>::showLabels(void) const {
    return !labels.isEmpty();
  }

  template<class T>
  bool
  NodeAllocatorBase<T>::hasLabel(T* n) const {
    return labels.contains(n);
  }
  
  template<class T>
  void
  NodeAllocatorBase<T>::setLabel(T* n, const QString& l) {
    labels[n] = l;
  }

  template<class T>
  void
  NodeAllocatorBase<T>::clearLabel(T* n) {
    labels.remove(n);
  }

  template<class T>
  QString
  NodeAllocatorBase<T>::getLabel(T* n) const {
    return labels.value(n);
  }
  
  forceinline unsigned int
  Node::getTag(void) const {
    return static_cast<unsigned int>
      (reinterpret_cast<ptrdiff_t>(childrenOrFirstChild) & 3);
  }

  forceinline void
  Node::setTag(unsigned int tag) {
    assert(tag <= 3);
    assert(getTag() == UNDET);
    childrenOrFirstChild = reinterpret_cast<void*>
      ( (reinterpret_cast<ptrdiff_t>(childrenOrFirstChild) & ~(3)) | tag);
  }

  forceinline void*
  Node::getPtr(void) const {
    return reinterpret_cast<void*>
      (reinterpret_cast<ptrdiff_t>(childrenOrFirstChild) & ~(3));
  }

  forceinline int
  Node::getFirstChild(void) const {
    return static_cast<int>
      ((reinterpret_cast<ptrdiff_t>(childrenOrFirstChild) & ~(3)) >> 2);
  }

  forceinline
  Node::Node(int p, bool failed) : parent(p) {
    childrenOrFirstChild = NULL;
    noOfChildren = 0;
    setTag(failed ? LEAF : UNDET);
  }

  forceinline int
  Node::getParent(void) const {
    return parent;
  }

  forceinline VisualNode*
  Node::getParent(const NodeAllocator& na) const {
    return parent < 0 ? NULL : na[parent];
  }

  forceinline bool
  Node::isUndetermined(void) const { return getTag() == UNDET; }

  forceinline int
  Node::getChild(int n) const {
    assert(getTag() != UNDET && getTag() != LEAF);
    if (getTag() == TWO_CHILDREN) {
      assert(n != 1 || noOfChildren <= 0);
      return n == 0 ? getFirstChild() : -noOfChildren;
    }
    assert(n < noOfChildren);
    return static_cast<int*>(getPtr())[n];
  }

  forceinline VisualNode*
  Node::getChild(const NodeAllocator& na, int n) const {
    return na[getChild(n)];
  }

  forceinline bool
  Node::isRoot(void) const { return parent == -1; }

  forceinline unsigned int
  Node::getNumberOfChildren(void) const {
    switch (getTag()) {
    case UNDET: return 0;
    case LEAF:  return 0;
    case TWO_CHILDREN: return 1+(noOfChildren <= 0);
    default: return noOfChildren;
    }
  }

  inline int
  Node::getIndex(const NodeAllocator& na) const {
    if (parent==-1)
      return 0;
    Node* p = na[parent];
    for (int i=p->getNumberOfChildren(); i--;)
      if (p->getChild(na,i) == this)
        return p->getChild(i);
    GECODE_NEVER;
    return -1;
  }

}}

// STATISTICS: gist-any
