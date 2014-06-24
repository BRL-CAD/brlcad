/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
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

#include <iostream>

namespace Gecode { namespace Set {

  /**
   * \defgroup TaskActorSetView Set views
   *
   * Set propagators and branchings compute with set views.
   * Set views provide views on set variable implementations,
   * set constants, and integer variable implementations.
   * \ingroup TaskActorSet
   */

  /**
   * \brief %Set view for set variables
   * \ingroup TaskActorSetView
   */

  class SetView : public VarImpView<SetVar> {
  protected:
    using VarImpView<SetVar>::x;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    SetView(void);
    /// Initialize from set variable \a y
    SetView(const SetVar& y);
    /// Initialize from set variable implementation \a y
    SetView(SetVarImp* y);
    //@}

    /// \name Value access
    //@{

    /// Return minimum cardinality
    unsigned int cardMin(void) const;
    /// Return maximum cardinality
    unsigned int cardMax(void) const;
    /// Return minimum of the least upper bound
    int lubMin(void) const;
    /// Return maximum of the least upper bound
    int lubMax(void) const;
    /// Return n-th smallest element of the least upper bound
    int lubMinN(unsigned int n) const;
    /// Return minimum of the greatest lower bound
    int glbMin(void) const;
    /// Return maximum of the greatest lower bound
    int glbMax(void) const;

    /// Return the number of elements in the greatest lower bound
    unsigned int glbSize(void) const;
    /// Return the number of elements in the least upper bound
    unsigned int lubSize(void) const;
    /// Return the number of unknown elements
    unsigned int unknownSize(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether \a i is in the greatest lower bound
    bool contains(int i) const;
    /// Test whether \a i is not in the least upper bound
    bool notContains(int i) const;
    //@}


    /// \name Domain update by value
    //@{
    /// Restrict cardinality to be greater than or equal to \a m
    ModEvent cardMin(Space& home, unsigned int m);
    /// Restrict cardinality to be less than or equal to \a m
    ModEvent cardMax(Space& home, unsigned int m);
    /**
     * \brief Update greatest lower bound to include all elements
     * between and including \a i and \a j
     */
    ModEvent include(Space& home,int i,int j);
    /**
     * \brief Restrict least upper bound to not contain all elements
     * between and including \a i and \a j
     */
    ModEvent exclude(Space& home,int i,int j);
    /// Update greatest lower bound to contain \a i
    ModEvent include(Space& home,int i);
    /// Restrict least upper bound to not contain \a i
    ModEvent exclude(Space& home,int i);
    /**
     * \brief Update least upper bound to contain at most all elements
     * between and including \a i and \a j
     */
    ModEvent intersect(Space& home,int i,int j);
    /// Update least upper bound to contain at most the element \a i
    ModEvent intersect(Space& home,int i);
    //@}

    /// \name Domain update by range iterator
    //@{

    /// Remove range sequence described by \a i from least upper bound
    template<class I> ModEvent excludeI(Space& home, I& i);
    /// Include range sequence described by \a i in greatest lower bound
    template<class I> ModEvent includeI(Space& home, I& i);
    /// Intersect least upper bound with range sequence described by \a i
    template<class I> ModEvent intersectI(Space& home, I& iter);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return modification event
    static ModEvent modevent(const Delta& d);
    /// Return minimum value just pruned from glb
    int glbMin(const Delta& d) const;
    /// Return maximum value just pruned from glb
    int glbMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from glb
    bool glbAny(const Delta& d) const;
    /// Return minimum value just pruned from lub
    int lubMin(const Delta& d) const;
    /// Return maximum value just pruned from lub
    int lubMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from lub
    bool lubAny(const Delta& d) const;
    //@}
  };

