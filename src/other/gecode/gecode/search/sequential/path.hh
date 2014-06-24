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

#ifndef __GECODE_SEARCH_SEQUENTIAL_PATH_HH__
#define __GECODE_SEARCH_SEQUENTIAL_PATH_HH__

#include <gecode/search.hh>
#include <gecode/search/support.hh>
#include <gecode/search/worker.hh>
#include <gecode/search/meta/nogoods.hh>

namespace Gecode { namespace Search { namespace Sequential {

  /**
   * \brief Depth-first path (stack of edges) supporting recomputation
   *
   * Maintains the invariant that it contains
   * the path of the space being currently explored. This
   * is required to support recomputation, of course.
   *
   * The path supports adaptive recomputation controlled
   * by the value of a_d: only if the recomputation
   * distance is at least this large, an additional
   * clone is created.
   *
   */
  class Path : public NoGoods {
    friend class Search::Meta::NoGoodsProp;
  public:
    /// %Search tree edge for recomputation
    class Edge {
    protected:
      /// Space corresponding to this edge (might be NULL)
      Space* _space;
      /// Current alternative
      unsigned int _alt;
      /// Choice
      const Choice* _choice;
    public:
      /// Default constructor
      Edge(void);
      /// Edge for space \a s with clone \a c (possibly NULL)
      Edge(Space* s, Space* c);
      
      /// Return space for edge
      Space* space(void) const;
      /// Set space to \a s
      void space(Space* s);
      
      /// Return choice
      const Choice* choice(void) const;
      
      /// Return number for alternatives
      unsigned int alt(void) const;
      /// Return true number for alternatives (excluding lao optimization)
      unsigned int truealt(void) const;
      /// Test whether current alternative is leftmost
      bool leftmost(void) const;
      /// Test whether current alternative is rightmost
      bool rightmost(void) const;
      /// Move to next alternative
      void next(void);
      /// Test whether current alternative was LAO
      bool lao(void) const;
      
      /// Free memory for edge
      void dispose(void);
    };
  protected:
    /// Stack to store edge information
    Support::DynamicStack<Edge,Heap> ds;
    /// Depth limit for no-good generation
    int _ngdl;
  public:
    /// Initialize with no-good depth limit \a l
    Path(int l);
    /// Return no-good depth limit
    int ngdl(void) const;
    /// Set no-good depth limit to \a l
    void ngdl(int l);
    /// Push space \a c (a clone of \a s or NULL)
    const Choice* push(Worker& stat, Space* s, Space* c);
    /// Generate path for next node and return whether a next node exists
    bool next(void);
    /// Provide access to topmost edge
    Edge& top(void) const;
    /// Test whether path is empty
    bool empty(void) const;
    /// Return position on stack of last copy
    int lc(void) const;
    /// Unwind the stack up to position \a l (after failure)
    void unwind(int l);
    /// Commit space \a s as described by stack entry at position \a i
    void commit(Space* s, int i) const;
    /// Recompute space according to path 
    Space* recompute(unsigned int& d, unsigned int a_d, Worker& s);
    /// Recompute space according to path
    Space* recompute(unsigned int& d, unsigned int a_d, Worker& s,
                     const Space* best, int& mark);
    /// Return number of entries on stack
    int entries(void) const;
    /// Reset stack
    void reset(void);
    /// Post no-goods
    void virtual post(Space& home);
  };


  /*
   * Edge for recomputation
   *
   */
  forceinline
  Path::Edge::Edge(void) {}

  forceinline
  Path::Edge::Edge(Space* s, Space* c)
    : _space(c), _alt(0), _choice(s->choice()) {}

  forceinline Space*
  Path::Edge::space(void) const {
    return _space;
  }
  forceinline void
  Path::Edge::space(Space* s) {
    _space = s;
  }

  forceinline unsigned int
  Path::Edge::alt(void) const {
    return _alt;
  }
  forceinline unsigned int
  Path::Edge::truealt(void) const {
    assert(_alt < _choice->alternatives());
    return _alt;
  }
  forceinline bool
  Path::Edge::leftmost(void) const {
    return _alt == 0;
  }
  forceinline bool
  Path::Edge::rightmost(void) const {
    return _alt+1 >= _choice->alternatives();
  }
  forceinline bool
  Path::Edge::lao(void) const {
    return _alt >= _choice->alternatives();
  }
  forceinline void
  Path::Edge::next(void) {
    _alt++;
  }

  forceinline const Choice*
  Path::Edge::choice(void) const {
    return _choice;
  }

  forceinline void
  Path::Edge::dispose(void) {
    delete _space;
    delete _choice;
  }



  /*
   * Depth-first stack with recomputation
   *
   */

  forceinline
  Path::Path(int l) 
    : ds(heap), _ngdl(l) {}

  forceinline int
  Path::ngdl(void) const {
    return _ngdl;
  }

  forceinline void
  Path::ngdl(int l) {
    _ngdl = l;
  }

  forceinline const Choice*
  Path::push(Worker& stat, Space* s, Space* c) {
    if (!ds.empty() && ds.top().lao()) {
      // Topmost stack entry was LAO -> reuse
      ds.pop().dispose();
    }
    Edge sn(s,c);
    ds.push(sn);
    stat.stack_depth(static_cast<unsigned long int>(ds.entries()));
    return sn.choice();
  }

  forceinline bool
  Path::next(void) {
    while (!ds.empty())
      if (ds.top().rightmost()) {
        ds.pop().dispose();
      } else {
        ds.top().next();
        return true;
      }
    return false;
  }

  forceinline Path::Edge&
  Path::top(void) const {
    assert(!ds.empty());
    return ds.top();
  }

