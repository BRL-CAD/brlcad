/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2012
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

namespace Gecode {

  /**
   * \defgroup TaskBranchView Generic brancher based on view selection
   *
   * Implements view-based brancher for an array of views and value.
   * \ingroup TaskActor
   */
  //@{
  /// Position information
  class Pos {
  public:
    /// Position of view
    const int pos;
    /// Create position information
    Pos(int p);
  };

  /// %Choices storing position
  class GECODE_VTABLE_EXPORT PosChoice : public Choice {
  private:
    /// Position information
    const Pos _pos;
  public:
    /// Initialize choice for brancher \a b, number of alternatives \a a, and position \a p
    PosChoice(const Brancher& b, unsigned int a, const Pos& p);
    /// Return position in array
    const Pos& pos(void) const;
    /// Report size occupied
    virtual size_t size(void) const;
    /// Archive into \a e
    virtual void archive(Archive& e) const;
  };

  /**
   * \brief Generic brancher by view selection
   *
   * Defined for views of type \a View and \a n view selectors for 
   * tie-breaking.
   */
  template<class View, int n>
  class ViewBrancher : public Brancher {
  protected:
    /// The branch filter that corresponds to the var type
    typedef typename BranchTraits<typename View::VarType>::Filter BranchFilter;
    /// Views to branch on
    ViewArray<View> x;
    /// Unassigned views start at x[start]
    mutable int start;
    /// View selection objects
    ViewSel<View>* vs[n];
    /// Branch filter function
    BranchFilter bf;
    /// Return position information
    Pos pos(Space& home);
    /// Return view according to position information \a p
    View view(const Pos& p) const;
    /// Constructor for cloning \a b
    ViewBrancher(Space& home, bool shared, ViewBrancher<View,n>& b);
    /// Constructor for creation
    ViewBrancher(Home home, ViewArray<View>& x,
                 ViewSel<View>* vs[n], BranchFilter bf);
  public:
    /// Check status of brancher, return true if alternatives left
    virtual bool status(const Space& home) const;
    /// Delete brancher and return its size
    virtual size_t dispose(Space& home);
  };

  //@}


  /*
   * Position information
   *
   */
  forceinline
  Pos::Pos(int p) : pos(p) {}

  /*
   * Choice with position
   *
   */
  forceinline
  PosChoice::PosChoice(const Brancher& b, unsigned int a, const Pos& p)
    : Choice(b,a), _pos(p) {}
  forceinline const Pos&
  PosChoice::pos(void) const {
    return _pos;
  }
  forceinline size_t
  PosChoice::size(void) const {
    return sizeof(PosChoice);
  }
  forceinline void
  PosChoice::archive(Archive& e) const {
    Choice::archive(e);
    e << _pos.pos;
  }

  template<class View, int n>
  forceinline
  ViewBrancher<View,n>::ViewBrancher(Home home, ViewArray<View>& x0,
                                     ViewSel<View>* vs0[n], BranchFilter bf0)
    : Brancher(home), x(x0), start(0), bf(bf0) {
    for (int i=0; i<n; i++)
      vs[i] = vs0[i];
    for (int i=0; i<n; i++)
      if (vs[i]->notice()) {
        home.notice(*this,AP_DISPOSE);
        break;
      }
  }

  template<class View, int n>
  forceinline
  ViewBrancher<View,n>::ViewBrancher(Space& home, bool shared,
                                     ViewBrancher<View,n>& vb)
    : Brancher(home,shared,vb), start(vb.start), bf(vb.bf) {
    x.update(home,shared,vb.x);
    for (int i=0; i<n; i++)
      vs[i] = vb.vs[i]->copy(home,shared);
  }

  template<class View, int n>
  bool
  ViewBrancher<View,n>::status(const Space& home) const {
    if (bf == NULL) {
      for (int i=start; i < x.size(); i++)
        if (!x[i].assigned()) {
          start = i;
          return true;
        }
    } else {
      for (int i=start; i < x.size(); i++) {
        typename View::VarType y(x[i].varimp());
        if (!x[i].assigned() && bf(home,y,i)) {
          start = i;
          return true;
        }
      }
    }
    return false;
  }

  template<class View, int n>
  inline Pos
  ViewBrancher<View,n>::pos(Space& home) {
    assert(!x[start].assigned());
    int s;
    if (bf == NULL) {
      if (n == 1) {
        s = vs[0]->select(home,x,start);
      } else {
        Region r(home);
        int* ties = r.alloc<int>(x.size()-start+1);
        int n_ties;
        vs[0]->ties(home,x,start,ties,n_ties);
        for (int i=1; (i < n-1) && (n_ties > 1); i++)
          vs[i]->brk(home,x,ties,n_ties);
        if (n_ties > 1)
          s = vs[n-1]->select(home,x,ties,n_ties);
        else
          s = ties[0];
      }
    } else {
      if (n == 1) {
        s = vs[0]->select(home,x,start,bf);
      } else {
        Region r(home);
        int* ties = r.alloc<int>(x.size()-start+1);
        int n_ties;
        vs[0]->ties(home,x,start,ties,n_ties,bf);
        for (int i=1; (i < n-1) && (n_ties > 1); i++)
          vs[i]->brk(home,x,ties,n_ties);
        if (n_ties > 1)
          s = vs[n-1]->select(home,x,ties,n_ties);
        else
          s = ties[0];
      }
    }
    Pos p(s);
    return p;
  }

  template<class View, int n>
  forceinline View
  ViewBrancher<View,n>::view(const Pos& p) const {
    return x[p.pos];
  }

  template<class View, int n>
  forceinline size_t
  ViewBrancher<View,n>::dispose(Space& home) {
    for (int i=0; i<n; i++)
      if (vs[i]->notice()) {
        home.ignore(*this,AP_DISPOSE,true);
        break;
      }
    for (int i=0; i<n; i++)
      vs[i]->dispose(home);
    (void) Brancher::dispose(home);
    return sizeof(ViewBrancher<View,n>);
  }

}

// STATISTICS: kernel-branch
