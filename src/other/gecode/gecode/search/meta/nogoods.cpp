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

#include <gecode/search/meta/nogoods.hh>

namespace Gecode { namespace Search { namespace Meta {

  /// Help function to cancel and dispose a no-good literal
  forceinline NGL*
  disposenext(NGL* ngl, Space& home, Propagator& p, bool c) {
    NGL* n = ngl->next();
    if (c)
      ngl->cancel(home,p);
    home.rfree(ngl,ngl->dispose(home));
    return n;
  }
    
  void
  NoNGL::subscribe(Space&, Propagator&) {
    GECODE_NEVER;
  }
  void
  NoNGL::cancel(Space&, Propagator&) {
    GECODE_NEVER;
  }
  NGL::Status
  NoNGL::status(const Space&) const {
    GECODE_NEVER;
    return NGL::NONE;
  }
  ExecStatus
  NoNGL::prune(Space&) {
    GECODE_NEVER;
    return ES_OK;
  }
  NGL*
  NoNGL::copy(Space&, bool) {
    GECODE_NEVER;
    return NULL;
  }

  Actor* 
  NoGoodsProp::copy(Space& home, bool share) {
    return new (home) NoGoodsProp(home,share,*this);
  }

  PropCost 
  NoGoodsProp::cost(const Space&, const ModEventDelta&) const {
    return PropCost::linear(PropCost::LO,n);
  }

  ExecStatus 
  NoGoodsProp::propagate(Space& home, const ModEventDelta&) {
  restart:
    // Start with checking the first literal
    switch (root->status(home)) {
    case NGL::FAILED:
      // All no-goods are dead, let dispose() clean up
      return home.ES_SUBSUMED(*this);
    case NGL::SUBSUMED:
      {
        NGL* l = disposenext(root,home,*this,true); n--;
        // Prune leaf-literals
        while ((l != NULL) && l->leaf()) {
          l->cancel(home,*this); n--;
          GECODE_ES_CHECK(l->prune(home));
          l = disposenext(l,home,*this,false);
        }
        root = l;
        // Is there anything left?
        if (l == NULL)
          return home.ES_SUBSUMED(*this);
        // Skip literal that already has a subscription
        l = l->next();
        // Create subscriptions for leafs
        while ((l != NULL) && l->leaf()) {
          l->subscribe(home,*this); n++;
          l = l->next();
        }
        // Create subscription for possible non-leaf literal
        if (l != NULL) {
          l->subscribe(home,*this); n++;
        }
        goto restart;
      }
    case NGL::NONE:
      break;
    default: GECODE_NEVER;
    }

    {
      NGL* p = root;
      NGL* l = p->next();

      // Check the leaves
      while ((l != NULL) && l->leaf()) {
        switch (l->status(home)) {
        case NGL::SUBSUMED:
          l = disposenext(l,home,*this,true); n--;
          p->next(l);
          GECODE_ES_CHECK(root->prune(home));
          if (root->status(home) == NGL::FAILED)
            return home.ES_SUBSUMED(*this);
          break;
        case NGL::FAILED:
          l = disposenext(l,home,*this,true); n--;
          p->next(l);
          break;
        case NGL::NONE:
          p = l; l = l->next();
          break;
        default: GECODE_NEVER;
        }
      }

      // Check the next subtree
      if (l != NULL) {
        switch (l->status(home)) {
        case NGL::FAILED:
          (void) disposenext(l,home,*this,true); n--;
          // Prune entire subtree
          p->next(NULL);
          break;
        case NGL::SUBSUMED:
          {
            // Unlink node
            l = disposenext(l,home,*this,true); n--;
            p->next(l);
            // Create subscriptions
            while ((l != NULL) && l->leaf()) {
              l->subscribe(home,*this); n++;
              l = l->next();
            }
            if (l != NULL) {
              l->subscribe(home,*this); n++;
            }
          }
          break;
        case NGL::NONE:
          break;
        default: GECODE_NEVER;
        }
      }
    }
    return ES_NOFIX;
  }

  size_t 
  NoGoodsProp::dispose(Space& home) {
    if (home.failed()) {
      // This will be executed when one ngl returned true for notice()
      NGL* l = root;
      while (l != NULL) {
        NGL* t = l->next();
        (void) l->dispose(home);
        l = t;
      }
    } else if (root != NULL) {
      // This will be executed for subsumption
      NGL* l = disposenext(root,home,*this,true);
      while ((l != NULL) && l->leaf())
        l = disposenext(l,home,*this,true);
      if (l != NULL)
        l = disposenext(l,home,*this,true);
      while (l != NULL)
        l = disposenext(l,home,*this,false);
    }
    home.ignore(*this,AP_DISPOSE,true);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

}}}

// STATISTICS: search-other
