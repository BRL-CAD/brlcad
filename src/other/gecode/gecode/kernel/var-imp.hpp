/*
 *  CAUTION:
 *    This file has been automatically generated. Do not edit,
 *    edit the following files instead:
 *     - ./gecode/int/var-imp/int.vis
 *     - ./gecode/int/var-imp/bool.vis
 *     - ./gecode/set/var-imp/set.vis
 *     - ./gecode/float/var-imp/float.vis
 *
 *  This file contains generated code fragments which are
 *  copyrighted as follows:
 *
 *  Main author:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2007
 *
 *  The generated code fragments are part of Gecode, the generic
 *  constraint development environment:
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

#ifdef GECODE_HAS_INT_VARS
namespace Gecode { namespace Int { 
  /// Base-class for Int-variable implementations
  class IntVarImpBase : public Gecode::VarImp<Gecode::Int::IntVarImpConf> {
  protected:
    /// Constructor for cloning \a x
    IntVarImpBase(Gecode::Space& home, bool share, IntVarImpBase& x);
  public:
    /// Constructor for creating static instance of variable
    IntVarImpBase(void);
    /// Constructor for creating variable
    IntVarImpBase(Gecode::Space& home);
    /// \name Dependencies
    //@{
    /** \brief Subscribe propagator \a p with propagation condition \a pc
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     *
     * In case the variable is assigned (that is, \a assigned is
     * true), the subscribing propagator is scheduled for execution.
     * Otherwise, the propagator subscribes and is scheduled for execution
     * with modification event \a me provided that \a pc is different
     * from \a PC_INT_VAL.
     */
    void subscribe(Gecode::Space& home, Gecode::Propagator& p, Gecode::PropCond pc, bool assigned, bool schedule);
    /// Subscribe advisor \a a if \a assigned is false.
    void subscribe(Gecode::Space& home, Gecode::Advisor& a, bool assigned);
    /// Notify that variable implementation has been modified with modification event \a me and delta information \a d
    Gecode::ModEvent notify(Gecode::Space& home, Gecode::ModEvent me, Gecode::Delta& d);
    //@}
  };
}}
#endif
#ifdef GECODE_HAS_INT_VARS
namespace Gecode { namespace Int { 
  /// Base-class for Bool-variable implementations
  class BoolVarImpBase : public Gecode::VarImp<Gecode::Int::BoolVarImpConf> {
  protected:
    /// Constructor for cloning \a x
    BoolVarImpBase(Gecode::Space& home, bool share, BoolVarImpBase& x);
  public:
    /// Constructor for creating static instance of variable
    BoolVarImpBase(void);
    /// Constructor for creating variable
    BoolVarImpBase(Gecode::Space& home);
    /// \name Dependencies
    //@{
    /** \brief Subscribe propagator \a p with propagation condition \a pc
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     *
     * In case the variable is assigned (that is, \a assigned is
     * true), the subscribing propagator is scheduled for execution.
     * Otherwise, the propagator subscribes and is scheduled for execution
     * with modification event \a me provided that \a pc is different
     * from \a PC_BOOL_VAL.
     */
    void subscribe(Gecode::Space& home, Gecode::Propagator& p, Gecode::PropCond pc, bool assigned, bool schedule);
    /// Subscribe advisor \a a if \a assigned is false.
    void subscribe(Gecode::Space& home, Gecode::Advisor& a, bool assigned);
    /// Notify that variable implementation has been modified with modification event \a me and delta information \a d
    Gecode::ModEvent notify(Gecode::Space& home, Gecode::ModEvent me, Gecode::Delta& d);
    //@}
  };
}}
#endif
#ifdef GECODE_HAS_SET_VARS
namespace Gecode { namespace Set { 
  /// Base-class for Set-variable implementations
  class SetVarImpBase : public Gecode::VarImp<Gecode::Set::SetVarImpConf> {
  protected:
    /// Constructor for cloning \a x
    SetVarImpBase(Gecode::Space& home, bool share, SetVarImpBase& x);
  public:
    /// Constructor for creating static instance of variable
    SetVarImpBase(void);
    /// Constructor for creating variable
    SetVarImpBase(Gecode::Space& home);
    /// \name Dependencies
    //@{
    /** \brief Subscribe propagator \a p with propagation condition \a pc
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     *
     * In case the variable is assigned (that is, \a assigned is
     * true), the subscribing propagator is scheduled for execution.
     * Otherwise, the propagator subscribes and is scheduled for execution
     * with modification event \a me provided that \a pc is different
     * from \a PC_SET_VAL.
     */
    void subscribe(Gecode::Space& home, Gecode::Propagator& p, Gecode::PropCond pc, bool assigned, bool schedule);
    /// Subscribe advisor \a a if \a assigned is false.
    void subscribe(Gecode::Space& home, Gecode::Advisor& a, bool assigned);
    /// Notify that variable implementation has been modified with modification event \a me and delta information \a d
    Gecode::ModEvent notify(Gecode::Space& home, Gecode::ModEvent me, Gecode::Delta& d);
    //@}
  };
}}
#endif
#ifdef GECODE_HAS_FLOAT_VARS
namespace Gecode { namespace Float { 
  /// Base-class for Float-variable implementations
  class FloatVarImpBase : public Gecode::VarImp<Gecode::Float::FloatVarImpConf> {
  protected:
    /// Constructor for cloning \a x
    FloatVarImpBase(Gecode::Space& home, bool share, FloatVarImpBase& x);
  public:
    /// Constructor for creating static instance of variable
    FloatVarImpBase(void);
    /// Constructor for creating variable
    FloatVarImpBase(Gecode::Space& home);
    /// \name Dependencies
    //@{
    /** \brief Subscribe propagator \a p with propagation condition \a pc
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     *
     * In case the variable is assigned (that is, \a assigned is
     * true), the subscribing propagator is scheduled for execution.
     * Otherwise, the propagator subscribes and is scheduled for execution
     * with modification event \a me provided that \a pc is different
     * from \a PC_FLOAT_VAL.
     */
    void subscribe(Gecode::Space& home, Gecode::Propagator& p, Gecode::PropCond pc, bool assigned, bool schedule);
    /// Subscribe advisor \a a if \a assigned is false.
    void subscribe(Gecode::Space& home, Gecode::Advisor& a, bool assigned);
    /// Notify that variable implementation has been modified with modification event \a me and delta information \a d
    Gecode::ModEvent notify(Gecode::Space& home, Gecode::ModEvent me, Gecode::Delta& d);
    //@}
  };
}}
#endif
#ifdef GECODE_HAS_INT_VARS
namespace Gecode { namespace Int { 

