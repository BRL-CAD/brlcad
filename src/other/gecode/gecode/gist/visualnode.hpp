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

  forceinline
  Extent::Extent(void) : l(-1), r(-1) {}

  forceinline
  Extent::Extent(int l0, int r0) : l(l0), r(r0) {}

  inline
  Extent::Extent(int width) {
    int halfWidth = width / 2;
    l = 0 - halfWidth;
    r = 0 + halfWidth;
  }

  inline void
  Extent::extend(int deltaL, int deltaR) {
    l += deltaL; r += deltaR;
  }

  inline void
  Extent::move(int delta) {
    l += delta; r += delta;
  }

  forceinline int
  Shape::depth(void) const { return _depth; }

  forceinline void
  Shape::setDepth(int d) {
    assert(d <= _depth);
    _depth = d;
  }

  forceinline const Extent&
  Shape::operator [](int i) const {
    assert(i < _depth);
    return shape[i];
  }

  forceinline Extent&
  Shape::operator [](int i) {
    assert(i < _depth);
    return shape[i];
  }

  inline Shape*
  Shape::allocate(int d) {
    assert(d >= 1);
    Shape* ret;
    ret = 
      static_cast<Shape*>(heap.ralloc(sizeof(Shape)+(d-1)*sizeof(Extent)));
    ret->_depth = d;
    return ret;
  }

  forceinline void
  Shape::deallocate(Shape* shape) {
    if (shape != hidden && shape != leaf)
      heap.rfree(shape);
  }

  forceinline bool
  Shape::getExtentAtDepth(int d, Extent& extent) {
    if (d > depth())
      return false;
    extent = Extent(0,0);
    for (int i=0; i <= d; i++) {
      Extent currentExtent = (*this)[i];
      extent.l += currentExtent.l;
      extent.r += currentExtent.r;
    }
    return true;
  }

  forceinline void
  Shape::computeBoundingBox(void) {
    int lastLeft = 0;
    int lastRight = 0;
    bb.left = 0;
    bb.right = 0;
    for (int i=0; i<depth(); i++) {
      lastLeft = lastLeft + (*this)[i].l;
      lastRight = lastRight + (*this)[i].r;
      bb.left = std::min(bb.left,lastLeft);
      bb.right = std::max(bb.right,lastRight);
    }
  }

  forceinline const BoundingBox&
  Shape::getBoundingBox(void) const {
    return bb;
  }

  forceinline bool
  VisualNode::isHidden(void) {
    return getFlag(HIDDEN);
  }

  forceinline void
  VisualNode::setHidden(bool h) {
    setFlag(HIDDEN, h);
  }

  forceinline void
  VisualNode::setStop(bool h) {
    if (getStatus() == BRANCH && h)
      setStatus(STOP);
    else if (getStatus() == STOP && !h)
      setStatus(UNSTOP);
  }

  forceinline int
  VisualNode::getOffset(void) { return offset; }

  forceinline void
  VisualNode::setOffset(int n) { offset = n; }

  forceinline bool
  VisualNode::isDirty(void) {
    return getFlag(DIRTY);
  }

  forceinline void
  VisualNode::setDirty(bool d) {
    setFlag(DIRTY, d);
  }

  forceinline bool
  VisualNode::childrenLayoutIsDone(void) {
    return getFlag(CHILDRENLAYOUTDONE);
  }

  forceinline void
  VisualNode::setChildrenLayoutDone(bool d) {
    setFlag(CHILDRENLAYOUTDONE, d);
  }

  forceinline bool
  VisualNode::isMarked(void) {
    return getFlag(MARKED);
  }

  forceinline void
  VisualNode::setMarked(bool m) {
    setFlag(MARKED, m);
  }

  forceinline bool
  VisualNode::isBookmarked(void) {
    return getFlag(BOOKMARKED);
  }

  forceinline void
  VisualNode::setBookmarked(bool m) {
    setFlag(BOOKMARKED, m);
  }

  forceinline bool
  VisualNode::isOnPath(void) {
    return getFlag(ONPATH);
  }

  forceinline void
  VisualNode::setOnPath(bool b) {
    setFlag(ONPATH, b);
  }

  forceinline Shape*
  VisualNode::getShape(void) {
    return isHidden() ? Shape::hidden : shape;
  }

  forceinline BoundingBox
  VisualNode::getBoundingBox(void) { return getShape()->getBoundingBox(); }

}}

// STATISTICS: gist-any
