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

#include <gecode/gist/visualnode.hh>

#include <gecode/gist/layoutcursor.hh>
#include <gecode/gist/nodevisitor.hh>

#include <utility>

namespace Gecode { namespace Gist {

  Shape* Shape::leaf;
  Shape* Shape::hidden;

  /// Allocate shapes statically
  class ShapeAllocator {
  public:
    /// Constructor
    ShapeAllocator(void) {
      Shape::leaf = Shape::allocate(1);
      (*Shape::leaf)[0] = Extent(Layout::extent);
      Shape::leaf->computeBoundingBox();

      Shape::hidden = Shape::allocate(2);
      (*Shape::hidden)[0] = Extent(Layout::extent);
      (*Shape::hidden)[1] = Extent(Layout::extent);
      Shape::hidden->computeBoundingBox();
    }
    ~ShapeAllocator(void) {
      Shape::deallocate(Shape::leaf);
      Shape::deallocate(Shape::hidden);
    }
  };

  /// Allocate shapes statically
  ShapeAllocator shapeAllocator;

  VisualNode::VisualNode(int p)
  : SpaceNode(p)
  , offset(0)
  {
    shape = NULL;
    setDirty(true);
    setChildrenLayoutDone(false);
    setHidden(false);
    setMarked(false);
    setOnPath(false);
    setBookmarked(false);
  }

  VisualNode::VisualNode(Space* root)
  : SpaceNode(root)
  , offset(0)
  {
    shape = NULL;
    setDirty(true);
    setChildrenLayoutDone(false);
    setHidden(false);
    setMarked(false);
    setOnPath(false);
    setBookmarked(false);
  }

  void
  VisualNode::dispose(void) {
    Shape::deallocate(shape);
    SpaceNode::dispose();
  }

  void
  VisualNode::dirtyUp(const NodeAllocator& na) {
    VisualNode* cur = this;
    while (!cur->isDirty()) {
      cur->setDirty(true);
      if (! cur->isRoot()) {
        cur = cur->getParent(na);
      }
    }
  }

  void
  VisualNode::layout(const NodeAllocator& na) {
    LayoutCursor l(this,na);
    PostorderNodeVisitor<LayoutCursor>(l).run();
    // int nodesLayouted = 1;
    // clock_t t0 = clock();
    // while (p.next()) {}
    // while (p.next()) { nodesLayouted++; }
    // double t = (static_cast<double>(clock()-t0) / CLOCKS_PER_SEC) * 1000.0;
    // double nps = static_cast<double>(nodesLayouted) /
    //   (static_cast<double>(clock()-t0) / CLOCKS_PER_SEC);
    // std::cout << "Layout done. " << nodesLayouted << " nodes in "
    //   << t << " ms. " << nps << " nodes/s." << std::endl;
  }

  void VisualNode::pathUp(const NodeAllocator& na) {
    VisualNode* cur = this;
    while (cur) {
      cur->setOnPath(true);
      cur = cur->getParent(na);
    }
  }

  void VisualNode::unPathUp(const NodeAllocator& na) {
    VisualNode* cur = this;
    while (!cur->isRoot()) {
      cur->setOnPath(false);
      cur = cur->getParent(na);
    }
  }

  int
  VisualNode::getPathAlternative(const NodeAllocator& na) {
    for (int i=getNumberOfChildren(); i--;) {
      if (getChild(na,i)->isOnPath())
        return i;
    }
    return -1;
  }

  void
  VisualNode::toggleHidden(const NodeAllocator& na) {
    setHidden(!isHidden());
    dirtyUp(na);
  }

  void
  VisualNode::hideFailed(const NodeAllocator& na, bool onlyDirty) {
    HideFailedCursor c(this,na,onlyDirty);
    PreorderNodeVisitor<HideFailedCursor>(c).run();
    dirtyUp(na);
  }

  void
  VisualNode::labelBranches(NodeAllocator& na,
                            BestNode* curBest, int c_d, int a_d) {
    bool clear = na.hasLabel(this);
    BranchLabelCursor c(this,curBest,c_d,a_d,clear,na);
    PreorderNodeVisitor<BranchLabelCursor>(c).run();
    dirtyUp(na);
  }

