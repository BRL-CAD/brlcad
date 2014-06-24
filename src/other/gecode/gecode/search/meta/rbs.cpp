/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2012
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


#include <gecode/search/meta/rbs.hh>

namespace Gecode { namespace Search { namespace Meta {

  Space*
  RBS::next(void) {
    while (true) {
      Space* n = e->next();
      unsigned long int i = stop->m_stat.restart;
      if (n != NULL) {
        NoGoods& ng = e->nogoods();
        ng.ng(0);
        master->constrain(*n);
        master->master(i,n,ng);
        stop->m_stat.nogood += ng.ng();
        stop->update(e->statistics());
        if (master->status(stop->m_stat) == SS_FAILED) {
          delete master;
          master = NULL;
          e->reset(NULL);
        } else {
          Space* slave = master;
          master = master->clone(shared);
          slave->slave(i,n);
          e->reset(slave);
        }
        return n;
      } else if (e->stopped() && stop->enginestopped()) {
        NoGoods& ng = e->nogoods();
        ng.ng(0);
        master->master(i,NULL,ng);
        stop->m_stat.nogood += ng.ng();
        long unsigned int nl = (*co)();
        stop->limit(e->statistics(),nl);
        if (master->status(stop->m_stat) == SS_FAILED)
          return NULL;
        Space* slave = master;
        master = master->clone(shared);
        slave->slave(i,n);
        e->reset(slave);
      } else {
        return NULL;
      }
    }
    GECODE_NEVER;
    return NULL;
  }
  
  Search::Statistics
  RBS::statistics(void) const {
    return stop->metastatistics()+e->statistics();
  }
  
  bool
  RBS::stopped(void) const {
    /*
     * What might happen during parallel search is that the
     * engine has been stopped but the meta engine has not, so
     * the meta engine does not perform a restart. However the
     * invocation of next will do so and no restart will be
     * missed.
     */
    return e->stopped(); 
  }
  
  void
  RBS::reset(Space*) { 
  }
  
  NoGoods RBS::eng;

  NoGoods&
  RBS::nogoods(void) {
    return eng;
  }
  
  RBS::~RBS(void) {
    // Deleting e also deletes stop
    delete e;
    delete master;
    delete co;
  }

}}}

// STATISTICS: search-other
