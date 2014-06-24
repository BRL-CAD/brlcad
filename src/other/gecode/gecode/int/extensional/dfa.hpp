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

namespace Gecode {

  /**
   * \brief Data stored for a %DFA
   *
   */
  class DFA::DFAI : public SharedHandle::Object {
  public:
    /// Number of states
    int n_states;
    /// Number of symbols
    unsigned int n_symbols;
    /// Number of transitions
    int n_trans;
    /// Maximal degree (in-degree and out-degree of any state) and maximal number of transitions per symbol
    unsigned int max_degree;
    /// First final state
    int final_fst;
    /// Last final state
    int final_lst;
    /// The transitions
    Transition* trans;
    /// Specification of transition range
    class HashEntry {
    public:
      int symbol;            ///< Symbol
      const Transition* fst; ///< First transition for the symbol
      const Transition* lst; ///< Last transition for the symbol
    };
    /// The transition hash table by symbol
    HashEntry* table;
    /// Size of table (as binary logarithm)
    int n_log;
    /// Fill hash table
    GECODE_INT_EXPORT void fill(void);
    /// Initialize automaton implementation with \a nt transitions
    DFAI(int nt);
    /// Initialize automaton implementation as empty
    GECODE_INT_EXPORT DFAI(void);
    /// Delete automaton implemenentation
    virtual ~DFAI(void);
    /// Create a copy
    GECODE_INT_EXPORT virtual SharedHandle::Object* copy(void) const;
  };

  forceinline
  DFA::DFAI::DFAI(int nt)
    : trans(nt == 0 ? NULL : heap.alloc<Transition>(nt)) {}

  forceinline
  DFA::DFAI::~DFAI(void) {
    if (n_trans > 0)
      heap.rfree(trans);
    heap.rfree(table);
  }

  forceinline
  DFA::DFA(void) {}


  forceinline
  DFA::DFA(const DFA& d)
    : SharedHandle(d) {}

  forceinline int
  DFA::n_states(void) const {
    const DFAI* d = static_cast<DFAI*>(object());
    return (d == NULL) ? 1 : d->n_states;
  }

  forceinline unsigned int
  DFA::n_symbols(void) const {
    const DFAI* d = static_cast<DFAI*>(object());
    return (d == NULL) ? 0 : d->n_symbols;
  }

  forceinline int
  DFA::n_transitions(void) const {
    const DFAI* d = static_cast<DFAI*>(object());
    return (d == NULL) ? 0 : d->n_trans;
  }

  forceinline unsigned int
  DFA::max_degree(void) const {
    const DFAI* d = static_cast<DFAI*>(object());
    return (d == NULL) ? 0 : d->max_degree;
  }

  forceinline int
  DFA::final_fst(void) const {
    const DFAI* d = static_cast<DFAI*>(object());
    return (d == NULL) ? 0 : d->final_fst;
  }

  forceinline int
  DFA::final_lst(void) const {
    const DFAI* d = static_cast<DFAI*>(object());
    return (d == NULL) ? 0 : d->final_lst;
  }

  forceinline int
  DFA::symbol_min(void) const {
    const DFAI* d = static_cast<DFAI*>(object());
    return ((d != NULL) && (d->n_trans > 0)) ?
      d->trans[0].symbol : Int::Limits::min;
  }

  forceinline int
  DFA::symbol_max(void) const {
    const DFAI* d = static_cast<DFAI*>(object());
    return ((d != NULL) && (d->n_trans > 0)) ?
      d->trans[d->n_trans-1].symbol : Int::Limits::max;
  }


  /*
   * Constructing transitions
   *
   */

  forceinline
  DFA::Transition::Transition(void) {}

  forceinline
  DFA::Transition::Transition(int i_state0, int symbol0, int o_state0) 
    : i_state(i_state0), symbol(symbol0), o_state(o_state0) {}

  /*
   * Iterating over all transitions
   *
   */

  forceinline
  DFA::Transitions::Transitions(const DFA& d) {
    const DFAI* o = static_cast<DFAI*>(d.object());
    if (o != NULL) {
      c_trans = &o->trans[0];
      e_trans = c_trans+o->n_trans;
    } else {
      c_trans = e_trans = NULL;
    }
  }

  forceinline
  DFA::Transitions::Transitions(const DFA& d, int n) {
    const DFAI* o = static_cast<DFAI*>(d.object());
    if (o != NULL) {
      int mask = (1<<o->n_log)-1;
      int p = n & mask;
      while ((o->table[p].fst != NULL) && (o->table[p].symbol != n))
        p = (p+1) & mask;
      c_trans = o->table[p].fst;
      e_trans = o->table[p].lst;
    } else {
      c_trans = e_trans = NULL;
    }
  }

  forceinline bool
  DFA::Transitions::operator ()(void) const {
    return c_trans < e_trans;
  }

  forceinline void
  DFA::Transitions::operator ++(void) {
    c_trans++;
  }

  forceinline int
  DFA::Transitions::i_state(void) const {
    return c_trans->i_state;
  }

  forceinline int
  DFA::Transitions::symbol(void) const {
    return c_trans->symbol;
  }

  forceinline int
  DFA::Transitions::o_state(void) const {
    return c_trans->o_state;
  }

  /*
   * Iterating over symbols
   *
   */

  forceinline
  DFA::Symbols::Symbols(const DFA& d) {
    const DFAI* o = static_cast<DFAI*>(d.object());
    if (o != NULL) {
      c_trans = &o->trans[0];
      e_trans = c_trans+o->n_trans;
    } else {
      c_trans = e_trans = NULL;
    }
  }

  forceinline bool
  DFA::Symbols::operator ()(void) const {
    return c_trans < e_trans;
  }

  forceinline void
  DFA::Symbols::operator ++(void) {
    int s = c_trans->symbol;
    do {
      c_trans++;
    } while ((c_trans < e_trans) && (s == c_trans->symbol));
  }

  forceinline int
  DFA::Symbols::val(void) const {
    return c_trans->symbol;
  }


  template<class Char, class Traits>
  std::basic_ostream<Char,Traits>&
  operator <<(std::basic_ostream<Char,Traits>& os, const DFA& d) {
    std::basic_ostringstream<Char,Traits> st;
    st.copyfmt(os); st.width(0);
    st << "Start state: 0" << std::endl
       << "States:      0..." << d.n_states()-1 << std::endl
       << "Transitions:";
    for (int s = 0; s < static_cast<int>(d.n_states()); s++) {
      DFA::Transitions t(d);
      int n = 0;
      while (t()) {
        if (t.i_state() == s) {
          if ((n % 4) == 0)
            st << std::endl << "\t";
          st << "[" << t.i_state() << "]"
             << "- " << t.symbol() << " >"
             << "[" << t.o_state() << "]  ";
          ++n;
        }
        ++t;
      }
    }
    st << std::endl << "Final states: "
       << std::endl
       << "\t[" << d.final_fst() << "] ... ["
       << d.final_lst()-1 << "]"
       << std::endl;
    return os << st.str();
  }

}


// STATISTICS: int-prop