  void
  VisualNode::labelPath(NodeAllocator& na,
                        BestNode* curBest, int c_d, int a_d) {
    if (na.hasLabel(this)) {
      // clear labels on path to root
      VisualNode* p = this;
      while (p) {
        na.clearLabel(p);
        p = p->getParent(na);
      }
    } else {
      // set labels on path to root
      std::vector<std::pair<VisualNode*,int> > path;
      VisualNode* p = this;
      while (p) {
        path.push_back(std::pair<VisualNode*,int>(p,p->getAlternative(na)));
        p = p->getParent(na);
      }
      while (!path.empty()) {
        std::pair<VisualNode*,int> cur = path.back(); path.pop_back();
        if (p) {
          std::string l =
            cur.first->getBranchLabel(na,p,p->getChoice(),
                                      curBest,c_d,a_d,cur.second);
          na.setLabel(cur.first,QString(l.c_str()));
        }
        p = cur.first;
      }
    }
    dirtyUp(na);
  }

  void
  VisualNode::unhideAll(const NodeAllocator& na) {
    UnhideAllCursor c(this,na);
    PreorderNodeVisitor<UnhideAllCursor>(c).run();
    dirtyUp(na);
  }

  void
  VisualNode::toggleStop(const NodeAllocator& na) {
    if (getStatus() == STOP)
      setStatus(UNSTOP);
    else if (getStatus() == UNSTOP)
      setStatus(STOP);
    dirtyUp(na);
  }
  
  void
  VisualNode::unstopAll(const NodeAllocator& na) {
    UnstopAllCursor c(this,na);
    PreorderNodeVisitor<UnstopAllCursor>(c).run();
    dirtyUp(na);
  }

  void
  VisualNode::changedStatus(const NodeAllocator& na) { dirtyUp(na); }

  bool
  VisualNode::containsCoordinateAtDepth(int x, int depth) {
    BoundingBox box = getShape()->getBoundingBox();
    if (x < box.left ||
        x > box.right ||
        depth >= getShape()->depth()) {
      return false;
    }
    Extent theExtent;
    if (getShape()->getExtentAtDepth(depth, theExtent)) {
      return (theExtent.l <= x && x <= theExtent.r);
    } else {
      return false;
    }
  }

  VisualNode*
  VisualNode::findNode(const NodeAllocator& na, int x, int y) {
    VisualNode* cur = this;
    int depth = y / Layout::dist_y;

    while (depth > 0 && cur != NULL) {
      if (cur->isHidden()) {
        break;
      }
      VisualNode* oldCur = cur;
      cur = NULL;
      for (unsigned int i=0; i<oldCur->getNumberOfChildren(); i++) {
        VisualNode* nextChild = oldCur->getChild(na,i);
        int newX = x - nextChild->getOffset();
        if (nextChild->containsCoordinateAtDepth(newX, depth - 1)) {
          cur = nextChild;
          x = newX;
          break;
        }
      }
      depth--;
      y -= Layout::dist_y;
    }

    if(cur == this && !cur->containsCoordinateAtDepth(x, 0)) {
      return NULL;
    }
    return cur;
  }

  std::string
  VisualNode::toolTip(NodeAllocator&, BestNode*, int, int) {
    return "";
  }

  std::string
  VisualNode::getBranchLabel(NodeAllocator& na,
                             VisualNode* p, const Choice* c,
                             BestNode* curBest, int c_d, int a_d, int alt) {
    std::ostringstream oss;
    p->acquireSpace(na,curBest,c_d,a_d);
    p->getWorkingSpace()->print(*c,alt,oss);
    return oss.str();
  }


  /// \brief Helper functions for the layout algorithm
  class Layouter {
  public:
    /// Compute distance needed between \a shape1 and \a shape2
    template<class S1, class S2>
    static int getAlpha(const S1& shape1, int depth1,
                        const S2& shape2, int depth2);
    /// Merge \a shape1 and \a shape2 into \a result with distance \a alpha
    template<class S1, class S2>
    static void merge(Extent* result,
                      const S1& shape1, int depth1,
                      const S2& shape2, int depth2, int alpha);
  };

  template<class S1, class S2>
  int
  Layouter::getAlpha(const S1& shape1, int depth1,
                     const S2& shape2, int depth2) {
    int alpha = Layout::minimalSeparation;
    int extentR = 0;
    int extentL = 0;
    for (int i=0; i<depth1 && i<depth2; i++) {
      extentR += shape1[i].r;
      extentL += shape2[i].l;
      alpha = std::max(alpha, extentR - extentL + Layout::minimalSeparation);
    }
    return alpha;
  }

