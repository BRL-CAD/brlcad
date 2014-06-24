/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2003
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

#include <sstream>

namespace Gecode { namespace Int {

  template<class Char, class Traits, class View>
  std::basic_ostream<Char,Traits>&
  print_view(std::basic_ostream<Char,Traits>& os, const View& x) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    if (x.assigned()) {
      s << x.val();
    } else if (x.range()) {
      s << '[' << x.min() << ".." << x.max() << ']';
    } else {
      s << '{';
      ViewRanges<View> r(x);
      while (true) {
        if (r.min() == r.max()) {
          s << r.min();
        } else {
          s << r.min() << ".." << r.max();
        }
        ++r;
        if (!r()) break;
        s << ',';
      }
      s << '}';
    }
    return os << s.str();
  }

  template<class Char, class Traits, class Val, class UnsVal>
  std::basic_ostream<Char,Traits>&
  print_scale(std::basic_ostream<Char,Traits>& os,
              const ScaleView<Val,UnsVal>& x) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    if (x.assigned()) {
      s << x.val();
    } else {
      s << '{';
      ViewRanges<ScaleView<Val,UnsVal> > r(x);
      while (true) {
        if (r.min() == r.max()) {
          s << r.min();
        } else {
          s << r.min() << ".." << r.max();
        }
        ++r;
        if (!r()) break;
        s << ',';
      }
      s << '}';
    }
    return os << s.str();
  }

  template<class Char, class Traits>
  inline std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const IntView& x) {
    return print_view(os,x);
  }
  template<class Char, class Traits>
  inline std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const MinusView& x) {
    return print_view(os,x);
  }
  template<class Char, class Traits>
  inline std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OffsetView& x) {
    return print_view(os,x);
  }
  template<class Char, class Traits, class View>
  inline std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,
              const CachedView<View>& x) {
    return print_view(os,x);
  }

  template<class Char, class Traits>
  inline std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const IntScaleView& x) {
    return print_scale<Char,Traits,int,unsigned int>(os,x);
  }
  template<class Char, class Traits>
  inline std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const LLongScaleView& x) {
    return print_scale<Char,Traits,long long int,unsigned long long int>(os,x);
  }

  template<class Char, class Traits>
  inline std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ConstIntView& x) {
    return os << x.val();
  }
  template<class Char, class Traits>
  inline std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ZeroIntView&) {
    return os << 0;
  }


  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const BoolView& x) {
    if (x.one())
      return os << 1;
    if (x.zero())
      return os << 0;
    return os << "[0..1]";
  }
  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const NegBoolView& x) {
    if (x.one())
      return os << 0;
    if (x.zero())
      return os << 1;
    return os << "[0..1]";
  }

}}

// STATISTICS: int-var

