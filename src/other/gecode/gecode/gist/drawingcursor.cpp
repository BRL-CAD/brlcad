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

#include <gecode/gist/drawingcursor.hh>

namespace Gecode { namespace Gist {

  /// Red color for failed nodes
  const QColor DrawingCursor::red(218, 37, 29);
  /// Green color for solved nodes
  const QColor DrawingCursor::green(11, 118, 70);
  /// Blue color for choice nodes
  const QColor DrawingCursor::blue(0, 92, 161);
  /// Orange color for best solutions
  const QColor DrawingCursor::orange(235, 137, 27);
  /// White color
  const QColor DrawingCursor::white(255,255,255);

  /// Red color for expanded failed nodes
  const QColor DrawingCursor::lightRed(218, 37, 29, 120);
  /// Green color for expanded solved nodes
  const QColor DrawingCursor::lightGreen(11, 118, 70, 120);
  /// Blue color for expanded choice nodes
  const QColor DrawingCursor::lightBlue(0, 92, 161, 120);

  const double nodeWidth = 20.0;
  const double halfNodeWidth = nodeWidth / 2.0;
  const double quarterNodeWidth = halfNodeWidth / 2.0;
  const double failedWidth = 14.0;
  const double halfFailedWidth = failedWidth / 2.0;
  const double quarterFailedWidthF = failedWidth / 4.0;
  const double shadowOffset = 3.0;
  const double hiddenDepth =
    static_cast<double>(Layout::dist_y) + failedWidth;

  DrawingCursor::DrawingCursor(VisualNode* root,
                               const VisualNode::NodeAllocator& na,
                               BestNode* curBest0,
                               QPainter& painter0,
                               const QRect& clippingRect0, bool showCopies)
    : NodeCursor<VisualNode>(root,na), painter(painter0),
      clippingRect(clippingRect0), curBest(curBest0),
      x(0.0), y(0.0), copies(showCopies) {
    QPen pen = painter.pen();
    pen.setWidth(1);
    painter.setPen(pen);
  }