  /**
   * \brief Print set variable view
   * \relates Gecode::Set::SetView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const SetView& x);



  // Forward declarations for friends
  class ConstSetView;
  bool same(const ConstSetView&, const ConstSetView&);
  bool before(const ConstSetView&, const ConstSetView&);

  /**
   * \brief Constant view
   *
   * A constant set view \f$x\f$ for a set \f$s\f$ provides operations such
   * that \f$x\f$ behaves like \f$s\f$.
   * \ingroup TaskActorSetView
   */
  class ConstSetView : public ConstView<SetView> {
    friend class LubRanges<ConstSetView>;
    friend class GlbRanges<ConstSetView>;
    friend bool Gecode::Set::same(const Gecode::Set::ConstSetView&,
                                  const Gecode::Set::ConstSetView&);
    friend bool Gecode::Set::before(const Gecode::Set::ConstSetView&,
                                    const Gecode::Set::ConstSetView&);
  private:
    int *ranges;
    int size;
    unsigned int domSize;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ConstSetView(void);
    /// Construct with \a s as the domain
    ConstSetView(Space& home, const IntSet& s);
    //@}

    /// \name Value access
    //@{
    /// Return minimum cardinality
    unsigned int cardMin(void) const;
    /// Return maximum cardinality
    unsigned int cardMax(void) const;
    /// Return minimum of the least upper bound
    int lubMin(void) const;
    /// Return maximum of the least upper bound
    int lubMax(void) const;
    /// Return n-th smallest element of the least upper bound
    int lubMinN(unsigned int n) const;
    /// Return minimum of the greatest lower bound
    int glbMin(void) const;
    /// Return maximum of the greatest lower bound
    int glbMax(void) const;

    /// Return the number of elements in the greatest lower bound
    unsigned int glbSize(void) const;
    /// Return the number of elements in the least upper bound
    unsigned int lubSize(void) const;
    /// Return the number of unknown elements
    unsigned int unknownSize(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether \a i is in the greatest lower bound
    bool contains(int i) const;
    /// Test whether \a i is not in the least upper bound
    bool notContains(int i) const;
    //@}


    /// \name Domain update by value
    //@{
    /// Restrict cardinality to be greater than or equal to \a m
    ModEvent cardMin(Space& home, unsigned int m);
    /// Restrict cardinality to be less than or equal to \a m
    ModEvent cardMax(Space& home, unsigned int m);
    /**
     * \brief Update greatest lower bound to include all elements
     * between and including \a i and \a j
     */
    ModEvent include(Space& home,int i,int j);
    /**
     * \brief Restrict least upper bound to not contain all elements
     * between and including \a i and \a j
     */
    ModEvent exclude(Space& home,int i,int j);
    /// Update greatest lower bound to contain \a i
    ModEvent include(Space& home,int i);
    /// Restrict least upper bound to not contain \a i
    ModEvent exclude(Space& home,int i);
    /**
     * \brief Update least upper bound to contain at most all elements
     * between and including \a i and \a j
     */
    ModEvent intersect(Space& home,int i,int j);
    /// Update least upper bound to contain at most the element \a i
    ModEvent intersect(Space& home,int i);
    //@}

    /// \name Domain update by range iterator
    //@{

    /// Remove range sequence described by \a i from least upper bound
    template<class I> ModEvent excludeI(Space& home, I& i);
    /// Include range sequence described by \a i in greatest lower bound
    template<class I> ModEvent includeI(Space& home, I& i);
    /// Intersect least upper bound with range sequence described by \a i
    template<class I> ModEvent intersectI(Space& home, I& iter);
    //@}

    /// \name Cloning
    //@{
    /// Update this view to be a clone of view \a y
    void update(Space& home, bool share, ConstSetView& y);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned from glb
    int glbMin(const Delta& d) const;
    /// Return maximum value just pruned from glb
    int glbMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from glb
    bool glbAny(const Delta& d) const;
    /// Return minimum value just pruned from lub
    int lubMin(const Delta& d) const;
    /// Return maximum value just pruned from lub
    int lubMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from lub
    bool lubAny(const Delta& d) const;
    //@}

  };

  /**
   * \brief Print constant set view
   * \relates Gecode::Set::ConstSetView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ConstSetView& x);

  /** \name View comparison
   *  \relates Gecode::Set::ConstSetView
   */
  //@{
  /// Test whether views \a x and \a y are the same
  bool same(const ConstSetView& x, const ConstSetView& y);
  /// Test whether view \a x comes before \a y (arbitrary order)
  bool before(const ConstSetView& x, const ConstSetView& y);
  //@}


