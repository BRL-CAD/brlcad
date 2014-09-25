/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Gabriel Hjort Blindell <gabriel.hjort.blindell@gmail.com>
 *
 *  Copyright:
 *     Guido Tack, 2007-2012
 *     Gabriel Hjort Blindell, 2012
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

#include <gecode/flatzinc.hh>
#include <gecode/flatzinc/registry.hh>
#include <gecode/flatzinc/plugin.hh>

#include <gecode/search.hh>

#include <vector>
#include <string>
#include <sstream>
#include <limits>
using namespace std;

namespace Gecode { namespace FlatZinc {

  /**
   * \brief Branching on the introduced variables
   *
   * This brancher makes sure that when a solution is found for the model 
   * variables, all introduced variables are either assigned, or the solution
   * can be extended to a solution of the introduced variables.
   *
   * The advantage over simply branching over the introduced variables is that 
   * only one such extension will be searched for, instead of enumerating all 
   * possible (equivalent) extensions.
   *
   */
  class AuxVarBrancher : public Brancher {
  protected:
    /// Flag whether brancher is done
    bool done;
    /// Construct brancher
    AuxVarBrancher(Home home, TieBreak<IntVarBranch> int_varsel0,
                   IntValBranch int_valsel0,
                   TieBreak<IntVarBranch> bool_varsel0,
                   IntValBranch bool_valsel0
#ifdef GECODE_HAS_SET_VARS
                   ,
                   SetVarBranch set_varsel0,
                   SetValBranch set_valsel0
#endif
#ifdef GECODE_HAS_FLOAT_VARS
                   ,
                   TieBreak<FloatVarBranch> float_varsel0,
                   FloatValBranch float_valsel0
#endif
                   )
      : Brancher(home), done(false),
        int_varsel(int_varsel0), int_valsel(int_valsel0),
        bool_varsel(bool_varsel0), bool_valsel(bool_valsel0)
#ifdef GECODE_HAS_SET_VARS
        , set_varsel(set_varsel0), set_valsel(set_valsel0)
#endif
#ifdef GECODE_HAS_FLOAT_VARS
        , float_varsel(float_varsel0), float_valsel(float_valsel0)
#endif
        {}
    /// Copy constructor
    AuxVarBrancher(Space& home, bool share, AuxVarBrancher& b)
      : Brancher(home, share, b), done(b.done) {}

    /// %Choice that only signals failure or success
    class Choice : public Gecode::Choice {
    public:
      /// Whether brancher should fail
      bool fail;
      /// Initialize choice for brancher \a b
      Choice(const Brancher& b, bool fail0)
        : Gecode::Choice(b,1), fail(fail0) {}
      /// Report size occupied
      virtual size_t size(void) const {
        return sizeof(Choice);
      }
      /// Archive into \a e
      virtual void archive(Archive& e) const {
        Gecode::Choice::archive(e);
        e.put(fail);
      }
    };

    TieBreak<IntVarBranch> int_varsel;
    IntValBranch int_valsel;
    TieBreak<IntVarBranch> bool_varsel;
    IntValBranch bool_valsel;
#ifdef GECODE_HAS_SET_VARS
    SetVarBranch set_varsel;
    SetValBranch set_valsel;
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    TieBreak<FloatVarBranch> float_varsel;
    FloatValBranch float_valsel;
#endif

  public:
    /// Check status of brancher, return true if alternatives left.
    virtual bool status(const Space& _home) const {
      if (done) return false;
      const FlatZincSpace& home = static_cast<const FlatZincSpace&>(_home);
      for (int i=0; i<home.iv_aux.size(); i++)
        if (!home.iv_aux[i].assigned()) return true;
      for (int i=0; i<home.bv_aux.size(); i++)
        if (!home.bv_aux[i].assigned()) return true;
#ifdef GECODE_HAS_SET_VARS
      for (int i=0; i<home.sv_aux.size(); i++)
        if (!home.sv_aux[i].assigned()) return true;
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      for (int i=0; i<home.fv_aux.size(); i++)
        if (!home.fv_aux[i].assigned()) return true;
#endif
      // No non-assigned variables left
      return false;
    }
    /// Return choice
    virtual Choice* choice(Space& home) {
      done = true;
      FlatZincSpace& fzs = static_cast<FlatZincSpace&>(*home.clone());
      fzs.needAuxVars = false;
      branch(fzs,fzs.iv_aux,int_varsel,int_valsel);
      branch(fzs,fzs.bv_aux,bool_varsel,bool_valsel);
#ifdef GECODE_HAS_SET_VARS
      branch(fzs,fzs.sv_aux,set_varsel,set_valsel);
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      branch(fzs,fzs.fv_aux,float_varsel,float_valsel);
#endif
      Search::Options opt; opt.clone = false;
      FlatZincSpace* sol = dfs(&fzs, opt);
      if (sol) {
        delete sol;
        return new Choice(*this,false);
      } else {
        return new Choice(*this,true);
      }
    }
    /// Return choice
    virtual Choice* choice(const Space&, Archive& e) {
      bool fail; e >> fail;
      return new Choice(*this, fail);
    }
    /// Perform commit for choice \a c
    virtual ExecStatus commit(Space&, const Gecode::Choice& c, unsigned int) {
      return static_cast<const Choice&>(c).fail ? ES_FAILED : ES_OK;
    }
    /// Print explanation
    virtual void print(const Space&, const Gecode::Choice& c, 
                       unsigned int,
                       std::ostream& o) const {
      o << "FlatZinc(" 
        << (static_cast<const Choice&>(c).fail ? "fail" : "ok")
        << ")";
    }
    /// Copy brancher
    virtual Actor* copy(Space& home, bool share) {
      return new (home) AuxVarBrancher(home, share, *this);
    }
    /// Post brancher
    static void post(Home home,
                     TieBreak<IntVarBranch> int_varsel,
                     IntValBranch int_valsel,
                     TieBreak<IntVarBranch> bool_varsel,
                     IntValBranch bool_valsel
#ifdef GECODE_HAS_SET_VARS
                     ,
                     SetVarBranch set_varsel,
                     SetValBranch set_valsel
#endif
#ifdef GECODE_HAS_FLOAT_VARS
                     ,
                     TieBreak<FloatVarBranch> float_varsel,
                     FloatValBranch float_valsel
#endif
                   ) {
      (void) new (home) AuxVarBrancher(home, int_varsel, int_valsel,
                                       bool_varsel, bool_valsel
#ifdef GECODE_HAS_SET_VARS
                                       , set_varsel, set_valsel
#endif
#ifdef GECODE_HAS_FLOAT_VARS
                                       , float_varsel, float_valsel
#endif
                                       );
    }
    /// Delete brancher and return its size
    virtual size_t dispose(Space&) {
      return sizeof(*this);
    }      
  };

  class BranchInformationO : public SharedHandle::Object {
  private:
    struct BI {
      string r0;
      string r1;
      vector<string> n;
      BI(void) : r0(""), r1(""), n(0) {}
      BI(const string& r00, const string& r10, const vector<string>& n0)
        : r0(r00), r1(r10), n(n0) {}
    };
    vector<BI> v;
    BranchInformationO(vector<BI> v0) : v(v0) {}
  public:
    BranchInformationO(void) {}
    virtual ~BranchInformationO(void) {}
    virtual SharedHandle::Object* copy(void) const {
      return new BranchInformationO(v);
    }
    /// Add new brancher information
    void add(const BrancherHandle& bh,
             const string& rel0,
             const string& rel1,
             const vector<string>& n) {
      v.resize(std::max(static_cast<unsigned int>(v.size()),bh.id()+1));
      v[bh.id()] = BI(rel0,rel1,n);
    }
    /// Output branch information
    void print(const BrancherHandle& bh,
               int a, int i, int n, ostream& o) const {
      const BI& bi = v[bh.id()];
      o << bi.n[i] << " " << (a==0 ? bi.r0 : bi.r1) << " " << n;
    }
#ifdef GECODE_HAS_FLOAT_VARS
    void print(const BrancherHandle& bh,
               int a, int i, const FloatNumBranch& nl, ostream& o) const {
      const BI& bi = v[bh.id()];
      o << bi.n[i] << " "
        << (((a == 0) == nl.l) ? "<=" : ">=") << nl.n;
    }
#endif
  };
  
  BranchInformation::BranchInformation(void)
    : SharedHandle(NULL) {}

  BranchInformation::BranchInformation(const BranchInformation& bi)
    : SharedHandle(bi) {}

  void
  BranchInformation::init(void) {
    assert(object() == NULL);
    object(new BranchInformationO());
  }

  void
  BranchInformation::add(const BrancherHandle& bh,
                         const std::string& rel0,
                         const std::string& rel1,
                         const std::vector<std::string>& n) {
    static_cast<BranchInformationO*>(object())->add(bh,rel0,rel1,n);
  }
  void
  BranchInformation::print(const BrancherHandle& bh, int a, int i,
                           int n, std::ostream& o) const {
    static_cast<const BranchInformationO*>(object())->print(bh,a,i,n,o);
  }
#ifdef GECODE_HAS_FLOAT_VARS
  void
  BranchInformation::print(const BrancherHandle& bh, int a, int i,
                           const FloatNumBranch& nl, std::ostream& o) const {
    static_cast<const BranchInformationO*>(object())->print(bh,a,i,nl,o);
  }
#endif
  template<class Var>
  void varValPrint(const Space &home, const BrancherHandle& bh,
                   unsigned int a,
                   Var, int i, const int& n,
                   std::ostream& o) {
    static_cast<const FlatZincSpace&>(home).branchInfo.print(bh,a,i,n,o);
  }

#ifdef GECODE_HAS_FLOAT_VARS
  void varValPrintF(const Space &home, const BrancherHandle& bh,
                    unsigned int a,
                    FloatVar, int i, const FloatNumBranch& nl,
                    std::ostream& o) {
    static_cast<const FlatZincSpace&>(home).branchInfo.print(bh,a,i,nl,o);
  }
#endif