  void
  DrawingCursor::processCurrentNode(void) {
    Gist::VisualNode* n = node();
    double parentX = x - static_cast<double>(n->getOffset());
    double parentY = y - static_cast<double>(Layout::dist_y) + nodeWidth;
    if (!n->isRoot() &&
        (n->getParent(na)->getStatus() == STOP ||
         n->getParent(na)->getStatus() == UNSTOP) )
      parentY -= (nodeWidth-failedWidth)/2;

    double myx = x;
    double myy = y;

    if (n->getStatus() == STOP || n->getStatus() == UNSTOP)
      myy += (nodeWidth-failedWidth)/2;

    if (n != startNode()) {
      if (n->isOnPath())
        painter.setPen(Qt::red);
      else
        painter.setPen(Qt::black);
      // Here we use drawPath instead of drawLine in order to
      // workaround a strange redraw artefact on Windows
      QPainterPath path;
      path.moveTo(myx,myy);
      path.lineTo(parentX,parentY);
      painter.drawPath(path);
      
      QFontMetrics fm = painter.fontMetrics();
      QString label = na.getLabel(n);
      int alt = n->getAlternative(na);
      int n_alt = n->getParent(na)->getNumberOfChildren();
      int tw = fm.width(label);
      int lx;
      if (alt==0 && n_alt > 1) {
        lx = myx-tw-4;
      } else if (alt==n_alt-1 && n_alt > 1) {
        lx = myx+4;
      } else {
        lx = myx-tw/2;
      }
      painter.drawText(QPointF(lx,myy-2),label);
    }

    // draw shadow
    if (n->isMarked()) {
      painter.setBrush(Qt::gray);
      painter.setPen(Qt::NoPen);
      if (n->isHidden()) {
        QPointF points[3] = {QPointF(myx+shadowOffset,myy+shadowOffset),
                             QPointF(myx+nodeWidth+shadowOffset,
                                     myy+hiddenDepth+shadowOffset),
                             QPointF(myx-nodeWidth+shadowOffset,
                                     myy+hiddenDepth+shadowOffset),
                            };
        painter.drawConvexPolygon(points, 3);

      } else {
        switch (n->getStatus()) {
        case Gist::SOLVED:
          {
            QPointF points[4] = {QPointF(myx+shadowOffset,myy+shadowOffset),
                                 QPointF(myx+halfNodeWidth+shadowOffset,
                                         myy+halfNodeWidth+shadowOffset),
                                 QPointF(myx+shadowOffset,
                                         myy+nodeWidth+shadowOffset),
                                 QPointF(myx-halfNodeWidth+shadowOffset,
                                         myy+halfNodeWidth+shadowOffset)
                                };
            painter.drawConvexPolygon(points, 4);
          }
          break;
        case Gist::FAILED:
          painter.drawRect(myx-halfFailedWidth+shadowOffset,
                           myy+shadowOffset, failedWidth, failedWidth);
          break;
        case Gist::UNSTOP:
        case Gist::STOP:
          {
            QPointF points[8] = {QPointF(myx+shadowOffset-quarterFailedWidthF,
                                         myy+shadowOffset),
                                 QPointF(myx+shadowOffset+quarterFailedWidthF,
                                         myy+shadowOffset),
                                 QPointF(myx+shadowOffset+halfFailedWidth,
                                         myy+shadowOffset
                                            +quarterFailedWidthF),
                                 QPointF(myx+shadowOffset+halfFailedWidth,
                                         myy+shadowOffset+halfFailedWidth+
                                             quarterFailedWidthF),
                                 QPointF(myx+shadowOffset+quarterFailedWidthF,
                                         myy+shadowOffset+failedWidth),
                                 QPointF(myx+shadowOffset-quarterFailedWidthF,
                                         myy+shadowOffset+failedWidth),
                                 QPointF(myx+shadowOffset-halfFailedWidth,
                                         myy+shadowOffset+halfFailedWidth+
                                             quarterFailedWidthF),
                                 QPointF(myx+shadowOffset-halfFailedWidth,
                                         myy+shadowOffset
                                            +quarterFailedWidthF),
                                };
            painter.drawConvexPolygon(points, 8);
          }
          break;
        case Gist::BRANCH:
          painter.drawEllipse(myx-halfNodeWidth+shadowOffset,
                              myy+shadowOffset, nodeWidth, nodeWidth);
          break;
        case Gist::UNDETERMINED:
          painter.drawEllipse(myx-halfNodeWidth+shadowOffset,
                              myy+shadowOffset, nodeWidth, nodeWidth);
          break;
        }
      }
    }

    painter.setPen(Qt::SolidLine);
    if (n->isHidden()) {
      if (n->hasOpenChildren()) {
        QLinearGradient gradient(myx-nodeWidth,myy,
                                 myx+nodeWidth*1.3,myy+hiddenDepth*1.3);
        if (n->hasSolvedChildren()) {
          gradient.setColorAt(0, white);
          gradient.setColorAt(1, green);
        } else if (n->hasFailedChildren()) {
          gradient.setColorAt(0, white);
          gradient.setColorAt(1, red);          
        } else {
          gradient.setColorAt(0, white);
          gradient.setColorAt(1, QColor(0,0,0));
        }
        painter.setBrush(gradient);
      } else {
        if (n->hasSolvedChildren())
          painter.setBrush(QBrush(green));
        else
          painter.setBrush(QBrush(red));
      }
      
      QPointF points[3] = {QPointF(myx,myy),
                           QPointF(myx+nodeWidth,myy+hiddenDepth),
                           QPointF(myx-nodeWidth,myy+hiddenDepth),
                          };
      painter.drawConvexPolygon(points, 3);
    } else {
      switch (n->getStatus()) {
      case Gist::SOLVED:
        {
          if (n->isCurrentBest(curBest)) {
            painter.setBrush(QBrush(orange));
          } else {
            painter.setBrush(QBrush(green));
          }
          QPointF points[4] = {QPointF(myx,myy),
                               QPointF(myx+halfNodeWidth,myy+halfNodeWidth),
                               QPointF(myx,myy+nodeWidth),
                               QPointF(myx-halfNodeWidth,myy+halfNodeWidth)
                              };
          painter.drawConvexPolygon(points, 4);
        }
        break;
      case Gist::FAILED:
        painter.setBrush(QBrush(red));
        painter.drawRect(myx-halfFailedWidth, myy, failedWidth, failedWidth);
        break;
      case Gist::UNSTOP:
      case Gist::STOP:
        {
          painter.setBrush(n->getStatus() == STOP ? 
                           QBrush(red) : QBrush(green));
          QPointF points[8] = {QPointF(myx-quarterFailedWidthF,myy),
                               QPointF(myx+quarterFailedWidthF,myy),
                               QPointF(myx+halfFailedWidth,
                                       myy+quarterFailedWidthF),
                               QPointF(myx+halfFailedWidth,
                                       myy+halfFailedWidth+
                                           quarterFailedWidthF),
                               QPointF(myx+quarterFailedWidthF,
                                       myy+failedWidth),
                               QPointF(myx-quarterFailedWidthF,
                                       myy+failedWidth),
                               QPointF(myx-halfFailedWidth,
                                       myy+halfFailedWidth+
                                           quarterFailedWidthF),
                               QPointF(myx-halfFailedWidth,
                                       myy+quarterFailedWidthF),
                              };
          painter.drawConvexPolygon(points, 8);
        }
        break;
      case Gist::BRANCH:
        painter.setBrush(n->childrenLayoutIsDone() ? QBrush(blue) : 
                                                     QBrush(white));
        painter.drawEllipse(myx-halfNodeWidth, myy, nodeWidth, nodeWidth);
        break;
      case Gist::UNDETERMINED:
        painter.setBrush(Qt::white);
        painter.drawEllipse(myx-halfNodeWidth, myy, nodeWidth, nodeWidth);
        break;
      }
    }

    if (copies && (n->hasCopy() && !n->hasWorkingSpace())) {
     painter.setBrush(Qt::darkRed);
     painter.drawEllipse(myx, myy, 10.0, 10.0);
    }

    if (copies && n->hasWorkingSpace()) {
     painter.setBrush(Qt::darkYellow);
     painter.drawEllipse(myx, myy + 10.0, 10.0, 10.0);
    }

    if (n->isBookmarked()) {
     painter.setBrush(Qt::black);
     painter.drawEllipse(myx-10-0, myy, 10.0, 10.0);
    }

  }

}}

// STATISTICS: gist-any
