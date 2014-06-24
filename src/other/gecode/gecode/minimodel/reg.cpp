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

#include <gecode/minimodel.hh>

namespace Gecode {

  namespace MiniModel {

    class PosSet;
    /**
     * \brief Allocator for position sets
     */
    typedef Support::BlockAllocator<PosSet,Heap> PosSetAllocator;

    class NodeInfo;
    class PosInfo;

  }

  /// Implementation of the actual expression tree
  class REG::Exp {
  public:
    /// Reference counter
    unsigned int use_cnt;
    /// Number of positions
    int _n_pos;
    /**
     * \brief Type of regular expression
     */
    enum ExpType {
      ET_SYMBOL,
      ET_CONC,
      ET_OR,
      ET_STAR
    };
    /// Type of regular expression
    ExpType type;
    /// Symbol or subexpressions
    union {
      /// Symbol
      int  symbol;
      /// Subexpressions
      Exp* kids[2];
    } data;

    MiniModel::PosSet*
    followpos(MiniModel::PosSetAllocator&,MiniModel::PosInfo*);
    void inc(void);
    void dec(void);
    int n_pos(void) const;
    /// Print expression
    template<class Char, class Traits>
    std::basic_ostream<Char,Traits>&
    print(std::basic_ostream<Char,Traits>& os) const;

    static void* operator new(size_t);
    static void  operator delete(void*);
  private:
    void dispose(void);
  };


  /*
   * Operations on expression nodes
   *
   */


  forceinline void*
  REG::Exp::operator new(size_t s) {
    return heap.ralloc(s);
  }
  forceinline void
  REG::Exp::operator delete(void*) {
    // Deallocation happens in dispose
  }

  void
  REG::Exp::dispose(void) {
    Support::DynamicStack<Exp*,Heap> todo(heap);
    todo.push(this);
    while (!todo.empty()) {
      Exp* e = todo.pop();
      switch (e->type) {
      case ET_OR:
      case ET_CONC:
        if ((e->data.kids[1] != NULL) && (--e->data.kids[1]->use_cnt == 0))
          todo.push(e->data.kids[1]);
      case ET_STAR:
        if ((e->data.kids[0] != NULL) && (--e->data.kids[0]->use_cnt == 0))
          todo.push(e->data.kids[0]);
      default: ;
      }
      heap.rfree(e);
    }
  }

  forceinline void
  REG::Exp::inc(void) {
    if (this != NULL)
      use_cnt++;
  }
  forceinline void
  REG::Exp::dec(void) {
    if ((this != NULL) && (--use_cnt == 0))
      dispose();
  }


  forceinline int
  REG::Exp::n_pos(void) const {
    return (this != NULL) ? _n_pos : 0;
  }


  /*
   * Regular expressions
   *
   */

  forceinline
  REG::REG(Exp* f) : e(f) {}

  REG::REG(void) : e(NULL) {}

  REG::REG(const REG& r) : e(r.e) {
    e->inc();
  }

  const REG&
  REG::operator =(const REG& r) {
    if (&r != this) {
      r.e->inc();
      e->dec();
      e = r.e;
    }
    return *this;
  }

  REG::~REG(void) {
    e->dec();
  }

  REG::REG(int s) : e(new Exp) {
    e->use_cnt     = 1;
    e->_n_pos      = 1;
    e->type        = REG::Exp::ET_SYMBOL;
    e->data.symbol = s;
  }

  REG::REG(const IntArgs& x) {
    int n = x.size();
    if (n < 1)
      throw MiniModel::TooFewArguments("REG");
    Exp** a = heap.alloc<Exp*>(n);
    // Initialize with symbols
    for (int i=n; i--; ) {
      a[i] = new Exp();
      a[i]->use_cnt     = 1;
      a[i]->_n_pos      = 1;
      a[i]->type        = REG::Exp::ET_SYMBOL;
      a[i]->data.symbol = x[i];
    }
    // Build a balanced tree of alternative nodes
    for (int m=n; m>1; ) {
      if (m & 1) {
        m -= 1;
        Exp* e1 = a[m];
        Exp* e2 = a[0];
        a[0] = new Exp;
        a[0]->use_cnt      = 1;
        a[0]->_n_pos       = e1->n_pos() + e2->n_pos();
        a[0]->type         = REG::Exp::ET_OR;
        a[0]->data.kids[0] = e1;
        a[0]->data.kids[1] = e2;
      } else {
        m >>= 1;
        for (int i=0; i<m; i++) {
          Exp* e1 = a[2*i];
          Exp* e2 = a[2*i+1];
          a[i] = new Exp;
          a[i]->use_cnt      = 1;
          a[i]->_n_pos       = e1->n_pos() + e2->n_pos();
          a[i]->type         = REG::Exp::ET_OR;
          a[i]->data.kids[0] = e1;
          a[i]->data.kids[1] = e2;
        }
      }
    }
    e = a[0];
    heap.free<Exp*>(a,n);
  }

