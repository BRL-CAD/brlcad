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

  template<class Node>
  forceinline
  NodeCursor<Node>::NodeCursor(Node* theNode,
                               const typename Node::NodeAllocator& na0)
   : _startNode(theNode), _node(theNode), 
     _alternative(theNode->getAlternative(na0)),
     na(na0) {}

  template<class Node>
  forceinline Node*
  NodeCursor<Node>::node(void) { return _node; }

  template<class Node>
  forceinline unsigned int
  NodeCursor<Node>::alternative(void) { return _alternative; }

  template<class Node>
  forceinline void
  NodeCursor<Node>::alternative(unsigned int a) { _alternative=a; }

  template<class Node>
  forceinline Node*
  NodeCursor<Node>::startNode(void) { return _startNode; }

  template<class Node>
  forceinline void
  NodeCursor<Node>::node(Node* n) { _node = n; }

  template<class Node>
  forceinline bool
  NodeCursor<Node>::mayMoveUpwards(void) {
    return _node != _startNode && !_node->isRoot();
  }

  template<class Node>
  forceinline void
  NodeCursor<Node>::moveUpwards(void) {
    _node = static_cast<Node*>(_node->getParent(na));
    if (_node->isRoot()) {
      _alternative = 0;
    } else {
      Node* p = static_cast<Node*>(_node->getParent(na));
      for (int i=p->getNumberOfChildren(); i--;) {
        if (p->getChild(na,i) == _node) {
          _alternative = i;
          break;
        }
      }
    }
  }

  template<class Node>
  forceinline bool
  NodeCursor<Node>::mayMoveDownwards(void) {
    return _node->getNumberOfChildren() > 0;
  }

  template<class Node>
  forceinline void
  NodeCursor<Node>::moveDownwards(void) {
    _alternative = 0;
    _node = _node->getChild(na,0);
  }

  template<class Node>
  forceinline bool
  NodeCursor<Node>::mayMoveSidewards(void) {
    return (!_node->isRoot()) && (_node != _startNode) &&
      (_alternative < _node->getParent(na)->getNumberOfChildren() - 1);
  }

  template<class Node>
  forceinline void
  NodeCursor<Node>::moveSidewards(void) {
    _node = 
      static_cast<Node*>(_node->getParent(na)->getChild(na,++_alternative));
  }

  forceinline bool
  HideFailedCursor::mayMoveDownwards(void) {
    VisualNode* n = node();
    return (!onlyDirty || n->isDirty()) &&
           NodeCursor<VisualNode>::mayMoveDownwards() &&
           (n->hasSolvedChildren() || n->getNoOfOpenChildren(na) > 0) &&
           (! n->isHidden());
  }

  forceinline
  HideFailedCursor::HideFailedCursor(VisualNode* root,
                                     const VisualNode::NodeAllocator& na,
                                     bool onlyDirtyNodes)
   : NodeCursor<VisualNode>(root,na), onlyDirty(onlyDirtyNodes) {}

  forceinline void
  HideFailedCursor::processCurrentNode(void) {
    VisualNode* n = node();
    if (n->getStatus() == BRANCH &&
        !n->hasSolvedChildren() &&
        n->getNoOfOpenChildren(na) == 0) {
      n->setHidden(true);
      n->setChildrenLayoutDone(false);
      n->dirtyUp(na);
    }
  }

  forceinline
  UnhideAllCursor::UnhideAllCursor(VisualNode* root,
                                   const VisualNode::NodeAllocator& na)
   : NodeCursor<VisualNode>(root,na) {}

  forceinline void
  UnhideAllCursor::processCurrentNode(void) {
    VisualNode* n = node();
    if (n->isHidden()) {
      n->setHidden(false);
      n->dirtyUp(na);
    }
  }

  forceinline
  UnstopAllCursor::UnstopAllCursor(VisualNode* root,
                                   const VisualNode::NodeAllocator& na)
   : NodeCursor<VisualNode>(root,na) {}

  forceinline void
  UnstopAllCursor::processCurrentNode(void) {
    VisualNode* n = node();
    if (n->getStatus() == STOP) {
      n->setStop(false);
      n->dirtyUp(na);
    }
  }

  forceinline
  NextSolCursor::NextSolCursor(VisualNode* theNode, bool backwards,
                               const VisualNode::NodeAllocator& na)
   : NodeCursor<VisualNode>(theNode,na), back(backwards) {}

  forceinline void
  NextSolCursor::processCurrentNode(void) {}

  forceinline bool
  NextSolCursor::notOnSol(void) {
    return node() == startNode() || node()->getStatus() != SOLVED;
  }

  forceinline bool
  NextSolCursor::mayMoveUpwards(void) {
    return notOnSol() && !node()->isRoot();
  }

  forceinline bool
  NextSolCursor::mayMoveDownwards(void) {
    return notOnSol() && !(back && node() == startNode())
           && node()->hasSolvedChildren()
           && NodeCursor<VisualNode>::mayMoveDownwards();
  }

  forceinline void
  NextSolCursor::moveDownwards(void) {
    NodeCursor<VisualNode>::moveDownwards();
    if (back) {
      while (NodeCursor<VisualNode>::mayMoveSidewards())
        NodeCursor<VisualNode>::moveSidewards();
    }
  }

  forceinline bool
  NextSolCursor::mayMoveSidewards(void) {
    if (back) {
      return notOnSol() && !node()->isRoot() && alternative() > 0;
    } else {
      return notOnSol() && !node()->isRoot() &&
             (alternative() <
              node()->getParent(na)->getNumberOfChildren() - 1);
    }
  }

  forceinline void
  NextSolCursor::moveSidewards(void) {
    if (back) {
      alternative(alternative()-1);
      node(node()->getParent(na)->getChild(na,alternative()));
    } else {
      NodeCursor<VisualNode>::moveSidewards();
    }
  }

  forceinline
  StatCursor::StatCursor(VisualNode* root,
                         const VisualNode::NodeAllocator& na)
   : NodeCursor<VisualNode>(root,na),
     curDepth(0), depth(0), failed(0), solved(0), choice(0), open(0) {}

  forceinline void
  StatCursor::processCurrentNode(void) {
    VisualNode* n = node();
    switch (n->getStatus()) {
    case SOLVED: solved++; break;
    case FAILED: failed++; break;
    case BRANCH: choice++; break;
    case UNDETERMINED: open++; break;
    default: break;
    }
  }

  forceinline void
  StatCursor::moveDownwards(void) {
    curDepth++;
    depth = std::max(depth,curDepth); 
    NodeCursor<VisualNode>::moveDownwards();
  }

  forceinline void
  StatCursor::moveUpwards(void) {
    curDepth--;
    NodeCursor<VisualNode>::moveUpwards();
  }

  forceinline
  BranchLabelCursor::BranchLabelCursor(VisualNode* root, BestNode* curBest,
                                       int c_d, int a_d, bool clear,
                                       VisualNode::NodeAllocator& na)
    : NodeCursor<VisualNode>(root,na), _na(na), _curBest(curBest),
      _c_d(c_d), _a_d(a_d), _clear(clear) {}

  forceinline void
  BranchLabelCursor::processCurrentNode(void) {
    VisualNode* n = node();
    if (!_clear) {
      if (!na.hasLabel(n)) {
        VisualNode* p = n->getParent(_na);
        if (p) {
          std::string l =
            n->getBranchLabel(_na,p,p->getChoice(),
              _curBest,_c_d,_a_d,alternative());
          _na.setLabel(n,QString(l.c_str()));
          if (n->getNumberOfChildren() < 1 &&
              alternative() == p->getNumberOfChildren()-1)
            p->purge(_na);
        } else {
          _na.setLabel(n,"");
        }
      }
    } else {
      _na.clearLabel(n);
    }
    n->dirtyUp(na);
  }

  forceinline
  DisposeCursor::DisposeCursor(VisualNode* root,
                               const VisualNode::NodeAllocator& na)
   : NodeCursor<VisualNode>(root,na) {}

  forceinline void
  DisposeCursor::processCurrentNode(void) {
    node()->dispose();
  }

}}

// STATISTICS: gist-any