  /**
   * \brief Constant view for the empty set
   *
   * A constant set view \f$x\f$ for the empty set provides operations such
   * that \f$x\f$ behaves like the empty set.
   * \ingroup TaskActorSetView
   */

  class EmptyView : public ConstView<SetView> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    EmptyView(void);
    //@}

    /// \name Value access
    //@{
    /// Return minimum cardinality
    unsigned int cardMin(void) const;
    /// Return maximum cardinality
    unsigned int cardMax(void) const;
    /// Return minimum of the least upper bound
    int lubMin(void) const;
    /// Return maximum of the least upper bound
    int lubMax(void) const;
    /// Return n-th smallest element of the least upper bound
    int lubMinN(unsigned int n) const;
    /// Return minimum of the greatest lower bound
    int glbMin(void) const;
    /// Return maximum of the greatest lower bound
    int glbMax(void) const;

    /// Return the number of elements in the greatest lower bound
    unsigned int glbSize(void) const;
    /// Return the number of elements in the least upper bound
    unsigned int lubSize(void) const;
    /// Return the number of unknown elements
    unsigned int unknownSize(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether \a i is in the greatest lower bound
    bool contains(int i) const;
    /// Test whether \a i is not in the least upper bound
    bool notContains(int i) const;
    //@}


    /// \name Domain update by value
    //@{
    /// Restrict cardinality to be greater than or equal to \a m
    ModEvent cardMin(Space& home, unsigned int m);
    /// Restrict cardinality to be less than or equal to \a m
    ModEvent cardMax(Space& home, unsigned int m);
    /**
     * \brief Update greatest lower bound to include all elements
     * between and including \a i and \a j
     */
    ModEvent include(Space& home,int i,int j);
    /**
     * \brief Restrict least upper bound to not contain all elements
     * between and including \a i and \a j
     */
    ModEvent exclude(Space& home,int i,int j);
    /// Update greatest lower bound to contain \a i
    ModEvent include(Space& home,int i);
    /// Restrict least upper bound to not contain \a i
    ModEvent exclude(Space& home,int i);
    /**
     * \brief Update least upper bound to contain at most all elements
     * between and including \a i and \a j
     */
    ModEvent intersect(Space& home,int i,int j);
    /// Update least upper bound to contain at most the element \a i
    ModEvent intersect(Space& home,int i);
    //@}

    /// \name Domain update by range iterator
    //@{

    /// Remove range sequence described by \a i from least upper bound
    template<class I> ModEvent excludeI(Space& home, I& i);
    /// Include range sequence described by \a i in greatest lower bound
    template<class I> ModEvent includeI(Space& home, I& i);
    /// Intersect least upper bound with range sequence described by \a i
    template<class I> ModEvent intersectI(Space& home, I& iter);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned from glb
    int glbMin(const Delta& d) const;
    /// Return maximum value just pruned from glb
    int glbMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from glb
    bool glbAny(const Delta& d) const;
    /// Return minimum value just pruned from lub
    int lubMin(const Delta& d) const;
    /// Return maximum value just pruned from lub
    int lubMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from lub
    bool lubAny(const Delta& d) const;
    //@}

  };

  /**
   * \brief Print empty set view
   * \relates Gecode::Set::EmptyView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const EmptyView& x);


  /** \name View comparison
   *  \relates Gecode::Set::EmptyView
   */
  //@{
  /// Test whether views \a x and \a y are the same
  bool same(const EmptyView& x, const EmptyView& y);
  //@}


  /**
   * \brief Constant view for the universe
   *
   * A constant set view \f$x\f$ for the universe provides operations such
   * that \f$x\f$ behaves like the universe.
   * \ingroup TaskActorSetView
   */

  class UniverseView : public ConstView<SetView> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    UniverseView(void);
    //@}

    /// \name Value access
    //@{

    /// Return minimum cardinality
    unsigned int cardMin(void) const;
    /// Return maximum cardinality
    unsigned int cardMax(void) const;
    /// Return minimum of the least upper bound
    int lubMin(void) const;
    /// Return maximum of the least upper bound
    int lubMax(void) const;
    /// Return n-th smallest element of the least upper bound
    int lubMinN(unsigned int n) const;
    /// Return minimum of the greatest lower bound
    int glbMin(void) const;
    /// Return maximum of the greatest lower bound
    int glbMax(void) const;