  REG
  REG::operator |(const REG& r2) {
    if (e == r2.e)
      return *this;
    Exp* f = new Exp();
    f->use_cnt      = 1;
    f->_n_pos       = e->n_pos() + r2.e->n_pos();
    f->type         = REG::Exp::ET_OR;
    f->data.kids[0] = e;    e->inc();
    f->data.kids[1] = r2.e; r2.e->inc();
    REG r(f);
    return r;
  }

  REG&
  REG::operator |=(const REG& r2) {
    if (e == r2.e)
      return *this;
    Exp* f = new Exp();
    f->use_cnt      = 1;
    f->_n_pos       = e->n_pos() + r2.e->n_pos();
    f->type         = REG::Exp::ET_OR;
    f->data.kids[0] = e;
    f->data.kids[1] = r2.e; r2.e->inc();
    e=f;
    return *this;
  }

  REG
  REG::operator +(const REG& r2) {
    if (e == NULL)    return r2;
    if (r2.e == NULL) return *this;
    Exp* f = new Exp();
    f->use_cnt      = 1;
    f->_n_pos       = e->n_pos() + r2.e->n_pos();
    f->type         = REG::Exp::ET_CONC;
    f->data.kids[0] = e;    e->inc();
    f->data.kids[1] = r2.e; r2.e->inc();
    REG r(f);
    return r;
  }

  REG&
  REG::operator +=(const REG& r2) {
    if (r2.e == NULL)
      return *this;
    if (e == NULL) {
      e=r2.e; e->inc();
    } else {
      Exp* f = new Exp();
      f->use_cnt      = 1;
      f->_n_pos       = e->n_pos() + r2.e->n_pos();
      f->type         = REG::Exp::ET_CONC;
      f->data.kids[0] = e;
      f->data.kids[1] = r2.e; r2.e->inc();
      e=f;
    }
    return *this;
  }

  REG
  REG::operator *(void) {
    if ((e == NULL) || (e->type == REG::Exp::ET_STAR))
      return *this;
    Exp* f = new Exp();
    f->use_cnt      = 1;
    f->_n_pos       = e->n_pos();
    f->type         = REG::Exp::ET_STAR;
    f->data.kids[0] = e; e->inc();
    REG r(f);
    return r;
  }

  REG
  REG::operator ()(unsigned int n, unsigned int m) {
    REG r;
    if ((n>m) || (m == 0))
      return r;
    if (n>0) {
      unsigned int i = n;
      REG r0 = *this;
      while (i>0)
        if (i & 1) {
          r = r0+r; i--;
        } else {
          r0 = r0+r0; i >>= 1;
        }
    }
    if (m > n) {
      unsigned int i = m-n;
      REG s0;
      s0 = s0 | *this;
      REG s;
      while (i>0)
        if (i & 1) {
          s = s0+s; i--;
        } else {
          s0 = s0+s0; i >>= 1;
        }
      r = r + s;
    }
    return r;
  }

  REG
  REG::operator ()(unsigned int n) {
    REG r;
    if (n > 0) {
      REG r0 = *this;
      unsigned int i = n;
      while (i>0)
        if (i & 1) {
          r = r0+r; i--;
        } else {
          r0 = r0+r0; i >>= 1;
        }
    }
    return r+**this;
  }

  REG
  REG::operator +(void) {
    return this->operator ()(1);
  }


  namespace MiniModel {

    /*
     * Sets of positions
     *
     */

