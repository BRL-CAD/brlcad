/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
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

#include <cstring>

namespace Gecode {

  namespace Driver {

    /*
     * String option
     *
     */
    inline const char*
    StringValueOption::value(void) const {
      return cur;
    }

    /*
     * String option
     *
     */
    inline
    StringOption::StringOption(const char* o, const char* e, int v)
      : BaseOption(o,e), cur(v), fst(NULL), lst(NULL) {}
    inline void
    StringOption::value(int v) {
      cur = v;
    }
    inline int
    StringOption::value(void) const {
      return cur;
    }
    
    /*
     * Integer option
     *
     */
    inline
    IntOption::IntOption(const char* o, const char* e, int v)
      : BaseOption(o,e), cur(v) {}
    inline void
    IntOption::value(int v) {
      cur = v;
    }
    inline int
    IntOption::value(void) const {
      return cur;
    }

    /*
     * Unsigned integer option
     *
     */
    inline
    UnsignedIntOption::UnsignedIntOption(const char* o, const char* e,
                                         unsigned int v)
      : BaseOption(o,e), cur(v) {}
    inline void
    UnsignedIntOption::value(unsigned int v) {
      cur = v;
    }
    inline unsigned int
    UnsignedIntOption::value(void) const {
      return cur;
    }

    /*
     * Double option
     *
     */
    inline
    DoubleOption::DoubleOption(const char* o, const char* e,
                               double v)
      : BaseOption(o,e), cur(v) {}
    inline void
    DoubleOption::value(double v) {
      cur = v;
    }
    inline double
    DoubleOption::value(void) const {
      return cur;
    }

    /*
     * Bool option
     *
     */
    inline
    BoolOption::BoolOption(const char* o, const char* e, bool v) 
      : BaseOption(o,e), cur(v) {}
    inline void
    BoolOption::value(bool v) {
      cur = v;
    }
    inline bool
    BoolOption::value(void) const {
      return cur;
    }


  }
  
  /*
   * Options
   *
   */
  inline const char*
  BaseOptions::name(void) const {
    return _name;
  }



  /*
   * Model options
   *
   */
  inline void
  Options::model(int v) {
    _model.value(v);
  }
  inline void
  Options::model(int v, const char* o, const char* h) {
    _model.add(v,o,h);
  }
  inline int
  Options::model(void) const {
    return _model.value();
  }
  
  inline void
  Options::symmetry(int v) {
    _symmetry.value(v);
  }
  inline void
  Options::symmetry(int v, const char* o, const char* h) {
    _symmetry.add(v,o,h);
  }
  inline int
  Options::symmetry(void) const {
    return _symmetry.value();
  }
  
  inline void
  Options::propagation(int v) {
    _propagation.value(v);
  }
  inline void
  Options::propagation(int v, const char* o, const char* h) {
    _propagation.add(v,o,h);
  }
  inline int
  Options::propagation(void) const {
    return _propagation.value();
  }
  
  inline void
  Options::icl(IntConLevel i) {
    _icl.value(i);
  }
  inline IntConLevel
  Options::icl(void) const {
    return static_cast<IntConLevel>(_icl.value());
  }
  
  inline void
  Options::branching(int v) {
    _branching.value(v);
  }
  inline void
  Options::branching(int v, const char* o, const char* h) {
    _branching.add(v,o,h);
  }
  inline int
  Options::branching(void) const {
    return _branching.value();
  }

  inline void
  Options::decay(double d) {
    _decay.value(d);
  }
  inline double
  Options::decay(void) const {
    return _decay.value();
  }
  
  
  /*
   * Search options
   *
   */
  inline void
  Options::search(int v) {
    _search.value(v);
  }
  inline void
  Options::search(int v, const char* o, const char* h) {
    _search.add(v,o,h);
  }
  inline int
  Options::search(void) const {
    return _search.value();
  }
  
  inline void
  Options::solutions(unsigned int n) {
    _solutions.value(n);
  }
  inline unsigned int
  Options::solutions(void) const {
    return _solutions.value();
  }
  
  inline void
  Options::threads(double n) {
    _threads.value(n);
  }
  inline double
  Options::threads(void) const {
    return _threads.value();
  }
  
  inline void
  Options::c_d(unsigned int d) {
    _c_d.value(d);
  }
  inline unsigned int
  Options::c_d(void) const {
    return _c_d.value();
  }
  
  inline void
  Options::a_d(unsigned int d) {
    _a_d.value(d);
  }
  inline unsigned int
  Options::a_d(void) const {
    return _a_d.value();
  }
  