    /// Return the number of elements in the greatest lower bound
    unsigned int glbSize(void) const;
    /// Return the number of elements in the least upper bound
    unsigned int lubSize(void) const;
    /// Return the number of unknown elements
    unsigned int unknownSize(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether \a i is in the greatest lower bound
    bool contains(int i) const;
    /// Test whether \a i is not in the least upper bound
    bool notContains(int i) const;
    //@}


    /// \name Domain update by value
    //@{
    /// Restrict cardinality to be greater than or equal to \a m
    ModEvent cardMin(Space& home, unsigned int m);
    /// Restrict cardinality to be less than or equal to \a m
    ModEvent cardMax(Space& home, unsigned int m);
    /**
     * \brief Update greatest lower bound to include all elements
     * between and including \a i and \a j
     */
    ModEvent include(Space& home,int i,int j);
    /**
     * \brief Restrict least upper bound to not contain all elements
     * between and including \a i and \a j
     */
    ModEvent exclude(Space& home,int i,int j);
    /// Update greatest lower bound to contain \a i
    ModEvent include(Space& home,int i);
    /// Restrict least upper bound to not contain \a i
    ModEvent exclude(Space& home,int i);
    /**
     * \brief Update least upper bound to contain at most all elements
     * between and including \a i and \a j
     */
    ModEvent intersect(Space& home,int i,int j);
    /// Update least upper bound to contain at most the element \a i
    ModEvent intersect(Space& home,int i);
    //@}

    /// \name Domain update by range iterator
    //@{

    /// Remove range sequence described by \a i from least upper bound
    template<class I> ModEvent excludeI(Space& home, I& i);
    /// Include range sequence described by \a i in greatest lower bound
    template<class I> ModEvent includeI(Space& home, I& i);
    /// Intersect least upper bound with range sequence described by \a i
    template<class I> ModEvent intersectI(Space& home, I& iter);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned from glb
    int glbMin(const Delta& d) const;
    /// Return maximum value just pruned from glb
    int glbMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from glb
    bool glbAny(const Delta& d) const;
    /// Return minimum value just pruned from lub
    int lubMin(const Delta& d) const;
    /// Return maximum value just pruned from lub
    int lubMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from lub
    bool lubAny(const Delta& d) const;
    //@}

  };

  /**
   * \brief Print universe set view
   * \relates Gecode::Set::UniverseView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const UniverseView& x);


  /** \name View comparison
   *  \relates Gecode::Set::UniverseView
   */
  //@{
  /// Test whether views \a x and \a y are the same
  bool same(const UniverseView& x, const UniverseView& y);
  //@}



  /**
   * \brief Singleton set view
   *
   * A singleton set view \f$s\f$ for an integer view \f$x\f$ provides
   * operations such that \f$s\f$ behaves like the singleton set \f$\{x\}\f$.
   * \ingroup TaskActorSetView
   */

