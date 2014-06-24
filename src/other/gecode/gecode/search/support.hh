/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
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

#ifndef __GECODE_SEARCH_SUPPORT_HH__
#define __GECODE_SEARCH_SUPPORT_HH__

#include <gecode/search.hh>

namespace Gecode { namespace Search {

  /// Clone space \a s dependening on options \a o
  forceinline Space*
  snapshot(Space* s, const Options& o, bool share=true);

  /// Virtualize a worker to an engine
  template<class Worker>
  class WorkerToEngine : public Engine {
  protected:
    /// The worker to wrap into an engine
    Worker w;
  public:
    /// Initialization
    WorkerToEngine(Space* s, const Options& o);
    /// Return next solution (NULL, if none exists or search has been stopped)
    virtual Space* next(void);
    /// Return statistics
    virtual Search::Statistics statistics(void) const;
    /// Check whether engine has been stopped
    virtual bool stopped(void) const;
    /// Reset engine to restart at space \a s
    virtual void reset(Space* s);
    /// Return no-goods
    virtual NoGoods& nogoods(void);
  };



  forceinline Space*
  snapshot(Space* s, const Options& o, bool share) {
    return o.clone ? s->clone(share) : s;
  }

  
  template<class Worker>
  WorkerToEngine<Worker>::WorkerToEngine(Space* s, const Options& o) 
    : w(s,o) {}
  template<class Worker>
  Space* 
  WorkerToEngine<Worker>::next(void) {
    return w.next();
  }
  template<class Worker>
  Search::Statistics 
  WorkerToEngine<Worker>::statistics(void) const {
    return w.statistics();
  }
  template<class Worker>
  bool 
  WorkerToEngine<Worker>::stopped(void) const {
    return w.stopped();
  }
  template<class Worker>
  void
  WorkerToEngine<Worker>::reset(Space* s) {
    w.reset(s);
  }

  template<class Worker>
  NoGoods&
  WorkerToEngine<Worker>::nogoods(void) {
    return w.nogoods();
  }

}}

#endif

// STATISTICS: search-other