  template<class S1, class S2>
  void
  Layouter::merge(Extent* result,
                  const S1& shape1, int depth1,
                  const S2& shape2, int depth2, int alpha) {
    if (depth1 == 0) {
      for (int i=depth2; i--;)
        result[i] = shape2[i];
    } else if (depth2 == 0) {
      for (int i=depth1; i--;)
        result[i] = shape1[i];
    } else {
      // Extend the topmost right extent by alpha.  This, in effect,
      // moves the second shape to the right by alpha units.
      int topmostL = shape1[0].l;
      int topmostR = shape2[0].r;
      int backoffTo1 =
        shape1[0].r - alpha - shape2[0].r;
      int backoffTo2 =
        shape2[0].l + alpha - shape1[0].l;

      result[0] = Extent(topmostL, topmostR+alpha);

      // Now, since extents are given in relative units, in order to
      // compute the extents of the merged shape, we can just collect the
      // extents of shape1 and shape2, until one of the shapes ends.  If
      // this happens, we need to "back-off" to the axis of the deeper
      // shape in order to properly determine the remaining extents.
      int i=1;
      for (; i<depth1 && i<depth2; i++) {
        Extent currentExtent1 = shape1[i];
        Extent currentExtent2 = shape2[i];
        int newExtentL = currentExtent1.l;
        int newExtentR = currentExtent2.r;
        result[i] = Extent(newExtentL, newExtentR);
        backoffTo1 += currentExtent1.r - currentExtent2.r;
        backoffTo2 += currentExtent2.l - currentExtent1.l;
      }

      // If shape1 is deeper than shape2, back off to the axis of shape1,
      // and process the remaining extents of shape1.
      if (i<depth1) {
        Extent currentExtent1 = shape1[i];
        int newExtentL = currentExtent1.l;
        int newExtentR = currentExtent1.r;
        result[i] = Extent(newExtentL, newExtentR+backoffTo1);
        ++i;
        for (; i<depth1; i++) {
          result[i] = shape1[i];
        }
      }

      // Vice versa, if shape2 is deeper than shape1, back off to the
      // axis of shape2, and process the remaining extents of shape2.
      if (i<depth2) {
        Extent currentExtent2 = shape2[i];
        int newExtentL = currentExtent2.l;
        int newExtentR = currentExtent2.r;
        result[i] = Extent(newExtentL+backoffTo2, newExtentR);
        ++i;
        for (; i<depth2; i++) {
          result[i] = shape2[i];
        }
      }
    }
  }

  void
  VisualNode::setShape(Shape* s) {
    if (shape != s)
      Shape::deallocate(shape);
    shape = s;
    shape->computeBoundingBox();
  }