  IntSet vs2is(IntVarSpec* vs) {
    if (vs->assigned) {
      return IntSet(vs->i,vs->i);
    }
    if (vs->domain()) {
      AST::SetLit* sl = vs->domain.some();
      if (sl->interval) {
        return IntSet(sl->min, sl->max);
      } else {
        int* newdom = heap.alloc<int>(static_cast<unsigned long int>(sl->s.size()));
        for (int i=sl->s.size(); i--;)
          newdom[i] = sl->s[i];
        IntSet ret(newdom, sl->s.size());
        heap.free(newdom, static_cast<unsigned long int>(sl->s.size()));
        return ret;
      }
    }
    return IntSet(Int::Limits::min, Int::Limits::max);
  }

  int vs2bsl(BoolVarSpec* bs) {
    if (bs->assigned) {
      return bs->i;
    }
    if (bs->domain()) {
      AST::SetLit* sl = bs->domain.some();
      assert(sl->interval);
      return std::min(1, std::max(0, sl->min));
    }
    return 0;
  }

  int vs2bsh(BoolVarSpec* bs) {
    if (bs->assigned) {
      return bs->i;
    }
    if (bs->domain()) {
      AST::SetLit* sl = bs->domain.some();
      assert(sl->interval);
      return std::max(0, std::min(1, sl->max));
    }
    return 1;
  }

  TieBreak<IntVarBranch> ann2ivarsel(AST::Node* ann, Rnd rnd, double decay) {
    if (AST::Atom* s = dynamic_cast<AST::Atom*>(ann)) {
      if (s->id == "input_order")
        return TieBreak<IntVarBranch>(INT_VAR_NONE());
      if (s->id == "first_fail")
        return TieBreak<IntVarBranch>(INT_VAR_SIZE_MIN());
      if (s->id == "anti_first_fail")
        return TieBreak<IntVarBranch>(INT_VAR_SIZE_MAX());
      if (s->id == "smallest")
        return TieBreak<IntVarBranch>(INT_VAR_MIN_MIN());
      if (s->id == "largest")
        return TieBreak<IntVarBranch>(INT_VAR_MAX_MAX());
      if (s->id == "occurrence")
        return TieBreak<IntVarBranch>(INT_VAR_DEGREE_MAX());
      if (s->id == "max_regret")
        return TieBreak<IntVarBranch>(INT_VAR_REGRET_MIN_MAX());
      if (s->id == "most_constrained")
        return TieBreak<IntVarBranch>(INT_VAR_SIZE_MIN(),
                                      INT_VAR_DEGREE_MAX());
      if (s->id == "random") {
        return TieBreak<IntVarBranch>(INT_VAR_RND(rnd));
      }
      if (s->id == "afc_min")
        return TieBreak<IntVarBranch>(INT_VAR_AFC_MIN(decay));
      if (s->id == "afc_max")
        return TieBreak<IntVarBranch>(INT_VAR_AFC_MAX(decay));
      if (s->id == "afc_size_min")
        return TieBreak<IntVarBranch>(INT_VAR_AFC_SIZE_MIN(decay));
      if (s->id == "afc_size_max") {
        return TieBreak<IntVarBranch>(INT_VAR_AFC_SIZE_MAX(decay));
      }
      if (s->id == "activity_min")
        return TieBreak<IntVarBranch>(INT_VAR_ACTIVITY_MIN(decay));
      if (s->id == "activity_max")
        return TieBreak<IntVarBranch>(INT_VAR_ACTIVITY_MAX(decay));
      if (s->id == "activity_size_min")
        return TieBreak<IntVarBranch>(INT_VAR_ACTIVITY_SIZE_MIN(decay));
      if (s->id == "activity_size_max")
        return TieBreak<IntVarBranch>(INT_VAR_ACTIVITY_SIZE_MAX(decay));
    }
    std::cerr << "Warning, ignored search annotation: ";
    ann->print(std::cerr);
    std::cerr << std::endl;
    return TieBreak<IntVarBranch>(INT_VAR_NONE());
  }

  IntValBranch ann2ivalsel(AST::Node* ann, std::string& r0, std::string& r1,
                           Rnd rnd) {
    if (AST::Atom* s = dynamic_cast<AST::Atom*>(ann)) {
      if (s->id == "indomain_min") {
        r0 = "="; r1 = "!=";
        return INT_VAL_MIN();
      }
      if (s->id == "indomain_max") {
        r0 = "="; r1 = "!=";
        return INT_VAL_MAX();
      }
      if (s->id == "indomain_median") {
        r0 = "="; r1 = "!=";
        return INT_VAL_MED();
      }
      if (s->id == "indomain_split") {
        r0 = "<="; r1 = ">";
        return INT_VAL_SPLIT_MIN();
      }
      if (s->id == "indomain_reverse_split") {
        r0 = ">"; r1 = "<=";
        return INT_VAL_SPLIT_MAX();
      }
      if (s->id == "indomain_random") {
        r0 = "="; r1 = "!=";
        return INT_VAL_RND(rnd);
      }
      if (s->id == "indomain") {
        r0 = "="; r1 = "=";
        return INT_VALUES_MIN();
      }
      if (s->id == "indomain_middle") {
        std::cerr << "Warning, replacing unsupported annotation "
                  << "indomain_middle with indomain_median" << std::endl;
        r0 = "="; r1 = "!=";
        return INT_VAL_MED();
      }
      if (s->id == "indomain_interval") {
        std::cerr << "Warning, replacing unsupported annotation "
                  << "indomain_interval with indomain_split" << std::endl;
        r0 = "<="; r1 = ">";
        return INT_VAL_SPLIT_MIN();
      }
    }
    std::cerr << "Warning, ignored search annotation: ";
    ann->print(std::cerr);
    std::cerr << std::endl;
    r0 = "="; r1 = "!=";
    return INT_VAL_MIN();
  }

  IntAssign ann2asnivalsel(AST::Node* ann, Rnd rnd) {
    if (AST::Atom* s = dynamic_cast<AST::Atom*>(ann)) {
      if (s->id == "indomain_min")
        return INT_ASSIGN_MIN();
      if (s->id == "indomain_max")
        return INT_ASSIGN_MAX();
      if (s->id == "indomain_median")
        return INT_ASSIGN_MED();
      if (s->id == "indomain_random") {
        return INT_ASSIGN_RND(rnd);
      }
    }
    std::cerr << "Warning, ignored search annotation: ";
    ann->print(std::cerr);
    std::cerr << std::endl;
    return INT_ASSIGN_MIN();
  }

#ifdef GECODE_HAS_SET_VARS
  SetVarBranch ann2svarsel(AST::Node* ann, Rnd rnd, double decay) {
    if (AST::Atom* s = dynamic_cast<AST::Atom*>(ann)) {
      if (s->id == "input_order")
        return SET_VAR_NONE();
      if (s->id == "first_fail")
        return SET_VAR_SIZE_MIN();
      if (s->id == "anti_first_fail")
        return SET_VAR_SIZE_MAX();
      if (s->id == "smallest")
        return SET_VAR_MIN_MIN();
      if (s->id == "largest")
        return SET_VAR_MAX_MAX();
      if (s->id == "afc_min")
        return SET_VAR_AFC_MIN(decay);
      if (s->id == "afc_max")
        return SET_VAR_AFC_MAX(decay);
      if (s->id == "afc_size_min")
        return SET_VAR_AFC_SIZE_MIN(decay);
      if (s->id == "afc_size_max")
        return SET_VAR_AFC_SIZE_MAX(decay);
      if (s->id == "activity_min")
        return SET_VAR_ACTIVITY_MIN(decay);
      if (s->id == "activity_max")
        return SET_VAR_ACTIVITY_MAX(decay);
      if (s->id == "activity_size_min")
        return SET_VAR_ACTIVITY_SIZE_MIN(decay);
      if (s->id == "activity_size_max")
        return SET_VAR_ACTIVITY_SIZE_MAX(decay);
      if (s->id == "random") {
        return SET_VAR_RND(rnd);
      }
    }
    std::cerr << "Warning, ignored search annotation: ";
    ann->print(std::cerr);
    std::cerr << std::endl;
    return SET_VAR_NONE();
  }

  SetValBranch ann2svalsel(AST::Node* ann, std::string r0, std::string r1,
                           Rnd rnd) {
    (void) rnd;
    if (AST::Atom* s = dynamic_cast<AST::Atom*>(ann)) {
      if (s->id == "indomain_min") {
        r0 = "in"; r1 = "not in";
        return SET_VAL_MIN_INC();
      }
      if (s->id == "indomain_max") {
        r0 = "in"; r1 = "not in";
        return SET_VAL_MAX_INC();
      }
      if (s->id == "outdomain_min") {
        r1 = "in"; r0 = "not in";
        return SET_VAL_MIN_EXC();
      }
      if (s->id == "outdomain_max") {
        r1 = "in"; r0 = "not in";
        return SET_VAL_MAX_EXC();
      }
    }
    std::cerr << "Warning, ignored search annotation: ";
    ann->print(std::cerr);
    std::cerr << std::endl;
    r0 = "in"; r1 = "not in";
    return SET_VAL_MIN_INC();
  }
#endif

#ifdef GECODE_HAS_FLOAT_VARS
  TieBreak<FloatVarBranch> ann2fvarsel(AST::Node* ann, Rnd rnd,
                                       double decay) {
    if (AST::Atom* s = dynamic_cast<AST::Atom*>(ann)) {
      if (s->id == "input_order")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_NONE());
      if (s->id == "first_fail")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_SIZE_MIN());
      if (s->id == "anti_first_fail")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_SIZE_MAX());
      if (s->id == "smallest")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_MIN_MIN());
      if (s->id == "largest")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_MAX_MAX());
      if (s->id == "occurrence")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_DEGREE_MAX());
      if (s->id == "most_constrained")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_SIZE_MIN(),
                                        FLOAT_VAR_DEGREE_MAX());
      if (s->id == "random") {
        return TieBreak<FloatVarBranch>(FLOAT_VAR_RND(rnd));
      }
      if (s->id == "afc_min")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_AFC_MIN(decay));
      if (s->id == "afc_max")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_AFC_MAX(decay));
      if (s->id == "afc_size_min")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_AFC_SIZE_MIN(decay));
      if (s->id == "afc_size_max")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_AFC_SIZE_MAX(decay));
      if (s->id == "activity_min")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_ACTIVITY_MIN(decay));
      if (s->id == "activity_max")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_ACTIVITY_MAX(decay));
      if (s->id == "activity_size_min")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_ACTIVITY_SIZE_MIN(decay));
      if (s->id == "activity_size_max")
        return TieBreak<FloatVarBranch>(FLOAT_VAR_ACTIVITY_SIZE_MAX(decay));
    }
    std::cerr << "Warning, ignored search annotation: ";
    ann->print(std::cerr);
    std::cerr << std::endl;
    return TieBreak<FloatVarBranch>(FLOAT_VAR_NONE());
  }

  FloatValBranch ann2fvalsel(AST::Node* ann, std::string r0, std::string r1) {
    if (AST::Atom* s = dynamic_cast<AST::Atom*>(ann)) {
      if (s->id == "indomain_split") {
        r0 = "<="; r1 = ">";
        return FLOAT_VAL_SPLIT_MIN();
      }
      if (s->id == "indomain_reverse_split") {
        r1 = "<="; r0 = ">";
        return FLOAT_VAL_SPLIT_MAX();
      }
    }
    std::cerr << "Warning, ignored search annotation: ";
    ann->print(std::cerr);
    std::cerr << std::endl;
    r0 = "<="; r1 = ">";
    return FLOAT_VAL_SPLIT_MIN();
  }

