/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2005
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
   * \brief Base-class for constant views
   * \ingroup TaskVarView
   */
  template<class View>
  class ConstView {
  public:
    /// The variable implementation type corresponding to the constant view
    typedef typename View::VarImpType VarImpType;
    /// The variable type corresponding to the constant view
    typedef typename View::VarType VarType;
    /// \name Generic view information
    //@{
    /// Return degree (number of subscribed propagators and advisors)
    unsigned int degree(void) const;
    /// Return accumulated failure count
    double afc(const Space& home) const;
    /// Return whether this view is derived from a VarImpView
    static bool varderived(void);
    /// Return dummy variable implementation of view
    VarImpType* varimp(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether view is assigned
    bool assigned(void) const;
    //@}

    /// \name View-dependent propagator support
    //@{
    /// Schedule propagator \a p with modification event \a me
    static void schedule(Space& home, Propagator& p, ModEvent me);
    /// Return modification event for view type in \a med
    static ModEvent me(const ModEventDelta& med);
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}

    /// \name Dependencies
    //@{
    /**
     * \brief Subscribe propagator \a p with propagation condition \a pc to view
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     */
    void subscribe(Space& home, Propagator& p, PropCond pc, bool schedule=true);
    /// Cancel subscription of propagator \a p with propagation condition \a pc to view
    void cancel(Space& home, Propagator& p, PropCond pc);
    /// Subscribe advisor \a a to view
    void subscribe(Space& home, Advisor& a);
    /// Cancel subscription of advisor \a a
    void cancel(Space& home, Advisor& a);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return modification event
    static ModEvent modevent(const Delta& d);
    //@}

    /// \name Cloning
    //@{
    /// Update this view to be a clone of view \a y
    void update(Space& home, bool share, ConstView& y);
    //@}
  };



  /**
   * \brief Base-class for variable implementation views
   * \ingroup TaskVarView
   */
  template<class Var>
  class VarImpView {
  public:
    /// The variable type corresponding to the view
    typedef Var VarType;
    /// The variable implementation type corresponding to the view
    typedef typename Var::VarImpType VarImpType;
  protected:
    /// Pointer to variable implementation
    VarImpType* x;
    /// Default constructor
    VarImpView(void);
    /// Initialize with variable implementation \a y
    VarImpView(VarImpType* y);
  public:
    /// \name Generic view information
    //@{
    /// Return whether this view is derived from a VarImpView
    static bool varderived(void);
    /// Return variable implementation of view
    VarImpType* varimp(void) const;
    /// Return degree (number of subscribed propagators and advisors)
    unsigned int degree(void) const;
    /// Return accumulated failure count
    double afc(const Space& home) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether view is assigned
    bool assigned(void) const;
    //@}

    /// \name View-dependent propagator support
    //@{
    /// Schedule propagator \a p with modification event \a me
    static void schedule(Space& home, Propagator& p, ModEvent me);
    /// Return modification event for view type in \a med
    static ModEvent me(const ModEventDelta& med);
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent me);
    //@}

    /// \name Dependencies
    //@{
    /**
     * \brief Subscribe propagator \a p with propagation condition \a pc to view
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     */
    void subscribe(Space& home, Propagator& p, PropCond pc, bool schedule=true);
    /// Cancel subscription of propagator \a p with propagation condition \a pc to view
    void cancel(Space& home, Propagator& p, PropCond pc);
    /// Subscribe advisor \a a to view
    void subscribe(Space& home, Advisor& a);
    /// Cancel subscription of advisor \a a
    void cancel(Space& home, Advisor& a);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return modification event
    static ModEvent modevent(const Delta& d);
    //@}

    /// \name Cloning
    //@{
    /// Update this view to be a clone of view \a y
    void update(Space& home, bool share, VarImpView<Var>& y);
    //@}
  };

  /** \name View comparison
   *  \relates VarImpView
   */
  //@{
  /// Test whether views \a x and \a y are the same
  template<class VarA, class VarB>
  bool same(const VarImpView<VarA>& x, const VarImpView<VarB>& y);
  /// Test whether view \a x comes before \a y (arbitrary order)
  template<class ViewA, class ViewB>
  bool before(const ViewA& x, const ViewB& y);
  //@}


  /**
   * \brief Base-class for derived views
   * \ingroup TaskVarView
   */
  template<class View>
  class DerivedView {
  public:
    /// The variable implementation type belonging to the \a View
    typedef typename View::VarImpType VarImpType;
    /// The variable type belonging to the \a View
    typedef typename View::VarType VarType;
  protected:
    /// View from which this view is derived
    View x;
    /// Default constructor
    DerivedView(void);
    /// Initialize with view \a y
    DerivedView(const View& y);
  public:
    /// \name Generic view information
    //@{
    /// Return whether this view is derived from a VarImpView
    static bool varderived(void);
    /// Return variable implementation of view
    VarImpType* varimp(void) const;
    /// Return view from which this view is derived
    View base(void) const;
    /// Return degree (number of subscribed propagators)
    unsigned int degree(void) const;
    /// Return accumulated failure count
    double afc(const Space& home) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether view is assigned
    bool assigned(void) const;
    //@}

    /// \name View-dependent propagator support
    //@{
    /// Schedule propagator \a p with modification event \a me
    static void schedule(Space& home, Propagator& p, ModEvent me);
    /// Return modification event for view type in \a med
    static ModEvent me(const ModEventDelta& med);
    /// Translate modification event \a me to modification event delta for view
    static ModEventDelta med(ModEvent);
    //@}

    /// \name Dependencies
    //@{
    /**
     * \brief Subscribe propagator \a p with propagation condition \a pc to view
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     */
    void subscribe(Space& home, Propagator& p, PropCond pc, bool schedule=true);
    /// Cancel subscription of propagator \a p with propagation condition \a pc to view
    void cancel(Space& home, Propagator& p, PropCond pc);
    /// Subscribe advisor \a a to view
    void subscribe(Space& home, Advisor& a);
    /// Cancel subscription of advisor \a a
    void cancel(Space& home, Advisor& a);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return modification event
    static ModEvent modevent(const Delta& d);
    //@}

    /// \name Cloning
    //@{
    /// Update this view to be a clone of view \a y
    void update(Space& home, bool share, DerivedView<View>& y);
    //@}
  };


  /**
   * \brief Test whether views share same variable
   * \ingroup TaskVarView
   */
  template<class ViewA, class ViewB>
  bool shared(const ConstView<ViewA>&, const ConstView<ViewB>&);
  /**
   * \brief Test whether views share same variable
   * \ingroup TaskVarView
   */
  template<class Var, class View>
  bool shared(const VarImpView<Var>&, const ConstView<View>&);
  /**
   * \brief Test whether views share same variable
   * \ingroup TaskVarView
   */
  template<class ViewA, class ViewB>
  bool shared(const DerivedView<ViewA>&, const ConstView<ViewB>&);
  /**
   * \brief Test whether views share same variable
   * \ingroup TaskVarView
   */
  template<class View, class Var>
  bool shared(const ConstView<View>&, const VarImpView<Var>&);
  /**
   * \brief Test whether views share same variable
   * \ingroup TaskVarView
   */
  template<class ViewA, class ViewB>
  bool shared(const ConstView<ViewA>&, const DerivedView<ViewB>&);
  /**
   * \brief Test whether views share same variable
   * \ingroup TaskVarView
   */
  template<class VarA, class VarB>
  bool shared(const VarImpView<VarA>&, const VarImpView<VarB>&);
  /**
   * \brief Test whether views share same variable
   * \ingroup TaskVarView
   */
  template<class Var, class View>
  bool shared(const VarImpView<Var>&, const DerivedView<View>&);
  /**
   * \brief Test whether views share same variable
   * \ingroup TaskVarView
   */
  template<class View, class Var>
  bool shared(const DerivedView<View>&, const VarImpView<Var>&);
  /**
   * \brief Test whether views share same variable
   * \ingroup TaskVarView
   */
  template<class ViewA, class ViewB>
  bool shared(const DerivedView<ViewA>&, const DerivedView<ViewB>&);


  /*
   * Constant view: has no variable implementation
   *
   */
  template<class View>
  forceinline unsigned int
  ConstView<View>::degree(void) const {
    return 0;
  }
  template<class View>
  forceinline double
  ConstView<View>::afc(const Space&) const {
    return 0.0;
  }
  template<class View>
  forceinline bool
  ConstView<View>::varderived(void) {
    return false;
  }
  template<class View>
  forceinline typename View::VarImpType*
  ConstView<View>::varimp(void) const {
    return NULL;
  }
  template<class View>
  forceinline bool
  ConstView<View>::assigned(void) const {
    return true;
  }
  template<class View>
  forceinline void
  ConstView<View>::subscribe(Space& home, Propagator& p, PropCond, 
                             bool schedule) {
    if (schedule)
      View::schedule(home,p,ME_GEN_ASSIGNED);
  }
  template<class View>
  forceinline void
  ConstView<View>::cancel(Space&, Propagator&, PropCond) {
  }
  template<class View>
  forceinline void
  ConstView<View>::subscribe(Space&, Advisor&) {
  }
  template<class View>
  forceinline void
  ConstView<View>::cancel(Space&, Advisor&) {
  }
  template<class View>
  forceinline void
  ConstView<View>::schedule(Space& home, Propagator& p, ModEvent me) {
    View::schedule(home,p,me);
  }
  template<class View>
  forceinline ModEvent
  ConstView<View>::me(const ModEventDelta& med) {
    return View::me(med);
  }
  template<class View>
  forceinline ModEventDelta
  ConstView<View>::med(ModEvent me) {
    return View::med(me);
  }
  template<class View>
  forceinline ModEvent
  ConstView<View>::modevent(const Delta& d) {
    (void) d;
    return ME_GEN_NONE;
  }
  template<class View>
  forceinline void
  ConstView<View>::update(Space&, bool, ConstView<View>&) {
  }

  /*
   * Variable view: contains a pointer to a variable implementation
   *
   */
  template<class Var>
  forceinline
  VarImpView<Var>::VarImpView(void)
    : x(NULL) {}
  template<class Var>
  forceinline
  VarImpView<Var>::VarImpView(VarImpType* y)
    : x(y) {}
  template<class Var>
  forceinline bool
  VarImpView<Var>::varderived(void) {
    return true;
  }
  template<class Var>
  forceinline typename Var::VarImpType*
  VarImpView<Var>::varimp(void) const {
    return x;
  }
  template<class Var>
  forceinline unsigned int
  VarImpView<Var>::degree(void) const {
    return x->degree();
  }
  template<class Var>
  forceinline double
  VarImpView<Var>::afc(const Space& home) const {
    return x->afc(home);
  }
  template<class Var>
  forceinline bool
  VarImpView<Var>::assigned(void) const {
    return x->assigned();
  }
  template<class Var>
  forceinline void
  VarImpView<Var>::subscribe(Space& home, Propagator& p, PropCond pc,
                             bool schedule) {
    x->subscribe(home,p,pc,schedule);
  }
  template<class Var>
  forceinline void
  VarImpView<Var>::cancel(Space& home, Propagator& p, PropCond pc) {
    x->cancel(home,p,pc);
  }
  template<class Var>
  forceinline void
  VarImpView<Var>::subscribe(Space& home, Advisor& a) {
    x->subscribe(home,a);
  }
  template<class Var>
  forceinline void
  VarImpView<Var>::cancel(Space& home, Advisor& a) {
    x->cancel(home,a);
  }
  template<class Var>
  forceinline void
  VarImpView<Var>::schedule(Space& home, Propagator& p, ModEvent me) {
    VarImpType::schedule(home,p,me);
  }
  template<class Var>
  forceinline ModEvent
  VarImpView<Var>::me(const ModEventDelta& med) {
    return VarImpType::me(med);
  }
  template<class Var>
  forceinline ModEventDelta
  VarImpView<Var>::med(ModEvent me) {
    return VarImpType::med(me);
  }
  template<class Var>
  forceinline ModEvent
  VarImpView<Var>::modevent(const Delta& d) {
    return VarImpType::modevent(d);
  }
  template<class Var>
  forceinline void
  VarImpView<Var>::update(Space& home, bool share, VarImpView<Var>& y) {
    x = y.x->copy(home,share);
  }

  /*
   * Derived view: contain the base view from which they are derived
   *
   */

  template<class View>
  forceinline
  DerivedView<View>::DerivedView(void) {}

  template<class View>
  forceinline
  DerivedView<View>::DerivedView(const View& y)
    : x(y) {}

  template<class View>
  forceinline bool
  DerivedView<View>::varderived(void) {
    return View::varderived();
  }

  template<class View>
  forceinline typename View::VarImpType*
  DerivedView<View>::varimp(void) const {
    return x.varimp();
  }

  template<class View>
  forceinline View
  DerivedView<View>::base(void) const {
    return x;
  }

  template<class View>
  forceinline unsigned int
  DerivedView<View>::degree(void) const {
    return x.degree();
  }
  template<class View>
  forceinline double
  DerivedView<View>::afc(const Space& home) const {
    return x.afc(home);
  }
  template<class View>
  forceinline bool
  DerivedView<View>::assigned(void) const {
    return x.assigned();
  }

  template<class View>
  forceinline void
  DerivedView<View>::schedule(Space& home, Propagator& p, ModEvent me) {
    return View::schedule(home,p,me);
  }
  template<class View>
  forceinline ModEvent
  DerivedView<View>::me(const ModEventDelta& med) {
    return View::me(med);
  }
  template<class View>
  forceinline ModEventDelta
  DerivedView<View>::med(ModEvent me) {
    return View::med(me);
  }

  template<class View>
  forceinline void
  DerivedView<View>::subscribe(Space& home, Propagator& p, PropCond pc,
                               bool schedule) {
    x.subscribe(home,p,pc,schedule);
  }
  template<class View>
  forceinline void
  DerivedView<View>::cancel(Space& home, Propagator& p, PropCond pc) {
    x.cancel(home,p,pc);
  }
  template<class View>
  forceinline void
  DerivedView<View>::subscribe(Space& home, Advisor& a) {
    x.subscribe(home,a);
  }
  template<class View>
  forceinline void
  DerivedView<View>::cancel(Space& home, Advisor& a) {
    x.cancel(home,a);
  }
  template<class View>
  forceinline ModEvent
  DerivedView<View>::modevent(const Delta& d) {
    return View::modevent(d);
  }
  template<class View>
  forceinline void
  DerivedView<View>::update(Space& home, bool share, DerivedView<View>& y) {
    x.update(home,share,y.x);
  }


  /*
   * Tests whether two views are the same
   *
   */

  /// Test whether two views are the same
  template<class ViewA, class ViewB>
  forceinline bool 
  same(const ConstView<ViewA>&, const ConstView<ViewB>&) {
    return false;
  }
  /// Test whether two views are the same
  template<class Var, class View>
  forceinline bool 
  same(const VarImpView<Var>&, const ConstView<View>&) {
    return false;
  }
  /// Test whether two views are the same
  template<class ViewA, class ViewB>
  forceinline bool 
  same(const ConstView<ViewA>&, const DerivedView<ViewB>&) {
    return false;
  }
  /// Test whether two views are the same
  template<class Var, class View>
  forceinline bool 
  same(const VarImpView<Var>&, const DerivedView<View>&) {
    return false;
  }
  /// Test whether two views are the same
  template<class View, class Var>
  forceinline bool 
  same(const DerivedView<View>&, const VarImpView<Var>&) {
    return false;
  }
  /// Test whether two views are the same
  template<class Var>
  forceinline bool
  same(const VarImpView<Var>& x, const VarImpView<Var>& y) {
    return x.varimp() == y.varimp();
  }
  /// Test whether two views are the same
  template<class ViewA, class ViewB>
  forceinline bool 
  same(const DerivedView<ViewA>& x, const DerivedView<ViewB>& y) {
    return same(x.base(),y.base());
  }


  /*
   * Tests whether one view is before the other
   *
   */
  template<class ViewA, class ViewB>
  forceinline bool
  before(const ViewA& x, const ViewB& y) {
    return x.varimp() < y.varimp();
  }


  /*
   * Testing whether two views share the same variable
   *
   */

  template<class ViewA, class ViewB>
  forceinline bool
  shared(const ConstView<ViewA>&, const ConstView<ViewB>&) {
    return false;
  }
  template<class Var, class View>
  forceinline bool
  shared(const VarImpView<Var>&, const ConstView<View>&) {
    return false;
  }
  template<class ViewA, class ViewB>
  forceinline bool
  shared(const DerivedView<ViewA>&, const ConstView<ViewB>&) {
    return false;
  }
  template<class View, class Var>
  forceinline bool
  shared(const ConstView<View>&, const VarImpView<Var>&) {
    return false;
  }
  template<class ViewA, class ViewB>
  forceinline bool
  shared(const ConstView<ViewA>&, const DerivedView<ViewB>&) {
    return false;
  }
  template<class VarA, class VarB>
  forceinline bool
  shared(const VarImpView<VarA>& x, const VarImpView<VarB>& y) {
    return (static_cast<VarImpBase*>(x.varimp()) ==
            static_cast<VarImpBase*>(y.varimp()));
  }
  template<class Var, class View>
  forceinline bool
  shared(const VarImpView<Var>& x, const DerivedView<View>& y) {
    return (View::varderived() &&
            static_cast<VarImpBase*>(x.varimp()) ==
            static_cast<VarImpBase*>(y.varimp()));
  }
  template<class View, class Var>
  forceinline bool
  shared(const DerivedView<View>& x, const VarImpView<Var>& y) {
    return (View::varderived() &&
            static_cast<VarImpBase*>(x.varimp()) ==
            static_cast<VarImpBase*>(y.varimp()));
  }
  template<class ViewA, class ViewB>
  forceinline bool
  shared(const DerivedView<ViewA>& x, const DerivedView<ViewB>& y) {
    return (ViewA::varderived() && ViewB::varderived() &&
            static_cast<VarImpBase*>(x.varimp()) ==
            static_cast<VarImpBase*>(y.varimp()));
  }

}

// STATISTICS: kernel-var