  class SingletonView :
    public DerivedView<Gecode::Int::IntView> {
  protected:
    using DerivedView<Gecode::Int::IntView>::x;

    /// Convert set variable PropCond \a pc to a PropCond for integer variables
    static PropCond pc_settoint(PropCond pc);
    /// Convert integer variable ModEvent \a me to a ModEvent for set variables
    static ModEvent me_inttoset(ModEvent me);
    /// Convert set variable ModEvent \a me to a ModEvent for integer variables
    static ModEvent me_settoint(ModEvent me);

  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    SingletonView(void);
    /// Initialize with integer view \a y
    SingletonView(Gecode::Int::IntView& y);
    /// Initialize with integer variable \a y
    SingletonView(const Gecode::IntVar& y);
    //@}

    /// \name Value access
    //@{

    /// Return minimum cardinality
    unsigned int cardMin(void) const;
    /// Return maximum cardinality
    unsigned int cardMax(void) const;
    /// Return minimum of the least upper bound
    int lubMin(void) const;
    /// Return maximum of the least upper bound
    int lubMax(void) const;
    /// Return n-th smallest element of the least upper bound
    int lubMinN(unsigned int n) const;
    /// Return minimum of the greatest lower bound
    int glbMin(void) const;
    /// Return maximum of the greatest lower bound
    int glbMax(void) const;

    /// Return the number of elements in the greatest lower bound
    unsigned int glbSize(void) const;
    /// Return the number of elements in the least upper bound
    unsigned int lubSize(void) const;
    /// Return the number of unknown elements
    unsigned int unknownSize(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether \a i is in the greatest lower bound
    bool contains(int i) const;
    /// Test whether \a i is not in the least upper bound
    bool notContains(int i) const;
    //@}


    /// \name Domain update by value
    //@{
    /// Restrict cardinality to be greater than or equal to \a m
    ModEvent cardMin(Space& home, unsigned int m);
    /// Restrict cardinality to be less than or equal to \a m
    ModEvent cardMax(Space& home, unsigned int m);
    /**
     * \brief Update greatest lower bound to include all elements
     * between and including \a i and \a j
     */
    ModEvent include(Space& home,int i,int j);
    /**
     * \brief Restrict least upper bound to not contain all elements
     * between and including \a i and \a j
     */
    ModEvent exclude(Space& home,int i,int j);
    /// Update greatest lower bound to contain \a i
    ModEvent include(Space& home,int i);
    /// Restrict least upper bound to not contain \a i
    ModEvent exclude(Space& home,int i);
    /**
     * \brief Update least upper bound to contain at most all elements
     * between and including \a i and \a j
     */
    ModEvent intersect(Space& home,int i,int j);
    /// Update least upper bound to contain at most the element \a i
    ModEvent intersect(Space& home,int i);
    //@}

    /// \name Domain update by range iterator
    //@{

    /// Remove range sequence described by \a i from least upper bound
    template<class I> ModEvent excludeI(Space& home, I& i);
    /// Include range sequence described by \a i in greatest lower bound
    template<class I> ModEvent includeI(Space& home, I& i);
    /// Intersect least upper bound with range sequence described by \a i
    template<class I> ModEvent intersectI(Space& home, I& iter);
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
    /// Return minimum value just pruned from glb
    int glbMin(const Delta& d) const;
    /// Return maximum value just pruned from glb
    int glbMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from glb
    bool glbAny(const Delta& d) const;
    /// Return minimum value just pruned from lub
    int lubMin(const Delta& d) const;
    /// Return maximum value just pruned from lub
    int lubMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from lub
    bool lubAny(const Delta& d) const;
    //@}

  };

  /**
   * \brief Print singleton set view
   * \relates Gecode::Set::SingletonView
   */
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const SingletonView& x);


  /**
   * \brief Complement set view
   *
   * A complement set view \f$s\f$ for a set view \f$t\f$ provides
   * operations such that \f$s\f$ behaves like the complement of \f$\{t\}\f$.
   * The complement is defined in terms of the set universe.
   * \ingroup TaskActorSetView
   */