#endif

  FlatZincSpace::FlatZincSpace(bool share, FlatZincSpace& f)
    : Space(share, f), _solveAnnotations(NULL), iv_boolalias(NULL),
      needAuxVars(f.needAuxVars) {
      _optVar = f._optVar;
      _optVarIsInt = f._optVarIsInt;
      _method = f._method;
      branchInfo.update(*this, share, f.branchInfo);
      iv.update(*this, share, f.iv);
      intVarCount = f.intVarCount;
      
      if (needAuxVars) {
        IntVarArgs iva;
        for (int i=0; i<f.iv_aux.size(); i++) {
          if (!f.iv_aux[i].assigned()) {
            iva << IntVar();
            iva[iva.size()-1].update(*this, share, f.iv_aux[i]);
          }
        }
        iv_aux = IntVarArray(*this, iva);
      }
      
      bv.update(*this, share, f.bv);
      boolVarCount = f.boolVarCount;
      if (needAuxVars) {
        BoolVarArgs bva;
        for (int i=0; i<f.bv_aux.size(); i++) {
          if (!f.bv_aux[i].assigned()) {
            bva << BoolVar();
            bva[bva.size()-1].update(*this, share, f.bv_aux[i]);
          }
        }
        bv_aux = BoolVarArray(*this, bva);
      }

#ifdef GECODE_HAS_SET_VARS
      sv.update(*this, share, f.sv);
      setVarCount = f.setVarCount;
      if (needAuxVars) {
        SetVarArgs sva;
        for (int i=0; i<f.sv_aux.size(); i++) {
          if (!f.sv_aux[i].assigned()) {
            sva << SetVar();
            sva[sva.size()-1].update(*this, share, f.sv_aux[i]);
          }
        }
        sv_aux = SetVarArray(*this, sva);
      }
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      fv.update(*this, share, f.fv);
      floatVarCount = f.floatVarCount;
      if (needAuxVars) {
        FloatVarArgs fva;
        for (int i=0; i<f.fv_aux.size(); i++) {
          if (!f.fv_aux[i].assigned()) {
            fva << FloatVar();
            fva[fva.size()-1].update(*this, share, f.fv_aux[i]);
          }
        }
        fv_aux = FloatVarArray(*this, fva);
      }
#endif
    }
  
  FlatZincSpace::FlatZincSpace(void)
  : intVarCount(-1), boolVarCount(-1), floatVarCount(-1), setVarCount(-1),
    _optVar(-1), _optVarIsInt(true), 
    _solveAnnotations(NULL), needAuxVars(true) {
    branchInfo.init();
  }

  void
  FlatZincSpace::init(int intVars, int boolVars,
                      int setVars, int floatVars) {
    (void) setVars;
    (void) floatVars;

    intVarCount = 0;
    iv = IntVarArray(*this, intVars);
    iv_introduced = std::vector<bool>(2*intVars);
    iv_boolalias = alloc<int>(intVars+(intVars==0?1:0));
    boolVarCount = 0;
    bv = BoolVarArray(*this, boolVars);
    bv_introduced = std::vector<bool>(2*boolVars);
#ifdef GECODE_HAS_SET_VARS
    setVarCount = 0;
    sv = SetVarArray(*this, setVars);
    sv_introduced = std::vector<bool>(2*setVars);
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    floatVarCount = 0;
    fv = FloatVarArray(*this, floatVars);
    fv_introduced = std::vector<bool>(2*floatVars);
#endif
  }

  void
  FlatZincSpace::newIntVar(IntVarSpec* vs) {
    if (vs->alias) {
      iv[intVarCount++] = iv[vs->i];
    } else {
      IntSet dom(vs2is(vs));
      if (dom.size()==0) {
        fail();
        return;
      } else {
        iv[intVarCount++] = IntVar(*this, dom);
      }
    }
    iv_introduced[2*(intVarCount-1)] = vs->introduced;
    iv_introduced[2*(intVarCount-1)+1] = vs->funcDep;
    iv_boolalias[intVarCount-1] = -1;
  }

  void
  FlatZincSpace::aliasBool2Int(int iv, int bv) {
    iv_boolalias[iv] = bv;
  }
  int
  FlatZincSpace::aliasBool2Int(int iv) {
    return iv_boolalias[iv];
  }

  void
  FlatZincSpace::newBoolVar(BoolVarSpec* vs) {
    if (vs->alias) {
      bv[boolVarCount++] = bv[vs->i];
    } else {
      bv[boolVarCount++] = BoolVar(*this, vs2bsl(vs), vs2bsh(vs));
    }
    bv_introduced[2*(boolVarCount-1)] = vs->introduced;
    bv_introduced[2*(boolVarCount-1)+1] = vs->funcDep;
  }

#ifdef GECODE_HAS_SET_VARS
  void
  FlatZincSpace::newSetVar(SetVarSpec* vs) {
    if (vs->alias) {
      sv[setVarCount++] = sv[vs->i];
    } else if (vs->assigned) {
      assert(vs->upperBound());
      AST::SetLit* vsv = vs->upperBound.some();
      if (vsv->interval) {
        IntSet d(vsv->min, vsv->max);
        sv[setVarCount++] = SetVar(*this, d, d);
      } else {
        int* is = heap.alloc<int>(static_cast<unsigned long int>(vsv->s.size()));
        for (int i=vsv->s.size(); i--; )
          is[i] = vsv->s[i];
        IntSet d(is, vsv->s.size());
        heap.free(is,static_cast<unsigned long int>(vsv->s.size()));
        sv[setVarCount++] = SetVar(*this, d, d);
      }
    } else if (vs->upperBound()) {
      AST::SetLit* vsv = vs->upperBound.some();
      if (vsv->interval) {
        IntSet d(vsv->min, vsv->max);
        sv[setVarCount++] = SetVar(*this, IntSet::empty, d);
      } else {
        int* is = heap.alloc<int>(static_cast<unsigned long int>(vsv->s.size()));
        for (int i=vsv->s.size(); i--; )
          is[i] = vsv->s[i];
        IntSet d(is, vsv->s.size());
        heap.free(is,static_cast<unsigned long int>(vsv->s.size()));
        sv[setVarCount++] = SetVar(*this, IntSet::empty, d);
      }
    } else {
      sv[setVarCount++] = SetVar(*this, IntSet::empty,
                                 IntSet(Set::Limits::min, 
                                        Set::Limits::max));
    }
    sv_introduced[2*(setVarCount-1)] = vs->introduced;
    sv_introduced[2*(setVarCount-1)+1] = vs->funcDep;
  }
#else
  void
  FlatZincSpace::newSetVar(SetVarSpec*) {
    throw FlatZinc::Error("Gecode", "set variables not supported");
  }
#endif

#ifdef GECODE_HAS_FLOAT_VARS
  void
  FlatZincSpace::newFloatVar(FloatVarSpec* vs) {
    if (vs->alias) {
      fv[floatVarCount++] = fv[vs->i];
    } else {
      double dmin, dmax;
      if (vs->domain()) {
        dmin = vs->domain.some().first;
        dmax = vs->domain.some().second;
        if (dmin > dmax) {
          fail();
          return;
        }
      } else {
        dmin = Float::Limits::min;
        dmax = Float::Limits::max;
      }
      fv[floatVarCount++] = FloatVar(*this, dmin, dmax);
    }
    fv_introduced[2*(floatVarCount-1)] = vs->introduced;
    fv_introduced[2*(floatVarCount-1)+1] = vs->funcDep;
  }
#else
  void
  FlatZincSpace::newFloatVar(FloatVarSpec*) {
    throw FlatZinc::Error("Gecode", "float variables not supported");
  }
