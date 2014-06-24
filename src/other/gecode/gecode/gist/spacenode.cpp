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

#include <gecode/gist/spacenode.hh>
#include <gecode/gist/visualnode.hh>
#include <gecode/gist/stopbrancher.hh>
#include <gecode/search.hh>
#include <stack>

#include <QString>
#include <QVector>

namespace Gecode { namespace Gist {

  /// \brief Representation of a branch in the search tree
  class Branch {
  public:
    /// Alternative number
    int alternative;
    /// The best space known when the branch was created
    SpaceNode* ownBest;
    const Choice* choice;

    /// Constructor
    Branch(int a, const Choice* c, SpaceNode* best = NULL)
      : alternative(a), ownBest(best) {
        choice = c;
      }
  };

  BestNode::BestNode(SpaceNode* s0) : s(s0) {}

  int
  SpaceNode::recompute(NodeAllocator& na,
                       BestNode* curBest, int c_d, int a_d) {
    int rdist = 0;

    if (copy == NULL) {
      SpaceNode* curNode = this;
      SpaceNode* lastFixpoint = NULL;

      lastFixpoint = curNode;

      std::stack<Branch> stck;

      int idx = getIndex(na);
      while (curNode->copy == NULL) {
        SpaceNode* parent = curNode->getParent(na);
        int parentIdx = curNode->getParent();
        int alternative = curNode->getAlternative(na);

        SpaceNode* ownBest = na.best(idx);
        Branch b(alternative, parent->choice,
                 curBest == NULL ? NULL : ownBest);
        stck.push(b);

        curNode = parent;
        idx = parentIdx;
        rdist++;
      }
      
      Space* curSpace;
      if (Support::marked(curNode->copy)) {
        curSpace = static_cast<Space*>(Support::unmark(curNode->copy));
        curNode->copy = NULL;
        a_d = -1;
      } else {
        curSpace = curNode->copy->clone();
        curNode->setDistance(0);
      }

      SpaceNode* lastBest = NULL;
      SpaceNode* middleNode = curNode;
      int curDist = 0;

      while (!stck.empty()) {
        if (a_d >= 0 &&
            curDist > a_d &&
            curDist == rdist / 2) {
            if (curSpace->status() == SS_FAILED) {
              copy = static_cast<Space*>(Support::mark(curSpace));
              return rdist;
            } else {
              middleNode->copy = curSpace->clone();
            }
        }
        Branch b = stck.top(); stck.pop();

        if(middleNode == lastFixpoint) {
          curSpace->status();
        }

        curSpace->commit(*b.choice, b.alternative);

        if (b.ownBest != NULL && b.ownBest != lastBest) {
          b.ownBest->acquireSpace(na,curBest, c_d, a_d);
          Space* ownBestSpace =
            static_cast<Space*>(Support::funmark(b.ownBest->copy));
          if (ownBestSpace->status() != SS_SOLVED) {
            // in the presence of weakly monotonic propagators, we may have to
            // use search to find the solution here
            ownBestSpace = Gecode::dfs(ownBestSpace);
            if (Support::marked(b.ownBest->copy)) {
              delete static_cast<Space*>(Support::unmark(b.ownBest->copy));
              b.ownBest->copy = 
                static_cast<Space*>(Support::mark(ownBestSpace));
            } else {
              delete b.ownBest->copy;
              b.ownBest->copy = ownBestSpace;
            }
          }
          curSpace->constrain(*ownBestSpace);
          lastBest = b.ownBest;
        }
        curDist++;
        middleNode = middleNode->getChild(na,b.alternative);
        middleNode->setDistance(curDist);
      }
      copy = static_cast<Space*>(Support::mark(curSpace));

    }
    return rdist;
  }

  void
  SpaceNode::acquireSpace(NodeAllocator& na,
                          BestNode* curBest, int c_d, int a_d) {
    SpaceNode* p = getParent(na);
    int parentIdx = getParent();
    int idx = getIndex(na);

    if (getStatus() == UNDETERMINED && curBest != NULL &&
        na.best(idx) == NULL &&
        p != NULL && curBest->s != na.best(parentIdx)) {
      na.setBest(idx, curBest->s->getIndex(na));
    }
    SpaceNode* ownBest = na.best(idx);

    if (copy == NULL && p != NULL && p->copy != NULL && 
        Support::marked(p->copy)) {
      // If parent has a working space, steal it
      copy = p->copy;
      p->copy = NULL;
      if (p->choice != NULL)
        static_cast<Space*>(Support::unmark(copy))->
          commit(*p->choice, getAlternative(na));

      if (ownBest != NULL) {
        ownBest->acquireSpace(na,curBest, c_d, a_d);
        Space* ownBestSpace = 
          static_cast<Space*>(Support::funmark(ownBest->copy));
        if (ownBestSpace->status() != SS_SOLVED) {
          // in the presence of weakly monotonic propagators, we may have to
          // use search to find the solution here

          ownBestSpace = Gecode::dfs(ownBestSpace);
          if (Support::marked(ownBest->copy)) {
            delete static_cast<Space*>(Support::unmark(ownBest->copy));
            ownBest->copy =
              static_cast<Space*>(Support::mark(ownBestSpace));
          } else {
            delete ownBest->copy;
            ownBest->copy = ownBestSpace;
          }
        }
        static_cast<Space*>(Support::unmark(copy))->constrain(*ownBestSpace);
      }
      int d = p->getDistance()+1;
      if (d > c_d && c_d >= 0 &&
          static_cast<Space*>(Support::unmark(copy))->status() == SS_BRANCH) {
        copy = static_cast<Space*>(Support::unmark(copy));
        d = 0;
      }
      setDistance(d);
    }

    if (copy == NULL) {
      if (recompute(na, curBest, c_d, a_d) > c_d && c_d >= 0 &&
          static_cast<Space*>(Support::unmark(copy))->status() == SS_BRANCH) {
        copy = static_cast<Space*>(Support::unmark(copy));
        setDistance(0);
      }
    }

    // always return a fixpoint
    static_cast<Space*>(Support::funmark(copy))->status();
    if (Support::marked(copy) && p != NULL && isOpen() &&
        p->copy != NULL && p->getNoOfOpenChildren(na) == 1 &&
        !p->isRoot()) {
      // last alternative optimization

      copy = static_cast<Space*>(Support::unmark(copy));
      delete static_cast<Space*>(Support::funmark(p->copy));
      p->copy = NULL;
      setDistance(0);
      p->setDistance(p->getParent(na)->getDistance()+1);
    }
  }

