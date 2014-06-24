/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004
 *     Christian Schulte, 2004
 *     Gabor Szokoli, 2004
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

  class SetVarImp;
  class LUBndSet;
  class GLBndSet;

  /**
   * \brief Finite set delta information for advisors
   *
   */
  class SetDelta : public Delta {
    friend class SetVarImp;
    friend class LUBndSet;
    friend class GLBndSet;
  private:
    int _glbMin; ///< Minimum glb value just pruned
    int _glbMax; ///< Largest glb value just pruned
    int _lubMin; ///< Minimum lub value just pruned
    int _lubMax; ///< Largest lub value just pruned
  public:
    /// Create set delta as providing no information (if \a any is true)
    SetDelta(void);
    /// Create set delta with \a min and \a max
    SetDelta(int glbMin, int glbMax, int lubMin, int lubMax);
    /// Return glb minimum
    int glbMin(void) const;
    /// Return glb maximum
    int glbMax(void) const;
    /// Return lub minimum
    int lubMin(void) const;
    /// Return lub maximum
    int lubMax(void) const;
    /// Test whether delta represents any domain change in glb
    bool glbAny(void) const;
    /// Test whether delta represents any domain change in lub
    bool lubAny(void) const;
  };

}}

#include <gecode/set/var-imp/delta.hpp>

namespace Gecode { namespace Set {

  /**
   * \brief Sets of integers
   */
  class BndSet  {
  private:
    RangeList* first;
    RangeList* last;
  protected:
    /// The size of this set
    unsigned int _size;
    /// The cardinality this set represents
    unsigned int _card;
    /// Set first range to \a r
    void fst(RangeList* r);
    /// Set last range to \a r
    void lst(RangeList* r);

    /// Return first range
    RangeList* fst(void) const;
    /// Return last range
    RangeList* lst(void) const;

  public:
    /// Returned by empty sets when asked for their maximum element
    static const int MAX_OF_EMPTY = Limits::min-1;
    /// Returned by empty sets when asked for their minimum element
    static const int MIN_OF_EMPTY = Limits::max+1;

    /// \name Constructors and initialization
    //@{
    /// Default constructor. Creates an empty set.
    BndSet(void);
    /// Initialize as the set \f$ \{i,\dots,j\}\f$
    BndSet(Space& home, int i, int j);
    /// Initialize as the set represented by \a s
    GECODE_SET_EXPORT BndSet(Space& home, const IntSet& s);
    //@}

    /// \name Memory management
    //@{
    /// Free memory used by this set
    void dispose(Space& home);
    //@}

    /// \name Value access
    //@{
    /// Return smallest element
    int min(void) const;
    /// Return greatest element
    int max(void) const;
    /// Return \a n -th smallest element
    int minN(unsigned int n) const;
    /// Return size
    unsigned int size(void) const;
    /// Return cardinality
    unsigned int card(void) const;
    /// Set cardinality
    void card(unsigned int c);
    //@}

    /// \name Tests
    //@{
    /// Test whether this set is empty
    bool empty(void) const;
    /// Test whether \a i is an element of this set
    bool in(int i) const;
    //@}

    /// \name Update operations
    //@{
    /// Make this set equal to \a s
    void become(Space& home, const BndSet& s);
    //@}

    /// \name Range list access for iteration
    //@{
    /// Return range list for iteration
    RangeList* ranges(void) const;
    //@}

  protected:
    /// Overwrite the ranges with those represented by \a i
    template<class I> bool overwrite(Space& home,I& i);

  public:
    /// \name Cloning
    //@{
    /// Update this set to be a clone of set \a x
    void update(Space& home, BndSet& x);
    //@}

    /// Check whether internal invariants hold
    GECODE_SET_EXPORT bool isConsistent(void) const;
  };

  /**
   * \brief Range iterator for integer sets
   *
   */
  class BndSetRanges : public Iter::Ranges::RangeList {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    BndSetRanges(void);
    /// Initialize with BndSet \a s
    BndSetRanges(const BndSet& s);
    /// Initialize with BndSet \a s
    void init(const BndSet& s);
    //@}
  };

  /**
   * \brief Growing sets of integers
   *
   * These sets provide operations for monotonically growing the set.
   * Growing sets are used for implementing the greatest lower bound of
   * set variables.
   */
  class GLBndSet : public BndSet {
  private:
    /// Include the set \f$\{i,\dots,j\}\f$ in this set
    GECODE_SET_EXPORT bool include_full(Space& home,int,int,SetDelta&);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor. Creates an empty set.
    GLBndSet(void);
    /// Default constructor. Creates an empty set.
    GLBndSet(Space&);
    /// Initialize as the set \f$ \{i,\dots,j\}\f$
    GLBndSet(Space& home, int i, int j);
    /// Initialize as the set represented by \a s
    GLBndSet(Space& home, const IntSet& s);
    /// Initialize as the empty set
    void init(Space& home);
    //@}