#endif

  namespace {
    struct ConExprOrder {
      bool operator() (ConExpr* ce0, ConExpr* ce1) {
        return ce0->args->a.size() < ce1->args->a.size();
      }
    };
  }

  void
  FlatZincSpace::postConstraints(std::vector<ConExpr*>& ces) {
    ConExprOrder ceo;
    std::sort(ces.begin(), ces.end(), ceo);
    
    for (unsigned int i=0; i<ces.size(); i++) {
      const ConExpr& ce = *ces[i];
      try {
        registry().post(*this, ce);
      } catch (Gecode::Exception& e) {
        throw FlatZinc::Error("Gecode", e.what());
      } catch (AST::TypeError& e) {
        throw FlatZinc::Error("Type error", e.what());
      }
      delete ces[i];
      ces[i] = NULL;
    }
  }

  void flattenAnnotations(AST::Array* ann, std::vector<AST::Node*>& out) {
      for (unsigned int i=0; i<ann->a.size(); i++) {
        if (ann->a[i]->isCall("seq_search")) {
          AST::Call* c = ann->a[i]->getCall();
          if (c->args->isArray())
            flattenAnnotations(c->args->getArray(), out);
          else
            out.push_back(c->args);
        } else {
          out.push_back(ann->a[i]);
        }
      }
  }

  void
  FlatZincSpace::createBranchers(AST::Node* ann, int seed, double decay,
                                 bool ignoreUnknown,
                                 std::ostream& err) {
    Rnd rnd(static_cast<unsigned int>(seed));
    TieBreak<IntVarBranch> def_int_varsel = INT_VAR_AFC_SIZE_MAX(0.99);
    IntValBranch def_int_valsel = INT_VAL_MIN();
    TieBreak<IntVarBranch> def_bool_varsel = INT_VAR_AFC_MAX(0.99);
    IntValBranch def_bool_valsel = INT_VAL_MIN();
#ifdef GECODE_HAS_SET_VARS
    SetVarBranch def_set_varsel = SET_VAR_AFC_SIZE_MAX(0.99);
    SetValBranch def_set_valsel = SET_VAL_MIN_INC();
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    TieBreak<FloatVarBranch> def_float_varsel = FLOAT_VAR_SIZE_MIN();
    FloatValBranch def_float_valsel = FLOAT_VAL_SPLIT_MIN();
#endif

    std::vector<bool> iv_searched(iv.size());
    for (unsigned int i=iv.size(); i--;)
      iv_searched[i] = false;
    std::vector<bool> bv_searched(bv.size());
    for (unsigned int i=bv.size(); i--;)
      bv_searched[i] = false;
#ifdef GECODE_HAS_SET_VARS
    std::vector<bool> sv_searched(sv.size());
    for (unsigned int i=sv.size(); i--;)
      sv_searched[i] = false;
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    std::vector<bool> fv_searched(fv.size());
    for (unsigned int i=fv.size(); i--;)
      fv_searched[i] = false;
#endif

    if (ann) {
      std::vector<AST::Node*> flatAnn;
      if (ann->isArray()) {
        flattenAnnotations(ann->getArray()  , flatAnn);
      } else {
        flatAnn.push_back(ann);
      }

      for (unsigned int i=0; i<flatAnn.size(); i++) {
        if (flatAnn[i]->isCall("gecode_search")) {
          AST::Call* c = flatAnn[i]->getCall();
          branchWithPlugin(c->args);
        } else if (flatAnn[i]->isCall("int_search")) {
          AST::Call *call = flatAnn[i]->getCall("int_search");
          AST::Array *args = call->getArgs(4);
          AST::Array *vars = args->a[0]->getArray();
          int k=vars->a.size();
          for (int i=vars->a.size(); i--;)
            if (vars->a[i]->isInt())
              k--;
          IntVarArgs va(k);
          vector<string> names;
          k=0;
          for (unsigned int i=0; i<vars->a.size(); i++) {
            if (vars->a[i]->isInt())
              continue;
            va[k++] = iv[vars->a[i]->getIntVar()];
            iv_searched[vars->a[i]->getIntVar()] = true;
            names.push_back(vars->a[i]->getVarName());
          }
          std::string r0, r1;
          BrancherHandle bh = branch(*this, va, 
            ann2ivarsel(args->a[1],rnd,decay), 
            ann2ivalsel(args->a[2],r0,r1,rnd),
            NULL,
            &varValPrint<IntVar>);
          branchInfo.add(bh,r0,r1,names);
        } else if (flatAnn[i]->isCall("int_assign")) {
          AST::Call *call = flatAnn[i]->getCall("int_assign");
          AST::Array *args = call->getArgs(2);
          AST::Array *vars = args->a[0]->getArray();
          int k=vars->a.size();
          for (int i=vars->a.size(); i--;)
            if (vars->a[i]->isInt())
              k--;
          IntVarArgs va(k);
          k=0;
          for (unsigned int i=0; i<vars->a.size(); i++) {
            if (vars->a[i]->isInt())
              continue;
            va[k++] = iv[vars->a[i]->getIntVar()];
            iv_searched[vars->a[i]->getIntVar()] = true;
          }
          assign(*this, va, ann2asnivalsel(args->a[1],rnd), NULL,
                &varValPrint<IntVar>);
        } else if (flatAnn[i]->isCall("bool_search")) {
          AST::Call *call = flatAnn[i]->getCall("bool_search");
          AST::Array *args = call->getArgs(4);
          AST::Array *vars = args->a[0]->getArray();
          int k=vars->a.size();
          for (int i=vars->a.size(); i--;)
            if (vars->a[i]->isBool())
              k--;
          BoolVarArgs va(k);
          k=0;
          vector<string> names;
          for (unsigned int i=0; i<vars->a.size(); i++) {
            if (vars->a[i]->isBool())
              continue;
            va[k++] = bv[vars->a[i]->getBoolVar()];
            bv_searched[vars->a[i]->getBoolVar()] = true;
            names.push_back(vars->a[i]->getVarName());
          }
          
          std::string r0, r1;
          BrancherHandle bh = branch(*this, va, 
            ann2ivarsel(args->a[1],rnd,decay), 
            ann2ivalsel(args->a[2],r0,r1,rnd), NULL,
            &varValPrint<BoolVar>);
          branchInfo.add(bh,r0,r1,names);
        } else if (flatAnn[i]->isCall("int_default_search")) {
          AST::Call *call = flatAnn[i]->getCall("int_default_search");
          AST::Array *args = call->getArgs(2);
          def_int_varsel = ann2ivarsel(args->a[0],rnd,decay);
          std::string r0;
          def_int_valsel = ann2ivalsel(args->a[1],r0,r0,rnd);
        } else if (flatAnn[i]->isCall("bool_default_search")) {
          AST::Call *call = flatAnn[i]->getCall("bool_default_search");
          AST::Array *args = call->getArgs(2);
          def_bool_varsel = ann2ivarsel(args->a[0],rnd,decay);
          std::string r0;
          def_bool_valsel = ann2ivalsel(args->a[1],r0,r0,rnd);
        } else if (flatAnn[i]->isCall("set_search")) {
#ifdef GECODE_HAS_SET_VARS
          AST::Call *call = flatAnn[i]->getCall("set_search");
          AST::Array *args = call->getArgs(4);
          AST::Array *vars = args->a[0]->getArray();
          int k=vars->a.size();
          for (int i=vars->a.size(); i--;)
            if (vars->a[i]->isSet())
              k--;
          SetVarArgs va(k);
          k=0;
          vector<string> names;
          for (unsigned int i=0; i<vars->a.size(); i++) {
            if (vars->a[i]->isSet())
              continue;
            va[k++] = sv[vars->a[i]->getSetVar()];
            sv_searched[vars->a[i]->getSetVar()] = true;
            names.push_back(vars->a[i]->getVarName());
          }
          std::string r0, r1;
          BrancherHandle bh = branch(*this, va, 
            ann2svarsel(args->a[1],rnd,decay), 
            ann2svalsel(args->a[2],r0,r1,rnd),
            NULL,
            &varValPrint<SetVar>);
          branchInfo.add(bh,r0,r1,names);
#else
          if (!ignoreUnknown) {
            err << "Warning, ignored search annotation: ";
            flatAnn[i]->print(err);
            err << std::endl;
          }
#endif
        } else if (flatAnn[i]->isCall("set_default_search")) {
#ifdef GECODE_HAS_SET_VARS
          AST::Call *call = flatAnn[i]->getCall("set_default_search");
          AST::Array *args = call->getArgs(2);
          def_set_varsel = ann2svarsel(args->a[0],rnd,decay);
          std::string r0;
          def_set_valsel = ann2svalsel(args->a[1],r0,r0,rnd);
#else
          if (!ignoreUnknown) {
            err << "Warning, ignored search annotation: ";
            flatAnn[i]->print(err);
            err << std::endl;
          }
#endif
        } else if (flatAnn[i]->isCall("float_default_search")) {
#ifdef GECODE_HAS_FLOAT_VARS
          AST::Call *call = flatAnn[i]->getCall("float_default_search");
          AST::Array *args = call->getArgs(2);
          def_float_varsel = ann2fvarsel(args->a[0],rnd,decay);
          std::string r0;
          def_float_valsel = ann2fvalsel(args->a[1],r0,r0);
#else
          if (!ignoreUnknown) {
            err << "Warning, ignored search annotation: ";
            flatAnn[i]->print(err);
            err << std::endl;
          }
#endif
        } else if (flatAnn[i]->isCall("float_search")) {
#ifdef GECODE_HAS_FLOAT_VARS
          AST::Call *call = flatAnn[i]->getCall("float_search");
          AST::Array *args = call->getArgs(5);
          AST::Array *vars = args->a[0]->getArray();
          int k=vars->a.size();
          for (int i=vars->a.size(); i--;)
            if (vars->a[i]->isFloat())
              k--;
          FloatVarArgs va(k);
          k=0;
          vector<string> names;
          for (unsigned int i=0; i<vars->a.size(); i++) {
            if (vars->a[i]->isFloat())
              continue;
            va[k++] = fv[vars->a[i]->getFloatVar()];
            fv_searched[vars->a[i]->getFloatVar()] = true;
            names.push_back(vars->a[i]->getVarName());
          }
          std::string r0, r1;
          BrancherHandle bh = branch(*this, va,
            ann2fvarsel(args->a[2],rnd,decay), 
            ann2fvalsel(args->a[3],r0,r1),
            NULL,
            &varValPrintF);
          branchInfo.add(bh,r0,r1,names);
#else
          if (!ignoreUnknown) {
            err << "Warning, ignored search annotation: ";
            flatAnn[i]->print(err);
            err << std::endl;
          }
#endif
        } else {
          if (!ignoreUnknown) {
            err << "Warning, ignored search annotation: ";
            flatAnn[i]->print(err);
            err << std::endl;
          }
        }
      }
    }
    int introduced = 0;
    int funcdep = 0;
    int searched = 0;
    for (int i=iv.size(); i--;) {
      if (iv_searched[i]) {
        searched++;
      } else if (iv_introduced[2*i]) {
        if (iv_introduced[2*i+1]) {
          funcdep++;
        } else {
          introduced++;
        }
      }
    }
    IntVarArgs iv_sol(iv.size()-(introduced+funcdep+searched));
    IntVarArgs iv_tmp(introduced);
    for (int i=iv.size(), j=0, k=0; i--;) {
      if (iv_searched[i])
        continue;
      if (iv_introduced[2*i]) {
        if (!iv_introduced[2*i+1]) {
          iv_tmp[j++] = iv[i];
        }
      } else {
        iv_sol[k++] = iv[i];
      }
    }

    introduced = 0;
    funcdep = 0;
    searched = 0;
    for (int i=bv.size(); i--;) {
      if (bv_searched[i]) {
        searched++;
      } else if (bv_introduced[2*i]) {
        if (bv_introduced[2*i+1]) {
          funcdep++;
        } else {
          introduced++;
        }
      }
    }
    BoolVarArgs bv_sol(bv.size()-(introduced+funcdep+searched));
    BoolVarArgs bv_tmp(introduced);
    for (int i=bv.size(), j=0, k=0; i--;) {
      if (bv_searched[i])
        continue;
      if (bv_introduced[2*i]) {
        if (!bv_introduced[2*i+1]) {
          bv_tmp[j++] = bv[i];
        }
      } else {
        bv_sol[k++] = bv[i];
      }
    }

    if (iv_sol.size() > 0) {
      branch(*this, iv_sol, def_int_varsel, def_int_valsel);
    }
    if (bv_sol.size() > 0)
      branch(*this, bv_sol, def_bool_varsel, def_bool_valsel);
#ifdef GECODE_HAS_FLOAT_VARS
    introduced = 0;
    funcdep = 0;
    searched = 0;
    for (int i=fv.size(); i--;) {
      if (fv_searched[i]) {
        searched++;
      } else if (fv_introduced[2*i]) {
        if (fv_introduced[2*i+1]) {
          funcdep++;
        } else {
          introduced++;
        }
      }
    }
    FloatVarArgs fv_sol(fv.size()-(introduced+funcdep+searched));
    FloatVarArgs fv_tmp(introduced);
    for (int i=fv.size(), j=0, k=0; i--;) {
      if (fv_searched[i])
        continue;
      if (fv_introduced[2*i]) {
        if (!fv_introduced[2*i+1]) {
          fv_tmp[j++] = fv[i];
        }
      } else {
        fv_sol[k++] = fv[i];
      }
    }

    if (fv_sol.size() > 0)
      branch(*this, fv_sol, def_float_varsel, def_float_valsel);
#endif
#ifdef GECODE_HAS_SET_VARS
    introduced = 0;
    funcdep = 0;
    searched = 0;
    for (int i=sv.size(); i--;) {
      if (sv_searched[i]) {
        searched++;
      } else if (sv_introduced[2*i]) {
        if (sv_introduced[2*i+1]) {
          funcdep++;
        } else {
          introduced++;
        }
      }
    }
    SetVarArgs sv_sol(sv.size()-(introduced+funcdep+searched));
    SetVarArgs sv_tmp(introduced);
    for (int i=sv.size(), j=0, k=0; i--;) {
      if (sv_searched[i])
        continue;
      if (sv_introduced[2*i]) {
        if (!sv_introduced[2*i+1]) {
          sv_tmp[j++] = sv[i];
        }
      } else {
        sv_sol[k++] = sv[i];
      }
    }

    if (sv_sol.size() > 0)
      branch(*this, sv_sol, def_set_varsel, def_set_valsel);
#endif
    iv_aux = IntVarArray(*this, iv_tmp);
    bv_aux = BoolVarArray(*this, bv_tmp);
    int n_aux = iv_aux.size() + bv_aux.size();
#ifdef GECODE_HAS_SET_VARS
    sv_aux = SetVarArray(*this, sv_tmp);
    n_aux += sv_aux.size();
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    fv_aux = FloatVarArray(*this, fv_tmp);
    n_aux += fv_aux.size();
#endif
    if (n_aux > 0) {
      AuxVarBrancher::post(*this, def_int_varsel, def_int_valsel,
                           def_bool_varsel, def_bool_valsel
#ifdef GECODE_HAS_SET_VARS
                           , def_set_varsel, def_set_valsel
#endif
#ifdef GECODE_HAS_FLOAT_VARS
                           , def_float_varsel, def_float_valsel
#endif
                           );
    }
  }

  AST::Array*
  FlatZincSpace::solveAnnotations(void) const {
    return _solveAnnotations;
  }

  void
  FlatZincSpace::solve(AST::Array* ann) {
    _method = SAT;
    _solveAnnotations = ann;
  }

  void
  FlatZincSpace::minimize(int var, bool isInt, AST::Array* ann) {
    _method = MIN;
    _optVar = var;
    _optVarIsInt = isInt;
    // Branch on optimization variable to ensure that it is given a value.
    AST::Call* c;
    if (isInt) {
      AST::Array* args = new AST::Array(4);
      args->a[0] = new AST::Array(new AST::IntVar(_optVar));
      args->a[1] = new AST::Atom("input_order");
      args->a[2] = new AST::Atom("indomain_min");
      args->a[3] = new AST::Atom("complete");
      c = new AST::Call("int_search", args);
    } else {
      AST::Array* args = new AST::Array(5);
      args->a[0] = new AST::Array(new AST::FloatVar(_optVar));
      args->a[1] = new AST::FloatLit(0.0);
      args->a[2] = new AST::Atom("input_order");
      args->a[3] = new AST::Atom("indomain_split");
      args->a[4] = new AST::Atom("complete");
      c = new AST::Call("float_search", args);
    }
    if (!ann)
      ann = new AST::Array(c);
    else
      ann->a.push_back(c);
    _solveAnnotations = ann;
  }

  void
  FlatZincSpace::maximize(int var, bool isInt, AST::Array* ann) {
    _method = MAX;
    _optVar = var;
    _optVarIsInt = isInt;
    // Branch on optimization variable to ensure that it is given a value.
    AST::Call* c;
    if (isInt) {
      AST::Array* args = new AST::Array(4);
      args->a[0] = new AST::Array(new AST::IntVar(_optVar));
      args->a[1] = new AST::Atom("input_order");
      args->a[2] = new AST::Atom("indomain_max");
      args->a[3] = new AST::Atom("complete");
      c = new AST::Call("int_search", args);
    } else {
      AST::Array* args = new AST::Array(5);
      args->a[0] = new AST::Array(new AST::FloatVar(_optVar));
      args->a[1] = new AST::FloatLit(0.0);
      args->a[2] = new AST::Atom("input_order");
      args->a[3] = new AST::Atom("indomain_split_reverse");
      args->a[4] = new AST::Atom("complete");
      c = new AST::Call("float_search", args);
    }
    if (!ann)
      ann = new AST::Array(c);
    else
      ann->a.push_back(c);
    _solveAnnotations = ann;
  }

  FlatZincSpace::~FlatZincSpace(void) {
    delete _solveAnnotations;
  }

