/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2006
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

namespace Gecode { namespace Gist {

  template<class S>
  VarComparator<S>::VarComparator(std::string name)
    : TextOutput(name) {}

  template<class S>
  void
  VarComparator<S>::compare(const Space& s0, const Space& s1) {
    std::ostringstream result;
    dynamic_cast<const S&>(s0).compare(s1,result);
    if (result.str() != "") {
      init();
      addHtml("<pre>\n");
      getStream() << result.str() << std::endl;
      addHtml("</pre><hr />");
    }
  }

  template<class S>
  std::string
  VarComparator<S>::name(void) {
    return TextOutput::name();
  }

  template<class S>
  void
  VarComparator<S>::finalize(void) {
    TextOutput::finalize();
  }
  
  inline std::string
  Comparator::compare(std::string x_n, IntVar x, IntVar y) {
    IntVarRanges xr(x), yr(y);
    if (!Iter::Ranges::equal(xr,yr)) {
      std::ostringstream ret;
      ret << x_n << "=" << x << " -> " << y;
      return ret.str();
    }
    return "";
  }
  inline std::string
  Comparator::compare(std::string x_n, BoolVar x, BoolVar y) {
    if (! (x.min() == y.min() && x.max() == y.max()) ) {
      std::ostringstream ret;
      ret << x_n << "=" << x << " -> " << y;
      return ret.str();
    }
    return "";
  }
#ifdef GECODE_HAS_SET_VARS
  inline std::string
  Comparator::compare(std::string x_n, SetVar x, SetVar y) {
    SetVarGlbRanges xglbr(x), yglbr(y);
    SetVarLubRanges xlubr(x), ylubr(y);
    if (! (Iter::Ranges::equal(xglbr,yglbr) && 
           Iter::Ranges::equal(xlubr,ylubr) &&
           x.cardMin() == y.cardMin() &&
           y.cardMax() == y.cardMax()) ) {
      std::ostringstream ret;
      ret << x_n << "=" << x << " -> " << y;
      return ret.str();
    }
    return "";
  }
#endif
#ifdef GECODE_HAS_FLOAT_VARS
  inline std::string
  Comparator::compare(std::string x_n, FloatVar x, FloatVar y) {
    if (! (x.min() == y.min() && x.max() == y.max()) ) {
      std::ostringstream ret;
      ret << x_n << "=" << x << " -> " << y;
      return ret.str();
    }
    return "";
  }
#endif
  template<class Var>
  std::string
  Comparator::compare(std::string x_n, const VarArgArray<Var>& x, 
    const VarArgArray<Var>& y) {
    if (x.size() != y.size())
      return "Error: array size mismatch";
    std::ostringstream ret;
    bool first = true;
    for (int i=0; i<x.size(); i++) {
      std::ostringstream xni;
      xni << x_n << "[" << i << "]";
      std::string cmp = compare(xni.str(),x[i],y[i]);
      if (cmp != "") {
        if (!first) {
          ret << ", ";
        } else {
          first = false;          
        }
        ret << cmp;
      }
    }
    return ret.str();
  }
  
  template<class S>
  Print<S>::Print(const std::string& name)
    : TextOutput(name) {}

  template<class S>
  void
  Print<S>::inspect(const Space& node) {
    init();
    addHtml("<pre>\n");
    dynamic_cast<const S&>(node).print(getStream());
    flush();
    addHtml("</pre><hr />");
  }

  template<class S>
  std::string
  Print<S>::name(void) {
    return TextOutput::name();
  }

  template<class S>
  void
  Print<S>::finalize(void) {
    TextOutput::finalize();
  }

  forceinline
  Options::Options(void) {} 

  forceinline
  Options::_I::_I(void) : _click(heap,1), n_click(0),
    _solution(heap,1), n_solution(0),
    _move(heap,1), n_move(0), _compare(heap,1), n_compare(0) {}

  forceinline void
  Options::_I::click(Inspector* i) {
    _click[static_cast<int>(n_click++)] = i;
  }
  forceinline void
  Options::_I::solution(Inspector* i) {
    _solution[static_cast<int>(n_solution++)] = i;
  }
  forceinline void
  Options::_I::move(Inspector* i) {
    _move[static_cast<int>(n_move++)] = i;
  }
  forceinline void
  Options::_I::compare(Comparator* c) {
    _compare[static_cast<int>(n_compare++)] = c;
  }
  forceinline Inspector*
  Options::_I::click(unsigned int i) const {
    return (i < n_click) ? _click[i] : NULL;
  }
  forceinline Inspector*
  Options::_I::solution(unsigned int i) const {
    return (i < n_solution) ? _solution[i] : NULL;
  }
  forceinline Inspector*
  Options::_I::move(unsigned int i) const {
    return (i < n_move) ? _move[i] : NULL;
  }
  forceinline Comparator*
  Options::_I::compare(unsigned int i) const {
    return (i < n_compare) ? _compare[i] : NULL;
  }

  inline int
  dfs(Space* root, const Gist::Options& opt) {
    return explore(root, false, opt);
  }

  inline int
  bab(Space* root, const Gist::Options& opt) {
    return Gist::explore(root, true, opt);
  }

}}

// STATISTICS: gist-any
