/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
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

#ifndef __GECODE_SEARCH_WORKER_HH__
#define __GECODE_SEARCH_WORKER_HH__

#include <gecode/search.hh>

namespace Gecode { namespace Search {

  /**
   * \brief %Search worker statistics
   */
  class Worker : public Statistics {
  protected:
    /// Whether engine has been stopped
    bool _stopped;
    /// Depth of root node (for work stealing)
    unsigned long int root_depth;
  public:
    /// Initialize
    Worker(void);
    /// Reset stop information
    void start(void);
    /// Check whether engine must be stopped
    bool stop(const Options& o);
    /// Check whether engine has been stopped
    bool stopped(void) const;
    /// Reset statistics with root depth \a d
    void reset(unsigned long int d=0);
    /// Record stack depth \a d
    void stack_depth(unsigned long int d);
    /// Return steal depth
    unsigned long int steal_depth(unsigned long int d) const;
  };



  forceinline
  Worker::Worker(void)
    : _stopped(false), root_depth(0) {}

  forceinline void
  Worker::start(void) {
    _stopped = false;
  }

  forceinline bool
  Worker::stop(const Options& o) {
    if (o.stop == NULL)
      return false;
    _stopped |= o.stop->stop(*this,o);
    return _stopped;
  }

  forceinline bool
  Worker::stopped(void) const {
    return _stopped;
  }

  forceinline void
  Worker::reset(unsigned long int d) {
    Statistics::reset();
    root_depth = d;
    if (depth < d)
      depth = d;
  }

  forceinline void
  Worker::stack_depth(unsigned long int d) {
    if (depth < root_depth + d)
      depth = root_depth + d;
  }

  forceinline unsigned long int
  Worker::steal_depth(unsigned long int d) const {
    return root_depth + d;
  }

}}

#endif

// STATISTICS: search-other