#ifdef GECODE_HAS_GIST

  /**
   * \brief Traits class for search engines
   */
  template<class Engine>
  class GistEngine {
  };

  /// Specialization for DFS
  template<typename S>
  class GistEngine<DFS<S> > {
  public:
    static void explore(S* root, const FlatZincOptions& opt,
                        Gist::Inspector* i, Gist::Comparator* c) {
      Gecode::Gist::Options o;
      o.c_d = opt.c_d(); o.a_d = opt.a_d();
      o.inspect.click(i);
      o.inspect.compare(c);
      (void) Gecode::Gist::dfs(root, o);
    }
  };

  /// Specialization for BAB
  template<typename S>
  class GistEngine<BAB<S> > {
  public:
    static void explore(S* root, const FlatZincOptions& opt,
                        Gist::Inspector* i, Gist::Comparator* c) {
      Gecode::Gist::Options o;
      o.c_d = opt.c_d(); o.a_d = opt.a_d();
      o.inspect.click(i);
      o.inspect.compare(c);
      (void) Gecode::Gist::bab(root, o);
    }
  };

  /// \brief An inspector for printing simple text output
  template<class S>
  class FZPrintingInspector
   : public Gecode::Gist::TextOutput, public Gecode::Gist::Inspector {
  private:
    const Printer& p;
  public:
    /// Constructor
    FZPrintingInspector(const Printer& p0);
    /// Use the print method of the template class S to print a space
    virtual void inspect(const Space& node);
    /// Finalize when Gist exits
    virtual void finalize(void);
  };

  template<class S>
  FZPrintingInspector<S>::FZPrintingInspector(const Printer& p0)
  : TextOutput("Gecode/FlatZinc"), p(p0) {}

  template<class S>
  void
  FZPrintingInspector<S>::inspect(const Space& node) {
    init();
    dynamic_cast<const S&>(node).print(getStream(), p);
    getStream() << std::endl;
  }

  template<class S>
  void
  FZPrintingInspector<S>::finalize(void) {
    Gecode::Gist::TextOutput::finalize();
  }

  template<class S>
  class FZPrintingComparator
  : public Gecode::Gist::VarComparator<S> {
  private:
    const Printer& p;
  public:
    /// Constructor
    FZPrintingComparator(const Printer& p0);

    /// Use the compare method of the template class S to compare two spaces
    virtual void compare(const Space& s0, const Space& s1);
  };

  template<class S>
  FZPrintingComparator<S>::FZPrintingComparator(const Printer& p0)
  : Gecode::Gist::VarComparator<S>("Gecode/FlatZinc"), p(p0) {}

  template<class S>
  void
  FZPrintingComparator<S>::compare(const Space& s0, const Space& s1) {
    this->init();
    try {
      dynamic_cast<const S&>(s0).compare(dynamic_cast<const S&>(s1),
                                         this->getStream(), p);
    } catch (Exception& e) {
      this->getStream() << "Exception: " << e.what();
    }
    this->getStream() << std::endl;
  }