  template<class View>
  class ComplementView
    : public DerivedView<View> {
  protected:
    using DerivedView<View>::x;

  public:
    /// Negate the propagation condition \a pc
    static PropCond pc_negateset(PropCond pc);
    /// Negate the modification event \a me
    static ModEvent me_negateset(ModEvent me);

    /// \name Constructors and initialization
    //@{
    /// Default constructor
    ComplementView(void);
    /// Initialize with set view \a y
    explicit ComplementView(View& y);
    //@}

    /// \name Value access
    //@{

    /// Return minimum cardinality
    unsigned int cardMin(void) const;
    /// Return maximum cardinality
    unsigned int cardMax(void) const;
    /// Return minimum of the least upper bound
    int lubMin(void) const;
    /// Return maximum of the least upper bound
    int lubMax(void) const;
    /// Return \a n-th smallest element of the least upper bound
    int lubMinN(unsigned int n) const;
    /// Return minimum of the greatest lower bound
    int glbMin(void) const;
    /// Return maximum of the greatest lower bound
    int glbMax(void) const;

    /// Return the number of elements in the greatest lower bound
    unsigned int glbSize(void) const;
    /// Return the number of elements in the least upper bound
    unsigned int lubSize(void) const;
    /// Return the number of unknown elements
    unsigned int unknownSize(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether \a i is in the greatest lower bound
    bool contains(int i) const;
    /// Test whether \a i is not in the least upper bound
    bool notContains(int i) const;
    //@}


    /// \name Domain update by value
    //@{
    /// Restrict cardinality to be greater than or equal to \a m
    ModEvent cardMin(Space& home, unsigned int m);
    /// Restrict cardinality to be less than or equal to \a m
    ModEvent cardMax(Space& home, unsigned int m);
    /**
     * \brief Update greatest lower bound to include all elements
     * between and including \a i and \a j
     */
    ModEvent include(Space& home,int i,int j);
    /**
     * \brief Restrict least upper bound to not contain all elements
     * between and including \a i and \a j
     */
    ModEvent exclude(Space& home,int i,int j);
    /// Update greatest lower bound to contain \a i
    ModEvent include(Space& home,int i);
    /// Restrict least upper bound to not contain \a i
    ModEvent exclude(Space& home,int i);
    /**
     * \brief Update least upper bound to contain at most all elements
     * between and including \a i and \a j
     */
    ModEvent intersect(Space& home,int i,int j);
    /// Update least upper bound to contain at most the element \a i
    ModEvent intersect(Space& home,int i);
    //@}

    /// \name Domain update by range iterator
    //@{

    /// Remove range sequence described by \a i from least upper bound
    template<class I> ModEvent excludeI(Space& home, I& i);
    /// Include range sequence described by \a i in greatest lower bound
    template<class I> ModEvent includeI(Space& home, I& i);
    /// Intersect least upper bound with range sequence described by \a i
    template<class I> ModEvent intersectI(Space& home, I& iter);
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
    /// Return minimum value just pruned from glb
    int glbMin(const Delta& d) const;
    /// Return maximum value just pruned from glb
    int glbMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from glb
    bool glbAny(const Delta& d) const;
    /// Return minimum value just pruned from lub
    int lubMin(const Delta& d) const;
    /// Return maximum value just pruned from lub
    int lubMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from lub
    bool lubAny(const Delta& d) const;
    //@}

  };

  /**
   * \brief Print complement set view
   * \relates Gecode::Set::ComplementView
   */
  template<class Char, class Traits, class View>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
              const ComplementView<View>& x);


  template<class View> class LubDiffRanges;
  template<class View> class GlbDiffRanges;

  /**
   * \brief Cached set view
   *
   * A cached set view \f$s\f$ for a set view \f$t\f$ adds operations
   * for cacheing the current domain of \f$t\f$ and comparing the current
   * domain to the cached domain. Cached views make it easy to implement
   * incremental propagation algorithms.
   * 
   * \ingroup TaskActorSetView
   */

