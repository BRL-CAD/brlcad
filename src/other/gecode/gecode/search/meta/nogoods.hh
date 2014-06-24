/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2013
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

#ifndef __GECODE_SEARCH_META_NO_GOODS_HH__
#define __GECODE_SEARCH_META_NO_GOODS_HH__

#include <gecode/search.hh>

namespace Gecode { namespace Search { namespace Meta {

  /// Class for a sentinel no-good literal
  class NoNGL : public NGL {
  public:
    /// Constructor for creation
    NoNGL(void);
    /// Constructor for creation
    NoNGL(Space& home);
    /// Constructor for cloning \a ngl
    NoNGL(Space& home, bool share, NoNGL& ngl);
    /// Subscribe propagator \a p to all views of the no-good literal
    virtual void subscribe(Space& home, Propagator& p);
    /// Cancel propagator \a p from all views of the no-good literal
    virtual void cancel(Space& home, Propagator& p);
    /// Test the status of the no-good literal
    virtual NGL::Status status(const Space& home) const;
    /// Propagate the negation of the no-good literal
    virtual ExecStatus prune(Space& home);
    /// Create copy
    virtual NGL* copy(Space& home, bool share);
  };

  /// No-good propagator
  class NoGoodsProp : public Propagator {
  protected:
    /// Root of no-good literal tree
    NGL* root;
    /// Number of no-good literals with subscriptions
    unsigned int n;
    /// Constructor for creation
    NoGoodsProp(Home home, NGL* root);
    /// Constructor for cloning \a p
    NoGoodsProp(Space& home, bool shared, NoGoodsProp& p);
  public:
    /// Perform copying during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Const function (defined as low unary)
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);
    /// Post propagator for path \a p
    template<class Path>
    static ExecStatus post(Space& home, Path& p);
    /// Delete propagator and return its size
    virtual size_t dispose(Space& home);
  };

  forceinline
  NoNGL::NoNGL(void) {} 

  forceinline
  NoNGL::NoNGL(Space& home) 
    : NGL(home) {}

  forceinline
  NoNGL::NoNGL(Space& home, bool share, NoNGL& ngl) 
    : NGL(home,share,ngl) {}



  forceinline
  NoGoodsProp::NoGoodsProp(Home home, NGL* root0)
    : Propagator(home), root(root0), n(0U) {
    // Create subscriptions
    root->subscribe(home,*this); n++;
    bool notice = root->notice();
    NGL* l = root->next();
    while ((l != NULL) && l->leaf()) {
      l->subscribe(home,*this); n++;
      notice = notice || l->notice();
      l = l->next();
    }
    if (l != NULL) {
      l->subscribe(home,*this); n++;
    }
    while (!notice && (l != NULL)) {
      notice = notice || l->notice();
      l = l->next();
    }
    if (notice)
      home.notice(*this,AP_DISPOSE);
  }

  forceinline
  NoGoodsProp::NoGoodsProp(Space& home, bool shared, NoGoodsProp& p) 
    : Propagator(home,shared,p), n(p.n) {
    assert(p.root != NULL);
    NoNGL s;
    NGL* c = &s;
    for (NGL* pc = p.root; pc != NULL; pc = pc->next()) {
      NGL* n = pc->copy(home,shared);
      n->leaf(pc->leaf());
      c->next(n); c=n;
    }
    root = s.next();
  }
  


  template<class Path>
  forceinline ExecStatus 
  NoGoodsProp::post(Space& home, Path& p) {
    int s = 0;
    int n = std::min(p.ds.entries(),p.ngdl());

    unsigned long int n_nogood = 0;

    // Eliminate the alternatives which are not no-goods at the end
    while ((n > s) && (p.ds[n-1].truealt() == 0U))
      n--;

    // A sentinel element
    NoNGL nn;
    // Current no-good literal
    NGL* c = &nn;

    // Commit no-goods at the beginning
    while ((s < n) && (p.ds[s].truealt() > 0U))
      // Try whether this is a rightmost alternative
      if (p.ds[s].rightmost()) {
        // No literal needed, directly try to commit
        home.trycommit(*p.ds[s].choice(),p.ds[s].truealt());
        s++;
      } else {
        // Prune using no-good literals
        for (unsigned int a=0U; a<p.ds[s].truealt(); a++) {
          NGL* l = home.ngl(*p.ds[s].choice(),a);
          // Does the brancher support no-good literals?
          if (l == NULL)
            return ES_OK;
          GECODE_ES_CHECK(l->prune(home));
        }
        // Add literal as root if needed and stop
        if (NGL* l = home.ngl(*p.ds[s].choice(),p.ds[s].truealt())) {
          c = c->add(l,false);
          s++; break;
        }
      }

    // There are no literals
    if (home.failed()) 
      return ES_FAILED;
    if (s >= n)
      return ES_OK;

    // There must be at least two literals
    assert((n-s > 1) || 
           ((n-s == 1) && (c != &nn)));

    // Remember the last leaf
    NGL* ll = NULL;

    // Create literals
    for (int i=s; i<n; i++) {
      // Add leaves
      for (unsigned int a=0U; a<p.ds[i].truealt(); a++) {
        NGL* l = home.ngl(*p.ds[i].choice(),a);
        if (l == NULL) {
          // The brancher does not support no-goods
          if (ll == NULL)
            return ES_OK;
          ll->next(NULL);
          break;
        }
        c = c->add(l,true); ll = c;
        n_nogood++;
      }
      // Check whether to add an additional subtree
      if (NGL* l = home.ngl(*p.ds[i].choice(),p.ds[i].truealt())) {
        c = c->add(l,false);
      } else if (!p.ds[i].rightmost()) {
        // The brancher does not support no-goods
        if (ll == NULL)
          return ES_OK;
        ll->next(NULL);
        break;
      }
    }

    p.ng(n_nogood);

    (void) new (home) NoGoodsProp(home,nn.next());
    return ES_OK;
  }

}}}

#endif

// STATISTICS: search-other
