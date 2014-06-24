/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
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



#include <gecode/set.hh>

namespace Gecode { namespace Set {

  BndSet::BndSet(Space& home, const IntSet& is)  {
    if (is.ranges()==0) {
      fst(NULL); lst(NULL); _size = 0;
    } else {
      int n = is.ranges();
      RangeList* r = home.alloc<RangeList>(n);
      fst(r); lst(r+n-1);
      unsigned int s = 0;
      for (int i = n; i--; ) {
        s += is.width(i);
        r[i].min(is.min(i)); r[i].max(is.max(i));
        r[i].next(&r[i+1]);
      }
      r[n-1].next(NULL);
      _size = s;
    }
    assert(isConsistent());
  }

  bool
  GLBndSet::include_full(Space& home, int mi, int ma, SetDelta& d) {
    assert(ma >= mi);
    assert(fst() != NULL);

    RangeList* p = NULL;
    RangeList* c = fst();

    while (c != NULL) {
      if (c->max() >= mi-1){
        if (c->min() > ma+1) {  //in a hole before c
          _size+=(ma-mi+1);
          d._glbMin = mi;
          d._glbMax = ma;
          RangeList* q = new (home) RangeList(mi,ma,c);
          if (p==NULL)
            //the new range is the first
            fst(q);
          else
            p->next(q);
          return true;
        }
        //if the range starts before c, update c->min and size
        bool result = false;
        if (c->min()>mi) {
          _size+=(c->min()-mi);
          c->min(mi);
          d._glbMin = mi;
          result = true;
        } else {
          d._glbMin = c->max()+1;
        }
        assert(c->min()<=mi);
        assert(c->max()+1>=mi);
        if (c->max() >= ma) {
          d._glbMax = c->min()-1;
          return result;
        }
        RangeList* q = c;
        //sum up the size of holes eaten
        int prevMax = c->max();
        int growth = 0;
        // invariant: q->min()<=ma+1
        while (q->next() != NULL && q->next()->min() <= ma+1) {
          q = q->next();
          growth += q->min()-prevMax-1;
          prevMax = q->max();
        }
        assert(growth>=0);
        _size+=growth;
        if (q->max() < ma) {
          _size += ma-q->max();
          d._glbMax = ma;
        } else {
          d._glbMax = q->min()-1;
        }
        c->max(std::max(ma,q->max()));
        if (c!=q) {
          RangeList* oldCNext = c->next();
          assert(oldCNext!=NULL); //q would have stayed c if c was last.
          c->next(q->next());
          if (q->next()==NULL){
            assert(q==lst());
            lst(c);
          }
          oldCNext->dispose(home,q);
        }
        return true;
      }
      RangeList* nc = c->next();
      p=c; c=nc;
    }
    //the new range is disjoint from the old domain and we add it as last:
    assert(mi>max()+1);
    RangeList* q = new (home) RangeList(mi, ma, NULL);
    lst()->next(q);
    lst(q);
    _size+= q->width();
    d._glbMin = mi;
    d._glbMax = ma;
    return true;
  }

  bool
  LUBndSet::intersect_full(Space& home, int mi, int ma) {
    RangeList* p = NULL;
    RangeList* c = fst();

    assert(c != NULL); // Never intersect with an empty set

    // Skip ranges that are before mi
    while (c != NULL && c->max() < mi) {
      _size -= c->width();
      RangeList *nc = c->next();
      p=c; c=nc;
    }
    if (c == NULL) {
      // Delete entire domain
      fst()->dispose(home, lst());
      fst(NULL); lst(NULL);
      return true;
    }

    bool changed = false;
    if (c != fst()) {
      fst()->dispose(home, p);
      fst(c);
      p = NULL;
      changed = true;
    }
    // We have found the first range that intersects with [mi,ma]
    if (mi > c->min()) {
      _size -= mi-c->min();
      c->min(mi);
      changed = true;
    }

    while (c != NULL && c->max() <= ma) {
      RangeList *nc = c->next();
      p=c; c=nc;
    }

    if (c == NULL)
      return changed;

    RangeList* newlst = p;
    if (ma >= c->min()) {
      _size -= c->max() - ma;
      c->max(ma);
      newlst = c;
      RangeList* nc = c->next();
      c->next(NULL);
      p=c; c=nc;
    } else if (p != NULL) {
      p->next(NULL);
    }
    if (c != NULL) {
      for (RangeList* cc = c ; cc != NULL; cc = cc->next())
        _size -= cc->width();
      c->dispose(home, lst());
    }
    lst(newlst);
    if (newlst==NULL)
      fst(NULL);
    return true;
  }