  void
  VisualNode::computeShape(const NodeAllocator& na, VisualNode* root) {
    int numberOfShapes = getNumberOfChildren();
    Extent extent;
    if (na.hasLabel(this)) {
      int ll = na.getLabel(this).length();
      ll *= 7;
      VisualNode* p = getParent(na);
      int alt = 0;
      int n_alt = 1;
      if (p) {
        alt = getAlternative(na);
        n_alt = p->getNumberOfChildren();
      }
      extent = Extent(Layout::extent);
      if (alt==0 && n_alt > 1) {
        extent.l = std::min(extent.l, -ll);
      } else if (alt==n_alt-1 && n_alt > 1) {
        extent.r = std::max(extent.r, ll);
      } else {
        extent.l = std::min(extent.l, -ll);
        extent.r = std::max(extent.r, ll);
      }
    } else {
      if (numberOfShapes==0) {
        setShape(Shape::leaf);
        return;
      } else {
        extent = Extent(Layout::extent);
      }
    }

    int maxDepth = 0;
    for (int i = numberOfShapes; i--;)
      maxDepth = std::max(maxDepth, getChild(na,i)->getShape()->depth());
    Shape* mergedShape;
    if (getShape() && getShape() != Shape::leaf &&
        getShape()->depth() >= maxDepth+1) {
      mergedShape = getShape();
      mergedShape->setDepth(maxDepth+1);
    } else {
      mergedShape = Shape::allocate(maxDepth+1);
    }
    (*mergedShape)[0] = extent;
    if (numberOfShapes < 1) {
      setShape(mergedShape);
    } else if (numberOfShapes == 1) {
      getChild(na,0)->setOffset(0);
      const Shape* childShape = getChild(na,0)->getShape();
      for (int i=childShape->depth(); i--;)
        (*mergedShape)[i+1] = (*childShape)[i];
      (*mergedShape)[1].extend(- extent.l, - extent.r);
      setShape(mergedShape);
    } else {
      // alpha stores the necessary distances between the
      // axes of the shapes in the list: alpha[i].first gives the distance
      // between shape[i] and shape[i-1], when shape[i-1] and shape[i]
      // are merged left-to-right; alpha[i].second gives the distance between
      // shape[i] and shape[i+1], when shape[i] and shape[i+1] are merged
      // right-to-left.
      assert(root->copy != NULL);
      Region r(*root->copy);
      std::pair<int,int>* alpha =
        r.alloc<std::pair<int,int> >(numberOfShapes);

      // distance between the leftmost and the rightmost axis in the list
      int width = 0;

      Extent* currentShapeL = r.alloc<Extent>(maxDepth);
      int ldepth = getChild(na,0)->getShape()->depth();
      for (int i=ldepth; i--;)
        currentShapeL[i] = (*getChild(na,0)->getShape())[i];

      // After merging, we can pick the result of either merging left or right
      // Here we chose the result of merging right
      Shape* rShape = getChild(na,numberOfShapes-1)->getShape();
      int rdepth = rShape->depth();
      for (int i=rdepth; i--;)
        (*mergedShape)[i+1] = (*rShape)[i];
      Extent* currentShapeR = &(*mergedShape)[1];

      for (int i = 1; i < numberOfShapes; i++) {
        // Merge left-to-right.  Note that due to the asymmetry of the
        // merge operation, nextAlphaL is the distance between the
        // *leftmost* axis in the shape list, and the axis of
        // nextShapeL; what we are really interested in is the distance
        // between the *previous* axis and the axis of nextShapeL.
        // This explains the correction.

        Shape* nextShapeL = getChild(na,i)->getShape();
        int nextAlphaL =
          Layouter::getAlpha<Extent*,Shape>(&currentShapeL[0], ldepth,
                             *nextShapeL, nextShapeL->depth());
        Layouter::merge<Extent*,Shape>(&currentShapeL[0],
                                       &currentShapeL[0], ldepth,
                                       *nextShapeL, nextShapeL->depth(), 
                                       nextAlphaL);
        ldepth = std::max(ldepth,nextShapeL->depth());
        alpha[i].first = nextAlphaL - width;
        width = nextAlphaL;

        // Merge right-to-left.  Here, a correction of nextAlphaR is
        // not required.
        Shape* nextShapeR = getChild(na,numberOfShapes-1-i)->getShape();
        int nextAlphaR =
          Layouter::getAlpha<Shape,Extent*>(*nextShapeR, nextShapeR->depth(),
                               &currentShapeR[0], rdepth);
        Layouter::merge<Shape,Extent*>(&currentShapeR[0],
                                       *nextShapeR, nextShapeR->depth(),
                                       &currentShapeR[0], rdepth,
                                       nextAlphaR);
        rdepth = std::max(rdepth,nextShapeR->depth());
        alpha[numberOfShapes - i].second = nextAlphaR;
      }

      // The merged shape has to be adjusted to its topmost extent
      (*mergedShape)[1].extend(- extent.l, - extent.r);

      // After the loop, the merged shape has the same axis as the
      // leftmost shape in the list.  What we want is to move the axis
      // such that it is the center of the axis of the leftmost shape in
      // the list and the axis of the rightmost shape.
      int halfWidth = false ? 0 : width / 2;
      (*mergedShape)[1].move(- halfWidth);

      // Finally, for the offset lists.  Now that the axis of the merged
      // shape is at the center of the two extreme axes, the first shape
      // needs to be offset by -halfWidth units with respect to the new
      // axis.  As for the offsets for the other shapes, we take the
      // median of the alphaL and alphaR values, as suggested in
      // Kennedy's paper.
      int offset = - halfWidth;
      getChild(na,0)->setOffset(offset);
      for (int i = 1; i < numberOfShapes; i++) {
        offset += (alpha[i].first + alpha[i].second) / 2;
        getChild(na,i)->setOffset(offset);
      }
      setShape(mergedShape);
    }
  }

}}

// STATISTICS: gist-any