  forceinline
  IntVarImpBase::IntVarImpBase(void) {}

  forceinline
  IntVarImpBase::IntVarImpBase(Gecode::Space& home)
    : Gecode::VarImp<Gecode::Int::IntVarImpConf>(home) {}

  forceinline
  IntVarImpBase::IntVarImpBase(Gecode::Space& home, bool share, IntVarImpBase& x)
    : Gecode::VarImp<Gecode::Int::IntVarImpConf>(home,share,x) {}

  forceinline void
  IntVarImpBase::subscribe(Gecode::Space& home, Gecode::Propagator& p, Gecode::PropCond pc, bool assigned, bool schedule) {
    Gecode::VarImp<Gecode::Int::IntVarImpConf>::subscribe(home,p,pc,assigned,ME_INT_BND,schedule);
  }
  forceinline void
  IntVarImpBase::subscribe(Gecode::Space& home, Gecode::Advisor& a, bool assigned) {
    Gecode::VarImp<Gecode::Int::IntVarImpConf>::subscribe(home,a,assigned);
  }

  forceinline Gecode::ModEvent
  IntVarImpBase::notify(Gecode::Space& home, Gecode::ModEvent me, Gecode::Delta& d) {
    switch (me) {
    case ME_INT_VAL:
      // Conditions: VAL, BND, DOM
      schedule(home,PC_INT_VAL,PC_INT_DOM,ME_INT_VAL);
      if (!Gecode::VarImp<Gecode::Int::IntVarImpConf>::advise(home,ME_INT_VAL,d))
        return ME_INT_FAILED;
      cancel(home);
      break;
    case ME_INT_BND:
      // Conditions: BND, DOM
      schedule(home,PC_INT_BND,PC_INT_DOM,ME_INT_BND);
      if (!Gecode::VarImp<Gecode::Int::IntVarImpConf>::advise(home,ME_INT_BND,d))
        return ME_INT_FAILED;
      break;
    case ME_INT_DOM:
      // Conditions: DOM
      schedule(home,PC_INT_DOM,PC_INT_DOM,ME_INT_DOM);
      if (!Gecode::VarImp<Gecode::Int::IntVarImpConf>::advise(home,ME_INT_DOM,d))
        return ME_INT_FAILED;
      break;
    default: GECODE_NEVER;
    }
    return me;
  }

}}
#endif
#ifdef GECODE_HAS_INT_VARS
namespace Gecode { namespace Int { 

