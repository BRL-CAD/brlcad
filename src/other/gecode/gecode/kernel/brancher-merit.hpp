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
   * \defgroup TaskBranchMerit Generic merit for branchers based on view and value selection
   *
   * \ingroup TaskBranchViewVal
   */
  //@{
  /**
   * \brief Base-class for merit class
   */
  template<class _View, class _Val>
  class MeritBase {
  public:
    /// View type
    typedef _View View;
    /// Type of merit
    typedef _Val Val;
    /// Constructor for initialization
    MeritBase(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritBase(Space& home, bool share, MeritBase& mb);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Delete view merit class
    void dispose(Space& home);
  };

  /**
   * \brief Merit class for user-defined merit function
   */
  template<class View>
  class MeritFunction : public MeritBase<View,double> {
  public:
    /// Corresponding variable type
    typedef typename View::VarType Var;
    /// Corresponding merit function type
    typedef typename BranchTraits<Var>::Merit Function;
  protected:
    /// The user-defined merit function
    Function f;
  public:
    /// Constructor for initialization
    MeritFunction(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritFunction(Space& home, bool shared, MeritFunction& mf);
    /// Return degree as merit for view \a x at position \a i
    double operator ()(const Space& home, View x, int i);
  };

  /**
   * \brief Merit class for degree
   */
  template<class View>
  class MeritDegree : public MeritBase<View,unsigned int> {
  public:
    /// Constructor for initialization
    MeritDegree(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritDegree(Space& home, bool shared, MeritDegree& md);
    /// Return degree as merit for view \a x at position \a i
    unsigned int operator ()(const Space& home, View x, int i);
  };

  /**
   * \brief Merit class for AFC
   */
  template<class View>
  class MeritAFC : public MeritBase<View,double> {
  protected:
    /// AFC information
    AFC afc;    
  public:
    /// Constructor for initialization
    MeritAFC(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritAFC(Space& home, bool shared, MeritAFC& ma);
    /// Return AFC as merit for view \a x at position \a i
    double operator ()(const Space& home, View x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Dispose view selection
    void dispose(Space& home);
  };

  /**
   * \brief Merit class for activity
   */
  template<class View>
  class MeritActivity : public MeritBase<View,double> {
  protected:
    /// Activity information
    Activity activity;
  public:
    /// Constructor for initialization
    MeritActivity(Space& home, const VarBranch& vb);
    /// Constructor for cloning
    MeritActivity(Space& home, bool shared, MeritActivity& ma);
    /// Return activity as merit for view \a x at position \a i
    double operator ()(const Space& home, View x, int i);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Dispose view selection
    void dispose(Space& home);
  };
  //@}


  // Merit base class
  template<class View, class Val>
  forceinline
  MeritBase<View,Val>::MeritBase(Space&, const VarBranch&) {}
  template<class View, class Val>
  forceinline
  MeritBase<View,Val>::MeritBase(Space&, bool, MeritBase&) {}
  template<class View, class Val>
  forceinline bool
  MeritBase<View,Val>::notice(void) const {
    return false;
  }
  template<class View, class Val>
  forceinline void
  MeritBase<View,Val>::dispose(Space&) {}

  // User-defined function merit
  template<class View>
  forceinline
  MeritFunction<View>::MeritFunction(Space& home, const VarBranch& vb)
    : MeritBase<View,double>(home,vb),
      f(function_cast<Function>(vb.merit())) {}
  template<class View>
  forceinline
  MeritFunction<View>::MeritFunction(Space& home, bool shared, 
                                     MeritFunction& mf) 
    : MeritBase<View,double>(home,shared,mf), f(mf.f) {}
  template<class View>
  forceinline double
  MeritFunction<View>::operator ()(const Space& home, View x, int i) {
    typename View::VarType y(x.varimp());
    return f(home,y,i);
  }

  // Degree merit
  template<class View>
  forceinline
  MeritDegree<View>::MeritDegree(Space& home, const VarBranch& vb)
    : MeritBase<View,unsigned int>(home,vb) {}
  template<class View>
  forceinline
  MeritDegree<View>::MeritDegree(Space& home, bool shared, 
                                 MeritDegree& md)
    : MeritBase<View,unsigned int>(home,shared,md) {}
  template<class View>
  forceinline unsigned int
  MeritDegree<View>::operator ()(const Space&, View x, int) {
    return x.degree();
  }

  // AFC merit
  template<class View>
  forceinline
  MeritAFC<View>::MeritAFC(Space& home, const VarBranch& vb)
    : MeritBase<View,double>(home,vb), afc(vb.afc()) {}
  template<class View>
  forceinline
  MeritAFC<View>::MeritAFC(Space& home, bool shared, 
                           MeritAFC& ma)
    : MeritBase<View,double>(home,shared,ma) {
    afc.update(home,shared,ma.afc);
  }
  template<class View>
  forceinline double
  MeritAFC<View>::operator ()(const Space& home, View x, int) {
    return x.afc(home);
  }
  template<class View>
  forceinline bool
  MeritAFC<View>::notice(void) const {
    return true;
  }
  template<class View>
  forceinline void
  MeritAFC<View>::dispose(Space&) {
    afc.~AFC();
  }


  // Acitivity merit
  template<class View>
  forceinline
  MeritActivity<View>::MeritActivity(Space& home, const VarBranch& vb)
    : MeritBase<View,double>(home,vb), activity(vb.activity()) {}
  template<class View>
  forceinline
  MeritActivity<View>::MeritActivity(Space& home, bool shared, 
                                     MeritActivity& ma)
    : MeritBase<View,double>(home,shared,ma) {
    activity.update(home, shared, ma.activity);
  }
  template<class View>
  forceinline double
  MeritActivity<View>::operator ()(const Space&, View, int i) {
    return activity[i];
  }
  template<class View>
  forceinline bool
  MeritActivity<View>::notice(void) const {
    return true;
  }
  template<class View>
  forceinline void
  MeritActivity<View>::dispose(Space&) {
    activity.~Activity();
  }

}

// STATISTICS: kernel-branch
