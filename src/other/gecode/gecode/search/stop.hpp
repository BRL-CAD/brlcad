/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2006
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

namespace Gecode { namespace Search {

  /*
   * Base class
   *
   */
  forceinline
  Stop::Stop(void) {}
  forceinline
  Stop::~Stop(void) {}
  forceinline void*
  Stop::operator new(size_t s) {
    return heap.ralloc(s);
  }
  forceinline void
  Stop::operator delete(void* p) {
    heap.rfree(p);
  }




  /*
   * Stopping for node limit
   *
   */

  forceinline
  NodeStop::NodeStop(unsigned long int l0) : l(l0) {}

  forceinline unsigned long int
  NodeStop::limit(void) const {
    return l;
  }

  forceinline void
  NodeStop::limit(unsigned long int l0) {
    l=l0;
  }


  /*
   * Stopping for failure limit
   *
   */

  forceinline
  FailStop::FailStop(unsigned long int l0) : l(l0) {}

  forceinline unsigned long int
  FailStop::limit(void) const {
    return l;
  }

  forceinline void
  FailStop::limit(unsigned long int l0) {
    l=l0;
  }


  /*
   * Stopping for time limit
   *
   */

  forceinline
  TimeStop::TimeStop(unsigned long int l0)
    : l(l0) {
    t.start();
  }

  forceinline unsigned long int
  TimeStop::limit(void) const {
    return l;
  }

  forceinline void
  TimeStop::limit(unsigned long int l0) {
    l=l0;
  }

  forceinline void
  TimeStop::reset(void) {
    t.start();
  }


  /*
   * Stopping for meta search engines
   *
   */

  forceinline
  MetaStop::MetaStop(Stop* s) 
    : e_stop(new FailStop(0)), m_stop(s), e_stopped(false) {}

  forceinline void
  MetaStop::limit(const Search::Statistics& s, unsigned long int l) {
    m_stat += s;
    e_stopped = false;
    e_stop->limit(l);
  }

  forceinline void
  MetaStop::update(const Search::Statistics& s) {
    m_stat += s;
  }

  forceinline Stop*
  MetaStop::enginestop(void) const { 
    return e_stop; 
  }

  forceinline bool
  MetaStop::enginestopped(void) const { 
    return e_stopped; 
  }

  forceinline Statistics 
  MetaStop::metastatistics(void) const { 
    return m_stat; 
  }

  forceinline
  MetaStop::~MetaStop(void) {
    delete e_stop;
    delete m_stop;
  }

}}

// STATISTICS: search-other
