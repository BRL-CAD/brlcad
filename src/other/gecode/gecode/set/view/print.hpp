/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *     Gabor Szokoli <szokoli@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2004, 2005
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

#include <sstream>

namespace Gecode { namespace Set {

  /// Print bound of a set view or variable
  template<class Char, class Traits, class I>
  void
  printBound(std::basic_ostream<Char,Traits>& s, I& r) {
    s << '{';
    while (r()) {
      if (r.min() == r.max()) {
        s << r.min();
      } else if (r.min()+1 == r.max()) {
        s << r.min() << "," << r.max();
      } else {
        s << r.min() << ".." << r.max();
      }
      ++r;
      if (!r()) break;
      s << ',';
    }
    s << '}';
  }

  /// Print set view
  template<class Char, class Traits, class IL, class IU>
  void
  print(std::basic_ostream<Char,Traits>& s, bool assigned, IL& lb, IU& ub,
        unsigned int cardMin, unsigned int cardMax) {
    if (assigned) {
      printBound(s, ub);
    } else {
      printBound(s,lb);
      s << "..";
      printBound(s,ub);
      if (cardMin==cardMax) {
        s << "#(" << cardMin << ")";
      } else {
        s << "#(" << cardMin << "," << cardMax << ")";
      }
    }
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const SetView& x) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    LubRanges<SetView> ub(x);
    GlbRanges<SetView> lb(x);
    print(s, x.assigned(), lb, ub, x.cardMin(), x.cardMax()) ;
    return os << s.str();
  }

  template<class Char, class Traits>
  inline std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const EmptyView&) {
    return os << "{}#0";
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const UniverseView&) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << "{" << Gecode::Set::Limits::min << ".."
      << Gecode::Set::Limits::max << "}#("
      << Gecode::Set::Limits::card << ")";
    return os << s.str();
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ConstSetView& x) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    LubRanges<ConstSetView> ub(x);
    printBound(s, ub);
    s << "#(" << x.cardMin() << ")";
    return os << s.str();
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const SingletonView& x) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    if (x.assigned()) {
      s << "{" << x.glbMin() << "}#(1)";
    } else {
      LubRanges<SingletonView> ub(x);
      s << "{}..";
      printBound(s, ub);
      s << "#(1)";
    }
    return os << s.str();
  }

}}

// STATISTICS: set-var