  template<class View>
  class CachedView
    : public DerivedView<View> {
    friend class LubDiffRanges<View>;
    friend class GlbDiffRanges<View>;
  protected:
    using DerivedView<View>::x;

    /// The cached least upper bound
    LUBndSet lubCache;
    /// The cached greatest lower bound
    GLBndSet glbCache;

  public:

    /// \name Constructors and initialization
    //@{
    /// Default constructor
    CachedView(void);
    /// Initialize with set view \a y
    explicit CachedView(const View& y);
    //@}

    /// \name Value access
    //@{

    /// Return minimum cardinality
    unsigned int cardMin(void) const;
    /// Return maximum cardinality
    unsigned int cardMax(void) const;
    /// Return minimum of the least upper bound
    int lubMin(void) const;
    /// Return maximum of the least upper bound
    int lubMax(void) const;
    /// Return \a n-th smallest element of the least upper bound
    int lubMinN(unsigned int n) const;
    /// Return minimum of the greatest lower bound
    int glbMin(void) const;
    /// Return maximum of the greatest lower bound
    int glbMax(void) const;

    /// Return the number of elements in the greatest lower bound
    unsigned int glbSize(void) const;
    /// Return the number of elements in the least upper bound
    unsigned int lubSize(void) const;
    /// Return the number of unknown elements
    unsigned int unknownSize(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether \a i is in the greatest lower bound
    bool contains(int i) const;
    /// Test whether \a i is not in the least upper bound
    bool notContains(int i) const;
    //@}


    /// \name Domain update by value
    //@{
    /// Restrict cardinality to be greater than or equal to \a m
    ModEvent cardMin(Space& home, unsigned int m);
    /// Restrict cardinality to be less than or equal to \a m
    ModEvent cardMax(Space& home, unsigned int m);
    /**
     * \brief Update greatest lower bound to include all elements
     * between and including \a i and \a j
     */
    ModEvent include(Space& home,int i,int j);
    /**
     * \brief Restrict least upper bound to not contain all elements
     * between and including \a i and \a j
     */
    ModEvent exclude(Space& home,int i,int j);
    /// Update greatest lower bound to contain \a i
    ModEvent include(Space& home,int i);
    /// Restrict least upper bound to not contain \a i
    ModEvent exclude(Space& home,int i);
    /**
     * \brief Update least upper bound to contain at most all elements
     * between and including \a i and \a j
     */
    ModEvent intersect(Space& home,int i,int j);
    /// Update least upper bound to contain at most the element \a i
    ModEvent intersect(Space& home,int i);
    //@}

    /// \name Domain update by range iterator
    //@{

    /// Remove range sequence described by \a i from least upper bound
    template<class I> ModEvent excludeI(Space& home, I& i);
    /// Include range sequence described by \a i in greatest lower bound
    template<class I> ModEvent includeI(Space& home, I& i);
    /// Intersect least upper bound with range sequence described by \a i
    template<class I> ModEvent intersectI(Space& home, I& iter);
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
    /// Return minimum value just pruned from glb
    int glbMin(const Delta& d) const;
    /// Return maximum value just pruned from glb
    int glbMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from glb
    bool glbAny(const Delta& d) const;
    /// Return minimum value just pruned from lub
    int lubMin(const Delta& d) const;
    /// Return maximum value just pruned from lub
    int lubMax(const Delta& d) const;
    /// Test whether arbitrary values got pruned from lub
    bool lubAny(const Delta& d) const;
    //@}

    /// \name Domain cache operations
    //@{
    /// Initialize cache to bounds \a glb and \a lub
    void initCache(Space& home, const IntSet& glb, const IntSet& lub);
    /// Update greatest lower bound cache to current domain
    void cacheGlb(Space& home);
    /// Update least upper bound cache to current domain
    void cacheLub(Space& home);
    /// Check whether greatest lower bound cache differs from current domain
    bool glbModified(void) const;
    /// Check whether least upper bound cache differs from current domain
    bool lubModified(void) const;
    //@}

    /// \name Cloning
    //@{
    /// Update this view to be a clone of view \a y
    void update(Space& home, bool share, CachedView<View>& y);
    //@}
  };

  /**
   * \brief Print cached set view
   * \relates Gecode::Set::CachedView
   */
  template<class Char, class Traits, class View>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
              const CachedView<View>& x);

  /**
   * \brief %Range iterator for difference of greatest lower bound and cache
   * \relates Gecode::Set::CachedView
   */
  template<class View>
  class GlbDiffRanges
    : public Iter::Ranges::Diff<GlbRanges<View>,BndSetRanges> {
  protected:
    /// Lower bound iterator
    GlbRanges<View> gr;
    /// Cached lower bound
    BndSetRanges cr;
  public:
    /// Constructor
    GlbDiffRanges(const CachedView<View>& x);
  };

  /**
   * \brief %Range iterator for difference of least upper bound and cache
   * \relates Gecode::Set::CachedView
   */
  template<class View>
  class LubDiffRanges
    : public Iter::Ranges::Diff<BndSetRanges,LubRanges<View> > {
  protected:
    /// Cached upper bound
    BndSetRanges cr;
    /// Upper bound iterator
    LubRanges<View> lr;
  public:
    /// Constructor
    LubDiffRanges(const CachedView<View>& x);
  };

}}

#include <gecode/set/var/set.hpp>

#include <gecode/set/view/set.hpp>

#include <gecode/set/view/const.hpp>
#include <gecode/set/view/singleton.hpp>
#include <gecode/set/view/complement.hpp>
#include <gecode/set/view/cached.hpp>

#include <gecode/set/view/print.hpp>
#include <gecode/set/var/print.hpp>

// STATISTICS: set-var