  forceinline bool
  Path::empty(void) const {
    return ds.empty();
  }

  forceinline void
  Path::commit(Space* s, int i) const {
    const Edge& n = ds[i];
    s->commit(*n.choice(),n.alt());
  }

  forceinline int
  Path::lc(void) const {
    int l = ds.entries()-1;
    while (ds[l].space() == NULL)
      l--;
    return l;
  }

  forceinline int
  Path::entries(void) const {
    return ds.entries();
  }

  forceinline void
  Path::unwind(int l) {
    assert((ds[l].space() == NULL) || ds[l].space()->failed());
    int n = ds.entries();
    for (int i=l; i<n; i++)
      ds.pop().dispose();
    assert(ds.entries() == l);
  }

  inline void
  Path::reset(void) {
    while (!ds.empty())
      ds.pop().dispose();
  }

  forceinline Space*
  Path::recompute(unsigned int& d, unsigned int a_d, Worker& stat) {
    assert(!ds.empty());
    // Recompute space according to path
    // Also say distance to copy (d == 0) requires immediate copying

    // Check for LAO
    if ((ds.top().space() != NULL) && ds.top().rightmost()) {
      Space* s = ds.top().space();
      s->commit(*ds.top().choice(),ds.top().alt());
      assert(ds.entries()-1 == lc());
      ds.top().space(NULL);
      // Mark as reusable
      if (ds.entries() > ngdl())
        ds.top().next();
      d = 0;
      return s;
    }
    // General case for recomputation
    int l = lc();             // Position of last clone
    int n = ds.entries();     // Number of stack entries
    // New distance, if no adaptive recomputation
    d = static_cast<unsigned int>(n - l);

    Space* s = ds[l].space()->clone(); // Last clone

    if (d < a_d) {
      // No adaptive recomputation
      for (int i=l; i<n; i++)
        commit(s,i);
    } else {
      int m = l + static_cast<int>(d >> 1); // Middle between copy and top
      int i = l; // To iterate over all entries
      // Recompute up to middle
      for (; i<m; i++ )
        commit(s,i);
      // Skip over all rightmost branches
      for (; (i<n) && ds[i].rightmost(); i++)
        commit(s,i);
      // Is there any point to make a copy?
      if (i<n-1) {
        // Propagate to fixpoint
        SpaceStatus ss = s->status(stat);
        /*
         * Again, the space might already propagate to failure (due to
         * weakly monotonic propagators).
         */
        if (ss == SS_FAILED) {
          // s must be deleted as it is not on the stack
          delete s;
          stat.fail++;
          unwind(i);
          return NULL;
        }
        ds[i].space(s->clone());
        d = static_cast<unsigned int>(n-i);
      }
      // Finally do the remaining commits
      for (; i<n; i++)
        commit(s,i);
    }
    return s;
  }

  forceinline Space*
  Path::recompute(unsigned int& d, unsigned int a_d, Worker& stat,
                  const Space* best, int& mark) {
    assert(!ds.empty());
    // Recompute space according to path
    // Also say distance to copy (d == 0) requires immediate copying

    // Check for LAO
    if ((ds.top().space() != NULL) && ds.top().rightmost()) {
      Space* s = ds.top().space();
      s->commit(*ds.top().choice(),ds.top().alt());
      assert(ds.entries()-1 == lc());
      if (mark > ds.entries()-1) {
        mark = ds.entries()-1;
        s->constrain(*best);
      }
      ds.top().space(NULL);
      // Mark as reusable
      if (ds.entries() > ngdl())
        ds.top().next();
      d = 0;
      return s;
    }
    // General case for recomputation
    int l = lc();             // Position of last clone
    int n = ds.entries();     // Number of stack entries
    // New distance, if no adaptive recomputation
    d = static_cast<unsigned int>(n - l);

    Space* s = ds[l].space(); // Last clone

    if (l < mark) {
      mark = l;
      s->constrain(*best);
      // The space on the stack could be failed now as an additional
      // constraint might have been added.
      if (s->status(stat) == SS_FAILED) {
        // s does not need deletion as it is on the stack (unwind does this)
        stat.fail++;
        unwind(l);
        return NULL;
      }
      // It is important to replace the space on the stack with the
      // copy: a copy might be much smaller due to flushed caches
      // of propagators
      Space* c = s->clone();
      ds[l].space(c);
    } else {
      s = s->clone();
    }

    if (d < a_d) {
      // No adaptive recomputation
      for (int i=l; i<n; i++)
        commit(s,i);
    } else {
      int m = l + static_cast<int>(d >> 1); // Middle between copy and top
      int i = l;            // To iterate over all entries
      // Recompute up to middle
      for (; i<m; i++ )
        commit(s,i);
      // Skip over all rightmost branches
      for (; (i<n) && ds[i].rightmost(); i++)
        commit(s,i);
      // Is there any point to make a copy?
      if (i<n-1) {
        // Propagate to fixpoint
        SpaceStatus ss = s->status(stat);
        /*
         * Again, the space might already propagate to failure
         *
         * This can be for two reasons:
         *  - constrain is true, so we fail
         *  - the space has weakly monotonic propagators
         */
        if (ss == SS_FAILED) {
          // s must be deleted as it is not on the stack
          delete s;
          stat.fail++;
          unwind(i);
          return NULL;
        }
        ds[i].space(s->clone());
        d = static_cast<unsigned int>(n-i);
      }
      // Finally do the remaining commits
      for (; i<n; i++)
        commit(s,i);
    }
    return s;
  }

}}}

#endif

// STATISTICS: search-sequential