#endif


  template<template<class> class Engine>
  void
  FlatZincSpace::runEngine(std::ostream& out, const Printer& p,
                           const FlatZincOptions& opt, Support::Timer& t_total) {
    if (opt.restart()==RM_NONE) {
      runMeta<Engine,Driver::EngineToMeta>(out,p,opt,t_total);
    } else {
      runMeta<Engine,RBS>(out,p,opt,t_total);
    }
  }

  template<template<class> class Engine,
           template<template<class> class,class> class Meta>
  void
  FlatZincSpace::runMeta(std::ostream& out, const Printer& p,
                         const FlatZincOptions& opt, Support::Timer& t_total) {
#ifdef GECODE_HAS_GIST
    if (opt.mode() == SM_GIST) {
      FZPrintingInspector<FlatZincSpace> pi(p);
      FZPrintingComparator<FlatZincSpace> pc(p);
      (void) GistEngine<Engine<FlatZincSpace> >::explore(this,opt,&pi,&pc);
      return;
    }
#endif
    StatusStatistics sstat;
    unsigned int n_p = 0;
    Support::Timer t_solve;
    t_solve.start();
    if (status(sstat) != SS_FAILED) {
      n_p = propagators();
    }
    Search::Options o;
    o.stop = Driver::CombinedStop::create(opt.node(), opt.fail(), opt.time(), 
                                          true);
    o.c_d = opt.c_d();
    o.a_d = opt.a_d();
    o.threads = opt.threads();
    o.nogoods_limit = opt.nogoods() ? opt.nogoods_limit() : 0;
    o.cutoff  = Driver::createCutoff(opt);
    if (opt.interrupt())
      Driver::CombinedStop::installCtrlHandler(true);
    Meta<Engine,FlatZincSpace> se(this,o);
    int noOfSolutions = _method == SAT ? opt.solutions() : 0;
    bool printAll = _method == SAT || opt.allSolutions();
    int findSol = noOfSolutions;
    FlatZincSpace* sol = NULL;
    while (FlatZincSpace* next_sol = se.next()) {
      delete sol;
      sol = next_sol;
      if (printAll) {
        sol->print(out, p);
        out << "----------" << std::endl;
      }
      if (--findSol==0)
        goto stopped;
    }
    if (sol && !printAll) {
      sol->print(out, p);
      out << "----------" << std::endl;
    }
    if (!se.stopped()) {
      if (sol) {
        out << "==========" << endl;
      } else {
        out << "=====UNSATISFIABLE=====" << endl;
      }
    } else if (!sol) {
        out << "=====UNKNOWN=====" << endl;
    }
    delete sol;
    stopped:
    if (opt.interrupt())
      Driver::CombinedStop::installCtrlHandler(false);
    if (opt.mode() == SM_STAT) {
      Gecode::Search::Statistics stat = se.statistics();
      out << endl
           << "%%  runtime:       ";
      Driver::stop(t_total,out);
      out << endl
           << "%%  solvetime:     ";
      Driver::stop(t_solve,out);
      out << endl
           << "%%  solutions:     " 
           << std::abs(noOfSolutions - findSol) << endl
           << "%%  variables:     " 
           << (intVarCount + boolVarCount + setVarCount) << endl
           << "%%  propagators:   " << n_p << endl
           << "%%  propagations:  " << sstat.propagate+stat.propagate << endl
           << "%%  nodes:         " << stat.node << endl
           << "%%  failures:      " << stat.fail << endl
           << "%%  restarts:      " << stat.restart << endl
           << "%%  peak depth:    " << stat.depth << endl
           << endl;
    }
    delete o.stop;
  }

#ifdef GECODE_HAS_QT
  void
  FlatZincSpace::branchWithPlugin(AST::Node* ann) {
    if (AST::Call* c = dynamic_cast<AST::Call*>(ann)) {
      QString pluginName(c->id.c_str());
      if (QLibrary::isLibrary(pluginName+".dll")) {
        pluginName += ".dll";
      } else if (QLibrary::isLibrary(pluginName+".dylib")) {
        pluginName = "lib" + pluginName + ".dylib";
      } else if (QLibrary::isLibrary(pluginName+".so")) {
        // Must check .so after .dylib so that Mac OS uses .dylib
        pluginName = "lib" + pluginName + ".so";
      }
      QPluginLoader pl(pluginName);
      QObject* plugin_o = pl.instance();
      if (!plugin_o) {
        throw FlatZinc::Error("FlatZinc",
          "Error loading plugin "+pluginName.toStdString()+
          ": "+pl.errorString().toStdString());
      }
      BranchPlugin* pb = qobject_cast<BranchPlugin*>(plugin_o);
      if (!pb) {
        throw FlatZinc::Error("FlatZinc",
        "Error loading plugin "+pluginName.toStdString()+
        ": does not contain valid PluginBrancher");
      }
      pb->branch(*this, c);
    }
  }
#else
  void
  FlatZincSpace::branchWithPlugin(AST::Node*) {
    throw FlatZinc::Error("FlatZinc",
      "Branching with plugins not supported (requires Qt support)");
  }
#endif

  void
  FlatZincSpace::run(std::ostream& out, const Printer& p,
                      const FlatZincOptions& opt, Support::Timer& t_total) {
    switch (_method) {
    case MIN:
    case MAX:
      runEngine<BAB>(out,p,opt,t_total);
      break;
    case SAT:
      runEngine<DFS>(out,p,opt,t_total);
      break;
    }
  }

  void
  FlatZincSpace::constrain(const Space& s) {
    if (_optVarIsInt) {
      if (_method == MIN)
        rel(*this, iv[_optVar], IRT_LE, 
                   static_cast<const FlatZincSpace*>(&s)->iv[_optVar].val());
      else if (_method == MAX)
        rel(*this, iv[_optVar], IRT_GR,
                   static_cast<const FlatZincSpace*>(&s)->iv[_optVar].val());
    } else {
#ifdef GECODE_HAS_FLOAT_VARS
      if (_method == MIN)
        rel(*this, fv[_optVar], FRT_LE, 
                   static_cast<const FlatZincSpace*>(&s)->fv[_optVar].val());
      else if (_method == MAX)
        rel(*this, fv[_optVar], FRT_GR,
                   static_cast<const FlatZincSpace*>(&s)->fv[_optVar].val());
#endif
    }
  }

  Space*
  FlatZincSpace::copy(bool share) {
    return new FlatZincSpace(share, *this);
  }

  FlatZincSpace::Meth
  FlatZincSpace::method(void) const {
    return _method;
  }

  int
  FlatZincSpace::optVar(void) const {
    return _optVar;
  }

  bool
  FlatZincSpace::optVarIsInt(void) const {
    return _optVarIsInt;
  }

  void
  FlatZincSpace::print(std::ostream& out, const Printer& p) const {
    p.print(out, iv, bv
#ifdef GECODE_HAS_SET_VARS
    , sv
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    , fv
#endif
    );
  }

  void
  FlatZincSpace::compare(const Space& s, std::ostream& out) const {
    (void) s; (void) out;
#ifdef GECODE_HAS_GIST
    const FlatZincSpace& fs = dynamic_cast<const FlatZincSpace&>(s);
    for (int i = 0; i < iv.size(); ++i) {
      std::stringstream ss;
      ss << "iv[" << i << "]";
      std::string result(Gecode::Gist::Comparator::compare(ss.str(), iv[i],
                                                           fs.iv[i]));
      if (result.length() > 0) out << result << std::endl;
    }
    for (int i = 0; i < bv.size(); ++i) {
      std::stringstream ss;
      ss << "bv[" << i << "]";
      std::string result(Gecode::Gist::Comparator::compare(ss.str(), bv[i],
                                                           fs.bv[i]));
      if (result.length() > 0) out << result << std::endl;
    }
#ifdef GECODE_HAS_SET_VARS
    for (int i = 0; i < sv.size(); ++i) {
      std::stringstream ss;
      ss << "sv[" << i << "]";
      std::string result(Gecode::Gist::Comparator::compare(ss.str(), sv[i],
                                                           fs.sv[i]));
      if (result.length() > 0) out << result << std::endl;
    }
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    for (int i = 0; i < fv.size(); ++i) {
      std::stringstream ss;
      ss << "fv[" << i << "]";
      std::string result(Gecode::Gist::Comparator::compare(ss.str(), fv[i],
                                                           fs.fv[i]));
      if (result.length() > 0) out << result << std::endl;
    }
#endif
#endif
  }

  void
  FlatZincSpace::compare(const FlatZincSpace& s, std::ostream& out,
                         const Printer& p) const {
    p.printDiff(out, iv, s.iv, bv, s.bv
#ifdef GECODE_HAS_SET_VARS
     , sv, s.sv
#endif
#ifdef GECODE_HAS_FLOAT_VARS
     , fv, s.fv
#endif
    );
  }

  void
  FlatZincSpace::shrinkArrays(Printer& p) {
    p.shrinkArrays(*this, _optVar, _optVarIsInt, iv, bv
#ifdef GECODE_HAS_SET_VARS
    , sv
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    , fv
#endif
    );
  }

  IntArgs
  FlatZincSpace::arg2intargs(AST::Node* arg, int offset) {
    AST::Array* a = arg->getArray();
    IntArgs ia(a->a.size()+offset);
    for (int i=offset; i--;)
      ia[i] = 0;
    for (int i=a->a.size(); i--;)
      ia[i+offset] = a->a[i]->getInt();
    return ia;
  }
  IntArgs
  FlatZincSpace::arg2boolargs(AST::Node* arg, int offset) {
    AST::Array* a = arg->getArray();
    IntArgs ia(a->a.size()+offset);
    for (int i=offset; i--;)
      ia[i] = 0;
    for (int i=a->a.size(); i--;)
      ia[i+offset] = a->a[i]->getBool();
    return ia;
  }
  IntSet
  FlatZincSpace::arg2intset(AST::Node* n) {
    AST::SetLit* sl = n->getSet();
    IntSet d;
    if (sl->interval) {
      d = IntSet(sl->min, sl->max);
    } else {
      Region re(*this);
      int* is = re.alloc<int>(static_cast<unsigned long int>(sl->s.size()));
      for (int i=sl->s.size(); i--; )
        is[i] = sl->s[i];
      d = IntSet(is, sl->s.size());
    }
    return d;
  }
  IntSetArgs
  FlatZincSpace::arg2intsetargs(AST::Node* arg, int offset) {
    AST::Array* a = arg->getArray();
    if (a->a.size() == 0) {
      IntSetArgs emptyIa(0);
      return emptyIa;
    }
    IntSetArgs ia(a->a.size()+offset);      
    for (int i=offset; i--;)
      ia[i] = IntSet::empty;
    for (int i=a->a.size(); i--;) {
      ia[i+offset] = arg2intset(a->a[i]);
    }
    return ia;
  }
  IntVarArgs
  FlatZincSpace::arg2intvarargs(AST::Node* arg, int offset) {
    AST::Array* a = arg->getArray();
    if (a->a.size() == 0) {
      IntVarArgs emptyIa(0);
      return emptyIa;
    }
    IntVarArgs ia(a->a.size()+offset);
    for (int i=offset; i--;)
      ia[i] = IntVar(*this, 0, 0);
    for (int i=a->a.size(); i--;) {
      if (a->a[i]->isIntVar()) {
        ia[i+offset] = iv[a->a[i]->getIntVar()];        
      } else {
        int value = a->a[i]->getInt();
        IntVar iv(*this, value, value);
        ia[i+offset] = iv;        
      }
    }
    return ia;
  }
  BoolVarArgs
  FlatZincSpace::arg2boolvarargs(AST::Node* arg, int offset, int siv) {
    AST::Array* a = arg->getArray();
    if (a->a.size() == 0) {
      BoolVarArgs emptyIa(0);
      return emptyIa;
    }
    BoolVarArgs ia(a->a.size()+offset-(siv==-1?0:1));
    for (int i=offset; i--;)
      ia[i] = BoolVar(*this, 0, 0);
    for (int i=0; i<static_cast<int>(a->a.size()); i++) {
      if (i==siv)
        continue;
      if (a->a[i]->isBool()) {
        bool value = a->a[i]->getBool();
        BoolVar iv(*this, value, value);
        ia[offset++] = iv;
      } else if (a->a[i]->isIntVar() &&
                 aliasBool2Int(a->a[i]->getIntVar()) != -1) {
        ia[offset++] = bv[aliasBool2Int(a->a[i]->getIntVar())];
      } else {
        ia[offset++] = bv[a->a[i]->getBoolVar()];
      }
    }
    return ia;
  }
  BoolVar
  FlatZincSpace::arg2BoolVar(AST::Node* n) {
    BoolVar x0;
    if (n->isBool()) {
      x0 = BoolVar(*this, n->getBool(), n->getBool());
    }
    else {
      x0 = bv[n->getBoolVar()];
    }
    return x0;
  }
  IntVar
  FlatZincSpace::arg2IntVar(AST::Node* n) {
    IntVar x0;
    if (n->isIntVar()) {
      x0 = iv[n->getIntVar()];
    } else {
      x0 = IntVar(*this, n->getInt(), n->getInt());            
    }
    return x0;
  }
  bool
  FlatZincSpace::isBoolArray(AST::Node* b, int& singleInt) {
    AST::Array* a = b->getArray();
    singleInt = -1;
    if (a->a.size() == 0)
      return true;
    for (int i=a->a.size(); i--;) {
      if (a->a[i]->isBoolVar() || a->a[i]->isBool()) {
      } else if (a->a[i]->isIntVar()) {
        if (aliasBool2Int(a->a[i]->getIntVar()) == -1) {
          if (singleInt != -1) {
            return false;
          }
          singleInt = i;
        }
      } else {
        return false;
      }
    }
    return singleInt==-1 || a->a.size() > 1;
  }
