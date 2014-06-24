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

  forceinline void
  SpaceNode::setFlag(int flag, bool value) {
    if (value)
      nstatus |= 1<<(flag-1);
    else
      nstatus &= ~(1<<(flag-1));
  }

  forceinline bool
  SpaceNode::getFlag(int flag) const {
    return (nstatus & (1<<(flag-1))) != 0;
  }

  forceinline void
  SpaceNode::setHasOpenChildren(bool b) {
    setFlag(HASOPENCHILDREN, b);
  }

  forceinline void
  SpaceNode::setHasFailedChildren(bool b) {
    setFlag(HASFAILEDCHILDREN, b);
  }

  forceinline void
  SpaceNode::setHasSolvedChildren(bool b) {
    setFlag(HASSOLVEDCHILDREN, b);
  }

  forceinline void
  SpaceNode::setStatus(NodeStatus s) {
    nstatus &= ~( STATUSMASK );
    nstatus |= s << 20;
  }

  forceinline NodeStatus
  SpaceNode::getStatus(void) const {
    return static_cast<NodeStatus>((nstatus & STATUSMASK) >> 20);
  }

  forceinline void
  SpaceNode::setDistance(unsigned int d) {
    if (d > MAXDISTANCE)
      d = MAXDISTANCE;
    nstatus &= ~( DISTANCEMASK );
    nstatus |= d;
  }
  
  forceinline unsigned int
  SpaceNode::getDistance(void) const {
    return nstatus & DISTANCEMASK;
  }

  forceinline
  SpaceNode::SpaceNode(int p)
  : Node(p), copy(NULL), nstatus(0) {
    choice = NULL;
    setStatus(UNDETERMINED);
    setHasSolvedChildren(false);
    setHasFailedChildren(false);
  }

  forceinline Space*
  SpaceNode::getSpace(NodeAllocator& na,
                      BestNode* curBest, int c_d, int a_d) {
    acquireSpace(na,curBest,c_d,a_d);
    Space* ret;
    if (Support::marked(copy)) {
      ret = static_cast<Space*>(Support::unmark(copy));
      copy = NULL;
    } else {
      ret = copy->clone();
    }
    return ret;
  }

  forceinline const Space*
  SpaceNode::getWorkingSpace(void) const {
    assert(copy != NULL);
    if (Support::marked(copy))
      return static_cast<Space*>(Support::unmark(copy));
    return copy;
  }

  forceinline void
  SpaceNode::purge(const NodeAllocator& na) {
    if (!isRoot() && (getStatus() != SOLVED || !na.bab())) {
      // only delete copies from solutions if we are not in BAB
      if (Support::marked(copy))
        delete static_cast<Space*>(Support::unmark(copy));
      else
        delete copy;
      copy = NULL;
    }
  }


  forceinline bool
  SpaceNode::isCurrentBest(BestNode* curBest) {
    return curBest != NULL && curBest->s == this;
  }

  forceinline bool
  SpaceNode::isOpen(void) {
    return ((getStatus() == UNDETERMINED) ||
            getFlag(HASOPENCHILDREN));
  }

  forceinline bool
  SpaceNode::hasFailedChildren(void) {
    return getFlag(HASFAILEDCHILDREN);
  }

  forceinline bool
  SpaceNode::hasSolvedChildren(void) {
    return getFlag(HASSOLVEDCHILDREN);
  }

  forceinline bool
  SpaceNode::hasOpenChildren(void) {
    return getFlag(HASOPENCHILDREN);
  }

  forceinline bool
  SpaceNode::hasCopy(void) {
    return copy != NULL;
  }

  forceinline bool
  SpaceNode::hasWorkingSpace(void) {
    return copy != NULL && Support::marked(copy);
  }

  forceinline int
  SpaceNode::getAlternative(const NodeAllocator& na) const {
    SpaceNode* p = getParent(na);
    if (p == NULL)
      return -1;
    for (int i=p->getNumberOfChildren(); i--;)
      if (p->getChild(na,i) == this)
        return i;
    GECODE_NEVER;
    return -1;
  }

  forceinline const Choice*
  SpaceNode::getChoice(void) {
    return choice;
  }

}}

// STATISTICS: gist-any
