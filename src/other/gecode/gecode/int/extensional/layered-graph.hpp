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

#include <climits>
#include <algorithm>

namespace Gecode { namespace Int { namespace Extensional {

  /**
   * \brief Traits class for variables
   *
   * Each variable must specialize this traits class and add a \code
   * typedef \endcode for the view \a View corresponding to this variable.
   */
  template<class Var>
  class VarTraits {};

  /**
   * \brief Traits class for variables
   *
   * This class specializes the VarTraits for integer variables.
   */
  template<>
  class VarTraits<IntVar> {
  public:
    /// The variable type of an IntView
    typedef Int::IntView View;
  };

  /**
   * \brief Traits class for variables
   *
   * This class specializes the VarTraits for Boolean variables.
   */
  template<>
  class VarTraits<BoolVar> {
  public:
    /// The variable type of an IntView
    typedef Int::BoolView View;
  };


  /*
   * States
   */
  template<class View, class Val, class Degree, class StateIdx>
  forceinline void
  LayeredGraph<View,Val,Degree,StateIdx>::State::init(void) {
    i_deg=o_deg=0; 
  }

  
  template<class View, class Val, class Degree, class StateIdx>
  forceinline typename LayeredGraph<View,Val,Degree,StateIdx>::State& 
  LayeredGraph<View,Val,Degree,StateIdx>::i_state(int i, StateIdx is) {
    return layers[i].states[is];
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline typename LayeredGraph<View,Val,Degree,StateIdx>::State& 
  LayeredGraph<View,Val,Degree,StateIdx>::i_state
  (int i, const typename LayeredGraph<View,Val,Degree,StateIdx>::Edge& e) {
    return i_state(i,e.i_state);
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline bool 
  LayeredGraph<View,Val,Degree,StateIdx>::i_dec
  (int i, const typename LayeredGraph<View,Val,Degree,StateIdx>::Edge& e) {
    return --i_state(i,e).o_deg == 0;
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline typename LayeredGraph<View,Val,Degree,StateIdx>::State& 
  LayeredGraph<View,Val,Degree,StateIdx>::o_state(int i, StateIdx os) {
    return layers[i+1].states[os];
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline typename LayeredGraph<View,Val,Degree,StateIdx>::State& 
  LayeredGraph<View,Val,Degree,StateIdx>::o_state
  (int i, const typename LayeredGraph<View,Val,Degree,StateIdx>::Edge& e) {
    return o_state(i,e.o_state);
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline bool 
  LayeredGraph<View,Val,Degree,StateIdx>::o_dec
  (int i, const typename LayeredGraph<View,Val,Degree,StateIdx>::Edge& e) {
    return --o_state(i,e).i_deg == 0;
  }


  /*
   * Value iterator
   */
  template<class View, class Val, class Degree, class StateIdx>
  forceinline
  LayeredGraph<View,Val,Degree,StateIdx>::LayerValues::LayerValues(void) {}
  template<class View, class Val, class Degree, class StateIdx>
  forceinline
  LayeredGraph<View,Val,Degree,StateIdx>::LayerValues
  ::LayerValues(const Layer& l)
    : s1(l.support), s2(l.support+l.size) {}
  template<class View, class Val, class Degree, class StateIdx>
  forceinline void
  LayeredGraph<View,Val,Degree,StateIdx>::LayerValues::init(const Layer& l) {
    s1=l.support; s2=l.support+l.size;
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline bool
  LayeredGraph<View,Val,Degree,StateIdx>::LayerValues
  ::operator ()(void) const {
    return s1<s2;
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline void
  LayeredGraph<View,Val,Degree,StateIdx>::LayerValues::operator ++(void) {
    s1++;
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline int
  LayeredGraph<View,Val,Degree,StateIdx>::LayerValues::val(void) const {
    return s1->val;
  }


  /*
   * Index advisors
   *
   */
  template<class View, class Val, class Degree, class StateIdx>
  forceinline
  LayeredGraph<View,Val,Degree,StateIdx>::Index::Index(Space& home, Propagator& p,
                                                       Council<Index>& c,
                                                       int i0)
    : Advisor(home,p,c), i(i0) {}

  template<class View, class Val, class Degree, class StateIdx>
  forceinline
  LayeredGraph<View,Val,Degree,StateIdx>::Index::Index(Space& home, bool share,
                                                       Index& a)
    : Advisor(home,share,a), i(a.i) {}


  /*
   * Index ranges
   *
   */
  template<class View, class Val, class Degree, class StateIdx>
  forceinline
  LayeredGraph<View,Val,Degree,StateIdx>::IndexRange::IndexRange(void)
    : _fst(INT_MAX), _lst(INT_MIN) {}
  template<class View, class Val, class Degree, class StateIdx>
  forceinline void
  LayeredGraph<View,Val,Degree,StateIdx>::IndexRange::reset(void) {
    _fst=INT_MAX; _lst=INT_MIN;
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline void
  LayeredGraph<View,Val,Degree,StateIdx>::IndexRange::add(int i) {
    _fst=std::min(_fst,i); _lst=std::max(_lst,i);
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline void
  LayeredGraph<View,Val,Degree,StateIdx>::IndexRange::add
  (const typename LayeredGraph<View,Val,Degree,StateIdx>::IndexRange& ir) {
    _fst=std::min(_fst,ir._fst); _lst=std::max(_lst,ir._lst);
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline bool
  LayeredGraph<View,Val,Degree,StateIdx>::IndexRange::empty(void) const {
    return _fst>_lst;
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline void
  LayeredGraph<View,Val,Degree,StateIdx>::IndexRange::lshift(int n) {
    if (empty())
      return;
    if (n > _lst) {
      reset();
    } else {
      _fst = std::max(0,_fst-n);
      _lst -= n;
    }
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline int
  LayeredGraph<View,Val,Degree,StateIdx>::IndexRange::fst(void) const {
    return _fst;
  }
  template<class View, class Val, class Degree, class StateIdx>
  forceinline int
  LayeredGraph<View,Val,Degree,StateIdx>::IndexRange::lst(void) const {
    return _lst;
  }



  /*
   * The layered graph
   *
   */

  template<class View, class Val, class Degree, class StateIdx>
  template<class Var>
  forceinline
  LayeredGraph<View,Val,Degree,StateIdx>::LayeredGraph(Home home,
                                                       const VarArgArray<Var>& x, 
                                                       const DFA& dfa)
    : Propagator(home), c(home), n(x.size()), 
      max_states(static_cast<StateIdx>(dfa.n_states())) {
    assert(n > 0);
  }

  template<class View, class Val, class Degree, class StateIdx>
  forceinline void
  LayeredGraph<View,Val,Degree,StateIdx>::audit(void) {
#ifdef GECODE_AUDIT
    // Check states and edge information to be consistent
    unsigned int n_e = 0; // Number of edges
    unsigned int n_s = 0; // Number of states
    StateIdx m_s = 0; // Maximal number of states per layer
    for (int i=n; i--; ) {
      n_s += layers[i].n_states;
      m_s = std::max(m_s,layers[i].n_states);
      for (ValSize j=layers[i].size; j--; )
        n_e += layers[i].support[j].n_edges;
    }
    n_s += layers[n].n_states;
    m_s = std::max(m_s,layers[n].n_states);
    assert(n_e == n_edges);
    assert(n_s <= n_states);
    assert(m_s <= max_states);
#endif
  }

  template<class View, class Val, class Degree, class StateIdx>
  template<class Var>
  forceinline ExecStatus
  LayeredGraph<View,Val,Degree,StateIdx>::initialize(Space& home,
                                                     const VarArgArray<Var>& x, 
                                                     const DFA& dfa) {

    Region r(home);

    // Allocate memory for layers
    layers = home.alloc<Layer>(n+1);

    // Allocate temporary memory for all possible states
    State* states = r.alloc<State>(max_states*(n+1));
    for (int i=static_cast<int>(max_states)*(n+1); i--; )
      states[i].init();
    for (int i=n+1; i--; )
      layers[i].states = states + i*max_states;

    // Allocate temporary memory for edges
    Edge* edges = r.alloc<Edge>(dfa.max_degree());

    // Mark initial state as being reachable
    i_state(0,0).i_deg = 1;

    // Forward pass: add transitions
    for (int i=0; i<n; i++) {
      layers[i].x = x[i];
      layers[i].support = home.alloc<Support>(layers[i].x.size());
      ValSize j=0;
      // Enter links leaving reachable states (indegree != 0)
      for (ViewValues<View> nx(layers[i].x); nx(); ++nx) {
        Degree n_edges=0;
        for (DFA::Transitions t(dfa,nx.val()); t(); ++t)
          if (i_state(i,static_cast<StateIdx>(t.i_state())).i_deg != 0) {
            i_state(i,static_cast<StateIdx>(t.i_state())).o_deg++;
            o_state(i,static_cast<StateIdx>(t.o_state())).i_deg++;
            edges[n_edges].i_state = static_cast<StateIdx>(t.i_state());
            edges[n_edges].o_state = static_cast<StateIdx>(t.o_state());
            n_edges++;
          }
        assert(n_edges <= dfa.max_degree());
        // Found support for value
        if (n_edges > 0) {
          Support& s = layers[i].support[j];
          s.val = static_cast<Val>(nx.val());
          s.n_edges = n_edges;
          s.edges = Heap::copy(home.alloc<Edge>(n_edges),edges,n_edges);
          j++;
        }
      }
      if ((layers[i].size = j) == 0)
        return ES_FAILED;
    }

    // Mark final states as reachable
    for (int s=dfa.final_fst(); s<dfa.final_lst(); s++)
      if (o_state(n-1,static_cast<StateIdx>(s)).i_deg != 0)
        o_state(n-1,static_cast<StateIdx>(s)).o_deg = 1;

    // Backward pass: prune all transitions that do not lead to final state
    for (int i=n; i--; ) {
      ValSize k=0;
      for (ValSize j=0; j<layers[i].size; j++) {
        Support& s = layers[i].support[j];
        for (Degree d=s.n_edges; d--; )
          if (o_state(i,s.edges[d]).o_deg == 0) {
            // Adapt states
            i_dec(i,s.edges[d]); o_dec(i,s.edges[d]);
            // Prune edge
            s.edges[d] = s.edges[--s.n_edges];
          }
        // Value has support, copy the support information
        if (s.n_edges > 0)
          layers[i].support[k++]=s;
      }
      if ((layers[i].size = k) == 0)
        return ES_FAILED;
      LayerValues lv(layers[i]);
      GECODE_ME_CHECK(layers[i].x.narrow_v(home,lv,false));
      if (!layers[i].x.assigned())
        layers[i].x.subscribe(home, *new (home) Index(home,*this,c,i));
    }

    // Copy and compress states, setup other information
    {
      // State map for in-states
      StateIdx* i_map = r.alloc<StateIdx>(max_states);
      // State map for out-states
      StateIdx* o_map = r.alloc<StateIdx>(max_states);
      // Number of in-states
      StateIdx i_n = 0;

      // Initialize map for in-states (special for last layer)
      // Degree for single final state
      unsigned int d = 0;
      for (StateIdx j=max_states; j--; )
        d += static_cast<unsigned int>(layers[n].states[j].i_deg);
      // Check whether all final states can be joined to a single state
      if (d > 
          static_cast<unsigned int>
          (Gecode::Support::IntTypeTraits<Degree>::max)) {
        // Initialize map for in-states
        for (StateIdx j=max_states; j--; )
          if ((layers[n].states[j].o_deg != 0) ||
              (layers[n].states[j].i_deg != 0))
            i_map[j]=i_n++;
      } else {
        i_n = 1;
        for (StateIdx j=max_states; j--; ) {
          layers[n].states[j].init();
          i_map[j] = 0;
        }
        layers[n].states[0].i_deg = static_cast<Degree>(d);
        layers[n].states[0].o_deg = 1;
      }
      layers[n].n_states = i_n;
      
      // Total number of states
      n_states = i_n;
      // Total number of edges
      n_edges = 0;
      // New maximal number of states
      StateIdx max_s = i_n;

      for (int i=n; i--; ) {
        // In-states become out-states
        std::swap(o_map,i_map); i_n=0;
        // Initialize map for in-states
        for (StateIdx j=max_states; j--; )
          if ((layers[i].states[j].o_deg != 0) ||
              (layers[i].states[j].i_deg != 0))
            i_map[j]=i_n++;
        layers[i].n_states = i_n;
        n_states += i_n;
        max_s = std::max(max_s,i_n);

        // Update states in edges
        for (ValSize j=layers[i].size; j--; ) {
          Support& s = layers[i].support[j];
          n_edges += s.n_edges;
          for (Degree d=s.n_edges; d--; ) {
            s.edges[d].i_state = i_map[s.edges[d].i_state];
            s.edges[d].o_state = o_map[s.edges[d].o_state];
          }
        }
      }

      // Allocate and copy states
      State* a_states = home.alloc<State>(n_states);
      for (int i=n+1; i--; ) {
        StateIdx k=0;
        for (StateIdx j=max_states; j--; )
          if ((layers[i].states[j].o_deg != 0) ||
              (layers[i].states[j].i_deg != 0))
            a_states[k++] = layers[i].states[j];
        assert(k == layers[i].n_states);
        layers[i].states = a_states;
        a_states += layers[i].n_states;
      }
      
      // Update maximal number of states
      max_states = max_s;
    }

    // Schedule if subsumption is needed
    if (c.empty())
      View::schedule(home,*this,ME_INT_VAL);

    audit();
    return ES_OK;
  }

  template<class View, class Val, class Degree, class StateIdx>
  ExecStatus
  LayeredGraph<View,Val,Degree,StateIdx>::advise(Space& home,
                                                 Advisor& _a, const Delta& d) {
    // Check whether state information has already been created
    if (layers[0].states == NULL) {
      State* states = home.alloc<State>(n_states);
      for (unsigned int i=n_states; i--; )
        states[i].init();
      layers[n].states = states;
      states += layers[n].n_states;
      for (int i=n; i--; ) {
        layers[i].states = states;
        states += layers[i].n_states;
        for (ValSize j=layers[i].size; j--; ) {
          Support& s = layers[i].support[j];
          for (Degree d=s.n_edges; d--; ) {
            i_state(i,s.edges[d]).o_deg++;
            o_state(i,s.edges[d]).i_deg++;
          }
        }
      }
    }
    
    Index& a = static_cast<Index&>(_a);
    const int i = a.i;

    if (layers[i].size <= layers[i].x.size()) {
      // Propagator has already done everything
      if (View::modevent(d) == ME_INT_VAL) {
        a.dispose(home,c);
        return c.empty() ? ES_NOFIX : ES_FIX;
      } else {
        return ES_FIX;
      }
    }

    bool i_mod = false;
    bool o_mod = false;

    if (View::modevent(d) == ME_INT_VAL) {
      Val n = static_cast<Val>(layers[i].x.val());
      ValSize j=0;
      for (; layers[i].support[j].val < n; j++) {
        Support& s = layers[i].support[j];
        n_edges -= s.n_edges;
        // Supported value not any longer in view
        for (Degree d=s.n_edges; d--; ) {
          // Adapt states
          o_mod |= i_dec(i,s.edges[d]);
          i_mod |= o_dec(i,s.edges[d]);
        }
      }
      assert(layers[i].support[j].val == n);
      layers[i].support[0] = layers[i].support[j++];
      ValSize s=layers[i].size;
      layers[i].size = 1;
      for (; j<s; j++) {
        Support& s = layers[i].support[j];
        n_edges -= s.n_edges;
        for (Degree d=s.n_edges; d--; ) {
          // Adapt states
          o_mod |= i_dec(i,s.edges[d]);
          i_mod |= o_dec(i,s.edges[d]);
        }
      }
    } else if (layers[i].x.any(d)) {
      ValSize j=0;
      ValSize k=0;
      ValSize s=layers[i].size;
      for (ViewRanges<View> rx(layers[i].x); rx() && (j<s);) {
        Support& s = layers[i].support[j];
        if (s.val < static_cast<Val>(rx.min())) {
          // Supported value not any longer in view
          n_edges -= s.n_edges;
          for (Degree d=s.n_edges; d--; ) {
            // Adapt states
            o_mod |= i_dec(i,s.edges[d]);
            i_mod |= o_dec(i,s.edges[d]);
          }
          ++j;
        } else if (s.val > static_cast<Val>(rx.max())) {
          ++rx;
        } else {
          layers[i].support[k++]=s;
          ++j;
        }
      }
      assert(k > 0);
      layers[i].size = k;
      // Remove remaining values
      for (; j<s; j++) {
        Support& s=layers[i].support[j];
        n_edges -= s.n_edges;
        for (Degree d=s.n_edges; d--; ) {
          // Adapt states
          o_mod |= i_dec(i,s.edges[d]);
          i_mod |= o_dec(i,s.edges[d]);
        }
      }
    } else {
      Val min = static_cast<Val>(layers[i].x.min(d));
      ValSize j=0;
      // Skip values smaller than min (to keep)
      for (; layers[i].support[j].val < min; j++) {}
      Val max = static_cast<Val>(layers[i].x.max(d));
      ValSize k=j;
      ValSize s=layers[i].size;
      // Remove pruned values
      for (; (j<s) && (layers[i].support[j].val <= max); j++) {
        Support& s=layers[i].support[j];
        n_edges -= s.n_edges;
        for (Degree d=s.n_edges; d--; ) {
          // Adapt states
          o_mod |= i_dec(i,s.edges[d]);
          i_mod |= o_dec(i,s.edges[d]);
        }
      }
      // Keep remaining values
      while (j<s)
        layers[i].support[k++]=layers[i].support[j++];
      layers[i].size =k;
      assert(k > 0);
    }

    audit();

    bool fix = true;
    if (o_mod && (i > 0)) {
      o_ch.add(i-1); fix = false;
     }
    if (i_mod && (i+1 < n)) {
      i_ch.add(i+1); fix = false;
    }
    if (fix) {
      if (View::modevent(d) == ME_INT_VAL) {
        a.dispose(home,c);
        return c.empty() ? ES_NOFIX : ES_FIX;
      }
      return ES_FIX;
    } else {
      return (View::modevent(d) == ME_INT_VAL)
        ? home.ES_NOFIX_DISPOSE(c,a) : ES_NOFIX;
    }
  }

  template<class View, class Val, class Degree, class StateIdx>
  forceinline size_t
  LayeredGraph<View,Val,Degree,StateIdx>::dispose(Space& home) {
    c.dispose(home);
    (void) Propagator::dispose(home);
    return sizeof(*this);
  }

  template<class View, class Val, class Degree, class StateIdx>
  ExecStatus
  LayeredGraph<View,Val,Degree,StateIdx>::propagate(Space& home,
                                                    const ModEventDelta&) {
    // Forward pass
    for (int i=i_ch.fst(); i<=i_ch.lst(); i++) {
      bool i_mod = false;
      bool o_mod = false;
      ValSize j=0;
      ValSize k=0;
      ValSize s=layers[i].size;
      do {
        Support& s=layers[i].support[j];
        n_edges -= s.n_edges;
        for (Degree d=s.n_edges; d--; )
          if (i_state(i,s.edges[d]).i_deg == 0) {
            // Adapt states
            o_mod |= i_dec(i,s.edges[d]);
            i_mod |= o_dec(i,s.edges[d]);
            // Remove edge
            s.edges[d] = s.edges[--s.n_edges];
          }
        n_edges += s.n_edges;
        // Check whether value is still supported
        if (s.n_edges == 0) {
          layers[i].size--;
          GECODE_ME_CHECK(layers[i].x.nq(home,s.val));
        } else {
          layers[i].support[k++]=s;
        }
      } while (++j<s);
      assert(k > 0);
      // Update modification information
      if (o_mod && (i > 0))
        o_ch.add(i-1);
      if (i_mod && (i+1 < n))
        i_ch.add(i+1);
    }

    // Backward pass
    for (int i=o_ch.lst(); i>=o_ch.fst(); i--) {
      bool o_mod = false;
      ValSize j=0;
      ValSize k=0;
      ValSize s=layers[i].size;
      do {
        Support& s=layers[i].support[j];
        n_edges -= s.n_edges;
        for (Degree d=s.n_edges; d--; )
          if (o_state(i,s.edges[d]).o_deg == 0) {
            // Adapt states
            o_mod |= i_dec(i,s.edges[d]);
            (void)   o_dec(i,s.edges[d]);
            // Remove edge
            s.edges[d] = s.edges[--s.n_edges];
          }
        n_edges += s.n_edges;
        // Check whether value is still supported
        if (s.n_edges == 0) {
          layers[i].size--;
          GECODE_ME_CHECK(layers[i].x.nq(home,s.val));
        } else {
          layers[i].support[k++]=s;
        }
      } while (++j<s);
      assert(k > 0);
      // Update modification information
      if (o_mod && (i > 0))
        o_ch.add(i-1);
    }

    a_ch.add(i_ch); i_ch.reset();
    a_ch.add(o_ch); o_ch.reset();

    audit();

    // Check subsumption
    if (c.empty())
      return home.ES_SUBSUMED(*this);
    else
      return ES_FIX;
  }


  template<class View, class Val, class Degree, class StateIdx>
  template<class Var>
  ExecStatus
  LayeredGraph<View,Val,Degree,StateIdx>::post(Home home, 
                                               const VarArgArray<Var>& x,
                                               const DFA& dfa) {
    if (x.size() == 0) {
      // Check whether the start state 0 is also a final state
      if ((dfa.final_fst() <= 0) && (dfa.final_lst() >= 0))
        return ES_OK;
      return ES_FAILED;
    }
    assert(x.size() > 0);
    for (int i=x.size(); i--; ) {
      DFA::Symbols s(dfa);
      typename VarTraits<Var>::View xi(x[i]);
      GECODE_ME_CHECK(xi.inter_v(home,s,false));
    }
    LayeredGraph<View,Val,Degree,StateIdx>* p =
      new (home) LayeredGraph<View,Val,Degree,StateIdx>(home,x,dfa);
    return p->initialize(home,x,dfa);
  }

  template<class View, class Val, class Degree, class StateIdx>
  forceinline
  LayeredGraph<View,Val,Degree,StateIdx>
  ::LayeredGraph(Space& home, bool share,
                 LayeredGraph<View,Val,Degree,StateIdx>& p)
    : Propagator(home,share,p), 
      n(p.n), layers(home.alloc<Layer>(n+1)),
      max_states(p.max_states), n_states(p.n_states), n_edges(p.n_edges) {
    c.update(home,share,p.c);
    // Do not allocate states, postpone to advise!
    layers[n].n_states = p.layers[n].n_states;
    layers[n].states = NULL;
    // Allocate memory for edges
    Edge* edges = home.alloc<Edge>(n_edges);
    // Copy layers
    for (int i=n; i--; ) {
      layers[i].x.update(home,share,p.layers[i].x);
      assert(layers[i].x.size() == p.layers[i].size);
      layers[i].size = p.layers[i].size;
      layers[i].support = home.alloc<Support>(layers[i].size);
      for (ValSize j=layers[i].size; j--; ) {
        layers[i].support[j].val = p.layers[i].support[j].val;
        layers[i].support[j].n_edges = p.layers[i].support[j].n_edges;
        assert(layers[i].support[j].n_edges > 0);
        layers[i].support[j].edges = 
          Heap::copy(edges,p.layers[i].support[j].edges,
                     layers[i].support[j].n_edges);
        edges += layers[i].support[j].n_edges;
      }
      layers[i].n_states = p.layers[i].n_states;
      layers[i].states = NULL;
    }
    audit();
  }

  template<class View, class Val, class Degree, class StateIdx>
  PropCost
  LayeredGraph<View,Val,Degree,StateIdx>::cost(const Space&,
                                               const ModEventDelta&) const {
    return PropCost::linear(PropCost::HI,n);
  }

  template<class View, class Val, class Degree, class StateIdx>
  Actor*
  LayeredGraph<View,Val,Degree,StateIdx>::copy(Space& home, bool share) {
    // Eliminate an assigned prefix
    {
      int k=0;
      while (layers[k].size == 1) {
        assert(layers[k].support[0].n_edges == 1);
        n_states -= layers[k].n_states;
        k++;
      }
      if (k > 0) {
        /*
         * The state information is always available: either the propagator
         * has been created (hence, also the state information has been
         * created), or the first variable become assigned and hence
         * an advisor must have been run (which then has created the state
         * information).
         */
        // Eliminate assigned layers
        n -= k; layers += k;
        // Eliminate edges
        n_edges -= static_cast<unsigned int>(k);
        // Update advisor indices
        for (Advisors<Index> as(c); as(); ++as)
          as.advisor().i -= k;
        // Update all change information
        a_ch.lshift(k);
      }
    }
    audit();

    // Compress states
    if (!a_ch.empty()) {
      int f = a_ch.fst();
      int l = a_ch.lst();
      assert((f >= 0) && (l <= n));
      Region r(home);
      // State map for in-states
      StateIdx* i_map = r.alloc<StateIdx>(max_states);
      // State map for out-states
      StateIdx* o_map = r.alloc<StateIdx>(max_states);
      // Number of in-states
      StateIdx i_n = 0;

      n_states -= layers[l].n_states;
      // Initialize map for in-states and compress
      for (StateIdx j=0; j<layers[l].n_states; j++)
        if ((layers[l].states[j].i_deg != 0) ||
            (layers[l].states[j].o_deg != 0)) {
          layers[l].states[i_n]=layers[l].states[j];
          i_map[j]=i_n++;
        }
      layers[l].n_states = i_n;
      n_states += layers[l].n_states;
      assert(i_n > 0);

      // Update in-states in edges for last layer, if any
      if (l < n)
        for (ValSize j=layers[l].size; j--; ) {
          Support& s = layers[l].support[j];
          for (Degree d=s.n_edges; d--; )
            s.edges[d].i_state = i_map[s.edges[d].i_state];
        }

      // Update all changed layers
      for (int i=l-1; i>=f; i--) {
        // In-states become out-states
        std::swap(o_map,i_map); i_n=0;
        // Initialize map for in-states and compress
        n_states -= layers[i].n_states;
        for (StateIdx j=0; j<layers[i].n_states; j++)
          if ((layers[i].states[j].o_deg != 0) ||
              (layers[i].states[j].i_deg != 0)) {
            layers[i].states[i_n]=layers[i].states[j];
            i_map[j]=i_n++;
          }
        layers[i].n_states = i_n;
        n_states += layers[i].n_states;
        assert(i_n > 0);

        // Update states in edges
        for (ValSize j=layers[i].size; j--; ) {
          Support& s = layers[i].support[j];
          for (Degree d=s.n_edges; d--; ) {
            s.edges[d].i_state = i_map[s.edges[d].i_state];
            s.edges[d].o_state = o_map[s.edges[d].o_state];
          }
        }
      }

      // Update out-states in edges for previous layer, if any
      if (f > 0)
        for (ValSize j=layers[f-1].size; j--; ) {
          Support& s = layers[f-1].support[j];
          for (Degree d=s.n_edges; d--; )
            s.edges[d].o_state = i_map[s.edges[d].o_state];
        }

      a_ch.reset();
    }
    audit();

    return new (home) LayeredGraph<View,Val,Degree,StateIdx>(home,share,*this);
  }

  /// Select small types for the layered graph propagator
  template<class Var>
  forceinline ExecStatus
  post_lgp(Home home, const VarArgArray<Var>& x, const DFA& dfa) {
    Gecode::Support::IntType t_state_idx =
      Gecode::Support::u_type(static_cast<unsigned int>(dfa.n_states()));
    Gecode::Support::IntType t_degree =
      Gecode::Support::u_type(dfa.max_degree());
    Gecode::Support::IntType t_val = 
      std::max(Support::s_type(dfa.symbol_min()),
               Support::s_type(dfa.symbol_max()));
    switch (t_val) {
    case Gecode::Support::IT_CHAR:
    case Gecode::Support::IT_SHRT:
      switch (t_state_idx) {
      case Gecode::Support::IT_CHAR:
        switch (t_degree) {
        case Gecode::Support::IT_CHAR:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,short int,unsigned char,unsigned char>
            ::post(home,x,dfa);
        case Gecode::Support::IT_SHRT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,short int,unsigned short int,unsigned char>
            ::post(home,x,dfa);
        case Gecode::Support::IT_INT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,short int,unsigned int,unsigned char>
            ::post(home,x,dfa);
        default: GECODE_NEVER;
        }
        break;
      case Gecode::Support::IT_SHRT:
        switch (t_degree) {
        case Gecode::Support::IT_CHAR:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,short int,unsigned char,unsigned short int>
            ::post(home,x,dfa);
        case Gecode::Support::IT_SHRT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,short int,unsigned short int,unsigned short int>
            ::post(home,x,dfa);
        case Gecode::Support::IT_INT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,short int,unsigned int,unsigned short int>
            ::post(home,x,dfa);
        default: GECODE_NEVER;
        }
        break;
      case Gecode::Support::IT_INT:
        switch (t_degree) {
        case Gecode::Support::IT_CHAR:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,short int,unsigned char,unsigned int>
            ::post(home,x,dfa);
        case Gecode::Support::IT_SHRT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,short int,unsigned short int,unsigned int>
            ::post(home,x,dfa);
        case Gecode::Support::IT_INT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,short int,unsigned int,unsigned int>
            ::post(home,x,dfa);
        default: GECODE_NEVER;
        }
        break;
      default: GECODE_NEVER;
      }

    case Gecode::Support::IT_INT:
      switch (t_state_idx) {
      case Gecode::Support::IT_CHAR:
        switch (t_degree) {
        case Gecode::Support::IT_CHAR:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,int,unsigned char,unsigned char>
            ::post(home,x,dfa);
        case Gecode::Support::IT_SHRT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,int,unsigned short int,unsigned char>
            ::post(home,x,dfa);
        case Gecode::Support::IT_INT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,int,unsigned int,unsigned char>
            ::post(home,x,dfa);
        default: GECODE_NEVER;
        }
        break;
      case Gecode::Support::IT_SHRT:
        switch (t_degree) {
        case Gecode::Support::IT_CHAR:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,int,unsigned char,unsigned short int>
            ::post(home,x,dfa);
        case Gecode::Support::IT_SHRT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,int,unsigned short int,unsigned short int>
            ::post(home,x,dfa);
        case Gecode::Support::IT_INT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,int,unsigned int,unsigned short int>
            ::post(home,x,dfa);
        default: GECODE_NEVER;
        }
        break;
      case Gecode::Support::IT_INT:
        switch (t_degree) {
        case Gecode::Support::IT_CHAR:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,int,unsigned char,unsigned int>
            ::post(home,x,dfa);
        case Gecode::Support::IT_SHRT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,int,unsigned short int,unsigned int>
            ::post(home,x,dfa);
        case Gecode::Support::IT_INT:
          return Extensional::LayeredGraph
            <typename VarTraits<Var>::View,int,unsigned int,unsigned int>
            ::post(home,x,dfa);
        default: GECODE_NEVER;
        }
        break;
      default: GECODE_NEVER;
      }

    default: GECODE_NEVER;
    }
    return ES_OK;
  }

}}}

// STATISTICS: int-prop

