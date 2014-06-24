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

namespace Gecode {

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  REG::Exp::print(std::basic_ostream<Char,Traits>& os) const {
    if (this == NULL)
      return os << "[]";
    switch (type) {
    case ET_SYMBOL:
      return os << "[" << data.symbol << "]";
    case ET_STAR:
      {
        bool par = ((data.kids[0] != NULL) &&
                    ((data.kids[0]->type == ET_CONC) ||
                     (data.kids[0]->type == ET_OR)));
        return data.kids[0]->print(os << (par ? "*(" : "*"))
                                      << (par ? ")" : "");
      }
    case ET_CONC:
      {
        bool par0 = ((data.kids[0] != NULL) &&
                     (data.kids[0]->type == ET_OR));
        std::ostream& os1 = data.kids[0]->print(os << (par0 ? "(" : ""))
                                                   << (par0 ? ")+" : "+");
        bool par1 = ((data.kids[1] != NULL) &&
                     (data.kids[1]->type == ET_OR));
        return data.kids[1]->print(os1 << (par1 ? "(" : "") )
                                       << (par1 ? ")" : "");
      }
    case ET_OR:
      return data.kids[1]->print(data.kids[0]->print(os) << "|");
    default: GECODE_NEVER;
    }
    GECODE_NEVER;
    return os;
  }


  template<class Char, class Traits>
  inline std::basic_ostream<Char,Traits>&
  REG::print(std::basic_ostream<Char,Traits>& os) const {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    e->print(s);
    return os << s.str();
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const REG& r) {
    return r.print(os);
  }

}

// STATISTICS: minimodel-any

