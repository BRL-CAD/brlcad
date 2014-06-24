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


#ifndef __GECODE_SEARCH_META_RBS_HH__
#define __GECODE_SEARCH_META_RBS_HH__

#include <gecode/search.hh>

namespace Gecode { namespace Search { namespace Meta {

  /// Engine for restart-based search
  class RBS : public Engine {
  private:
    /// The actual engine
    Engine* e;
    /// The master space to restart from
    Space* master;
    /// The cutoff object
    Cutoff* co;
    /// The stop control object
    MetaStop* stop;
    /// Whether the slave can be shared with the master
    bool shared;
    /// Empty no-goods
    GECODE_SEARCH_EXPORT
    static NoGoods eng;
  public:
    /// Constructor
    RBS(Space* s, Cutoff* co0, MetaStop* stop0,
        Engine* e0, const Options& o);
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
    /// Destructor
    virtual ~RBS(void);
  };

  forceinline
  RBS::RBS(Space* s, Cutoff* co0, MetaStop* stop0,
           Engine* e0, const Options& opt)
    : e(e0), master(s), co(co0), stop(stop0), 
      shared(opt.threads == 1) {
    stop->limit(Statistics(),(*co)());
  }

}}}

#endif

// STATISTICS: search-other