#ifdef GECODE_HAS_SET_VARS
  SetVar
  FlatZincSpace::arg2SetVar(AST::Node* n) {
    SetVar x0;
    if (!n->isSetVar()) {
      IntSet d = arg2intset(n);
      x0 = SetVar(*this, d, d);
    } else {
      x0 = sv[n->getSetVar()];
    }
    return x0;
  }
  SetVarArgs
  FlatZincSpace::arg2setvarargs(AST::Node* arg, int offset, int doffset,
                                const IntSet& od) {
    AST::Array* a = arg->getArray();
    SetVarArgs ia(a->a.size()+offset);
    for (int i=offset; i--;) {
      IntSet d = i<doffset ? od : IntSet::empty;
      ia[i] = SetVar(*this, d, d);
    }
    for (int i=a->a.size(); i--;) {
      ia[i+offset] = arg2SetVar(a->a[i]);
    }
    return ia;
  }
#endif
#ifdef GECODE_HAS_FLOAT_VARS
  FloatValArgs
  FlatZincSpace::arg2floatargs(AST::Node* arg, int offset) {
    AST::Array* a = arg->getArray();
    FloatValArgs fa(a->a.size()+offset);
    for (int i=offset; i--;)
      fa[i] = 0.0;
    for (int i=a->a.size(); i--;)
      fa[i+offset] = a->a[i]->getFloat();
    return fa;
  }
  FloatVarArgs
  FlatZincSpace::arg2floatvarargs(AST::Node* arg, int offset) {
    AST::Array* a = arg->getArray();
    if (a->a.size() == 0) {
      FloatVarArgs emptyFa(0);
      return emptyFa;
    }
    FloatVarArgs fa(a->a.size()+offset);
    for (int i=offset; i--;)
      fa[i] = FloatVar(*this, 0.0, 0.0);
    for (int i=a->a.size(); i--;) {
      if (a->a[i]->isFloatVar()) {
        fa[i+offset] = fv[a->a[i]->getFloatVar()];        
      } else {
        double value = a->a[i]->getFloat();
        FloatVar fv(*this, value, value);
        fa[i+offset] = fv;
      }
    }
    return fa;
  }
  FloatVar
  FlatZincSpace::arg2FloatVar(AST::Node* n) {
    FloatVar x0;
    if (n->isFloatVar()) {
      x0 = fv[n->getFloatVar()];
    } else {
      x0 = FloatVar(*this, n->getFloat(), n->getFloat());
    }
    return x0;
  }
