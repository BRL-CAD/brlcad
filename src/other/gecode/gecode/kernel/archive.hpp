/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2011
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
   * \brief %Archive representation
   *
   * An Archive is an array of unsigned integers, used as an external
   * representation of internal data structures (such as Choice objects).
   */
  class Archive {
  private:
    /// Size of array
    int _size;
    /// Used size of array
    int _n;
    /// Array elements
    unsigned int* _a;
    /// Current position of read iterator
    int _pos;
    /// Resize to at least \a n + 1 elements
    GECODE_KERNEL_EXPORT void resize(int n);
  public:
    /// Construct empty representation
    Archive(void);
    /// Destructor
    GECODE_KERNEL_EXPORT ~Archive(void);
    /// Copy constructor
    GECODE_KERNEL_EXPORT Archive(const Archive& e);
    /// Assignment operator
    GECODE_KERNEL_EXPORT Archive& operator =(const Archive& e);
    /// Add \a i to the contents
    void put(unsigned int i);
    /// Return size
    int size(void) const;
    /// Return array element \a i
    unsigned int operator [](int i) const;
    /// Return next element to read
    unsigned int get(void);
  };

  /** Add \a i to the end of \a e
   * \relates Archive
   */
  Archive&
  operator <<(Archive& e, unsigned int i);
  /** Add \a i to the end of \a e
   * \relates Archive
   */
  Archive&
  operator <<(Archive& e, int i);
  /** Add \a i to the end of \a e
   * \relates Archive
   */
  Archive&
  operator <<(Archive& e, unsigned short i);
  /** Add \a i to the end of \a e
   * \relates Archive
   */
  Archive&
  operator <<(Archive& e, short i);
  /** Add \a i to the end of \a e
   * \relates Archive
   */
  Archive&
  operator <<(Archive& e, unsigned char i);
  /** Add \a i to the end of \a e
   * \relates Archive
   */
  Archive&
  operator <<(Archive& e, char i);
  /** Add \a i to the end of \a e
   * \relates Archive
   */
  Archive&
  operator <<(Archive& e, bool i);
  /** Add \a d to the end of \a e
   * \relates Archive
   */
  Archive&
  operator <<(Archive& e, float d);
  /** Add \a d to the end of \a e
   * \relates Archive
   */
  Archive&
  operator <<(Archive& e, double d);

  /** Read next element from \a e into \a i
   * \relates Archive
   */  
  Archive&
  operator >>(Archive& e, unsigned int& i);
  /** Read next element from \a e into \a i
   * \relates Archive
   */  
  Archive&
  operator >>(Archive& e, int& i);
  /** Read next element from \a e into \a i
   * \relates Archive
   */  
  Archive&
  operator >>(Archive& e, unsigned short& i);
  /** Read next element from \a e into \a i
   * \relates Archive
   */  
  Archive&
  operator >>(Archive& e, short& i);
  /** Read next element from \a e into \a i
   * \relates Archive
   */  
  Archive&
  operator >>(Archive& e, unsigned char& i);
  /** Read next element from \a e into \a i
   * \relates Archive
   */  
  Archive&
  operator >>(Archive& e, char& i);
  /** Read next element from \a e into \a i
   * \relates Archive
   */  
  Archive&
  operator >>(Archive& e, bool& i);
  /** Read next element from \a e into \a d
   * \relates Archive
   */  
  Archive&
  operator >>(Archive& e, float& d);
  /** Read next element from \a e into \a d
   * \relates Archive
   */  
  Archive&
  operator >>(Archive& e, double& d);

  /*
   * Implementation
   *
   */

  forceinline
  Archive::Archive(void) : _size(0), _n(0), _a(NULL), _pos(0) {}

  forceinline void
  Archive::put(unsigned int i) {
    if (_n==_size)
      resize(_n+1);
    _a[_n++] = i;
  }
    
  forceinline int
  Archive::size(void) const { return _n; }
  
  forceinline unsigned int
  Archive::operator [](int i) const {
    assert(i < _n);
    return _a[i];
  }

  forceinline unsigned int
  Archive::get(void) {
    assert(_pos < _n);
    return _a[_pos++];
  }

  forceinline Archive&
  operator <<(Archive& e, unsigned int i) {
    e.put(i);
    return e;
  }
  forceinline Archive&
  operator <<(Archive& e, int i) {
    e.put(static_cast<unsigned int>(i));
    return e;
  }
  forceinline Archive&
  operator <<(Archive& e, unsigned short i) {
    e.put(i);
    return e;
  }
  forceinline Archive&
  operator <<(Archive& e, short i) {
    e.put(static_cast<unsigned int>(i));
    return e;
  }
  forceinline Archive&
  operator <<(Archive& e, unsigned char i) {
    e.put(i);
    return e;
  }
  forceinline Archive&
  operator <<(Archive& e, char i) {
    e.put(static_cast<unsigned int>(i));
    return e;
  }
  forceinline Archive&
  operator <<(Archive& e, bool i) {
    e.put(static_cast<unsigned int>(i));
    return e;
  }
  forceinline Archive&
  operator <<(Archive& e, float d) {
    for (size_t i=0; i<sizeof(float); i++)
      e.put(static_cast<unsigned int>(reinterpret_cast<char*>(&d)[i]));
    return e;
  }
  forceinline Archive&
  operator <<(Archive& e, double d) {
    for (size_t i=0; i<sizeof(double); i++)
      e.put(static_cast<unsigned int>(reinterpret_cast<char*>(&d)[i]));
    return e;
  }

  forceinline Archive&
  operator >>(Archive& e, unsigned int& i) {
    i = e.get();
    return e;
  }
  forceinline Archive&
  operator >>(Archive& e, int& i) {
    i = static_cast<int>(e.get());
    return e;
  }
  forceinline Archive&
  operator >>(Archive& e, unsigned short& i) {
    i = static_cast<unsigned short>(e.get());
    return e;
  }
  forceinline Archive&
  operator >>(Archive& e, short& i) {
    i = static_cast<short>(e.get());
    return e;
  }
  forceinline Archive&
  operator >>(Archive& e, unsigned char& i) {
    i = static_cast<unsigned char>(e.get());
    return e;
  }
  forceinline Archive&
  operator >>(Archive& e, char& i) {
    i = static_cast<char>(e.get());
    return e;
  }
  forceinline Archive&
  operator >>(Archive& e, bool& i) {
    i = static_cast<bool>(e.get());
    return e;
  }
  forceinline Archive&
  operator >>(Archive& e, float& d) {
    char* cd = reinterpret_cast<char*>(&d);
    for (size_t i=0; i<sizeof(float); i++)
      cd[i] = static_cast<char>(e.get());
    return e;
  }
  forceinline Archive&
  operator >>(Archive& e, double& d) {
    char* cd = reinterpret_cast<char*>(&d);
    for (size_t i=0; i<sizeof(double); i++)
      cd[i] = static_cast<char>(e.get());
    return e;
  }

}

// STATISTICS: kernel-branch
