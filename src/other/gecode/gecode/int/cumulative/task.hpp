/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
 *     Guido Tack, 2010
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

namespace Gecode { namespace Int { namespace Cumulative {

  /*
   * Mandatory fixed task
   */

  forceinline
  ManFixPTask::ManFixPTask(void) {}
  forceinline
  ManFixPTask::ManFixPTask(IntVar s, int p, int c) 
    : Unary::ManFixPTask(s,p), _c(c) {}
  forceinline void
  ManFixPTask::init(IntVar s, int p, int c) {
    Unary::ManFixPTask::init(s,p); _c=c;
  }
  forceinline void
  ManFixPTask::init(const ManFixPTask& t) {
    Unary::ManFixPTask::init(t); _c=t._c;
  }

  forceinline int
  ManFixPTask::c(void) const {
    return _c;
  }
  forceinline long long int
  ManFixPTask::e(void) const {
    return static_cast<long long int>(pmin())*c();
  }

  forceinline void
  ManFixPTask::update(Space& home, bool share, ManFixPTask& t) {
    Unary::ManFixPTask::update(home,share,t); _c=t._c;
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFixPTask& t) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << t.est() << ":[" << t.pmin() << ',' << t.c() << "]:" << t.lct();
    return os << s.str();
  }

  /*
   * Mandatory fixed task with fixed processing, start or end time
   */

  forceinline
  ManFixPSETask::ManFixPSETask(void) {}
  forceinline
  ManFixPSETask::ManFixPSETask(TaskType t, IntVar s, int p, int c) 
    : Unary::ManFixPSETask(t,s,p), _c(c) {}
  forceinline void
  ManFixPSETask::init(TaskType t, IntVar s, int p, int c) {
    Unary::ManFixPSETask::init(t,s,p); _c=c;
  }
  forceinline void
  ManFixPSETask::init(const ManFixPSETask& t0) {
    Unary::ManFixPSETask::init(t0); _c=t0._c;
  }

  forceinline int
  ManFixPSETask::c(void) const {
    return _c;
  }
  forceinline long long int
  ManFixPSETask::e(void) const {
    return static_cast<long long int>(pmin())*c();
  }

  forceinline void
  ManFixPSETask::update(Space& home, bool share, ManFixPSETask& t) {
    Unary::ManFixPSETask::update(home,share,t); _c=t._c;
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os,const ManFixPSETask& t) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << t.est() << ":[" << t.pmin() << ',' << t.c() << "]:" << t.lct();
    return os << s.str();
  }
 
  /*
   * Mandatory flexible task
   */

  forceinline
  ManFlexTask::ManFlexTask(void) {}
  forceinline
  ManFlexTask::ManFlexTask(IntVar s, IntVar p, IntVar e, int c)
    : Unary::ManFlexTask(s,p,e), _c(c) {}
  forceinline void
  ManFlexTask::init(IntVar s, IntVar p, IntVar e, int c) {
    Unary::ManFlexTask::init(s,p,e); _c=c;
  }
  forceinline void
  ManFlexTask::init(const ManFlexTask& t) {
    Unary::ManFlexTask::init(t); _c=t._c;
  }

  forceinline int
  ManFlexTask::c(void) const {
    return _c;
  }
  forceinline long long int
  ManFlexTask::e(void) const {
    return static_cast<long long int>(pmin())*c();
  }

  forceinline void
  ManFlexTask::update(Space& home, bool share, ManFlexTask& t) {
    Unary::ManFlexTask::update(home,share,t); _c=t._c;
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const ManFlexTask& t) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << t.est() << ':' << t.lst() << ':' << t.pmin() << ':'
      << t.pmax() << ':' << t.c() << ':' << t.ect() << ':' << t.lct();
    return os << s.str();
  }

  /*
   * Optional fixed task
   */

  forceinline
  OptFixPTask::OptFixPTask(void) {}
  forceinline
  OptFixPTask::OptFixPTask(IntVar s, int p, int c, BoolVar m) {
    ManFixPTask::init(s,p,c); _m=m;
  }
  forceinline void
  OptFixPTask::init(IntVar s, int p, int c, BoolVar m) {
    ManFixPTask::init(s,p,c); _m=m;
  }
  forceinline
  OptFixPTask::operator Unary::OptFixPTask (void) {
    return Unary::OptFixPTask(_s,_p,_m);
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFixPTask& t) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << t.est() << ":[" << t.pmin() << ',' << t.c() << "]:" << t.lct() << ':'
      << (t.mandatory() ? '1' : (t.optional() ? '?' : '0'));
    return os << s.str();
  }

  /*
   * Optional fixed task
   */

  forceinline
  OptFixPSETask::OptFixPSETask(void) {}
  forceinline
  OptFixPSETask::OptFixPSETask(TaskType t,IntVar s,int p,int c,BoolVar m) {
    ManFixPSETask::init(t,s,p,c); _m=m;
  }
  forceinline void
  OptFixPSETask::init(TaskType t, IntVar s, int p, int c, BoolVar m) {
    ManFixPSETask::init(t,s,p,c); _m=m;
  }
  forceinline
  OptFixPSETask::operator Unary::OptFixPSETask (void) {
    return Unary::OptFixPSETask(_t,_s,_p,_m);
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFixPSETask& t) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << t.est() << ":[" << t.pmin() << ',' << t.c() << "]:" << t.lct() << ':'
      << (t.mandatory() ? '1' : (t.optional() ? '?' : '0'));
    return os << s.str();
  }

  /*
   * Optional flexible task
   */

  forceinline
  OptFlexTask::OptFlexTask(void) {}
  forceinline
  OptFlexTask::OptFlexTask(IntVar s, IntVar p, IntVar e, int c, BoolVar m) {
    ManFlexTask::init(s,p,e,c); _m=m;
  }
  forceinline void
  OptFlexTask::init(IntVar s, IntVar p, IntVar e, int c, BoolVar m) {
    ManFlexTask::init(s,p,e,c); _m=m;
  }
  forceinline
  OptFlexTask::operator Unary::OptFlexTask (void) {
    return Unary::OptFlexTask(_s,_p,_e,_m);
  }

  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const OptFlexTask& t) {
    std::basic_ostringstream<Char,Traits> s;
    s.copyfmt(os); s.width(0);
    s << t.est() << ':' << t.lst() << ':' << t.pmin() << ':'
      << t.pmax() << ':' << t.c() << ':' << t.ect() << ':' << t.lct()
      << (t.mandatory() ? '1' : (t.optional() ? '?' : '0'));
    return os << s.str();
  }
    
}}}

// STATISTICS: int-var