  forceinline
  BoolVarImpBase::BoolVarImpBase(void) {}

  forceinline
  BoolVarImpBase::BoolVarImpBase(Gecode::Space& home)
    : Gecode::VarImp<Gecode::Int::BoolVarImpConf>(home) {}

  forceinline
  BoolVarImpBase::BoolVarImpBase(Gecode::Space& home, bool share, BoolVarImpBase& x)
    : Gecode::VarImp<Gecode::Int::BoolVarImpConf>(home,share,x) {}

  forceinline void
  BoolVarImpBase::subscribe(Gecode::Space& home, Gecode::Propagator& p, Gecode::PropCond pc, bool assigned, bool schedule) {
    Gecode::VarImp<Gecode::Int::BoolVarImpConf>::subscribe(home,p,pc,assigned,ME_BOOL_VAL,schedule);
  }
  forceinline void
  BoolVarImpBase::subscribe(Gecode::Space& home, Gecode::Advisor& a, bool assigned) {
    Gecode::VarImp<Gecode::Int::BoolVarImpConf>::subscribe(home,a,assigned);
  }

  forceinline Gecode::ModEvent
  BoolVarImpBase::notify(Gecode::Space& home, Gecode::ModEvent, Gecode::Delta& d) {
    schedule(home,PC_BOOL_VAL,PC_BOOL_VAL,ME_BOOL_VAL);
    if (!Gecode::VarImp<Gecode::Int::BoolVarImpConf>::advise(home,ME_BOOL_VAL,d))
      return ME_BOOL_FAILED;
    cancel(home);
    return ME_BOOL_VAL;
  }

}}
#endif
#ifdef GECODE_HAS_SET_VARS
namespace Gecode { namespace Set { 

  forceinline
  SetVarImpBase::SetVarImpBase(void) {}

  forceinline
  SetVarImpBase::SetVarImpBase(Gecode::Space& home)
    : Gecode::VarImp<Gecode::Set::SetVarImpConf>(home) {}

  forceinline
  SetVarImpBase::SetVarImpBase(Gecode::Space& home, bool share, SetVarImpBase& x)
    : Gecode::VarImp<Gecode::Set::SetVarImpConf>(home,share,x) {}

  forceinline void
  SetVarImpBase::subscribe(Gecode::Space& home, Gecode::Propagator& p, Gecode::PropCond pc, bool assigned, bool schedule) {
    Gecode::VarImp<Gecode::Set::SetVarImpConf>::subscribe(home,p,pc,assigned,ME_SET_CBB,schedule);
  }
  forceinline void
  SetVarImpBase::subscribe(Gecode::Space& home, Gecode::Advisor& a, bool assigned) {
    Gecode::VarImp<Gecode::Set::SetVarImpConf>::subscribe(home,a,assigned);
  }