    /// \name Update operations
    //@{
    /// Include the set \f$\{i,\dots,j\}\f$ in this set
    bool include(Space& home,int i,int j,SetDelta& d);
    /// Include the set represented by \a i in this set
    template<class I> bool includeI(Space& home,I& i);
    //@}
  private:
    GLBndSet(const GLBndSet&);
    const GLBndSet& operator =(const GLBndSet&);
  };

  /**
   * \brief Shrinking sets of integers
   *
   * These sets provide operations for monotonically shrinking the set.
   * Shrinking sets are used for implementing the least upper bound of
   * set variables.
   */
  class LUBndSet: public BndSet{
  private:
    GECODE_SET_EXPORT bool exclude_full(Space& home, int, int, SetDelta&);
    GECODE_SET_EXPORT bool intersect_full(Space& home, int i, int j);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor. Creates an empty set.
    LUBndSet(void);
    /// Initialize as the full set including everything between Limits::min and Limits::max
    LUBndSet(Space& home);
    /// Initialize as the set \f$ \{i,\dots,j\}\f$
    LUBndSet(Space& home, int i, int j);
    /// Initialize as the set represented by \a s
    LUBndSet(Space& home, const IntSet& s);
    /// Initialize as the full set including everything between Limits::min and Limits::max
    void init(Space& home);
    //@}

    /// \name Update operations
    //@{
    /// Exclude the set \f$\{i,\dots,j\}\f$ from this set
    bool exclude(Space& home, int i, int j, SetDelta& d);
    /// Intersect this set with the set \f$\{i,\dots,j\}\f$
    bool intersect(Space& home, int i, int j);
    /// Exclude all elements not in the set represented by \a i from this set
    template<class I> bool intersectI(Space& home, I& i);
    /// Exclude all elements in the set represented by \a i from this set
    template<class I> bool excludeI(Space& home, I& i);
    /// Exclude all elements from this set
    void excludeAll(Space& home);
    //@}
  private:
    LUBndSet(const LUBndSet&);
    const LUBndSet& operator =(const LUBndSet&);
  };

  /*
   * Iterators
   *
   */


  /**
   * \brief A complement iterator spezialized for the %BndSet limits
   *
   * \ingroup TaskActorSet
   */
  template<class I>
  class RangesCompl :
    public Iter::Ranges::Compl<Limits::min, Limits::max, I> {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    RangesCompl(void);
    /// Initialize with iterator \a i
    RangesCompl(I& i);
    /// Initialize with iterator \a i
    void init(I& i);
    //@}
  };

  /**
   * \brief Range iterator for the least upper bound
   *
   * This class provides (by specialization) a range iterator
   * for the least upper bounds of all set views.
   *
   * Note that this template class serves only as a specification
   * of the interface of the various specializations.
   *
   * \ingroup TaskActorSet
   */
  template<class T> class LubRanges {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    LubRanges(void);
    /// Initialize with least upper bound ranges for set variable \a x
    LubRanges(const T& x);
    /// Initialize with least upper bound ranges for set variable \a x
    void init(const T& x);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}

    /// \name Range access
    //@{
    /// Return smallest value of range
    int min(void) const;
    /// Return largest value of range
    int max(void) const;
    /// Return width of range (distance between minimum and maximum)
    unsigned int width(void) const;
    //@}
  };

  /**
   * \brief Range iterator for the greatest lower bound
   *
   * This class provides (by specialization) a range iterator
   * for the greatest lower bounds of all set views.
   *
   * Note that this template class serves only as a specification
   * of the interface of the various specializations.
   *
   * \ingroup TaskActorSet
   */
  template<class T> class GlbRanges {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    GlbRanges(void);
    /// Initialize with greatest lower bound ranges for set variable \a x
    GlbRanges(const T& x);
    /// Initialize with greatest lower bound ranges for set variable \a x
    void init(const T& x);
    //@}

    /// \name Iteration control
    //@{
    /// Test whether iterator is still at a range or done
    bool operator ()(void) const;
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}

