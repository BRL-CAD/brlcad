/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
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

#ifndef __GECODE_SEARCH_SEQUENTIAL_DFS_HH__
#define __GECODE_SEARCH_SEQUENTIAL_DFS_HH__

#include <gecode/search.hh>
#include <gecode/search/support.hh>
#include <gecode/search/worker.hh>
#include <gecode/search/sequential/path.hh>

namespace Gecode { namespace Search { namespace Sequential {

  /// Depth-first search engine implementation
  class DFS : public Worker {
  private:
    /// Search options
    Options opt;
    /// Current path ins search tree
    Path path;
    /// Current space being explored
    Space* cur;
    /// Distance until next clone
    unsigned int d;
  public:
    /// Initialize for space \a s with options \a o
    DFS(Space* s, const Options& o);
    /// %Search for next solution
    Space* next(void);
    /// Return statistics
    Statistics statistics(void) const;
    /// Reset engine to restart at space \a s
    void reset(Space* s);
    /// Return no-goods
    NoGoods& nogoods(void);
    /// Destructor
    ~DFS(void);
  };

  forceinline 
  DFS::DFS(Space* s, const Options& o)
    : opt(o), path(static_cast<int>(opt.nogoods_limit)), d(0) {
    if ((s == NULL) || (s->status(*this) == SS_FAILED)) {
      fail++;
      cur = NULL;
      if (!opt.clone)
        delete s;
    } else {
      cur = snapshot(s,opt);
    }
  }

  forceinline void
  DFS::reset(Space* s) {
    delete cur;
    path.reset();
    d = 0;
    if ((s == NULL) || (s->status(*this) == SS_FAILED)) {
      cur = NULL;
    } else {
      cur = s;
    }
    Worker::reset();
  }

  forceinline NoGoods&
  DFS::nogoods(void) {
    return path;
  }

  forceinline Space*
  DFS::next(void) {
    start();
    while (true) {
      while (cur) {
        if (stop(opt))
          return NULL;
        node++;
        switch (cur->status(*this)) {
        case SS_FAILED:
          fail++;
          delete cur;
          cur = NULL;
          break;
        case SS_SOLVED:
          {
            // Deletes all pending branchers
            (void) cur->choice();
            Space* s = cur;
            cur = NULL;
            return s;
          }
        case SS_BRANCH:
          {
            Space* c;
            if ((d == 0) || (d >= opt.c_d)) {
              c = cur->clone();
              d = 1;
            } else {
              c = NULL;
              d++;
            }
            const Choice* ch = path.push(*this,cur,c);
            cur->commit(*ch,0);
            break;
          }
        default:
          GECODE_NEVER;
        }
      }
      do {
        if (!path.next())
          return NULL;
        cur = path.recompute(d,opt.a_d,*this);
      } while (cur == NULL);
    }
    GECODE_NEVER;
    return NULL;
  }

  forceinline Statistics
  DFS::statistics(void) const {
    return *this;
  }

  forceinline 
  DFS::~DFS(void) {
    delete cur;
    path.reset();
  }

}}}

#endif

// STATISTICS: search-sequential