    /**
     * \brief Order on position sets
     */
    enum PosSetCmp {
      PSC_LE,
      PSC_EQ,
      PSC_GR
    };

    /**
     * \brief Sets of positions
     */
    class PosSet : public Support::BlockClient<PosSet,Heap> {
      // Maintain sets of positions in inverse order
      // This makes the check whether the last position is included
      // more efficient.
    public:
      int pos; PosSet* next;

      PosSet(void);
      PosSet(int);

      bool in(int) const;
      static PosSetCmp cmp(PosSet*,PosSet*);
      static PosSet*   cup(PosSetAllocator&,PosSet*,PosSet*);
    };


    forceinline
    PosSet::PosSet(void) {}
    forceinline
    PosSet::PosSet(int p) : pos(p), next(NULL) {}


    forceinline bool
    PosSet::in(int p) const {
      for (const PosSet* ps = this; ps != NULL; ps = ps->next)
        if (ps->pos == p) {
          return true;
        } else if (ps->pos < p) {
          return false;
        }
      return false;
    }

    forceinline PosSetCmp
    PosSet::cmp(PosSet* ps1, PosSet* ps2) {
      while ((ps1 != NULL) && (ps2 != NULL)) {
        if (ps1 == ps2)
          return PSC_EQ;
        if (ps1->pos < ps2->pos)
          return PSC_LE;
        if (ps1->pos > ps2->pos)
          return PSC_GR;
        ps1 = ps1->next; ps2 = ps2->next;
      }
      if (ps1 == ps2)
        return PSC_EQ;
      return ps1 == NULL ? PSC_LE : PSC_GR;
    }

    PosSet*
    PosSet::cup(PosSetAllocator& psm, PosSet* ps1, PosSet* ps2) {
      PosSet*  ps;
      PosSet** p = &ps;
      while ((ps1 != NULL) && (ps2 != NULL)) {
        if (ps1 == ps2) {
          *p = ps1; return ps;
        }
        PosSet* n = new (psm) PosSet;
        *p = n; p = &n->next;
        if (ps1->pos == ps2->pos) {
          n->pos = ps1->pos;
          ps1 = ps1->next; ps2 = ps2->next;
        } else if (ps1->pos > ps2->pos) {
          n->pos = ps1->pos; ps1 = ps1->next;
        } else {
          n->pos = ps2->pos; ps2 = ps2->next;
        }
      }
      *p = (ps1 != NULL) ? ps1 : ps2;
      return ps;
    }


    /// Node information computed during traversal of the expressions
    class NodeInfo {
    public:
      bool    nullable;
      PosSet* firstpos;
      PosSet* lastpos;
      NodeInfo(bool n=false, PosSet* fp=NULL, PosSet* lp=NULL);
    };

    /// Expression information
    class ExpInfo {
    public:
      REG::Exp* exp;
      bool open;
      ExpInfo(REG::Exp* e=NULL);
    };
    
    /**
     * \brief Information on positions collected during traversal
     *
     */
    class PosInfo {
    public:
      int     symbol;
      PosSet* followpos;
    };

    forceinline
    NodeInfo::NodeInfo(bool n, PosSet* fp, PosSet* lp)
      : nullable(n), firstpos(fp), lastpos(lp) {}

    forceinline
    ExpInfo::ExpInfo(REG::Exp* e)
      : exp(e), open(true) {}

  }