    /// \name Range access
    //@{
    /// Return smallest value of range
    int min(void) const;
    /// Return largest value of range
    int max(void) const;
    /// Return width of range (distance between minimum and maximum)
    unsigned int width(void) const;
    //@}
  };

  /**
   * \brief Range iterator for the unknown set
   *
   * This class provides a range iterator
   * for the unknown set of all set views. The unknown set is the
   * difference between least upper and greatest lower bound, i.e.
   * those elements which still may be in the set, but are not yet
   * known to be in.
   *
   * \ingroup TaskActorSet
   */
  template<class T>
  class UnknownRanges : public Iter::Ranges::Diff<LubRanges<T>, GlbRanges<T> >{
  private:
    LubRanges<T> i1;
    GlbRanges<T> i2;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    UnknownRanges(void);
    /// Initialize with unknown set ranges for set variable \a x
    UnknownRanges(const T& x);
    /// Initialize with unknown set ranges for set variable \a x
    void init(const T& x);
    //@}
  };

}}

#include <gecode/set/var-imp/integerset.hpp>
#include <gecode/set/var-imp/iter.hpp>

namespace Gecode { namespace Set {

  /**
   * \brief Finite integer set variable implementation
   *
   * \ingroup Other
   */
  class SetVarImp : public SetVarImpBase {
    friend class LubRanges<SetVarImp*>;
    friend class GlbRanges<SetVarImp*>;
  private:
    /// The least upper bound of the domain
    LUBndSet lub;
    /// The greatest lower bound of the domain
    GLBndSet glb;

  protected:
    /// Constructor for cloning \a x
    SetVarImp(Space& home, bool share, SetVarImp& x);
  public:
    /// \name Constructors and initialization
    //@{
    /// Initialize with empty lower and full upper bound
    SetVarImp(Space& home);
    /**
     * \brief Initialize with given bounds and cardinality
     *
     * Creates a set variable \f$s\f$ with
     * \f$\mathit{glb}(s)=\{\mathit{glbMin},\dots,\mathit{glbMax}\}\f$,
     * \f$\mathit{lub}(s)=\{\mathit{lubMin},\dots,\mathit{lubMax}\}\f$, and
     * \f$\mathit{cardMin}\leq |s|\leq\mathit{cardMax}\f$
     */
    SetVarImp(Space& home,int glbMin,int glbMax,int lubMin,int lubMax,
               unsigned int cardMin = 0,
               unsigned int cardMax = Limits::card);
    /**
     * \brief Initialize with given bounds and cardinality
     *
     * Creates a set variable \f$s\f$ with
     * \f$\mathit{glb}(s)=\mathit{glbD}\f$,
     * \f$\mathit{lub}(s)=\{\mathit{lubMin},\dots,\mathit{lubMax}\}\f$, and
     * \f$\mathit{cardMin}\leq |s|\leq\mathit{cardMax}\f$
     */
    SetVarImp(Space& home,const IntSet& glbD,int lubMin,int lubMax,
              unsigned int cardMin,unsigned int cardMax);
    /**
     * \brief Initialize with given bounds and cardinality
     *
     * Creates a set variable \f$s\f$ with
     * \f$\mathit{glb}(s)=\{\mathit{glbMin},\dots,\mathit{glbMax}\}\f$,
     * \f$\mathit{lub}(s)=\mathit{lubD}\f$, and
     * \f$\mathit{cardMin}\leq |s|\leq\mathit{cardMax}\f$
     */
    SetVarImp(Space& home,int glbMin,int glbMax,const IntSet& lubD,
              unsigned int cardMin,unsigned int cardMax);
    /**
     * \brief Initialize with given bounds and cardinality
     *
     * Creates a set variable \f$s\f$ with
     * \f$\mathit{glb}(s)=\mathit{glbD}\f$,
     * \f$\mathit{lub}(s)=\mathit{lubD}\f$, and
     * \f$\mathit{cardMin}\leq |s|\leq\mathit{cardMax}\f$
     */
    SetVarImp(Space& home,const IntSet& glbD,const IntSet& lubD,
              unsigned int cardMin,unsigned int cardMax);
    //@}

    /// \name Value access
    //@{
    /// Return current cardinality minimum
    unsigned int cardMin(void) const;
    /// Return current cardinality maximum
    unsigned int cardMax(void) const;
    /// Return minimum of the least upper bound
    int lubMin(void) const;
    /// Return maximum of the least upper bound
    int lubMax(void) const;
    /// Return \a n -th smallest element in the least upper bound
    int lubMinN(unsigned int n) const;
    /// Return minimum of the greatest lower bound
    int glbMin(void) const;
    /// Return maximum of the greatest lower bound
    int glbMax(void) const;
    /// Return the size of the greatest lower bound
    unsigned int glbSize(void) const;
    /// Return the size of the least upper bound
    unsigned int lubSize(void) const;
    //@}

