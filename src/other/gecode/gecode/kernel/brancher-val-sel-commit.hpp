/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main author:
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
   * \defgroup TaskBranchValSelCommit Generic value selection and value commit for brancher based on view and value selection
   *
   * \ingroup TaskBranchViewVal
   */
  //@{
  /// Base class for value selection and commit
  template<class _View, class _Val>
  class ValSelCommitBase {
  public:
    /// View type
    typedef _View View;
    /// Value type
    typedef _Val Val;
  public:
    /// Constructor for initialization
    ValSelCommitBase(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelCommitBase(Space& home, bool shared, 
                     ValSelCommitBase<View,Val>& vsc);
    /// Return value of view \a x at position \a i
    virtual Val val(const Space& home, View x, int i) = 0;
    /// Commit view \a x at position \a i to value \a n for alternative \a a
    virtual ModEvent commit(Space& home, unsigned int a, 
                            View x, int i, Val n) = 0;
    /// Create no-good literal for choice \a c and alternative \a a
    virtual NGL* ngl(Space& home, unsigned int a,
                     View x, Val n) const = 0;
    /// Print on \a o branch for alternative \a a, view \a x at position \a i, and value \a n
    virtual void print(const Space& home, unsigned int a,
                       View x, int i, const Val& n,
                       std::ostream& o) const = 0;
    /// Perform cloning
    virtual ValSelCommitBase<View,Val>* copy(Space& home, bool shared) = 0;
    /// Whether dispose must always be called (that is, notice is needed)
    virtual bool notice(void) const = 0;
    /// Delete value selection
    virtual void dispose(Space& home) = 0;
    /// \name Memory management
    //@{
    /// Allocate memory from space
    static void* operator new(size_t s, Space& home);
    /// Return memory to space
    static void operator delete(void* p, Space& home);
    /// Needed for exceptions
    static void operator delete(void* p);
    //@}
  };

  /// Class for value selection and commit
  template<class ValSel, class ValCommit>
  class ValSelCommit 
    : public ValSelCommitBase<typename ValSel::View,typename ValSel::Val> {
  protected:
    typedef typename ValSelCommitBase<typename ValSel::View,
                                      typename ValSel::Val>::Val Val;
    typedef typename ValSelCommitBase<typename ValSel::View,
                                      typename ValSel::Val>::View View;
    /// The value selection object used
    ValSel s;
    /// The commit object used
    ValCommit c;
  public:
    /// Constructor for initialization
    ValSelCommit(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelCommit(Space& home, bool shared, 
                 ValSelCommit<ValSel,ValCommit>& vsc);
    /// Return value of view \a x at position \a i
    virtual Val val(const Space& home, View x, int i);
    /// Commit view \a x at position \a i to value \a n for alternative \a a
    virtual ModEvent commit(Space& home, unsigned int a, View x, int i, Val n);
    /// Create no-good literal for choice \a c and alternative \a a
    virtual NGL* ngl(Space& home, unsigned int a,
                     View x, Val n) const;
    /// Print on \a o branch for alternative \a a, view \a x at position \a i, and value \a n
    virtual void print(const Space& home, unsigned int a,
                       View x, int i, const Val& n,
                       std::ostream& o) const;
    /// Perform cloning
    virtual ValSelCommit<ValSel,ValCommit>* copy(Space& home, bool shared);
    /// Whether dispose must always be called (that is, notice is needed)
    virtual bool notice(void) const;
    /// Delete value selection
    virtual void dispose(Space& home);
  };
  //@}


  template<class View, class Val>
  forceinline
  ValSelCommitBase<View,Val>::ValSelCommitBase(Space&, const ValBranch&) {}
  template<class View, class Val>
  forceinline
  ValSelCommitBase<View,Val>::
    ValSelCommitBase(Space&, bool, ValSelCommitBase<View,Val>&) {}

  template<class View, class Val>
  forceinline void
  ValSelCommitBase<View,Val>::operator delete(void*) {}
  template<class View, class Val>
  forceinline void
  ValSelCommitBase<View,Val>::operator delete(void*, Space&) {}
  template<class View, class Val>
  forceinline void*
  ValSelCommitBase<View,Val>::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }




  template<class ValSel, class ValCommit>
  forceinline
  ValSelCommit<ValSel,ValCommit>::ValSelCommit(Space& home, 
                                               const ValBranch& vb) 
    : ValSelCommitBase<View,Val>(home,vb), s(home,vb), c(home,vb) {}

  template<class ValSel, class ValCommit>
  forceinline
  ValSelCommit<ValSel,ValCommit>::ValSelCommit(Space& home, bool shared, 
                                               ValSelCommit<ValSel,ValCommit>& vsc)
    : ValSelCommitBase<View,Val>(home,shared,vsc),
      s(home,shared,vsc.s), c(home,shared,vsc.c) {}

  template<class ValSel, class ValCommit>
  typename ValSelCommit<ValSel,ValCommit>::Val
  ValSelCommit<ValSel,ValCommit>::val(const Space& home, View x, int i) {
    return s.val(home,x,i);
  }

  template<class ValSel, class ValCommit>
  ModEvent
  ValSelCommit<ValSel,ValCommit>::commit(Space& home, unsigned int a, 
                                         View x, int i, Val n) {
    return c.commit(home,a,x,i,n);
  }

  template<class ValSel, class ValCommit>
  NGL* 
  ValSelCommit<ValSel,ValCommit>::ngl(Space& home, unsigned int a,
                                      View x, Val n) const {
    return c.ngl(home, a, x, n);
  }

  template<class ValSel, class ValCommit>
  void
  ValSelCommit<ValSel,ValCommit>::print(const Space& home, unsigned int a, 
                                        View x, int i, const Val& n,
                                        std::ostream& o) const {
    c.print(home,a,x,i,n,o);
  }

  template<class ValSel, class ValCommit>
  ValSelCommit<ValSel,ValCommit>*
  ValSelCommit<ValSel,ValCommit>::copy(Space& home, bool shared) {
    return new (home) ValSelCommit<ValSel,ValCommit>(home,shared,*this);
  }

  template<class ValSel, class ValCommit>
  bool
  ValSelCommit<ValSel,ValCommit>::notice(void) const {
    return s.notice() || c.notice();
  }

  template<class ValSel, class ValCommit>
  void
  ValSelCommit<ValSel,ValCommit>::dispose(Space& home) {
    s.dispose(home);
    c.dispose(home);
  }

}

// STATISTICS: kernel-branch