  forceinline MiniModel::PosSet*
  REG::Exp::followpos(MiniModel::PosSetAllocator& psm,
                      MiniModel::PosInfo* pi) {
    int p=0;

    using MiniModel::PosSet;
    using MiniModel::NodeInfo;
    using MiniModel::ExpInfo;

    Support::DynamicStack<ExpInfo,Heap> todo(heap);
    Support::DynamicStack<NodeInfo,Heap> done(heap);

    // Start with first expression to be processed
    todo.push(ExpInfo(this));

    do {
      if (todo.top().exp == NULL) {
        todo.pop();
        done.push(NodeInfo(true,NULL,NULL));
      } else {
        switch (todo.top().exp->type) {
        case ET_SYMBOL:
          {
            pi[p].symbol = todo.pop().exp->data.symbol;
            PosSet* ps = new (psm) PosSet(p++);
            done.push(NodeInfo(false,ps,ps));
          }
          break;
        case ET_STAR:
          if (todo.top().open) {
            // Evaluate subexpression recursively
            todo.top().open = false;
            todo.push(todo.top().exp->data.kids[0]);
          } else {
            todo.pop();
            NodeInfo ni = done.pop();
            for (PosSet* ps = ni.lastpos; ps != NULL; ps = ps->next)
              pi[ps->pos].followpos =
                PosSet::cup(psm,pi[ps->pos].followpos,ni.firstpos);
            done.push(NodeInfo(true,ni.firstpos,ni.lastpos));
          }
          break;
        case ET_CONC:
          if (todo.top().open) {
            // Evaluate subexpressions recursively
            todo.top().open = false;
            REG::Exp* e = todo.top().exp;
            todo.push(e->data.kids[1]);
            todo.push(e->data.kids[0]);
          } else {
            todo.pop();
            NodeInfo ni1 = done.pop();
            NodeInfo ni0 = done.pop();
            for (PosSet* ps = ni0.lastpos; ps != NULL; ps = ps->next)
              pi[ps->pos].followpos =
                PosSet::cup(psm,pi[ps->pos].followpos,ni1.firstpos);
            done.push(NodeInfo(ni0.nullable & ni1.nullable,
                               ni0.nullable ? 
                               PosSet::cup(psm,ni0.firstpos,ni1.firstpos) : ni0.firstpos,
                               ni1.nullable ? 
                               PosSet::cup(psm,ni0.lastpos,ni1.lastpos) : ni1.lastpos));
          }
          break;
        case ET_OR:
          if (todo.top().open) {
            // Evaluate subexpressions recursively
            todo.top().open = false;
            REG::Exp* e = todo.top().exp;
            todo.push(e->data.kids[1]);
            todo.push(e->data.kids[0]);
          } else {
            todo.pop();
            NodeInfo ni1 = done.pop();
            NodeInfo ni0 = done.pop();
            done.push(NodeInfo(ni0.nullable | ni1.nullable,
                               PosSet::cup(psm,ni0.firstpos,ni1.firstpos),
                               PosSet::cup(psm,ni0.lastpos,ni1.lastpos)));
          }
          break;
        default: GECODE_NEVER;
        }
      }
    } while (!todo.empty());
    return done.top().firstpos;
  }

    
  namespace MiniModel {

    class StateNode;

    /**
     * \brief Allocator for state nodes
     */
    typedef Support::BlockAllocator<StateNode,Heap> StatePoolAllocator;

    /**
     * \brief %Node together with state information
     */
    class StateNode : public Support::BlockClient<StateNode,Heap> {
    public:
      PosSet*    pos;
      int        state;
      StateNode* next;
      StateNode* left;
      StateNode* right;
    };

    /**
     * \brief %State pool combines a tree of states together with yet unprocessed states
     */
    class StatePool {
    public:
      int   n_states;
      StateNode  root;
      StateNode* next;
      StateNode* all;

      StatePool(PosSet*);

      StateNode* pop(void);
      bool empty(void) const;

      int state(StatePoolAllocator&, PosSet*);
    };

    forceinline
    StatePool::StatePool(PosSet* ps) {
      next     = &root;
      all      = NULL;
      n_states = 1;
      root.pos   = ps;
      root.state = 0;
      root.next  = NULL;
      root.left  = NULL;
      root.right = NULL;
    }

    forceinline StateNode*
    StatePool::pop(void) {
      StateNode* n = next;
      next = n->next;
      n->next = all;
      all = n;
      return n;
    }

    forceinline bool
    StatePool::empty(void) const {
      return next == NULL;
    }

    forceinline int
    StatePool::state(StatePoolAllocator& spm, PosSet* ps) {
      int d = 0;
      StateNode** p = NULL;
      StateNode*  n = &root;
      do {
        switch (PosSet::cmp(ps,n->pos)) {
        case PSC_EQ: return n->state;
        case PSC_LE: p = &n->left;  n = *p; break;
        case PSC_GR: p = &n->right; n = *p; break;
        default: GECODE_NEVER;
        }
        d++;
      } while (n != NULL);
      n = new (spm) StateNode; *p = n;
      n->pos   = ps;
      n->state = n_states++;
      n->next  = next;
      n->left  = NULL;
      n->right = NULL;
      next = n;
      return n->state;
    }