  inline void
  Options::node(unsigned int n) {
    _node.value(n);
  }
  inline unsigned int
  Options::node(void) const {
    return _node.value();
  }
  
  inline void
  Options::fail(unsigned int n) {
    _fail.value(n);
  }
  inline unsigned int
  Options::fail(void) const {
    return _fail.value();
  }
  
  inline void
  Options::time(unsigned int t) {
    _time.value(t);
  }
  inline unsigned int
  Options::time(void) const {
    return _time.value();
  }

  inline void
  Options::restart(RestartMode rm) {
    _restart.value(rm);
  }
  inline RestartMode
  Options::restart(void) const {
    return static_cast<RestartMode>(_restart.value());
  }
  
  inline void
  Options::restart_base(double n) {
    _r_base.value(n);
  }
  inline double
  Options::restart_base(void) const {
    return _r_base.value();
  }
  
  inline void
  Options::restart_scale(unsigned int n) {
    _r_scale.value(n);
  }
  inline unsigned int
  Options::restart_scale(void) const {
    return _r_scale.value();
  }
  
  inline void
  Options::nogoods(bool b) {
    _nogoods.value(b);
  }
  inline bool
  Options::nogoods(void) const {
    return _nogoods.value();
  }
  
  inline void
  Options::nogoods_limit(unsigned int l) {
    _nogoods_limit.value(l);
  }
  inline unsigned int
  Options::nogoods_limit(void) const {
    return _nogoods_limit.value();
  }
  
  
  
  inline void
  Options::interrupt(bool b) {
    _interrupt.value(b);
  }
  inline bool
  Options::interrupt(void) const {
    return _interrupt.value();
  }
  
  
  /*
   * Execution options
   *
   */
  inline void
  Options::mode(ScriptMode sm) {
    _mode.value(sm);
  }
  inline ScriptMode
  Options::mode(void) const {
    return static_cast<ScriptMode>(_mode.value());
  }
  
  inline void
  Options::samples(unsigned int s) {
    _samples.value(s);
  }
  inline unsigned int
  Options::samples(void) const {
    return _samples.value();
  }

  inline void
  Options::iterations(unsigned int i) {
    _iterations.value(i);
  }
  inline unsigned int
  Options::iterations(void) const {
    return _iterations.value();
  }
  
  inline void
  Options::print_last(bool p) {
    _print_last.value(p);
  }
  inline bool
  Options::print_last(void) const {
    return _print_last.value();
  }

  inline void
  Options::out_file(const char *f) {
    _out_file.value(f);
  }

  inline const char*
  Options::out_file(void) const {
    return _out_file.value();
  }

  inline void
  Options::log_file(const char* f) {
    _log_file.value(f);
  }

  inline const char*
  Options::log_file(void) const {
    return _log_file.value();
  }

#ifdef GECODE_HAS_GIST
  forceinline
  Options::_I::_I(void) : _click(heap,1), n_click(0),
    _solution(heap,1), n_solution(0), _move(heap,1), n_move(0),
    _compare(heap,1), n_compare(0) {}

  forceinline void
  Options::_I::click(Gist::Inspector* i) {
    _click[static_cast<int>(n_click++)] = i;
  }
  forceinline void
  Options::_I::solution(Gist::Inspector* i) {
    _solution[static_cast<int>(n_solution++)] = i;
  }
  forceinline void
  Options::_I::move(Gist::Inspector* i) {
    _move[static_cast<int>(n_move++)] = i;
  }
  forceinline void
  Options::_I::compare(Gist::Comparator* i) {
    _compare[static_cast<int>(n_compare++)] = i;
  }
  forceinline Gist::Inspector*
  Options::_I::click(unsigned int i) const {
    return (i < n_click) ? _click[i] : NULL;
  }
  forceinline Gist::Inspector*
  Options::_I::solution(unsigned int i) const {
    return (i < n_solution) ? _solution[i] : NULL;
  }
  forceinline Gist::Inspector*
  Options::_I::move(unsigned int i) const {
    return (i < n_move) ? _move[i] : NULL;
  }
  forceinline Gist::Comparator*
  Options::_I::compare(unsigned int i) const {
    return (i < n_compare) ? _compare[i] : NULL;
  }
#endif

  /*
   * Options with additional size argument
   *
   */
  inline void
  SizeOptions::size(unsigned int s) {
    _size = s;
  }
  inline unsigned int
  SizeOptions::size(void) const {
    return _size;
  }
  
  /*
   * Options with additional string argument
   *
   */
  inline const char*
  InstanceOptions::instance(void) const {
    return _inst;
  }
  
}

// STATISTICS: driver-any
