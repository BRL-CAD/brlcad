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

  inline void
  DrawingCursor::moveUpwards(void) {
    x -= node()->getOffset();
    y -= Layout::dist_y;
    NodeCursor<VisualNode>::moveUpwards();
  }

  forceinline bool
  DrawingCursor::isClipped(void) {
    if (clippingRect.width() == 0 && clippingRect.x() == 0
        && clippingRect.height() == 0 && clippingRect.y() == 0)
      return false;
    BoundingBox b = node()->getBoundingBox();
    return (x + b.left > clippingRect.x() + clippingRect.width() ||
            x + b.right < clippingRect.x() ||
            y > clippingRect.y() + clippingRect.height() ||
            y + (node()->getShape()->depth()+1) * Layout::dist_y < 
            clippingRect.y());
  }

  inline bool
  DrawingCursor::mayMoveDownwards(void) {
    return NodeCursor<VisualNode>::mayMoveDownwards() &&
           !node()->isHidden() &&
           node()->childrenLayoutIsDone() &&
           !isClipped();
  }

  inline void
  DrawingCursor::moveDownwards(void) {
    NodeCursor<VisualNode>::moveDownwards();
    x += node()->getOffset();
    y += Layout::dist_y;
  }

  inline void
  DrawingCursor::moveSidewards(void) {
    x -= node()->getOffset();
    NodeCursor<VisualNode>::moveSidewards();
    x += node()->getOffset();
  }


}}
// STATISTICS: gist-any