    /**
     * \brief Sort symbols
     */
    class SymbolsInc {
    public:
      forceinline bool
      operator ()(int x, int y) {
        return x < y;
      }
      forceinline static void
      sort(int s[], int n) {
        SymbolsInc o;
        Support::quicksort<int,SymbolsInc>(s,n,o);
      }
    };


    /**
     * \brief For collecting transitions while constructing a %DFA
     *
     */
    class TransitionBag {
    private:
      Support::DynamicArray<DFA::Transition,Heap> t;
      int n;
    public:
      TransitionBag(void);
      void add(int,int,int);
      void finish(void);
      DFA::Transition* transitions(void);
    };

    forceinline
    TransitionBag::TransitionBag(void) : t(heap), n(0) {}

    forceinline void
    TransitionBag::add(int i_state, int symbol, int o_state) {
      t[n].i_state = i_state;
      t[n].symbol  = symbol;
      t[n].o_state = o_state;
      n++;
    }

    forceinline void
    TransitionBag::finish(void) {
      t[n].i_state = -1;
    }

    forceinline DFA::Transition*
    TransitionBag::transitions(void) {
      return &t[0];
    }


    /**
     * \brief For collecting final states while constructing a %DFA
     *
     */
    class FinalBag {
    private:
      Support::DynamicArray<int,Heap> f;
      int n;
    public:
      FinalBag(void);
      void add(int);
      void finish(void);
      int* finals(void);
    };

    forceinline
    FinalBag::FinalBag(void) : f(heap), n(0) {}

    forceinline void
    FinalBag::add(int state) {
      f[n++] = state;
    }

    forceinline void
    FinalBag::finish(void) {
      f[n] = -1;
    }

    forceinline int*
    FinalBag::finals(void) {
      return &f[0];
    }

  }

  REG::operator DFA(void) {
    using MiniModel::PosSetAllocator;
    using MiniModel::StatePoolAllocator;
    using MiniModel::PosInfo;
    using MiniModel::PosSet;
    using MiniModel::NodeInfo;

    using MiniModel::StatePool;
    using MiniModel::StateNode;

    using MiniModel::TransitionBag;
    using MiniModel::FinalBag;

    using MiniModel::SymbolsInc;

    PosSetAllocator    psm(heap);
    StatePoolAllocator spm(heap);
    REG r = *this + REG(Int::Limits::max+1);
    int n_pos = r.e->n_pos();

    PosInfo* pi = heap.alloc<PosInfo>(n_pos);
    for (int i=n_pos; i--; )
      pi[i].followpos = NULL;

    PosSet* firstpos = r.e->followpos(psm,&pi[0]);

    // Compute symbols
    int* symbols = heap.alloc<int>(n_pos);
    for (int i=n_pos; i--; )
      symbols[i] = pi[i].symbol;

    SymbolsInc::sort(&symbols[0],n_pos-1);
    int n_symbols = 1;
    for (int i = 1; i<n_pos-1; i++)
      if (symbols[i-1] != symbols[i])
        symbols[n_symbols++] = symbols[i];

    // Compute states and transitions
    TransitionBag tb;
    StatePool sp(firstpos);
    while (!sp.empty()) {
      StateNode* sn = sp.pop();
      for (int i = n_symbols; i--; ) {
        PosSet* u = NULL;
        for (PosSet* ps = sn->pos; ps != NULL; ps = ps->next)
          if (pi[ps->pos].symbol == symbols[i])
            u = PosSet::cup(psm,u,pi[ps->pos].followpos);
        if (u != NULL)
          tb.add(sn->state,symbols[i],sp.state(spm,u));
      }
    }
    tb.finish();

    // Compute final states
    FinalBag fb;
    for (StateNode* n = sp.all; n != NULL; n = n->next)
      if (n->pos->in(n_pos-1))
        fb.add(n->state);
    fb.finish();

    heap.free<PosInfo>(pi,n_pos);
    heap.free<int>(symbols,n_pos);
    return DFA(0,tb.transitions(),fb.finals(),true);
  }

}

// STATISTICS: minimodel-any