  bool
  LUBndSet::exclude_full(Space& home, int mi, int ma, SetDelta& d) {
    assert(ma >= mi);
    assert(mi <= max() && ma >= min() &&
           (mi > min() || ma < max()));
    bool result=false;

    RangeList* p = NULL;
    RangeList* c = fst();
    d._lubMin = Limits::max+1;
    while (c != NULL) {
      if (c->max() >= mi){
        if (c->min() > ma) { return result; } //in a hole

        if (c->min()<mi && c->max() > ma) {  //Range split:
          RangeList* q =
            new (home) RangeList(ma+1,c->max(),c->next());
          c->max(mi-1);
          if (c == lst()) {
            lst(q);
          }
          c->next(q);
          _size -= (ma-mi+1);
          d._lubMin = mi;
          d._lubMax = ma;
          return true;
        }

        if (c->max() > ma) { //start of range clipped, end remains
          d._lubMin = std::min(d._lubMin, c->min());
          d._lubMax = ma;
          _size-=(ma-c->min()+1);
          c->min(ma+1);
          return true;
        }

        if (c->min() < mi) { //end of range clipped, start remains
          _size-=(c->max()-mi+1);
          d._lubMin = mi;
          d._lubMax = c->max();
          c->max(mi-1);
          result=true;
        } else { //range *c destroyed
          d._lubMin = c->min();
          _size-=c->width();
          RangeList *cend = c;
          while ((cend->next()!=NULL) && (cend->next()->max()<=ma)){
            cend = cend->next();
            _size-=cend->width();
          }
          d._lubMax = cend->max();
          if (fst()==c) {
            fst(cend->next());
          } else {
            p->next(cend->next());
          }
          if (lst()==cend) {
            lst(p);
          }
          RangeList* nc=cend->next(); //performs loop step!
          c->dispose(home,cend);
          p=cend;
          c=nc;
          if (c != NULL && c->min() <= ma ) {
            //start of range clipped, end remains
            _size-=(ma-c->min()+1);
            c->min(ma+1);
            d._lubMax = ma;
          }
          return true;
        }
      }
      RangeList *nc = c->next();
      p=c; c=nc;
    }
    return result;
  }

#ifndef NDEBUG
  using namespace Gecode::Int;
#endif

  bool
  BndSet::isConsistent(void) const {
#ifndef NDEBUG
    if (fst()==NULL) {
      if (lst()!=NULL || size()!=0) {
        std::cerr<<"Strange empty set.\n";
        return false;
      } else return true;
    }

    if (fst()!=NULL && lst()==NULL) {
      std::cerr<<"First is not null, last is.\n";
      return false;
    }

    RangeList *p=NULL;
    RangeList *c=fst();

    int min = c->min();
    int max = c->max();

    if (max<min) {
      std::cerr << "Single range list twisted: ("<<min<<","<<max<<")" ;
      return false;
    }

    RangeList *nc=c->next();
    p=c; c=nc;
    while (c) {
      if (max<min) {
        std::cerr << "1";
        return false;
      }
      if ((max+1)>=c->min()) {
        std::cerr << "2";
        return false;
      }
      if (c->next()==NULL && c!=lst()) {
        std::cerr << "3";
        return false;
      }

      min = c->min();
      max = c->max();

      nc=c->next();
      p=c; c=nc;
    }
#endif
    return true;
  }


}}

// STATISTICS: set-var