  void
  SpaceNode::closeChild(const NodeAllocator& na,
                        bool hadFailures, bool hadSolutions) {
    setHasFailedChildren(hasFailedChildren() || hadFailures);
    setHasSolvedChildren(hasSolvedChildren() || hadSolutions);

    bool allClosed = true;
    for (int i=getNumberOfChildren(); i--;) {
      if (getChild(na,i)->isOpen()) {
        allClosed = false;
        break;
      }
    }

    if (allClosed) {
      setHasOpenChildren(false);
      for (unsigned int i=0; i<getNumberOfChildren(); i++)
        setHasSolvedChildren(hasSolvedChildren() ||
          getChild(na,i)->hasSolvedChildren());
      SpaceNode* p = getParent(na);
      if (p != NULL) {
        delete static_cast<Space*>(Support::funmark(copy));
        copy = NULL;
        p->closeChild(na, hasFailedChildren(), hasSolvedChildren());
      }
    } else {
      
      if (hadSolutions) {
        setHasSolvedChildren(true);
        SpaceNode* p = getParent(na);
        while (p != NULL && !p->hasSolvedChildren()) {
          p->setHasSolvedChildren(true);
          p = p->getParent(na);
        }
      }
      if (hadFailures) {
        SpaceNode* p = getParent(na);
        while (p != NULL && !p->hasFailedChildren()) {
          p->setHasFailedChildren(true);
          p = p->getParent(na);
        }        
      }
    }

  }

  SpaceNode::SpaceNode(Space* root)
  : Node(-1, root==NULL),
    copy(root), choice(NULL), nstatus(0) {
    if (root == NULL) {
      setStatus(FAILED);
      setHasSolvedChildren(false);
      setHasFailedChildren(true);
    } else {
      setStatus(UNDETERMINED);
      setHasSolvedChildren(false);
      setHasFailedChildren(false);
    }
  }

  void
  SpaceNode::dispose(void) {
    delete choice;
    delete static_cast<Space*>(Support::funmark(copy));
  }


  int
  SpaceNode::getNumberOfChildNodes(NodeAllocator& na,
                                   BestNode* curBest, Statistics& stats,
                                   int c_d, int a_d) {
    int kids = 0;
    if (isUndetermined()) {
      stats.undetermined--;
      acquireSpace(na, curBest, c_d, a_d);
      QVector<QString> labels;
      switch (static_cast<Space*>(Support::funmark(copy))->status(stats)) {
      case SS_FAILED:
        {
          purge(na);
          kids = 0;
          setHasOpenChildren(false);
          setHasSolvedChildren(false);
          setHasFailedChildren(true);
          setStatus(FAILED);
          stats.failures++;
          SpaceNode* p = getParent(na);
          if (p != NULL)
            p->closeChild(na, true, false);
        }
        break;
      case SS_SOLVED:
        {
          // Deletes all pending branchers
          (void) static_cast<Space*>(Support::funmark(copy))->choice();
          kids = 0;
          setStatus(SOLVED);
          setHasOpenChildren(false);
          setHasSolvedChildren(true);
          setHasFailedChildren(false);
          stats.solutions++;
          if (curBest != NULL) {
            curBest->s = this;
          }
          SpaceNode* p = getParent(na);
          if (p != NULL)
            p->closeChild(na, false, true);
        }
        break;
      case SS_BRANCH:
        {
          Space* s = static_cast<Space*>(Support::funmark(copy));
          choice = s->choice();
          kids = choice->alternatives();
          setHasOpenChildren(true);
          if (dynamic_cast<const StopChoice*>(choice)) {
            setStatus(STOP);
          } else {
            setStatus(BRANCH);
            stats.choices++;
          }
          stats.undetermined += kids;
          for (int i=0; i<kids; i++) {
            std::ostringstream oss;
            s->print(*choice,i,oss);
            labels.push_back(QString(oss.str().c_str()));
          }
        }
        break;
      }
      static_cast<VisualNode*>(this)->changedStatus(na);
      setNumberOfChildren(kids, na);
    } else {
      kids = getNumberOfChildren();
    }
    return kids;
  }

  int
  SpaceNode::getNoOfOpenChildren(const NodeAllocator& na) {
    if (!hasOpenChildren())
      return 0;
    int noOfOpenChildren = 0;
    for (int i=getNumberOfChildren(); i--;)
      noOfOpenChildren += (getChild(na,i)->isOpen());
    return noOfOpenChildren;
  }

}}

// STATISTICS: gist-any