  forceinline Gecode::ModEvent
  SetVarImpBase::notify(Gecode::Space& home, Gecode::ModEvent me, Gecode::Delta& d) {
    switch (me) {
    case ME_SET_VAL:
      // Conditions: VAL, CARD, CLUB, CGLB, ANY
      schedule(home,PC_SET_VAL,PC_SET_ANY,ME_SET_VAL);
      if (!Gecode::VarImp<Gecode::Set::SetVarImpConf>::advise(home,ME_SET_VAL,d))
        return ME_SET_FAILED;
      cancel(home);
      break;
    case ME_SET_CARD:
      // Conditions: CARD, CLUB, CGLB, ANY
      schedule(home,PC_SET_CARD,PC_SET_ANY,ME_SET_CARD);
      if (!Gecode::VarImp<Gecode::Set::SetVarImpConf>::advise(home,ME_SET_CARD,d))
        return ME_SET_FAILED;
      break;
    case ME_SET_LUB:
      // Conditions: CLUB, ANY
      schedule(home,PC_SET_CLUB,PC_SET_CLUB,ME_SET_LUB);
      schedule(home,PC_SET_ANY,PC_SET_ANY,ME_SET_LUB);
      if (!Gecode::VarImp<Gecode::Set::SetVarImpConf>::advise(home,ME_SET_LUB,d))
        return ME_SET_FAILED;
      break;
    case ME_SET_GLB:
      // Conditions: CGLB, ANY
      schedule(home,PC_SET_CGLB,PC_SET_ANY,ME_SET_GLB);
      if (!Gecode::VarImp<Gecode::Set::SetVarImpConf>::advise(home,ME_SET_GLB,d))
        return ME_SET_FAILED;
      break;
    case ME_SET_BB:
      // Conditions: CLUB, CGLB, ANY
      schedule(home,PC_SET_CLUB,PC_SET_ANY,ME_SET_BB);
      if (!Gecode::VarImp<Gecode::Set::SetVarImpConf>::advise(home,ME_SET_BB,d))
        return ME_SET_FAILED;
      break;
    case ME_SET_CLUB:
      // Conditions: CARD, CLUB, CGLB, ANY
      schedule(home,PC_SET_CARD,PC_SET_ANY,ME_SET_CLUB);
      if (!Gecode::VarImp<Gecode::Set::SetVarImpConf>::advise(home,ME_SET_CLUB,d))
        return ME_SET_FAILED;
      break;
    case ME_SET_CGLB:
      // Conditions: CARD, CLUB, CGLB, ANY
      schedule(home,PC_SET_CARD,PC_SET_ANY,ME_SET_CGLB);
      if (!Gecode::VarImp<Gecode::Set::SetVarImpConf>::advise(home,ME_SET_CGLB,d))
        return ME_SET_FAILED;
      break;
    case ME_SET_CBB:
      // Conditions: CARD, CLUB, CGLB, ANY
      schedule(home,PC_SET_CARD,PC_SET_ANY,ME_SET_CBB);
      if (!Gecode::VarImp<Gecode::Set::SetVarImpConf>::advise(home,ME_SET_CBB,d))
        return ME_SET_FAILED;
      break;
    default: GECODE_NEVER;
    }
    return me;
  }

}}
#endif
#ifdef GECODE_HAS_FLOAT_VARS
namespace Gecode { namespace Float { 

  forceinline
  FloatVarImpBase::FloatVarImpBase(void) {}

  forceinline
  FloatVarImpBase::FloatVarImpBase(Gecode::Space& home)
    : Gecode::VarImp<Gecode::Float::FloatVarImpConf>(home) {}

  forceinline
  FloatVarImpBase::FloatVarImpBase(Gecode::Space& home, bool share, FloatVarImpBase& x)
    : Gecode::VarImp<Gecode::Float::FloatVarImpConf>(home,share,x) {}

  forceinline void
  FloatVarImpBase::subscribe(Gecode::Space& home, Gecode::Propagator& p, Gecode::PropCond pc, bool assigned, bool schedule) {
    Gecode::VarImp<Gecode::Float::FloatVarImpConf>::subscribe(home,p,pc,assigned,ME_FLOAT_BND,schedule);
  }
  forceinline void
  FloatVarImpBase::subscribe(Gecode::Space& home, Gecode::Advisor& a, bool assigned) {
    Gecode::VarImp<Gecode::Float::FloatVarImpConf>::subscribe(home,a,assigned);
  }

  forceinline Gecode::ModEvent
  FloatVarImpBase::notify(Gecode::Space& home, Gecode::ModEvent me, Gecode::Delta& d) {
    switch (me) {
    case ME_FLOAT_VAL:
      // Conditions: VAL, BND
      schedule(home,PC_FLOAT_VAL,PC_FLOAT_BND,ME_FLOAT_VAL);
      if (!Gecode::VarImp<Gecode::Float::FloatVarImpConf>::advise(home,ME_FLOAT_VAL,d))
        return ME_FLOAT_FAILED;
      cancel(home);
      break;
    case ME_FLOAT_BND:
      // Conditions: BND
      schedule(home,PC_FLOAT_BND,PC_FLOAT_BND,ME_FLOAT_BND);
      if (!Gecode::VarImp<Gecode::Float::FloatVarImpConf>::advise(home,ME_FLOAT_BND,d))
        return ME_FLOAT_FAILED;
      break;
    default: GECODE_NEVER;
    }
    return me;
  }

}}
#endif
namespace Gecode {

  forceinline void
  Space::update(ActorLink** sub) {
#ifdef GECODE_HAS_INT_VARS
    Gecode::VarImp<Gecode::Int::IntVarImpConf>::update(*this,sub);
#endif
#ifdef GECODE_HAS_INT_VARS
    Gecode::VarImp<Gecode::Int::BoolVarImpConf>::update(*this,sub);
#endif
#ifdef GECODE_HAS_SET_VARS
    Gecode::VarImp<Gecode::Set::SetVarImpConf>::update(*this,sub);
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    Gecode::VarImp<Gecode::Float::FloatVarImpConf>::update(*this,sub);
#endif
  }
}
// STATISTICS: kernel-var