#endif
  IntConLevel
  FlatZincSpace::ann2icl(AST::Node* ann) {
    if (ann) {
      if (ann->hasAtom("val"))
        return ICL_VAL;
      if (ann->hasAtom("domain"))
        return ICL_DOM;
      if (ann->hasAtom("bounds") ||
          ann->hasAtom("boundsR") ||
          ann->hasAtom("boundsD") ||
          ann->hasAtom("boundsZ"))
        return ICL_BND;
    }
    return ICL_DEF;
  }


  void
  Printer::init(AST::Array* output) {
    _output = output;
  }

  void
  Printer::printElem(std::ostream& out,
                       AST::Node* ai,
                       const Gecode::IntVarArray& iv,
                       const Gecode::BoolVarArray& bv
#ifdef GECODE_HAS_SET_VARS
                       , const Gecode::SetVarArray& sv
#endif
#ifdef GECODE_HAS_FLOAT_VARS
                       ,
                       const Gecode::FloatVarArray& fv
#endif
                       ) const {
    int k;
    if (ai->isInt(k)) {
      out << k;
    } else if (ai->isIntVar()) {
      out << iv[ai->getIntVar()];
    } else if (ai->isBoolVar()) {
      if (bv[ai->getBoolVar()].min() == 1) {
        out << "true";
      } else if (bv[ai->getBoolVar()].max() == 0) {
        out << "false";
      } else {
        out << "false..true";
      }
#ifdef GECODE_HAS_SET_VARS
    } else if (ai->isSetVar()) {
      if (!sv[ai->getSetVar()].assigned()) {
        out << sv[ai->getSetVar()];
        return;
      }
      SetVarGlbRanges svr(sv[ai->getSetVar()]);
      if (!svr()) {
        out << "{}";
        return;
      }
      int min = svr.min();
      int max = svr.max();
      ++svr;
      if (svr()) {
        SetVarGlbValues svv(sv[ai->getSetVar()]);
        int i = svv.val();
        out << "{" << i;
        ++svv;
        for (; svv(); ++svv)
          out << ", " << svv.val();
        out << "}";
      } else {
        out << min << ".." << max;
      }
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    } else if (ai->isFloatVar()) {
      if (fv[ai->getFloatVar()].assigned()) {
        FloatVal vv = fv[ai->getFloatVar()].val();
        FloatNum v;
        if (vv.singleton())
          v = vv.min();
        else if (vv < 0.0)
          v = vv.max();
        else
          v = vv.min();
        std::ostringstream oss;
        // oss << std::scientific;
        oss << std::setprecision(std::numeric_limits<double>::digits10);
        oss << v;
        if (oss.str().find(".") == std::string::npos)
          oss << ".0";
        out << oss.str();
      } else {
        out << fv[ai->getFloatVar()];
      }
#endif
    } else if (ai->isBool()) {
      out << (ai->getBool() ? "true" : "false");
    } else if (ai->isSet()) {
      AST::SetLit* s = ai->getSet();
      if (s->interval) {
        out << s->min << ".." << s->max;
      } else {
        out << "{";
        for (unsigned int i=0; i<s->s.size(); i++) {
          out << s->s[i] << (i < s->s.size()-1 ? ", " : "}");
        }
      }
    } else if (ai->isString()) {
      std::string s = ai->getString();
      for (unsigned int i=0; i<s.size(); i++) {
        if (s[i] == '\\' && i<s.size()-1) {
          switch (s[i+1]) {
          case 'n': out << "\n"; break;
          case '\\': out << "\\"; break;
          case 't': out << "\t"; break;
          default: out << "\\" << s[i+1];
          }
          i++;
        } else {
          out << s[i];
        }
      }
    }
  }

  void
  Printer::printElemDiff(std::ostream& out,
                       AST::Node* ai,
                       const Gecode::IntVarArray& iv1,
                       const Gecode::IntVarArray& iv2,
                       const Gecode::BoolVarArray& bv1,
                       const Gecode::BoolVarArray& bv2
#ifdef GECODE_HAS_SET_VARS
                       , const Gecode::SetVarArray& sv1,
                       const Gecode::SetVarArray& sv2
#endif
#ifdef GECODE_HAS_FLOAT_VARS
                       , const Gecode::FloatVarArray& fv1,
                       const Gecode::FloatVarArray& fv2
#endif
                       ) const {
#ifdef GECODE_HAS_GIST
    using namespace Gecode::Gist;
    int k;
    if (ai->isInt(k)) {
      out << k;
    } else if (ai->isIntVar()) {
      std::string res(Comparator::compare("",iv1[ai->getIntVar()],
                                          iv2[ai->getIntVar()]));
      if (res.length() > 0) {
        res.erase(0, 1); // Remove '='
        out << res;
      } else {
        out << iv1[ai->getIntVar()];
      }
    } else if (ai->isBoolVar()) {
      std::string res(Comparator::compare("",bv1[ai->getBoolVar()],
                                          bv2[ai->getBoolVar()]));
      if (res.length() > 0) {
        res.erase(0, 1); // Remove '='
        out << res;
      } else {
        out << bv1[ai->getBoolVar()];
      }
#ifdef GECODE_HAS_SET_VARS
    } else if (ai->isSetVar()) {
      std::string res(Comparator::compare("",sv1[ai->getSetVar()],
                                          sv2[ai->getSetVar()]));
      if (res.length() > 0) {
        res.erase(0, 1); // Remove '='
        out << res;
      } else {
        out << sv1[ai->getSetVar()];
      }
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    } else if (ai->isFloatVar()) {
      std::string res(Comparator::compare("",fv1[ai->getFloatVar()],
                                          fv2[ai->getFloatVar()]));
      if (res.length() > 0) {
        res.erase(0, 1); // Remove '='
        out << res;
      } else {
        out << fv1[ai->getFloatVar()];
      }
#endif
    } else if (ai->isBool()) {
      out << (ai->getBool() ? "true" : "false");
    } else if (ai->isSet()) {
      AST::SetLit* s = ai->getSet();
      if (s->interval) {
        out << s->min << ".." << s->max;
      } else {
        out << "{";
        for (unsigned int i=0; i<s->s.size(); i++) {
          out << s->s[i] << (i < s->s.size()-1 ? ", " : "}");
        }
      }
    } else if (ai->isString()) {
      std::string s = ai->getString();
      for (unsigned int i=0; i<s.size(); i++) {
        if (s[i] == '\\' && i<s.size()-1) {
          switch (s[i+1]) {
          case 'n': out << "\n"; break;
          case '\\': out << "\\"; break;
          case 't': out << "\t"; break;
          default: out << "\\" << s[i+1];
          }
          i++;
        } else {
          out << s[i];
        }
      }
    }
#endif
  }

  void
  Printer::print(std::ostream& out,
                   const Gecode::IntVarArray& iv,
                   const Gecode::BoolVarArray& bv
#ifdef GECODE_HAS_SET_VARS
                   ,
                   const Gecode::SetVarArray& sv
#endif
#ifdef GECODE_HAS_FLOAT_VARS
                   ,
                   const Gecode::FloatVarArray& fv
#endif
                   ) const {
    if (_output == NULL)
      return;
    for (unsigned int i=0; i< _output->a.size(); i++) {
      AST::Node* ai = _output->a[i];
      if (ai->isArray()) {
        AST::Array* aia = ai->getArray();
        int size = aia->a.size();
        out << "[";
        for (int j=0; j<size; j++) {
          printElem(out,aia->a[j],iv,bv
#ifdef GECODE_HAS_SET_VARS
          ,sv
#endif
#ifdef GECODE_HAS_FLOAT_VARS
          ,fv
#endif
          );
          if (j<size-1)
            out << ", ";
        }
        out << "]";
      } else {
        printElem(out,ai,iv,bv
#ifdef GECODE_HAS_SET_VARS
        ,sv
#endif
#ifdef GECODE_HAS_FLOAT_VARS
          ,fv
#endif
        );
      }
    }
  }

  void
  Printer::printDiff(std::ostream& out,
                   const Gecode::IntVarArray& iv1,
                   const Gecode::IntVarArray& iv2,
                   const Gecode::BoolVarArray& bv1,
                   const Gecode::BoolVarArray& bv2
#ifdef GECODE_HAS_SET_VARS
                   ,
                   const Gecode::SetVarArray& sv1,
                   const Gecode::SetVarArray& sv2
#endif
#ifdef GECODE_HAS_FLOAT_VARS
                   ,
                   const Gecode::FloatVarArray& fv1,
                   const Gecode::FloatVarArray& fv2
#endif
                   ) const {
    if (_output == NULL)
      return;
    for (unsigned int i=0; i< _output->a.size(); i++) {
      AST::Node* ai = _output->a[i];
      if (ai->isArray()) {
        AST::Array* aia = ai->getArray();
        int size = aia->a.size();
        out << "[";
        for (int j=0; j<size; j++) {
          printElemDiff(out,aia->a[j],iv1,iv2,bv1,bv2
#ifdef GECODE_HAS_SET_VARS
            ,sv1,sv2
#endif
#ifdef GECODE_HAS_FLOAT_VARS
            ,fv1,fv2
#endif
          );
          if (j<size-1)
            out << ", ";
        }
        out << "]";
      } else {
        printElemDiff(out,ai,iv1,iv2,bv1,bv2
#ifdef GECODE_HAS_SET_VARS
          ,sv1,sv2
#endif
#ifdef GECODE_HAS_FLOAT_VARS
            ,fv1,fv2
#endif
        );
      }
    }
  }

  void
  Printer::shrinkElement(AST::Node* node,
                         std::map<int,int>& iv, std::map<int,int>& bv, 
                         std::map<int,int>& sv, std::map<int,int>& fv) {
    if (node->isIntVar()) {
      AST::IntVar* x = static_cast<AST::IntVar*>(node);
      if (iv.find(x->i) == iv.end()) {
        int newi = iv.size();
        iv[x->i] = newi;
      }
      x->i = iv[x->i];
    } else if (node->isBoolVar()) {
      AST::BoolVar* x = static_cast<AST::BoolVar*>(node);
      if (bv.find(x->i) == bv.end()) {
        int newi = bv.size();
        bv[x->i] = newi;
      }
      x->i = bv[x->i];
    } else if (node->isSetVar()) {
      AST::SetVar* x = static_cast<AST::SetVar*>(node);
      if (sv.find(x->i) == sv.end()) {
        int newi = sv.size();
        sv[x->i] = newi;
      }
      x->i = sv[x->i];      
    } else if (node->isFloatVar()) {
      AST::FloatVar* x = static_cast<AST::FloatVar*>(node);
      if (fv.find(x->i) == fv.end()) {
        int newi = fv.size();
        fv[x->i] = newi;
      }
      x->i = fv[x->i];      
    }
  }

  void
  Printer::shrinkArrays(Space& home,
                        int& optVar, bool optVarIsInt,
                        Gecode::IntVarArray& iv,
                        Gecode::BoolVarArray& bv
#ifdef GECODE_HAS_SET_VARS
                        ,
                        Gecode::SetVarArray& sv
#endif
#ifdef GECODE_HAS_FLOAT_VARS
                        ,
                        Gecode::FloatVarArray& fv
#endif
                       ) {
    if (_output == NULL) {
      if (optVarIsInt && optVar != -1) {
        IntVar ov = iv[optVar];
        iv = IntVarArray(home, 1);
        iv[0] = ov;
        optVar = 0;
      } else {
        iv = IntVarArray(home, 0);
      }
      bv = BoolVarArray(home, 0);
#ifdef GECODE_HAS_SET_VARS
      sv = SetVarArray(home, 0);
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      if (!optVarIsInt && optVar != -1) {
        FloatVar ov = fv[optVar];
        fv = FloatVarArray(home, 1);
        fv[0] = ov;
        optVar = 0;
      } else {
        fv = FloatVarArray(home,0);
      }
#endif
      return;
    }
    std::map<int,int> iv_new;
    std::map<int,int> bv_new;
    std::map<int,int> sv_new;
    std::map<int,int> fv_new;

    if (optVar != -1) {
      if (optVarIsInt)
        iv_new[optVar] = 0;
      else
        fv_new[optVar] = 0;
      optVar = 0;
    }

    for (unsigned int i=0; i< _output->a.size(); i++) {
      AST::Node* ai = _output->a[i];
      if (ai->isArray()) {
        AST::Array* aia = ai->getArray();
        for (unsigned int j=0; j<aia->a.size(); j++) {
          shrinkElement(aia->a[j],iv_new,bv_new,sv_new,fv_new);
        }
      } else {
        shrinkElement(ai,iv_new,bv_new,sv_new,fv_new);
      }
    }

    IntVarArgs iva(iv_new.size());
    for (map<int,int>::iterator i=iv_new.begin(); i != iv_new.end(); ++i) {
      iva[(*i).second] = iv[(*i).first];
    }
    iv = IntVarArray(home, iva);

    BoolVarArgs bva(bv_new.size());
    for (map<int,int>::iterator i=bv_new.begin(); i != bv_new.end(); ++i) {
      bva[(*i).second] = bv[(*i).first];
    }
    bv = BoolVarArray(home, bva);

#ifdef GECODE_HAS_SET_VARS
    SetVarArgs sva(sv_new.size());
    for (map<int,int>::iterator i=sv_new.begin(); i != sv_new.end(); ++i) {
      sva[(*i).second] = sv[(*i).first];
    }
    sv = SetVarArray(home, sva);
#endif

#ifdef GECODE_HAS_FLOAT_VARS
    FloatVarArgs fva(fv_new.size());
    for (map<int,int>::iterator i=fv_new.begin(); i != fv_new.end(); ++i) {
      fva[(*i).second] = fv[(*i).first];
    }
    fv = FloatVarArray(home, fva);
#endif
  }

  Printer::~Printer(void) {
    delete _output;
  }

}}

// STATISTICS: flatzinc-any