    /// \name Domain tests
    //@{
    /// Test whether variable is assigned
    bool assigned(void) const;
    /// Test whether \a n is contained in greatest lower bound
    bool knownIn(int n) const;
    /// Test whether \a n is not contained in least upper bound
    bool knownOut(int) const;
    //@}

  private:
    /// \name Domain update by range iterator, implementations
    //@{
    /// Include set described by \a i in the greatest lower bound
    template<class I> ModEvent includeI_full(Space& home,int mi, int ma, I& i);
    /// Exclude set described by \a i from the least upper bound
    template<class I> ModEvent excludeI_full(Space& home,int mi, int ma, I& i);
    /// Exclude everything but set described by \a i from the least upper bound
    template<class I> ModEvent intersectI_full(Space& home,int mi, int ma, I& i);
    //@}

    GECODE_SET_EXPORT ModEvent processLubChange(Space& home, SetDelta& d);
    GECODE_SET_EXPORT ModEvent processGlbChange(Space& home, SetDelta& d);

    /// \name Cardinality update implementation
    //@{
    /// Restrict cardinality to be at least n
    GECODE_SET_EXPORT ModEvent cardMin_full(Space& home);
    /// Restrict cardinality to be at most n
    GECODE_SET_EXPORT ModEvent cardMax_full(Space& home);
    //@}

  public:

    /// \name Domain update by value
    //@{
    /// Include \a n in the greatest lower bound
    ModEvent include(Space& home,int n);
    /// Include the range \f$\{i,\dots,j\}\f$ in the greatest lower bound
    ModEvent include(Space& home,int i,int j);
    /// Exclude \a n from the least upper bound
    ModEvent exclude(Space& home,int n);
    /// Exclude the range \f$\{i,\dots,j\}\f$ from the least upper bound
    ModEvent exclude(Space& home,int i,int j);
    /// Exclude everything but \a n from the least upper bound
    ModEvent intersect(Space& home,int n);
    /// Exclude everything but the range \f$\{i,\dots,j\}\f$ from the least upper bound
    ModEvent intersect(Space& home,int i,int j);
    /// Restrict cardinality to be at least n
    ModEvent cardMin(Space& home,unsigned int n);
    /// Restrict cardinality to be at most n
    ModEvent cardMax(Space& home,unsigned int n);
    //@}

    /// \name Domain update by range iterator
    //@{
    /// Include set described by \a i in the greatest lower bound
    template<class I> ModEvent includeI(Space& home,I& i);
    /// Exclude set described by \a i from the least upper bound
    template<class I> ModEvent excludeI(Space& home,I& i);
    /// Exclude everything but set described by \a i from the least upper bound
    template<class I> ModEvent intersectI(Space& home,I& i);
    //@}

  public:
    /// \name Dependencies
    //@{
    /**
     * \brief Subscribe propagator \a p with propagation condition \a pc to variable
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     */
    void subscribe(Space& home, Propagator& p, PropCond pc, bool schedule=true);
    /// Cancel subscription of propagator \a p with propagation condition \a pc
    void cancel(Space& home, Propagator& p, PropCond pc);
    /// Subscribe advisor \a a to variable
    void subscribe(Space& home, Advisor& a);
    /// Cancel subscription of advisor \a a
    void cancel(Space& home, Advisor& a);
    //@}

  private:
    /// Return copy of not-yet copied variable
    GECODE_SET_EXPORT SetVarImp* perform_copy(Space& home, bool share);

  public:
    /// \name Cloning
    //@{
    /// Return copy of this variable
    SetVarImp* copy(Space& home, bool share);
    //@}

    /// \name Delta information for advisors
    //@{
    /// Return minimum value just pruned from glb
    static int glbMin(const Delta& d);
    /// Return maximum value just pruned from glb
    static int glbMax(const Delta& d);
    /// Test whether arbitrary values got pruned from glb
    static bool glbAny(const Delta& d);
    /// Return minimum value just pruned from lub
    static int lubMin(const Delta& d);
    /// Return maximum value just pruned from lub
    static int lubMax(const Delta& d);
    /// Test whether arbitrary values got pruned from lub
    static bool lubAny(const Delta& d);
    //@}

  };

  class SetView;

}}

#include <gecode/set/var-imp/set.hpp>

// STATISTICS: set-var
