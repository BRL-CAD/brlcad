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
   * \defgroup TaskBranchValSel Generic value selection for brancher based on view and value selection
   *
   * \ingroup TaskBranchViewVal
   */
  //@{
  /// Base class for value selection
  template<class _View, class _Val>
  class ValSel {
  public:
    /// View type
    typedef _View View;
    /// Value type
    typedef _Val Val;
  public:
    /// Constructor for initialization
    ValSel(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSel(Space& home, bool shared, ValSel<View,Val>& vs);
    /// Whether dispose must always be called (that is, notice is needed)
    bool notice(void) const;
    /// Delete value selection
    void dispose(Space& home);
  };

  /// User-defined value selection
  template<class View>
  class ValSelFunction : 
    public ValSel<View,
                  typename BranchTraits<typename View::VarType>::ValType> {
  public:
    /// The corresponding value type
    typedef typename ValSel<View,
                            typename BranchTraits<typename View::VarType>
                              ::ValType>::Val Val;
    /// The corresponding variable type
    typedef typename View::VarType Var;
    /// The corresponding value function
    typedef typename BranchTraits<Var>::Val ValFunction;
  protected:
    /// The user-defined value function
    ValFunction v;
  public:
    /// Constructor for initialization
    ValSelFunction(Space& home, const ValBranch& vb);
    /// Constructor for cloning
    ValSelFunction(Space& home, bool shared, ValSelFunction<View>& vs);
    /// Return user-defined value of view \a x at position \a i
    Val val(const Space& home, View x, int i);
  };
  //@}


  // Baseclass value selection
  template<class View, class Val>
  forceinline
  ValSel<View,Val>::ValSel(Space&, const ValBranch&) {}
  template<class View, class Val>
  forceinline
  ValSel<View,Val>::ValSel(Space&, bool, ValSel<View,Val>&) {}
  template<class View, class Val>
  forceinline bool
  ValSel<View,Val>::notice(void) const {
    return false;
  }
  template<class View, class Val>
  forceinline void
  ValSel<View,Val>::dispose(Space&) {}


  // User-defined value selection
  template<class View>
  forceinline
  ValSelFunction<View>::ValSelFunction(Space& home, const ValBranch& vb)
    : ValSel<View,Val>(home,vb),
      v(function_cast<ValFunction>(vb.val())) {}
  template<class View>
  forceinline
  ValSelFunction<View>::ValSelFunction(Space& home, bool shared, 
                                       ValSelFunction<View>& vs) 
    : ValSel<View,Val>(home,shared,vs), v(vs.v) {}
  template<class View>
  forceinline typename ValSelFunction<View>::Val
  ValSelFunction<View>::val(const Space& home, View x, int i) {
    typename View::VarType y(x.varimp());
    return v(home,y,i);
  }

}

// STATISTICS: kernel-branch
