/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
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

namespace Gecode { namespace Iter { namespace Ranges {

  /**
   * \brief Range iterator for mapping ranges
   *
   * If \a strict is true, then mapped ranges are not allowed to
   * be adjacent or overlap.
   *
   * \ingroup FuncIterRanges
   */
  template<class I, class M, bool strict=true>
  class Map {
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Map(void);
    /// Initialize with ranges from \a i
    Map(I& i);
    /// Initialize with ranges from \a i and map \a m
    Map(I& i, const M& m);
    /// Initialize with ranges from \a i
    void init(I& i);
    /// Initialize with ranges from \a i and map \a m
    void init(I& i, const M& m);
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

  /// Specialized mapping of ranges for non-strict maps
  template<class I, class M>
  class Map<I,M,false> : public MinMax {
  protected:
    /// Input range
    I i;
    /// Map for ranges
    M m;
    /// Find next mapped range
    void next(void);
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Map(void);
    /// Initialize with ranges from \a i
    Map(I& i);
    /// Initialize with ranges from \a i and map \a m
    Map(I& i, const M& m);
    /// Initialize with ranges from \a i
    void init(I& i);
    /// Initialize with ranges from \a i and map \a m
    void init(I& i, const M& m);
    //@}

    /// \name Iteration control
    //@{
    /// Move iterator to next range (if possible)
    void operator ++(void);
    //@}
  };

  /// Specialized mapping of ranges for strict maps
  template<class I, class M>
  class Map<I,M,true> {
  protected:
    /// Input range iterator
    I i;
    /// Map object
    M m;
  public:
    /// \name Constructors and initialization
    //@{
    /// Default constructor
    Map(void);
    /// Initialize with ranges from \a i
    Map(I& i);
    /// Initialize with ranges from \a i and map \a m
    Map(I& i, const M& m);
    /// Initialize with ranges from \a i
    void init(I& i);
    /// Initialize with ranges from \a i and map \a m
    void init(I& i, const M& m);
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


  template<class I, class M>
  forceinline
  Map<I,M,false>::Map(void) {}

  template<class I, class M>
  forceinline void
  Map<I,M,false>::next(void) {
    if (i()) {
      mi = m.min(i.min());
      ma = m.max(i.max());
      ++i;
      while (i() && (ma+1 >= m.min(i.min()))) {
        ma = m.max(i.max()); ++i;
      }
    } else {
      finish();
    }
  }

  template<class I, class M>
  forceinline void
  Map<I,M,false>::init(I& i0) {
    i=i0; next();
  }
  template<class I, class M>
  forceinline void
  Map<I,M,false>::init(I& i0, const M& m0) {
    i=i0; m=m0; next();
  }

  template<class I, class M>
  forceinline
  Map<I,M,false>::Map(I& i0) : i(i0) {
    next();
  }
  template<class I, class M>
  forceinline
  Map<I,M,false>::Map(I& i0, const M& m0) : i(i0), m(m0) {
    next();
  }

  template<class I, class M>
  forceinline void
  Map<I,M,false>::operator ++(void) {
    next();
  }



  template<class I, class M>
  forceinline
  Map<I,M,true>::Map(void) {}

  template<class I, class M>
  forceinline void
  Map<I,M,true>::init(I& i0) {
    i=i0;
  }
  template<class I, class M>
  forceinline void
  Map<I,M,true>::init(I& i0, const M& m0) {
    i=i0; m=m0;
  }

  template<class I, class M>
  forceinline
  Map<I,M,true>::Map(I& i0) : i(i0) {}
  template<class I, class M>
  forceinline
  Map<I,M,true>::Map(I& i0, const M& m0) : i(i0), m(m0) {}

  template<class I, class M>
  forceinline bool
  Map<I,M,true>::operator ()(void) const {
    return i();
  }
  template<class I, class M>
  forceinline void
  Map<I,M,true>::operator ++(void) {
    ++i;
  }

  template<class I, class M>
  forceinline int
  Map<I,M,true>::min(void) const {
    return m.min(i.min());
  }
  template<class I, class M>
  forceinline int
  Map<I,M,true>::max(void) const {
    return m.max(i.max());
  }
  template<class I, class M>
  forceinline unsigned int
  Map<I,M,true>::width(void) const {
    return static_cast<unsigned int>(max()-min())+1;
  }

}}}

// STATISTICS: iter-any

