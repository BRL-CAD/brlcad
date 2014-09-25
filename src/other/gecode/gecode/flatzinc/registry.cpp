/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Mikael Lagerkvist <lagerkvist@gmail.com>
 *
 *  Copyright:
 *     Guido Tack, 2007
 *     Mikael Lagerkvist, 2009
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

#include <gecode/flatzinc/registry.hh>
#include <gecode/kernel.hh>
#include <gecode/int.hh>
#include <gecode/minimodel.hh>

#ifdef GECODE_HAS_SET_VARS
#include <gecode/set.hh>
#endif
#ifdef GECODE_HAS_FLOAT_VARS
#include <gecode/float.hh>
#endif
#include <gecode/flatzinc.hh>

namespace Gecode { namespace FlatZinc {

  Registry& registry(void) {
    static Registry r;
    return r;
  }

  void
  Registry::post(FlatZincSpace& s, const ConExpr& ce) {
    std::map<std::string,poster>::iterator i = r.find(ce.id);
    if (i == r.end()) {
      throw FlatZinc::Error("Registry",
        std::string("Constraint ")+ce.id+" not found");
    }
    i->second(s, ce, ce.ann);
  }

  void
  Registry::add(const std::string& id, poster p) {
    r[id] = p;
  }

  namespace {
    
    inline IntRelType
    swap(IntRelType irt) {
      switch (irt) {
      case IRT_LQ: return IRT_GQ;
      case IRT_LE: return IRT_GR;
      case IRT_GQ: return IRT_LQ;
      case IRT_GR: return IRT_LE;
      default:     return irt;
      }
    }

    inline IntRelType
    neg(IntRelType irt) {
      switch (irt) {
      case IRT_EQ: return IRT_NQ;
      case IRT_NQ: return IRT_EQ;
      case IRT_LQ: return IRT_GR;
      case IRT_LE: return IRT_GQ;
      case IRT_GQ: return IRT_LE;
      case IRT_GR:
      default:
        assert(irt == IRT_GR);
      }
      return IRT_LQ;
    }



    void p_distinct(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs va = s.arg2intvarargs(ce[0]);
      IntConLevel icl = s.ann2icl(ann);
      unshare(s, va);
      distinct(s, va, icl == ICL_DEF ? ICL_BND : icl);
    }
    void p_distinctOffset(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs va = s.arg2intvarargs(ce[1]);
      unshare(s, va);
      AST::Array* offs = ce.args->a[0]->getArray();
      IntArgs oa(offs->a.size());
      for (int i=offs->a.size(); i--; ) {
        oa[i] = offs->a[i]->getInt();    
      }
      IntConLevel icl = s.ann2icl(ann);
      distinct(s, oa, va, icl == ICL_DEF ? ICL_BND : icl);
    }

    void p_all_equal(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs va = s.arg2intvarargs(ce[0]);
      rel(s, va, IRT_EQ, s.ann2icl(ann));
    }

    void p_int_CMP(FlatZincSpace& s, IntRelType irt, const ConExpr& ce, 
                   AST::Node* ann) {
      if (ce[0]->isIntVar()) {
        if (ce[1]->isIntVar()) {
          rel(s, s.arg2IntVar(ce[0]), irt, s.arg2IntVar(ce[1]), 
              s.ann2icl(ann));
        } else {
          rel(s, s.arg2IntVar(ce[0]), irt, ce[1]->getInt(), s.ann2icl(ann));
        }
      } else {
        rel(s, s.arg2IntVar(ce[1]), swap(irt), ce[0]->getInt(), 
            s.ann2icl(ann));
      }
    }
    void p_int_eq(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP(s, IRT_EQ, ce, ann);
    }
    void p_int_ne(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP(s, IRT_NQ, ce, ann);
    }
    void p_int_ge(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP(s, IRT_GQ, ce, ann);
    }
    void p_int_gt(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP(s, IRT_GR, ce, ann);
    }
    void p_int_le(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP(s, IRT_LQ, ce, ann);
    }
    void p_int_lt(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP(s, IRT_LE, ce, ann);
    }
    void p_int_CMP_reif(FlatZincSpace& s, IntRelType irt, ReifyMode rm,
                        const ConExpr& ce, AST::Node* ann) {
      if (rm == RM_EQV && ce[2]->isBool()) {
        if (ce[2]->getBool()) {
          p_int_CMP(s, irt, ce, ann);
        } else {
          p_int_CMP(s, neg(irt), ce, ann);
        }
        return;
      }
      if (ce[0]->isIntVar()) {
        if (ce[1]->isIntVar()) {
          rel(s, s.arg2IntVar(ce[0]), irt, s.arg2IntVar(ce[1]),
                 Reify(s.arg2BoolVar(ce[2]), rm), s.ann2icl(ann));
        } else {
          rel(s, s.arg2IntVar(ce[0]), irt, ce[1]->getInt(),
                 Reify(s.arg2BoolVar(ce[2]), rm), s.ann2icl(ann));
        }
      } else {
        rel(s, s.arg2IntVar(ce[1]), swap(irt), ce[0]->getInt(),
               Reify(s.arg2BoolVar(ce[2]), rm), s.ann2icl(ann));
      }
    }

    /* Comparisons */
    void p_int_eq_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_EQ, RM_EQV, ce, ann);
    }
    void p_int_ne_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_NQ, RM_EQV, ce, ann);
    }
    void p_int_ge_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_GQ, RM_EQV, ce, ann);
    }
    void p_int_gt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_GR, RM_EQV, ce, ann);
    }
    void p_int_le_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_LQ, RM_EQV, ce, ann);
    }
    void p_int_lt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_LE, RM_EQV, ce, ann);
    }

    void p_int_eq_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_EQ, RM_IMP, ce, ann);
    }
    void p_int_ne_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_NQ, RM_IMP, ce, ann);
    }
    void p_int_ge_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_GQ, RM_IMP, ce, ann);
    }
    void p_int_gt_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_GR, RM_IMP, ce, ann);
    }
    void p_int_le_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_LQ, RM_IMP, ce, ann);
    }
    void p_int_lt_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_CMP_reif(s, IRT_LE, RM_IMP, ce, ann);
    }

    /* linear (in-)equations */
    void p_int_lin_CMP(FlatZincSpace& s, IntRelType irt, const ConExpr& ce,
                       AST::Node* ann) {
      IntArgs ia = s.arg2intargs(ce[0]);
      int singleIntVar;
      if (s.isBoolArray(ce[1],singleIntVar)) {
        if (singleIntVar != -1) {
          if (std::abs(ia[singleIntVar]) == 1 && ce[2]->getInt() == 0) {
            IntVar siv = s.arg2IntVar(ce[1]->getArray()->a[singleIntVar]);
            BoolVarArgs iv = s.arg2boolvarargs(ce[1], 0, singleIntVar);
            IntArgs ia_tmp(ia.size()-1);
            int count = 0;
            for (int i=0; i<ia.size(); i++) {
              if (i != singleIntVar)
                ia_tmp[count++] = ia[singleIntVar] == -1 ? ia[i] : -ia[i];
            }
            IntRelType t = (ia[singleIntVar] == -1 ? irt : swap(irt));
            linear(s, ia_tmp, iv, t, siv, s.ann2icl(ann));
          } else {
            IntVarArgs iv = s.arg2intvarargs(ce[1]);
            linear(s, ia, iv, irt, ce[2]->getInt(), s.ann2icl(ann));
          }
        } else {
          BoolVarArgs iv = s.arg2boolvarargs(ce[1]);
          linear(s, ia, iv, irt, ce[2]->getInt(), s.ann2icl(ann));
        }
      } else {
        IntVarArgs iv = s.arg2intvarargs(ce[1]);
        linear(s, ia, iv, irt, ce[2]->getInt(), s.ann2icl(ann));
      }
    }
    void p_int_lin_CMP_reif(FlatZincSpace& s, IntRelType irt, ReifyMode rm,
                            const ConExpr& ce, AST::Node* ann) {
      if (rm == RM_EQV && ce[2]->isBool()) {
        if (ce[2]->getBool()) {
          p_int_lin_CMP(s, irt, ce, ann);
        } else {
          p_int_lin_CMP(s, neg(irt), ce, ann);
        }
        return;
      }
      IntArgs ia = s.arg2intargs(ce[0]);
      int singleIntVar;
      if (s.isBoolArray(ce[1],singleIntVar)) {
        if (singleIntVar != -1) {
          if (std::abs(ia[singleIntVar]) == 1 && ce[2]->getInt() == 0) {
            IntVar siv = s.arg2IntVar(ce[1]->getArray()->a[singleIntVar]);
            BoolVarArgs iv = s.arg2boolvarargs(ce[1], 0, singleIntVar);
            IntArgs ia_tmp(ia.size()-1);
            int count = 0;
            for (int i=0; i<ia.size(); i++) {
              if (i != singleIntVar)
                ia_tmp[count++] = ia[singleIntVar] == -1 ? ia[i] : -ia[i];
            }
            IntRelType t = (ia[singleIntVar] == -1 ? irt : swap(irt));
            linear(s, ia_tmp, iv, t, siv, Reify(s.arg2BoolVar(ce[3]), rm), 
                   s.ann2icl(ann));
          } else {
            IntVarArgs iv = s.arg2intvarargs(ce[1]);
            linear(s, ia, iv, irt, ce[2]->getInt(),
                   Reify(s.arg2BoolVar(ce[3]), rm), s.ann2icl(ann));
          }
        } else {
          BoolVarArgs iv = s.arg2boolvarargs(ce[1]);
          linear(s, ia, iv, irt, ce[2]->getInt(),
                 Reify(s.arg2BoolVar(ce[3]), rm), s.ann2icl(ann));
        }
      } else {
        IntVarArgs iv = s.arg2intvarargs(ce[1]);
        linear(s, ia, iv, irt, ce[2]->getInt(),
               Reify(s.arg2BoolVar(ce[3]), rm), 
               s.ann2icl(ann));
      }
    }
    void p_int_lin_eq(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP(s, IRT_EQ, ce, ann);
    }
    void p_int_lin_eq_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_EQ, RM_EQV, ce, ann);    
    }
    void p_int_lin_eq_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_EQ, RM_IMP, ce, ann);    
    }
    void p_int_lin_ne(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP(s, IRT_NQ, ce, ann);
    }
    void p_int_lin_ne_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_NQ, RM_EQV, ce, ann);    
    }
    void p_int_lin_ne_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_NQ, RM_IMP, ce, ann);    
    }
    void p_int_lin_le(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP(s, IRT_LQ, ce, ann);
    }
    void p_int_lin_le_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_LQ, RM_EQV, ce, ann);    
    }
    void p_int_lin_le_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_LQ, RM_IMP, ce, ann);    
    }
    void p_int_lin_lt(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP(s, IRT_LE, ce, ann);
    }
    void p_int_lin_lt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_LE, RM_EQV, ce, ann);    
    }
    void p_int_lin_lt_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_LE, RM_IMP, ce, ann);    
    }
    void p_int_lin_ge(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP(s, IRT_GQ, ce, ann);
    }
    void p_int_lin_ge_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_GQ, RM_EQV, ce, ann);    
    }
    void p_int_lin_ge_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_GQ, RM_IMP, ce, ann);    
    }
    void p_int_lin_gt(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP(s, IRT_GR, ce, ann);
    }
    void p_int_lin_gt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_GR, RM_EQV, ce, ann);    
    }
    void p_int_lin_gt_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_int_lin_CMP_reif(s, IRT_GR, RM_IMP, ce, ann);    
    }

    void p_bool_lin_CMP(FlatZincSpace& s, IntRelType irt, const ConExpr& ce,
                        AST::Node* ann) {
      IntArgs ia = s.arg2intargs(ce[0]);
      BoolVarArgs iv = s.arg2boolvarargs(ce[1]);
      if (ce[2]->isIntVar())
        linear(s, ia, iv, irt, s.iv[ce[2]->getIntVar()], s.ann2icl(ann));
      else
        linear(s, ia, iv, irt, ce[2]->getInt(), s.ann2icl(ann));
    }
    void p_bool_lin_CMP_reif(FlatZincSpace& s, IntRelType irt, ReifyMode rm,
                            const ConExpr& ce, AST::Node* ann) {
      if (rm == RM_EQV && ce[2]->isBool()) {
        if (ce[2]->getBool()) {
          p_bool_lin_CMP(s, irt, ce, ann);
        } else {
          p_bool_lin_CMP(s, neg(irt), ce, ann);
        }
        return;
      }
      IntArgs ia = s.arg2intargs(ce[0]);
      BoolVarArgs iv = s.arg2boolvarargs(ce[1]);
      if (ce[2]->isIntVar())
        linear(s, ia, iv, irt, s.iv[ce[2]->getIntVar()],
               Reify(s.arg2BoolVar(ce[3]), rm), 
               s.ann2icl(ann));
      else
        linear(s, ia, iv, irt, ce[2]->getInt(),
               Reify(s.arg2BoolVar(ce[3]), rm), 
               s.ann2icl(ann));
    }
    void p_bool_lin_eq(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_lin_CMP(s, IRT_EQ, ce, ann);
    }
    void p_bool_lin_eq_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_EQ, RM_EQV, ce, ann);
    }
    void p_bool_lin_eq_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_EQ, RM_IMP, ce, ann);
    }
    void p_bool_lin_ne(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_lin_CMP(s, IRT_NQ, ce, ann);
    }
    void p_bool_lin_ne_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_NQ, RM_EQV, ce, ann);
    }
    void p_bool_lin_ne_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_NQ, RM_IMP, ce, ann);
    }
    void p_bool_lin_le(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_lin_CMP(s, IRT_LQ, ce, ann);
    }
    void p_bool_lin_le_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_LQ, RM_EQV, ce, ann);
    }
    void p_bool_lin_le_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_LQ, RM_IMP, ce, ann);
    }
    void p_bool_lin_lt(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP(s, IRT_LE, ce, ann);
    }
    void p_bool_lin_lt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_LE, RM_EQV, ce, ann);
    }
    void p_bool_lin_lt_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_LE, RM_IMP, ce, ann);
    }
    void p_bool_lin_ge(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_lin_CMP(s, IRT_GQ, ce, ann);
    }
    void p_bool_lin_ge_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_GQ, RM_EQV, ce, ann);
    }
    void p_bool_lin_ge_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_GQ, RM_IMP, ce, ann);
    }
    void p_bool_lin_gt(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_lin_CMP(s, IRT_GR, ce, ann);
    }
    void p_bool_lin_gt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_GR, RM_EQV, ce, ann);
    }
    void p_bool_lin_gt_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) 
    {
      p_bool_lin_CMP_reif(s, IRT_GR, RM_IMP, ce, ann);
    }

    /* arithmetic constraints */
  
    void p_int_plus(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      if (!ce[0]->isIntVar()) {
        rel(s, ce[0]->getInt() + s.arg2IntVar(ce[1])
                == s.arg2IntVar(ce[2]), s.ann2icl(ann));
      } else if (!ce[1]->isIntVar()) {
        rel(s, s.arg2IntVar(ce[0]) + ce[1]->getInt()
                == s.arg2IntVar(ce[2]), s.ann2icl(ann));
      } else if (!ce[2]->isIntVar()) {
        rel(s, s.arg2IntVar(ce[0]) + s.arg2IntVar(ce[1]) 
                == ce[2]->getInt(), s.ann2icl(ann));
      } else {
        rel(s, s.arg2IntVar(ce[0]) + s.arg2IntVar(ce[1]) 
                == s.arg2IntVar(ce[2]), s.ann2icl(ann));
      }
    }

    void p_int_minus(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      if (!ce[0]->isIntVar()) {
        rel(s, ce[0]->getInt() - s.arg2IntVar(ce[1])
                == s.arg2IntVar(ce[2]), s.ann2icl(ann));
      } else if (!ce[1]->isIntVar()) {
        rel(s, s.arg2IntVar(ce[0]) - ce[1]->getInt()
                == s.arg2IntVar(ce[2]), s.ann2icl(ann));
      } else if (!ce[2]->isIntVar()) {
        rel(s, s.arg2IntVar(ce[0]) - s.arg2IntVar(ce[1]) 
                == ce[2]->getInt(), s.ann2icl(ann));
      } else {
        rel(s, s.arg2IntVar(ce[0]) - s.arg2IntVar(ce[1]) 
                == s.arg2IntVar(ce[2]), s.ann2icl(ann));
      }
    }

    void p_int_times(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVar x0 = s.arg2IntVar(ce[0]);
      IntVar x1 = s.arg2IntVar(ce[1]);
      IntVar x2 = s.arg2IntVar(ce[2]);
      mult(s, x0, x1, x2, s.ann2icl(ann));    
    }
    void p_int_div(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVar x0 = s.arg2IntVar(ce[0]);
      IntVar x1 = s.arg2IntVar(ce[1]);
      IntVar x2 = s.arg2IntVar(ce[2]);
      div(s,x0,x1,x2, s.ann2icl(ann));
    }
    void p_int_mod(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVar x0 = s.arg2IntVar(ce[0]);
      IntVar x1 = s.arg2IntVar(ce[1]);
      IntVar x2 = s.arg2IntVar(ce[2]);
      mod(s,x0,x1,x2, s.ann2icl(ann));
    }

    void p_int_min(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVar x0 = s.arg2IntVar(ce[0]);
      IntVar x1 = s.arg2IntVar(ce[1]);
      IntVar x2 = s.arg2IntVar(ce[2]);
      min(s, x0, x1, x2, s.ann2icl(ann));
    }
    void p_int_max(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVar x0 = s.arg2IntVar(ce[0]);
      IntVar x1 = s.arg2IntVar(ce[1]);
      IntVar x2 = s.arg2IntVar(ce[2]);
      max(s, x0, x1, x2, s.ann2icl(ann));
    }
    void p_int_negate(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVar x0 = s.arg2IntVar(ce[0]);
      IntVar x1 = s.arg2IntVar(ce[1]);
      rel(s, x0 == -x1, s.ann2icl(ann));
    }

    /* Boolean constraints */
    void p_bool_CMP(FlatZincSpace& s, IntRelType irt, const ConExpr& ce, 
                   AST::Node* ann) {
      rel(s, s.arg2BoolVar(ce[0]), irt, s.arg2BoolVar(ce[1]), 
          s.ann2icl(ann));
    }
    void p_bool_CMP_reif(FlatZincSpace& s, IntRelType irt, ReifyMode rm,
                         const ConExpr& ce, AST::Node* ann) {
      rel(s, s.arg2BoolVar(ce[0]), irt, s.arg2BoolVar(ce[1]),
          Reify(s.arg2BoolVar(ce[2]), rm), s.ann2icl(ann));
    }
    void p_bool_eq(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP(s, IRT_EQ, ce, ann);
    }
    void p_bool_eq_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_EQ, RM_EQV, ce, ann);
    }
    void p_bool_eq_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_EQ, RM_IMP, ce, ann);
    }
    void p_bool_ne(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP(s, IRT_NQ, ce, ann);
    }
    void p_bool_ne_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_NQ, RM_EQV, ce, ann);
    }
    void p_bool_ne_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_NQ, RM_IMP, ce, ann);
    }
    void p_bool_ge(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP(s, IRT_GQ, ce, ann);
    }
    void p_bool_ge_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_GQ, RM_EQV, ce, ann);
    }
    void p_bool_ge_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_GQ, RM_IMP, ce, ann);
    }
    void p_bool_le(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP(s, IRT_LQ, ce, ann);
    }
    void p_bool_le_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_LQ, RM_EQV, ce, ann);
    }
    void p_bool_le_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_LQ, RM_IMP, ce, ann);
    }
    void p_bool_gt(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP(s, IRT_GR, ce, ann);
    }
    void p_bool_gt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_GR, RM_EQV, ce, ann);
    }
    void p_bool_gt_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_GR, RM_IMP, ce, ann);
    }
    void p_bool_lt(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP(s, IRT_LE, ce, ann);
    }
    void p_bool_lt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_LE, RM_EQV, ce, ann);
    }
    void p_bool_lt_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_bool_CMP_reif(s, IRT_LE, RM_IMP, ce, ann);
    }

#define BOOL_OP(op) \
    BoolVar b0 = s.arg2BoolVar(ce[0]); \
    BoolVar b1 = s.arg2BoolVar(ce[1]); \
    if (ce[2]->isBool()) { \
      rel(s, b0, op, b1, ce[2]->getBool(), s.ann2icl(ann)); \
    } else { \
      rel(s, b0, op, b1, s.bv[ce[2]->getBoolVar()], s.ann2icl(ann)); \
    }

#define BOOL_ARRAY_OP(op) \
    BoolVarArgs bv = s.arg2boolvarargs(ce[0]); \
    if (ce.size()==1) { \
      rel(s, op, bv, 1, s.ann2icl(ann)); \
    } else if (ce[1]->isBool()) { \
      rel(s, op, bv, ce[1]->getBool(), s.ann2icl(ann)); \
    } else { \
      rel(s, op, bv, s.bv[ce[1]->getBoolVar()], s.ann2icl(ann)); \
    }

    void p_bool_or(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BOOL_OP(BOT_OR);
    }
    void p_bool_or_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVar b0 = s.arg2BoolVar(ce[0]);
      BoolVar b1 = s.arg2BoolVar(ce[1]);
      BoolVar b2 = s.arg2BoolVar(ce[2]);
      clause(s, BOT_OR, BoolVarArgs()<<b0<<b1, BoolVarArgs()<<b2, 1, 
             s.ann2icl(ann));
    }
    void p_bool_and(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BOOL_OP(BOT_AND);
    }
    void p_bool_and_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVar b0 = s.arg2BoolVar(ce[0]);
      BoolVar b1 = s.arg2BoolVar(ce[1]);
      BoolVar b2 = s.arg2BoolVar(ce[2]);
      rel(s, b2, BOT_IMP, b0, 1, s.ann2icl(ann));
      rel(s, b2, BOT_IMP, b1, 1, s.ann2icl(ann));
    }
    void p_array_bool_and(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann)
    {
      BOOL_ARRAY_OP(BOT_AND);
    }
    void p_array_bool_and_imp(FlatZincSpace& s, const ConExpr& ce,
                              AST::Node* ann)
    {
      BoolVarArgs bv = s.arg2boolvarargs(ce[0]);
      BoolVar b1 = s.arg2BoolVar(ce[1]);
      for (unsigned int i=bv.size(); i--;)
        rel(s, b1, BOT_IMP, bv[i], 1, s.ann2icl(ann));
    }
    void p_array_bool_or(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann)
    {
      BOOL_ARRAY_OP(BOT_OR);
    }
    void p_array_bool_or_imp(FlatZincSpace& s, const ConExpr& ce,
                             AST::Node* ann)
    {
      BoolVarArgs bv = s.arg2boolvarargs(ce[0]);
      BoolVar b1 = s.arg2BoolVar(ce[1]);
      clause(s, BOT_OR, bv, BoolVarArgs()<<b1, 1, s.ann2icl(ann));
    }
    void p_array_bool_xor(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann)
    {
      BOOL_ARRAY_OP(BOT_XOR);
    }
    void p_array_bool_xor_imp(FlatZincSpace& s, const ConExpr& ce,
                              AST::Node* ann)
    {
      BoolVarArgs bv = s.arg2boolvarargs(ce[0]);
      BoolVar tmp(s,0,1);
      rel(s, BOT_XOR, bv, tmp, s.ann2icl(ann));
      rel(s, s.arg2BoolVar(ce[1]), BOT_IMP, tmp, 1);
    }
    void p_array_bool_clause(FlatZincSpace& s, const ConExpr& ce,
                             AST::Node* ann) {
      BoolVarArgs bvp = s.arg2boolvarargs(ce[0]);
      BoolVarArgs bvn = s.arg2boolvarargs(ce[1]);
      clause(s, BOT_OR, bvp, bvn, 1, s.ann2icl(ann));
    }
    void p_array_bool_clause_reif(FlatZincSpace& s, const ConExpr& ce,
                             AST::Node* ann) {
      BoolVarArgs bvp = s.arg2boolvarargs(ce[0]);
      BoolVarArgs bvn = s.arg2boolvarargs(ce[1]);
      BoolVar b0 = s.arg2BoolVar(ce[2]);
      clause(s, BOT_OR, bvp, bvn, b0, s.ann2icl(ann));
    }
    void p_array_bool_clause_imp(FlatZincSpace& s, const ConExpr& ce,
                             AST::Node* ann) {
      BoolVarArgs bvp = s.arg2boolvarargs(ce[0]);
      BoolVarArgs bvn = s.arg2boolvarargs(ce[1]);
      BoolVar b0 = s.arg2BoolVar(ce[2]);
      clause(s, BOT_OR, bvp, bvn, b0, s.ann2icl(ann));
    }
    void p_bool_xor(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BOOL_OP(BOT_XOR);
    }
    void p_bool_xor_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVar b0 = s.arg2BoolVar(ce[0]);
      BoolVar b1 = s.arg2BoolVar(ce[1]);
      BoolVar b2 = s.arg2BoolVar(ce[2]);
      clause(s, BOT_OR, BoolVarArgs()<<b0<<b1, BoolVarArgs()<<b2, 1,
             s.ann2icl(ann));
      clause(s, BOT_OR, BoolVarArgs(), BoolVarArgs()<<b0<<b1<<b2, 1,
             s.ann2icl(ann));
    }
    void p_bool_l_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVar b0 = s.arg2BoolVar(ce[0]);
      BoolVar b1 = s.arg2BoolVar(ce[1]);
      if (ce[2]->isBool()) {
        rel(s, b1, BOT_IMP, b0, ce[2]->getBool(), s.ann2icl(ann));
      } else {
        rel(s, b1, BOT_IMP, b0, s.bv[ce[2]->getBoolVar()], s.ann2icl(ann));
      }
    }
    void p_bool_r_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BOOL_OP(BOT_IMP);
    }
    void p_bool_not(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVar x0 = s.arg2BoolVar(ce[0]);
      BoolVar x1 = s.arg2BoolVar(ce[1]);
      rel(s, x0, BOT_XOR, x1, 1, s.ann2icl(ann));
    }
  
    /* element constraints */
    void p_array_int_element(FlatZincSpace& s, const ConExpr& ce, 
                                 AST::Node* ann) {
      bool isConstant = true;
      AST::Array* a = ce[1]->getArray();
      for (int i=a->a.size(); i--;) {
        if (!a->a[i]->isInt()) {
          isConstant = false;
          break;
        }
      }
      IntVar selector = s.arg2IntVar(ce[0]);
      rel(s, selector > 0);
      if (isConstant) {
        IntArgs ia = s.arg2intargs(ce[1], 1);
        element(s, ia, selector, s.arg2IntVar(ce[2]), s.ann2icl(ann));
      } else {
        IntVarArgs iv = s.arg2intvarargs(ce[1], 1);
        element(s, iv, selector, s.arg2IntVar(ce[2]), s.ann2icl(ann));
      }
    }
    void p_array_bool_element(FlatZincSpace& s, const ConExpr& ce, 
                                  AST::Node* ann) {
      bool isConstant = true;
      AST::Array* a = ce[1]->getArray();
      for (int i=a->a.size(); i--;) {
        if (!a->a[i]->isBool()) {
          isConstant = false;
          break;
        }
      }
      IntVar selector = s.arg2IntVar(ce[0]);
      rel(s, selector > 0);
      if (isConstant) {
        IntArgs ia = s.arg2boolargs(ce[1], 1);
        element(s, ia, selector, s.arg2BoolVar(ce[2]), s.ann2icl(ann));
      } else {
        BoolVarArgs iv = s.arg2boolvarargs(ce[1], 1);
        element(s, iv, selector, s.arg2BoolVar(ce[2]), s.ann2icl(ann));
      }
    }
  
    /* coercion constraints */
    void p_bool2int(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVar x0 = s.arg2BoolVar(ce[0]);
      IntVar x1 = s.arg2IntVar(ce[1]);
      if (ce[0]->isBoolVar() && ce[1]->isIntVar()) {
        s.aliasBool2Int(ce[1]->getIntVar(), ce[0]->getBoolVar());
      }
      channel(s, x0, x1, s.ann2icl(ann));
    }

    void p_int_in(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      IntSet d = s.arg2intset(ce[1]);
      if (ce[0]->isBoolVar()) {
        IntSetRanges dr(d);
        Iter::Ranges::Singleton sr(0,1);
        Iter::Ranges::Inter<IntSetRanges,Iter::Ranges::Singleton> i(dr,sr);
        IntSet d01(i);
        if (d01.size() == 0) {
          s.fail();
        } else {
          rel(s, s.arg2BoolVar(ce[0]), IRT_GQ, d01.min());
          rel(s, s.arg2BoolVar(ce[0]), IRT_LQ, d01.max());
        }
      } else {
        dom(s, s.arg2IntVar(ce[0]), d);
      }
    }
    void p_int_in_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      IntSet d = s.arg2intset(ce[1]);
      if (ce[0]->isBoolVar()) {
        IntSetRanges dr(d);
        Iter::Ranges::Singleton sr(0,1);
        Iter::Ranges::Inter<IntSetRanges,Iter::Ranges::Singleton> i(dr,sr);
        IntSet d01(i);
        if (d01.size() == 0) {
          rel(s, s.arg2BoolVar(ce[2]) == 0);
        } else if (d01.max() == 0) {
          rel(s, s.arg2BoolVar(ce[2]) == !s.arg2BoolVar(ce[0]));
        } else if (d01.min() == 1) {
          rel(s, s.arg2BoolVar(ce[2]) == s.arg2BoolVar(ce[0]));
        } else {
          rel(s, s.arg2BoolVar(ce[2]) == 1);
        }
      } else {
        dom(s, s.arg2IntVar(ce[0]), d, s.arg2BoolVar(ce[2]));
      }
    }
    void p_int_in_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      IntSet d = s.arg2intset(ce[1]);
      if (ce[0]->isBoolVar()) {
        IntSetRanges dr(d);
        Iter::Ranges::Singleton sr(0,1);
        Iter::Ranges::Inter<IntSetRanges,Iter::Ranges::Singleton> i(dr,sr);
        IntSet d01(i);
        if (d01.size() == 0) {
          rel(s, s.arg2BoolVar(ce[2]) == 0);
        } else if (d01.max() == 0) {
          rel(s, s.arg2BoolVar(ce[2]) >> !s.arg2BoolVar(ce[0]));
        } else if (d01.min() == 1) {
          rel(s, s.arg2BoolVar(ce[2]) >> s.arg2BoolVar(ce[0]));
        }
      } else {
        dom(s, s.arg2IntVar(ce[0]), d, Reify(s.arg2BoolVar(ce[2]),RM_IMP));
      }
    }

    /* constraints from the standard library */
  
    void p_abs(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVar x0 = s.arg2IntVar(ce[0]);
      IntVar x1 = s.arg2IntVar(ce[1]);
      abs(s, x0, x1, s.ann2icl(ann));
    }
  
    void p_array_int_lt(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs iv0 = s.arg2intvarargs(ce[0]);
      IntVarArgs iv1 = s.arg2intvarargs(ce[1]);
      rel(s, iv0, IRT_LE, iv1, s.ann2icl(ann));
    }

    void p_array_int_lq(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs iv0 = s.arg2intvarargs(ce[0]);
      IntVarArgs iv1 = s.arg2intvarargs(ce[1]);
      rel(s, iv0, IRT_LQ, iv1, s.ann2icl(ann));
    }

    void p_array_bool_lt(FlatZincSpace& s, const ConExpr& ce,
                         AST::Node* ann) {
      BoolVarArgs bv0 = s.arg2boolvarargs(ce[0]);
      BoolVarArgs bv1 = s.arg2boolvarargs(ce[1]);
      rel(s, bv0, IRT_LE, bv1, s.ann2icl(ann));
    }

    void p_array_bool_lq(FlatZincSpace& s, const ConExpr& ce,
                         AST::Node* ann) {
      BoolVarArgs bv0 = s.arg2boolvarargs(ce[0]);
      BoolVarArgs bv1 = s.arg2boolvarargs(ce[1]);
      rel(s, bv0, IRT_LQ, bv1, s.ann2icl(ann));
    }
  
    void p_count(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs iv = s.arg2intvarargs(ce[0]);
      if (!ce[1]->isIntVar()) {
        if (!ce[2]->isIntVar()) {
          count(s, iv, ce[1]->getInt(), IRT_EQ, ce[2]->getInt(), 
                s.ann2icl(ann));
        } else {
          count(s, iv, ce[1]->getInt(), IRT_EQ, s.arg2IntVar(ce[2]), 
                s.ann2icl(ann));
        }
      } else if (!ce[2]->isIntVar()) {
        count(s, iv, s.arg2IntVar(ce[1]), IRT_EQ, ce[2]->getInt(), 
              s.ann2icl(ann));
      } else {
        count(s, iv, s.arg2IntVar(ce[1]), IRT_EQ, s.arg2IntVar(ce[2]), 
              s.ann2icl(ann));
      }
    }

    void p_count_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs iv = s.arg2intvarargs(ce[0]);
      IntVar x = s.arg2IntVar(ce[1]);
      IntVar y = s.arg2IntVar(ce[2]);
      BoolVar b = s.arg2BoolVar(ce[3]);
      IntVar c(s,0,Int::Limits::max);
      count(s,iv,x,IRT_EQ,c,s.ann2icl(ann));
      rel(s, b == (c==y));
    }
    void p_count_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs iv = s.arg2intvarargs(ce[0]);
      IntVar x = s.arg2IntVar(ce[1]);
      IntVar y = s.arg2IntVar(ce[2]);
      BoolVar b = s.arg2BoolVar(ce[3]);
      IntVar c(s,0,Int::Limits::max);
      count(s,iv,x,IRT_EQ,c,s.ann2icl(ann));
      rel(s, b >> (c==y));
    }

    void count_rel(IntRelType irt,
                   FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs iv = s.arg2intvarargs(ce[1]);
      count(s, iv, ce[2]->getInt(), irt, ce[0]->getInt(), s.ann2icl(ann));
    }

    void p_at_most(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      count_rel(IRT_LQ, s, ce, ann);
    }

    void p_at_least(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      count_rel(IRT_GQ, s, ce, ann);
    }

    void p_bin_packing_load(FlatZincSpace& s, const ConExpr& ce,
                            AST::Node* ann) {
      int minIdx = ce[3]->getInt();
      IntVarArgs load = s.arg2intvarargs(ce[0]);
      IntVarArgs l;
      IntVarArgs bin = s.arg2intvarargs(ce[1]);
      for (int i=bin.size(); i--;)
        rel(s, bin[i] >= minIdx);
      if (minIdx > 0) {
        for (int i=minIdx; i--;)
          l << IntVar(s,0,0);
      } else if (minIdx < 0) {
        IntVarArgs bin2(bin.size());
        for (int i=bin.size(); i--;)
          bin2[i] = expr(s, bin[i]-minIdx, s.ann2icl(ann));
        bin = bin2;
      }
      l << load;
      IntArgs sizes = s.arg2intargs(ce[2]);

      IntVarArgs allvars = l + bin;
      unshare(s, allvars);
      binpacking(s, allvars.slice(0,1,l.size()), allvars.slice(l.size(),1,bin.size()),
                 sizes, s.ann2icl(ann));
    }

    void p_global_cardinality(FlatZincSpace& s, const ConExpr& ce,
                              AST::Node* ann) {
      IntVarArgs iv0 = s.arg2intvarargs(ce[0]);
      IntArgs cover = s.arg2intargs(ce[1]);
      IntVarArgs iv1 = s.arg2intvarargs(ce[2]);

      Region re(s);
      IntSet cover_s(cover);
      IntSetRanges cover_r(cover_s);
      IntVarRanges* iv0_ri = re.alloc<IntVarRanges>(iv0.size());
      for (int i=iv0.size(); i--;)
        iv0_ri[i] = IntVarRanges(iv0[i]);
      Iter::Ranges::NaryUnion iv0_r(re,iv0_ri,iv0.size());
      Iter::Ranges::Diff<Iter::Ranges::NaryUnion,IntSetRanges> 
        extra_r(iv0_r,cover_r);
      Iter::Ranges::ToValues<Iter::Ranges::Diff<
        Iter::Ranges::NaryUnion,IntSetRanges> > extra(extra_r);
      for (; extra(); ++extra) {
        cover << extra.val();
        iv1 << IntVar(s,0,iv0.size());
      }
      IntConLevel icl = s.ann2icl(ann);
      if (icl==ICL_DOM) {
        IntVarArgs allvars = iv0+iv1;
        unshare(s, allvars);
        count(s, allvars.slice(0,1,iv0.size()), 
                 allvars.slice(iv0.size(),1,iv1.size()),
                 cover, s.ann2icl(ann));
      } else {
        unshare(s, iv0);
        count(s, iv0, iv1, cover, s.ann2icl(ann));
      }
    }

    void p_global_cardinality_closed(FlatZincSpace& s, const ConExpr& ce,
                                     AST::Node* ann) {
      IntVarArgs iv0 = s.arg2intvarargs(ce[0]);
      IntArgs cover = s.arg2intargs(ce[1]);
      IntVarArgs iv1 = s.arg2intvarargs(ce[2]);
      unshare(s, iv0);
      count(s, iv0, iv1, cover, s.ann2icl(ann));
    }

    void p_global_cardinality_low_up(FlatZincSpace& s, const ConExpr& ce,
                                     AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      IntArgs cover = s.arg2intargs(ce[1]);

      IntArgs lbound = s.arg2intargs(ce[2]);
      IntArgs ubound = s.arg2intargs(ce[3]);
      IntSetArgs y(cover.size());
      for (int i=cover.size(); i--;)
        y[i] = IntSet(lbound[i],ubound[i]);

      IntSet cover_s(cover);
      Region re(s);
      IntVarRanges* xrs = re.alloc<IntVarRanges>(x.size());
      for (int i=x.size(); i--;)
        xrs[i].init(x[i]);
      Iter::Ranges::NaryUnion u(re, xrs, x.size());
      Iter::Ranges::ToValues<Iter::Ranges::NaryUnion> uv(u);
      for (; uv(); ++uv) {
        if (!cover_s.in(uv.val())) {
          cover << uv.val();
          y << IntSet(0,x.size());
        }
      }
      unshare(s, x);
      count(s, x, y, cover, s.ann2icl(ann));
    }

    void p_global_cardinality_low_up_closed(FlatZincSpace& s,
                                            const ConExpr& ce,
                                            AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      IntArgs cover = s.arg2intargs(ce[1]);

      IntArgs lbound = s.arg2intargs(ce[2]);
      IntArgs ubound = s.arg2intargs(ce[3]);
      IntSetArgs y(cover.size());
      for (int i=cover.size(); i--;)
        y[i] = IntSet(lbound[i],ubound[i]);
      unshare(s, x);
      count(s, x, y, cover, s.ann2icl(ann));
    }

    void p_minimum(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs iv = s.arg2intvarargs(ce[1]);
      min(s, iv, s.arg2IntVar(ce[0]), s.ann2icl(ann));
    }

    void p_maximum(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs iv = s.arg2intvarargs(ce[1]);
      max(s, iv, s.arg2IntVar(ce[0]), s.ann2icl(ann));
    }

    void p_regular(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs iv = s.arg2intvarargs(ce[0]);
      int q = ce[1]->getInt();
      int symbols = ce[2]->getInt();
      IntArgs d = s.arg2intargs(ce[3]);
      int q0 = ce[4]->getInt();

      int noOfTrans = 0;
      for (int i=1; i<=q; i++) {
        for (int j=1; j<=symbols; j++) {
          if (d[(i-1)*symbols+(j-1)] > 0)
            noOfTrans++;
        }
      }
    
      Region re(s);
      DFA::Transition* t = re.alloc<DFA::Transition>(noOfTrans+1);
      noOfTrans = 0;
      for (int i=1; i<=q; i++) {
        for (int j=1; j<=symbols; j++) {
          if (d[(i-1)*symbols+(j-1)] > 0) {
            t[noOfTrans].i_state = i;
            t[noOfTrans].symbol  = j;
            t[noOfTrans].o_state = d[(i-1)*symbols+(j-1)];
            noOfTrans++;
          }
        }
      }
      t[noOfTrans].i_state = -1;
    
      // Final states
      AST::SetLit* sl = ce[5]->getSet();
      int* f;
      if (sl->interval) {
        f = static_cast<int*>(malloc(sizeof(int)*(sl->max-sl->min+2)));
        for (int i=sl->min; i<=sl->max; i++)
          f[i-sl->min] = i;
        f[sl->max-sl->min+1] = -1;
      } else {
        f = static_cast<int*>(malloc(sizeof(int)*(sl->s.size()+1)));
        for (int j=sl->s.size(); j--; )
          f[j] = sl->s[j];
        f[sl->s.size()] = -1;
      }
        
      DFA dfa(q0,t,f);
      free(f);
      unshare(s, iv);
      extensional(s, iv, dfa, s.ann2icl(ann));
    }

    void
    p_sort(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      IntVarArgs y = s.arg2intvarargs(ce[1]);
      IntVarArgs xy(x.size()+y.size());
      for (int i=x.size(); i--;)
        xy[i] = x[i];
      for (int i=y.size(); i--;)
        xy[i+x.size()] = y[i];
      unshare(s, xy);
      for (int i=x.size(); i--;)
        x[i] = xy[i];
      for (int i=y.size(); i--;)
        y[i] = xy[i+x.size()];
      sorted(s, x, y, s.ann2icl(ann));
    }

    void
    p_inverse_offsets(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      unshare(s, x);
      int xoff = ce[1]->getInt();
      IntVarArgs y = s.arg2intvarargs(ce[2]);
      unshare(s, y);
      int yoff = ce[3]->getInt();
      channel(s, x, xoff, y, yoff, s.ann2icl(ann));
    }

    void
    p_increasing_int(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      rel(s,x,IRT_LQ,s.ann2icl(ann));
    }

    void
    p_increasing_bool(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVarArgs x = s.arg2boolvarargs(ce[0]);
      rel(s,x,IRT_LQ,s.ann2icl(ann));
    }

    void
    p_decreasing_int(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      rel(s,x,IRT_GQ,s.ann2icl(ann));
    }

    void
    p_decreasing_bool(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVarArgs x = s.arg2boolvarargs(ce[0]);
      rel(s,x,IRT_GQ,s.ann2icl(ann));
    }

    void
    p_table_int(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      IntArgs tuples = s.arg2intargs(ce[1]);
      int noOfVars   = x.size();
      int noOfTuples = tuples.size() == 0 ? 0 : (tuples.size()/noOfVars);
      TupleSet ts;
      for (int i=0; i<noOfTuples; i++) {
        IntArgs t(noOfVars);
        for (int j=0; j<x.size(); j++) {
          t[j] = tuples[i*noOfVars+j];
        }
        ts.add(t);
      }
      ts.finalize();
      extensional(s,x,ts,EPK_DEF,s.ann2icl(ann));
    }
    void
    p_table_bool(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVarArgs x = s.arg2boolvarargs(ce[0]);
      IntArgs tuples = s.arg2boolargs(ce[1]);
      int noOfVars   = x.size();
      int noOfTuples = tuples.size() == 0 ? 0 : (tuples.size()/noOfVars);
      TupleSet ts;
      for (int i=0; i<noOfTuples; i++) {
        IntArgs t(noOfVars);
        for (int j=0; j<x.size(); j++) {
          t[j] = tuples[i*noOfVars+j];
        }
        ts.add(t);
      }
      ts.finalize();
      extensional(s,x,ts,EPK_DEF,s.ann2icl(ann));
    }

    void p_cumulatives(FlatZincSpace& s, const ConExpr& ce,
                      AST::Node* ann) {
      IntVarArgs start = s.arg2intvarargs(ce[0]);
      IntVarArgs duration = s.arg2intvarargs(ce[1]);
      IntVarArgs height = s.arg2intvarargs(ce[2]);
      int n = start.size();
      IntVar bound = s.arg2IntVar(ce[3]);

      int minHeight = INT_MAX; int minHeight2 = INT_MAX;
      for (int i=n; i--;)
        if (height[i].min() < minHeight)
          minHeight = height[i].min();
        else if (height[i].min() < minHeight2)
          minHeight2 = height[i].min();
      bool disjunctive =
       (minHeight > bound.max()/2) ||
       (minHeight2 > bound.max()/2 && minHeight+minHeight2>bound.max());
      if (disjunctive) {
        rel(s, bound >= max(height));
        // Unary
        if (duration.assigned()) {
          IntArgs durationI(n);
          for (int i=n; i--;)
            durationI[i] = duration[i].val();
          unshare(s,start);
          unary(s,start,durationI);
        } else {
          IntVarArgs end(n);
          for (int i=n; i--;)
            end[i] = expr(s,start[i]+duration[i]);
          unshare(s,start);
          unary(s,start,duration,end);
        }
      } else if (height.assigned()) {
        IntArgs heightI(n);
        for (int i=n; i--;)
          heightI[i] = height[i].val();
        if (duration.assigned()) {
          IntArgs durationI(n);
          for (int i=n; i--;)
            durationI[i] = duration[i].val();
          cumulative(s, bound, start, durationI, heightI);
        } else {
          IntVarArgs end(n);
          for (int i = n; i--; )
            end[i] = expr(s,start[i]+duration[i]);
          cumulative(s, bound, start, duration, end, heightI);
        }
      } else if (bound.assigned()) {
        IntArgs machine = IntArgs::create(n,0,0);
        IntArgs limit(1, bound.val());
        IntVarArgs end(n);
        for (int i=n; i--;)
          end[i] = expr(s,start[i]+duration[i]);
        cumulatives(s, machine, start, duration, end, height, limit, true,
                    s.ann2icl(ann));
      } else {
        int min = Gecode::Int::Limits::max;
        int max = Gecode::Int::Limits::min;
        IntVarArgs end(start.size());
        for (int i = start.size(); i--; ) {
          min = std::min(min, start[i].min());
          max = std::max(max, start[i].max() + duration[i].max());
          end[i] = expr(s, start[i] + duration[i]);
        }
        for (int time = min; time < max; ++time) {
          IntVarArgs x(start.size());
          for (int i = start.size(); i--; ) {
            IntVar overlaps = channel(s, expr(s, (start[i] <= time) && 
                                                 (time < end[i])));
            x[i] = expr(s, overlaps * height[i]);
          }
          linear(s, x, IRT_LQ, bound);
        }
      }
    }

    void p_among_seq_int(FlatZincSpace& s, const ConExpr& ce,
                         AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      IntSet S = s.arg2intset(ce[1]);
      int q = ce[2]->getInt();
      int l = ce[3]->getInt();
      int u = ce[4]->getInt();
      unshare(s, x);
      sequence(s, x, S, q, l, u, s.ann2icl(ann));
    }

    void p_among_seq_bool(FlatZincSpace& s, const ConExpr& ce,
                          AST::Node* ann) {
      BoolVarArgs x = s.arg2boolvarargs(ce[0]);
      bool val = ce[1]->getBool();
      int q = ce[2]->getInt();
      int l = ce[3]->getInt();
      int u = ce[4]->getInt();
      IntSet S(val, val);
      unshare(s, x);
      sequence(s, x, S, q, l, u, s.ann2icl(ann));
    }

    void p_schedule_unary(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      IntArgs p = s.arg2intargs(ce[1]);
      unshare(s,x);
      unary(s, x, p);
    }

    void p_schedule_unary_optional(FlatZincSpace& s, const ConExpr& ce,
                                   AST::Node*) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      IntArgs p = s.arg2intargs(ce[1]);
      BoolVarArgs m = s.arg2boolvarargs(ce[2]);
      unshare(s,x);
      unary(s, x, p, m);
    }

    void p_circuit(FlatZincSpace& s, const ConExpr& ce, AST::Node *ann) {
      int off = ce[0]->getInt();
      IntVarArgs xv = s.arg2intvarargs(ce[1]);
      unshare(s,xv);
      circuit(s,off,xv,s.ann2icl(ann));
    }
    void p_circuit_cost_array(FlatZincSpace& s, const ConExpr& ce,
                              AST::Node *ann) {
      IntArgs c = s.arg2intargs(ce[0]);
      IntVarArgs xv = s.arg2intvarargs(ce[1]);
      IntVarArgs yv = s.arg2intvarargs(ce[2]);
      IntVar z = s.arg2IntVar(ce[3]);
      unshare(s,xv);
      circuit(s,c,xv,yv,z,s.ann2icl(ann));
    }
    void p_circuit_cost(FlatZincSpace& s, const ConExpr& ce, AST::Node *ann) {
      IntArgs c = s.arg2intargs(ce[0]);
      IntVarArgs xv = s.arg2intvarargs(ce[1]);
      IntVar z = s.arg2IntVar(ce[2]);
      unshare(s,xv);
      circuit(s,c,xv,z,s.ann2icl(ann));
    }

    void p_nooverlap(FlatZincSpace& s, const ConExpr& ce, AST::Node *ann) {
      IntVarArgs x0 = s.arg2intvarargs(ce[0]);
      IntVarArgs w = s.arg2intvarargs(ce[1]);
      IntVarArgs y0 = s.arg2intvarargs(ce[2]);
      IntVarArgs h = s.arg2intvarargs(ce[3]);
      if (w.assigned() && h.assigned()) {
        IntArgs iw(w.size());
        for (int i=w.size(); i--;)
          iw[i] = w[i].val();
        IntArgs ih(h.size());
        for (int i=h.size(); i--;)
          ih[i] = h[i].val();
        nooverlap(s,x0,iw,y0,ih,s.ann2icl(ann));
        
        int miny = y0[0].min();
        int maxy = y0[0].max();
        int maxdy = ih[0];
        for (int i=1; i<y0.size(); i++) {
          miny = std::min(miny,y0[i].min());
          maxy = std::max(maxy,y0[i].max());
          maxdy = std::max(maxdy,ih[i]);
        }
        int minx = x0[0].min();
        int maxx = x0[0].max();
        int maxdx = iw[0];
        for (int i=1; i<x0.size(); i++) {
          minx = std::min(minx,x0[i].min());
          maxx = std::max(maxx,x0[i].max());
          maxdx = std::max(maxdx,iw[i]);
        }
        if (miny > Int::Limits::min && maxy < Int::Limits::max) {
          cumulative(s,maxdy+maxy-miny,x0,iw,ih);
          cumulative(s,maxdx+maxx-minx,y0,ih,iw);
        }
      } else {
        IntVarArgs x1(x0.size()), y1(y0.size());
        for (int i=x0.size(); i--; )
          x1[i] = expr(s, x0[i] + w[i]);
        for (int i=y0.size(); i--; )
          y1[i] = expr(s, y0[i] + h[i]);
        nooverlap(s,x0,w,x1,y0,h,y1,s.ann2icl(ann));
      }
    }

    void p_precede(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      int p_s = ce[1]->getInt();
      int p_t = ce[2]->getInt();
      precede(s,x,p_s,p_t,s.ann2icl(ann));
    }

    void p_nvalue(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[1]);
      if (ce[0]->isIntVar()) {
        IntVar y = s.arg2IntVar(ce[0]);
        nvalues(s,x,IRT_EQ,y,s.ann2icl(ann));
      } else {
        nvalues(s,x,IRT_EQ,ce[0]->getInt(),s.ann2icl(ann));
      }
    }

    void p_among(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[1]);
      IntSet v = s.arg2intset(ce[2]);
      if (ce[0]->isIntVar()) {
        IntVar n = s.arg2IntVar(ce[0]);
        unshare(s, x);
        count(s,x,v,IRT_EQ,n,s.ann2icl(ann));
      } else {
        unshare(s, x);
        count(s,x,v,IRT_EQ,ce[0]->getInt(),s.ann2icl(ann));
      }
    }

    void p_member_int(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      IntVar y = s.arg2IntVar(ce[1]);
      member(s,x,y,s.ann2icl(ann));
    }
    void p_member_int_reif(FlatZincSpace& s, const ConExpr& ce,
                           AST::Node* ann) {
      IntVarArgs x = s.arg2intvarargs(ce[0]);
      IntVar y = s.arg2IntVar(ce[1]);
      BoolVar b = s.arg2BoolVar(ce[2]);
      member(s,x,y,b,s.ann2icl(ann));
    }
    void p_member_bool(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      BoolVarArgs x = s.arg2boolvarargs(ce[0]);
      BoolVar y = s.arg2BoolVar(ce[1]);
      member(s,x,y,s.ann2icl(ann));
    }
    void p_member_bool_reif(FlatZincSpace& s, const ConExpr& ce,
                            AST::Node* ann) {
      BoolVarArgs x = s.arg2boolvarargs(ce[0]);
      BoolVar y = s.arg2BoolVar(ce[1]);
      member(s,x,y,s.arg2BoolVar(ce[2]),s.ann2icl(ann));
    }

    class IntPoster {
    public:
      IntPoster(void) {
        registry().add("all_different_int", &p_distinct);
        registry().add("all_different_offset", &p_distinctOffset);
        registry().add("all_equal_int", &p_all_equal);
        registry().add("int_eq", &p_int_eq);
        registry().add("int_ne", &p_int_ne);
        registry().add("int_ge", &p_int_ge);
        registry().add("int_gt", &p_int_gt);
        registry().add("int_le", &p_int_le);
        registry().add("int_lt", &p_int_lt);
        registry().add("int_eq_reif", &p_int_eq_reif);
        registry().add("int_ne_reif", &p_int_ne_reif);
        registry().add("int_ge_reif", &p_int_ge_reif);
        registry().add("int_gt_reif", &p_int_gt_reif);
        registry().add("int_le_reif", &p_int_le_reif);
        registry().add("int_lt_reif", &p_int_lt_reif);
        registry().add("int_eq_imp", &p_int_eq_imp);
        registry().add("int_ne_imp", &p_int_ne_imp);
        registry().add("int_ge_imp", &p_int_ge_imp);
        registry().add("int_gt_imp", &p_int_gt_imp);
        registry().add("int_le_imp", &p_int_le_imp);
        registry().add("int_lt_imp", &p_int_lt_imp);
        registry().add("int_lin_eq", &p_int_lin_eq);
        registry().add("int_lin_eq_reif", &p_int_lin_eq_reif);
        registry().add("int_lin_eq_imp", &p_int_lin_eq_imp);
        registry().add("int_lin_ne", &p_int_lin_ne);
        registry().add("int_lin_ne_reif", &p_int_lin_ne_reif);
        registry().add("int_lin_ne_imp", &p_int_lin_ne_imp);
        registry().add("int_lin_le", &p_int_lin_le);
        registry().add("int_lin_le_reif", &p_int_lin_le_reif);
        registry().add("int_lin_le_imp", &p_int_lin_le_imp);
        registry().add("int_lin_lt", &p_int_lin_lt);
        registry().add("int_lin_lt_reif", &p_int_lin_lt_reif);
        registry().add("int_lin_lt_imp", &p_int_lin_lt_imp);
        registry().add("int_lin_ge", &p_int_lin_ge);
        registry().add("int_lin_ge_reif", &p_int_lin_ge_reif);
        registry().add("int_lin_ge_imp", &p_int_lin_ge_imp);
        registry().add("int_lin_gt", &p_int_lin_gt);
        registry().add("int_lin_gt_reif", &p_int_lin_gt_reif);
        registry().add("int_lin_gt_imp", &p_int_lin_gt_imp);
        registry().add("int_plus", &p_int_plus);
        registry().add("int_minus", &p_int_minus);
        registry().add("int_times", &p_int_times);
        registry().add("int_div", &p_int_div);
        registry().add("int_mod", &p_int_mod);
        registry().add("int_min", &p_int_min);
        registry().add("int_max", &p_int_max);
        registry().add("int_abs", &p_abs);
        registry().add("int_negate", &p_int_negate);
        registry().add("bool_eq", &p_bool_eq);
        registry().add("bool_eq_reif", &p_bool_eq_reif);
        registry().add("bool_eq_imp", &p_bool_eq_imp);
        registry().add("bool_ne", &p_bool_ne);
        registry().add("bool_ne_reif", &p_bool_ne_reif);
        registry().add("bool_ne_imp", &p_bool_ne_imp);
        registry().add("bool_ge", &p_bool_ge);
        registry().add("bool_ge_reif", &p_bool_ge_reif);
        registry().add("bool_ge_imp", &p_bool_ge_imp);
        registry().add("bool_le", &p_bool_le);
        registry().add("bool_le_reif", &p_bool_le_reif);
        registry().add("bool_le_imp", &p_bool_le_imp);
        registry().add("bool_gt", &p_bool_gt);
        registry().add("bool_gt_reif", &p_bool_gt_reif);
        registry().add("bool_gt_imp", &p_bool_gt_imp);
        registry().add("bool_lt", &p_bool_lt);
        registry().add("bool_lt_reif", &p_bool_lt_reif);
        registry().add("bool_lt_imp", &p_bool_lt_imp);
        registry().add("bool_or", &p_bool_or);
        registry().add("bool_or_imp", &p_bool_or_imp);
        registry().add("bool_and", &p_bool_and);
        registry().add("bool_and_imp", &p_bool_and_imp);
        registry().add("bool_xor", &p_bool_xor);
        registry().add("bool_xor_imp", &p_bool_xor_imp);
        registry().add("array_bool_and", &p_array_bool_and);
        registry().add("array_bool_and_imp", &p_array_bool_and_imp);
        registry().add("array_bool_or", &p_array_bool_or);
        registry().add("array_bool_or_imp", &p_array_bool_or_imp);
        registry().add("array_bool_xor", &p_array_bool_xor);
        registry().add("array_bool_xor_imp", &p_array_bool_xor_imp);
        registry().add("bool_clause", &p_array_bool_clause);
        registry().add("bool_clause_reif", &p_array_bool_clause_reif);
        registry().add("bool_clause_imp", &p_array_bool_clause_imp);
        registry().add("bool_left_imp", &p_bool_l_imp);
        registry().add("bool_right_imp", &p_bool_r_imp);
        registry().add("bool_not", &p_bool_not);
        registry().add("array_int_element", &p_array_int_element);
        registry().add("array_var_int_element", &p_array_int_element);
        registry().add("array_bool_element", &p_array_bool_element);
        registry().add("array_var_bool_element", &p_array_bool_element);
        registry().add("bool2int", &p_bool2int);
        registry().add("int_in", &p_int_in);
        registry().add("int_in_reif", &p_int_in_reif);
        registry().add("int_in_imp", &p_int_in_imp);
#ifndef GECODE_HAS_SET_VARS
        registry().add("set_in", &p_int_in);
        registry().add("set_in_reif", &p_int_in_reif);
        registry().add("set_in_imp", &p_int_in_imp);
#endif
      
        registry().add("array_int_lt", &p_array_int_lt);
        registry().add("array_int_lq", &p_array_int_lq);
        registry().add("array_bool_lt", &p_array_bool_lt);
        registry().add("array_bool_lq", &p_array_bool_lq);
        registry().add("count", &p_count);
        registry().add("count_reif", &p_count_reif);
        registry().add("count_imp", &p_count_imp);
        registry().add("at_least_int", &p_at_least);
        registry().add("at_most_int", &p_at_most);
        registry().add("gecode_bin_packing_load", &p_bin_packing_load);
        registry().add("global_cardinality", &p_global_cardinality);
        registry().add("global_cardinality_closed",
          &p_global_cardinality_closed);
        registry().add("global_cardinality_low_up", 
          &p_global_cardinality_low_up);
        registry().add("global_cardinality_low_up_closed", 
          &p_global_cardinality_low_up_closed);
        registry().add("minimum_int", &p_minimum);
        registry().add("maximum_int", &p_maximum);
        registry().add("regular", &p_regular);
        registry().add("sort", &p_sort);
        registry().add("inverse_offsets", &p_inverse_offsets);
        registry().add("increasing_int", &p_increasing_int);
        registry().add("increasing_bool", &p_increasing_bool);
        registry().add("decreasing_int", &p_decreasing_int);
        registry().add("decreasing_bool", &p_decreasing_bool);
        registry().add("table_int", &p_table_int);
        registry().add("table_bool", &p_table_bool);
        registry().add("cumulatives", &p_cumulatives);
        registry().add("gecode_among_seq_int", &p_among_seq_int);
        registry().add("gecode_among_seq_bool", &p_among_seq_bool);

        registry().add("bool_lin_eq", &p_bool_lin_eq);
        registry().add("bool_lin_ne", &p_bool_lin_ne);
        registry().add("bool_lin_le", &p_bool_lin_le);
        registry().add("bool_lin_lt", &p_bool_lin_lt);
        registry().add("bool_lin_ge", &p_bool_lin_ge);
        registry().add("bool_lin_gt", &p_bool_lin_gt);

        registry().add("bool_lin_eq_reif", &p_bool_lin_eq_reif);
        registry().add("bool_lin_eq_imp", &p_bool_lin_eq_imp);
        registry().add("bool_lin_ne_reif", &p_bool_lin_ne_reif);
        registry().add("bool_lin_ne_imp", &p_bool_lin_ne_imp);
        registry().add("bool_lin_le_reif", &p_bool_lin_le_reif);
        registry().add("bool_lin_le_imp", &p_bool_lin_le_imp);
        registry().add("bool_lin_lt_reif", &p_bool_lin_lt_reif);
        registry().add("bool_lin_lt_imp", &p_bool_lin_lt_imp);
        registry().add("bool_lin_ge_reif", &p_bool_lin_ge_reif);
        registry().add("bool_lin_ge_imp", &p_bool_lin_ge_imp);
        registry().add("bool_lin_gt_reif", &p_bool_lin_gt_reif);
        registry().add("bool_lin_gt_imp", &p_bool_lin_gt_imp);
        
        registry().add("gecode_schedule_unary", &p_schedule_unary);
        registry().add("gecode_schedule_unary_optional", &p_schedule_unary_optional);

        registry().add("gecode_circuit", &p_circuit);
        registry().add("gecode_circuit_cost_array", &p_circuit_cost_array);
        registry().add("gecode_circuit_cost", &p_circuit_cost);
        registry().add("gecode_nooverlap", &p_nooverlap);
        registry().add("gecode_precede", &p_precede);
        registry().add("nvalue",&p_nvalue);
        registry().add("among",&p_among);
        registry().add("member_int",&p_member_int);
        registry().add("gecode_member_int_reif",&p_member_int_reif);
        registry().add("member_bool",&p_member_bool);
        registry().add("gecode_member_bool_reif",&p_member_bool_reif);
      }
    };
    IntPoster __int_poster;

#ifdef GECODE_HAS_SET_VARS
    void p_set_OP(FlatZincSpace& s, SetOpType op,
                  const ConExpr& ce, AST::Node *) {
      rel(s, s.arg2SetVar(ce[0]), op, s.arg2SetVar(ce[1]), 
          SRT_EQ, s.arg2SetVar(ce[2]));
    }
    void p_set_union(FlatZincSpace& s, const ConExpr& ce, AST::Node *ann) {
      p_set_OP(s, SOT_UNION, ce, ann);
    }
    void p_set_intersect(FlatZincSpace& s, const ConExpr& ce, AST::Node *ann) {
      p_set_OP(s, SOT_INTER, ce, ann);
    }
    void p_set_diff(FlatZincSpace& s, const ConExpr& ce, AST::Node *ann) {
      p_set_OP(s, SOT_MINUS, ce, ann);
    }

    void p_set_symdiff(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      SetVar x = s.arg2SetVar(ce[0]);
      SetVar y = s.arg2SetVar(ce[1]);

      SetVarLubRanges xub(x);
      IntSet xubs(xub);
      SetVar x_y(s,IntSet::empty,xubs);
      rel(s, x, SOT_MINUS, y, SRT_EQ, x_y);

      SetVarLubRanges yub(y);
      IntSet yubs(yub);
      SetVar y_x(s,IntSet::empty,yubs);
      rel(s, y, SOT_MINUS, x, SRT_EQ, y_x);
    
      rel(s, x_y, SOT_UNION, y_x, SRT_EQ, s.arg2SetVar(ce[2]));
    }

    void p_array_set_OP(FlatZincSpace& s, SetOpType op,
                        const ConExpr& ce, AST::Node *) {
      SetVarArgs xs = s.arg2setvarargs(ce[0]);
      rel(s, op, xs, s.arg2SetVar(ce[1]));
    }
    void p_array_set_union(FlatZincSpace& s, const ConExpr& ce, AST::Node *ann) {
      p_array_set_OP(s, SOT_UNION, ce, ann);
    }
    void p_array_set_partition(FlatZincSpace& s, const ConExpr& ce, AST::Node *ann) {
      p_array_set_OP(s, SOT_DUNION, ce, ann);
    }


    void p_set_rel(FlatZincSpace& s, SetRelType srt, const ConExpr& ce) {
      rel(s, s.arg2SetVar(ce[0]), srt, s.arg2SetVar(ce[1]));
    }

    void p_set_eq(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel(s, SRT_EQ, ce);
    }
    void p_set_ne(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel(s, SRT_NQ, ce);
    }
    void p_set_subset(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel(s, SRT_SUB, ce);
    }
    void p_set_superset(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel(s, SRT_SUP, ce);
    }
    void p_set_le(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel(s, SRT_LQ, ce);
    }
    void p_set_lt(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel(s, SRT_LE, ce);
    }
    void p_set_card(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      if (!ce[1]->isIntVar()) {
        cardinality(s, s.arg2SetVar(ce[0]), ce[1]->getInt(), 
                    ce[1]->getInt());
      } else {
        cardinality(s, s.arg2SetVar(ce[0]), s.arg2IntVar(ce[1]));
      }
    }
    void p_set_in(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      if (!ce[1]->isSetVar()) {
        IntSet d = s.arg2intset(ce[1]);
        if (ce[0]->isBoolVar()) {
          IntSetRanges dr(d);
          Iter::Ranges::Singleton sr(0,1);
          Iter::Ranges::Inter<IntSetRanges,Iter::Ranges::Singleton> i(dr,sr);
          IntSet d01(i);
          if (d01.size() == 0) {
            s.fail();
          } else {
            rel(s, s.arg2BoolVar(ce[0]), IRT_GQ, d01.min());
            rel(s, s.arg2BoolVar(ce[0]), IRT_LQ, d01.max());
          }
        } else {
          dom(s, s.arg2IntVar(ce[0]), d);
        }
      } else {
        if (!ce[0]->isIntVar()) {
          dom(s, s.arg2SetVar(ce[1]), SRT_SUP, ce[0]->getInt());
        } else {
          rel(s, s.arg2SetVar(ce[1]), SRT_SUP, s.arg2IntVar(ce[0]));
        }
      }
    }
    void p_set_rel_reif(FlatZincSpace& s, SetRelType srt, const ConExpr& ce) {
      rel(s, s.arg2SetVar(ce[0]), srt, s.arg2SetVar(ce[1]),
          s.arg2BoolVar(ce[2]));
    }

    void p_set_eq_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel_reif(s,SRT_EQ,ce);
    }
    void p_set_le_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel_reif(s,SRT_LQ,ce);
    }
    void p_set_lt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel_reif(s,SRT_LE,ce);
    }
    void p_set_ne_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      p_set_rel_reif(s,SRT_NQ,ce);
    }
    void p_set_subset_reif(FlatZincSpace& s, const ConExpr& ce,
                           AST::Node *) {
      p_set_rel_reif(s,SRT_SUB,ce);
    }
    void p_set_superset_reif(FlatZincSpace& s, const ConExpr& ce,
                             AST::Node *) {
      p_set_rel_reif(s,SRT_SUP,ce);
    }
    void p_set_in_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann, ReifyMode rm) {
      if (!ce[1]->isSetVar()) {
        if (rm==RM_EQV) {
          p_int_in_reif(s,ce,ann);
        } else {
          assert(rm==RM_IMP);
          p_int_in_imp(s,ce,ann);
        }
      } else {
        if (!ce[0]->isIntVar()) {
          dom(s, s.arg2SetVar(ce[1]), SRT_SUP, ce[0]->getInt(),
              Reify(s.arg2BoolVar(ce[2]),rm));
        } else {
          rel(s, s.arg2SetVar(ce[1]), SRT_SUP, s.arg2IntVar(ce[0]),
              Reify(s.arg2BoolVar(ce[2]),rm));
        }
      }
    }
    void p_set_in_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_set_in_reif(s,ce,ann,RM_EQV);
    }
    void p_set_in_imp(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_set_in_reif(s,ce,ann,RM_IMP);
    }
    void p_set_disjoint(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      rel(s, s.arg2SetVar(ce[0]), SRT_DISJ, s.arg2SetVar(ce[1]));
    }
    
    void p_link_set_to_booleans(FlatZincSpace& s, const ConExpr& ce,
                                AST::Node *) {
      SetVar x = s.arg2SetVar(ce[0]);
      int idx = ce[2]->getInt();
      assert(idx >= 0);
      rel(s, x || IntSet(Set::Limits::min,idx-1));
      BoolVarArgs y = s.arg2boolvarargs(ce[1],idx);
      unshare(s, y);
      channel(s, y, x);
    }

    void p_array_set_element(FlatZincSpace& s, const ConExpr& ce,
                             AST::Node*) {
      bool isConstant = true;
      AST::Array* a = ce[1]->getArray();
      for (int i=a->a.size(); i--;) {
        if (a->a[i]->isSetVar()) {
          isConstant = false;
          break;
        }
      }
      IntVar selector = s.arg2IntVar(ce[0]);
      rel(s, selector > 0);
      if (isConstant) {
        IntSetArgs sv = s.arg2intsetargs(ce[1],1);
        element(s, sv, selector, s.arg2SetVar(ce[2]));
      } else {
        SetVarArgs sv = s.arg2setvarargs(ce[1], 1);
        element(s, sv, selector, s.arg2SetVar(ce[2]));
      }
    }

    void p_array_set_element_op(FlatZincSpace& s, const ConExpr& ce,
                                AST::Node*, SetOpType op,
                                const IntSet& universe = 
                                IntSet(Set::Limits::min,Set::Limits::max)) {
      bool isConstant = true;
      AST::Array* a = ce[1]->getArray();
      for (int i=a->a.size(); i--;) {
        if (a->a[i]->isSetVar()) {
          isConstant = false;
          break;
        }
      }
      SetVar selector = s.arg2SetVar(ce[0]);
      dom(s, selector, SRT_DISJ, 0);
      if (isConstant) {
        IntSetArgs sv = s.arg2intsetargs(ce[1], 1);
        element(s, op, sv, selector, s.arg2SetVar(ce[2]), universe);
      } else {
        SetVarArgs sv = s.arg2setvarargs(ce[1], 1);
        element(s, op, sv, selector, s.arg2SetVar(ce[2]), universe);
      }
    }

    void p_array_set_element_union(FlatZincSpace& s, const ConExpr& ce,
                                       AST::Node* ann) {
      p_array_set_element_op(s, ce, ann, SOT_UNION);
    }

    void p_array_set_element_intersect(FlatZincSpace& s, const ConExpr& ce,
                                       AST::Node* ann) {
      p_array_set_element_op(s, ce, ann, SOT_INTER);
    }

    void p_array_set_element_intersect_in(FlatZincSpace& s,
                                              const ConExpr& ce,
                                              AST::Node* ann) {
      IntSet d = s.arg2intset(ce[3]);
      p_array_set_element_op(s, ce, ann, SOT_INTER, d);
    }

    void p_array_set_element_partition(FlatZincSpace& s, const ConExpr& ce,
                                           AST::Node* ann) {
      p_array_set_element_op(s, ce, ann, SOT_DUNION);
    }

    void p_set_convex(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      convex(s, s.arg2SetVar(ce[0]));
    }

    void p_array_set_seq(FlatZincSpace& s, const ConExpr& ce, AST::Node *) {
      SetVarArgs sv = s.arg2setvarargs(ce[0]);
      sequence(s, sv);
    }

    void p_array_set_seq_union(FlatZincSpace& s, const ConExpr& ce,
                               AST::Node *) {
      SetVarArgs sv = s.arg2setvarargs(ce[0]);
      sequence(s, sv, s.arg2SetVar(ce[1]));
    }

    void p_int_set_channel(FlatZincSpace& s, const ConExpr& ce,
                           AST::Node *) {
      int xoff=ce[1]->getInt();
      assert(xoff >= 0);
      int yoff=ce[3]->getInt();
      assert(yoff >= 0);
      IntVarArgs xv = s.arg2intvarargs(ce[0], xoff);
      SetVarArgs yv = s.arg2setvarargs(ce[2], yoff, 1, IntSet(0, xoff-1));
      IntSet xd(yoff,yv.size()-1);
      for (int i=xoff; i<xv.size(); i++) {
        dom(s, xv[i], xd);
      }
      IntSet yd(xoff,xv.size()-1);
      for (int i=yoff; i<yv.size(); i++) {
        dom(s, yv[i], SRT_SUB, yd);
      }
      channel(s,xv,yv);
    }
    
    void p_range(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      int xoff=ce[1]->getInt();
      assert(xoff >= 0);
      IntVarArgs xv = s.arg2intvarargs(ce[0],xoff);
      element(s, SOT_UNION, xv, s.arg2SetVar(ce[2]), s.arg2SetVar(ce[3]));
    }
    
    void p_weights(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      IntArgs e = s.arg2intargs(ce[0]);
      IntArgs w = s.arg2intargs(ce[1]);
      SetVar x = s.arg2SetVar(ce[2]);
      IntVar y = s.arg2IntVar(ce[3]);
      weights(s,e,w,x,y);
    }
    
    void p_inverse_set(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      int xoff = ce[2]->getInt();
      int yoff = ce[3]->getInt();
      SetVarArgs x = s.arg2setvarargs(ce[0],xoff);
      SetVarArgs y = s.arg2setvarargs(ce[1],yoff);
      channel(s, x, y);
    }

    void p_precede_set(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      SetVarArgs x = s.arg2setvarargs(ce[0]);
      int p_s = ce[1]->getInt();
      int p_t = ce[2]->getInt();
      precede(s,x,p_s,p_t);
    }
    
    class SetPoster {
    public:
      SetPoster(void) {
        registry().add("set_eq", &p_set_eq);
        registry().add("set_le", &p_set_le);
        registry().add("set_lt", &p_set_lt);
        registry().add("equal", &p_set_eq);
        registry().add("set_ne", &p_set_ne);
        registry().add("set_union", &p_set_union);
        registry().add("array_set_element", &p_array_set_element);
        registry().add("array_var_set_element", &p_array_set_element);
        registry().add("set_intersect", &p_set_intersect);
        registry().add("set_diff", &p_set_diff);
        registry().add("set_symdiff", &p_set_symdiff);
        registry().add("set_subset", &p_set_subset);
        registry().add("set_superset", &p_set_superset);
        registry().add("set_card", &p_set_card);
        registry().add("set_in", &p_set_in);
        registry().add("set_eq_reif", &p_set_eq_reif);
        registry().add("set_le_reif", &p_set_le_reif);
        registry().add("set_lt_reif", &p_set_lt_reif);
        registry().add("equal_reif", &p_set_eq_reif);
        registry().add("set_ne_reif", &p_set_ne_reif);
        registry().add("set_subset_reif", &p_set_subset_reif);
        registry().add("set_superset_reif", &p_set_superset_reif);
        registry().add("set_in_reif", &p_set_in_reif);
        registry().add("set_in_imp", &p_set_in_imp);
        registry().add("disjoint", &p_set_disjoint);
        registry().add("gecode_link_set_to_booleans", 
                       &p_link_set_to_booleans);

        registry().add("array_set_union", &p_array_set_union);
        registry().add("array_set_partition", &p_array_set_partition);
        registry().add("set_convex", &p_set_convex);
        registry().add("array_set_seq", &p_array_set_seq);
        registry().add("array_set_seq_union", &p_array_set_seq_union);
        registry().add("gecode_array_set_element_union", 
                       &p_array_set_element_union);
        registry().add("gecode_array_set_element_intersect", 
                       &p_array_set_element_intersect);
        registry().add("gecode_array_set_element_intersect_in", 
                       &p_array_set_element_intersect_in);
        registry().add("gecode_array_set_element_partition", 
                       &p_array_set_element_partition);
        registry().add("gecode_int_set_channel", 
                       &p_int_set_channel);
        registry().add("gecode_range",
                       &p_range);
        registry().add("gecode_set_weights",
                       &p_weights);
        registry().add("gecode_inverse_set", &p_inverse_set);
        registry().add("gecode_precede_set", &p_precede_set);
      }
    };
    SetPoster __set_poster;
#endif

#ifdef GECODE_HAS_FLOAT_VARS

    void p_int2float(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      IntVar x0 = s.arg2IntVar(ce[0]);
      FloatVar x1 = s.arg2FloatVar(ce[1]);
      channel(s, x0, x1);
    }

    void p_float_lin_cmp(FlatZincSpace& s, FloatRelType frt,
                         const ConExpr& ce, AST::Node*) {
      FloatValArgs fa = s.arg2floatargs(ce[0]);
      FloatVarArgs fv = s.arg2floatvarargs(ce[1]);
      linear(s, fa, fv, frt, ce[2]->getFloat());
    }
    void p_float_lin_cmp_reif(FlatZincSpace& s, FloatRelType frt,
                              const ConExpr& ce, AST::Node*) {
      FloatValArgs fa = s.arg2floatargs(ce[0]);
      FloatVarArgs fv = s.arg2floatvarargs(ce[1]);
      linear(s, fa, fv, frt, ce[2]->getFloat(), s.arg2BoolVar(ce[3]));
    }
    void p_float_lin_eq(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_float_lin_cmp(s,FRT_EQ,ce,ann);
    }
    void p_float_lin_eq_reif(FlatZincSpace& s, const ConExpr& ce,
                             AST::Node* ann) {
      p_float_lin_cmp_reif(s,FRT_EQ,ce,ann);
    }
    void p_float_lin_le(FlatZincSpace& s, const ConExpr& ce, AST::Node* ann) {
      p_float_lin_cmp(s,FRT_LQ,ce,ann);
    }
    void p_float_lin_le_reif(FlatZincSpace& s, const ConExpr& ce,
                             AST::Node* ann) {
      p_float_lin_cmp_reif(s,FRT_LQ,ce,ann);
    }

    void p_float_times(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      FloatVar z = s.arg2FloatVar(ce[2]);
      mult(s,x,y,z);
    }

    void p_float_div(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      FloatVar z = s.arg2FloatVar(ce[2]);
      div(s,x,y,z);
    }

    void p_float_plus(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      FloatVar z = s.arg2FloatVar(ce[2]);
      rel(s,x+y==z);
    }

    void p_float_sqrt(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      sqrt(s,x,y);
    }

    void p_float_abs(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      abs(s,x,y);
    }

    void p_float_eq(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      rel(s,x,FRT_EQ,y);
    }
    void p_float_eq_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      BoolVar  b = s.arg2BoolVar(ce[2]);
      rel(s,x,FRT_EQ,y,b);
    }
    void p_float_le(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      rel(s,x,FRT_LQ,y);
    }
    void p_float_le_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      BoolVar  b = s.arg2BoolVar(ce[2]);
      rel(s,x,FRT_LQ,y,b);
    }
    void p_float_max(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      FloatVar z = s.arg2FloatVar(ce[2]);
      max(s,x,y,z);
    }
    void p_float_min(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      FloatVar z = s.arg2FloatVar(ce[2]);
      min(s,x,y,z);
    }
    void p_float_lt(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      rel(s, x, FRT_LQ, y);
      rel(s, x, FRT_EQ, y, BoolVar(s,0,0));
    }

    void p_float_lt_reif(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      BoolVar b = s.arg2BoolVar(ce[2]);
      BoolVar b0(s,0,1);
      BoolVar b1(s,0,1);
      rel(s, b == (b0 && !b1));
      rel(s, x, FRT_LQ, y, b0);
      rel(s, x, FRT_EQ, y, b1);
    }

    void p_float_ne(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      rel(s, x, FRT_EQ, y, BoolVar(s,0,0));
    }

#ifdef GECODE_HAS_MPFR
#define P_FLOAT_OP(Op) \
    void p_float_ ## Op (FlatZincSpace& s, const ConExpr& ce, AST::Node*) {\
      FloatVar x = s.arg2FloatVar(ce[0]);\
      FloatVar y = s.arg2FloatVar(ce[1]);\
      Op(s,x,y);\
    }
    P_FLOAT_OP(acos)
    P_FLOAT_OP(asin)
    P_FLOAT_OP(atan)
    P_FLOAT_OP(cos)
    P_FLOAT_OP(exp)
    P_FLOAT_OP(sin)
    P_FLOAT_OP(tan)
    // P_FLOAT_OP(sinh)
    // P_FLOAT_OP(tanh)
    // P_FLOAT_OP(cosh)
#undef P_FLOAT_OP

    void p_float_ln(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      log(s,x,y);
    }
    void p_float_log10(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      log(s,10.0,x,y);
    }
    void p_float_log2(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      FloatVar x = s.arg2FloatVar(ce[0]);
      FloatVar y = s.arg2FloatVar(ce[1]);
      log(s,2.0,x,y);
    }

#endif

    class FloatPoster {
    public:
      FloatPoster(void) {
        registry().add("int2float",&p_int2float);
        registry().add("float_abs",&p_float_abs);
        registry().add("float_sqrt",&p_float_sqrt);
        registry().add("float_eq",&p_float_eq);
        registry().add("float_eq_reif",&p_float_eq_reif);
        registry().add("float_le",&p_float_le);
        registry().add("float_le_reif",&p_float_le_reif);
        registry().add("float_lt",&p_float_lt);
        registry().add("float_lt_reif",&p_float_lt_reif);
        registry().add("float_ne",&p_float_ne);
        registry().add("float_times",&p_float_times);
        registry().add("float_div",&p_float_div);
        registry().add("float_plus",&p_float_plus);
        registry().add("float_max",&p_float_max);
        registry().add("float_min",&p_float_min);
        
        registry().add("float_lin_eq",&p_float_lin_eq);
        registry().add("float_lin_eq_reif",&p_float_lin_eq_reif);
        registry().add("float_lin_le",&p_float_lin_le);
        registry().add("float_lin_le_reif",&p_float_lin_le_reif);
        
#ifdef GECODE_HAS_MPFR
        registry().add("float_acos",&p_float_acos);
        registry().add("float_asin",&p_float_asin);
        registry().add("float_atan",&p_float_atan);
        registry().add("float_cos",&p_float_cos);
        // registry().add("float_cosh",&p_float_cosh);
        registry().add("float_exp",&p_float_exp);
        registry().add("float_ln",&p_float_ln);
        registry().add("float_log10",&p_float_log10);
        registry().add("float_log2",&p_float_log2);
        registry().add("float_sin",&p_float_sin);
        // registry().add("float_sinh",&p_float_sinh);
        registry().add("float_tan",&p_float_tan);
        // registry().add("float_tanh",&p_float_tanh);
#endif
      }
    } __float_poster;
#endif

  }
}}

// STATISTICS: flatzinc-any
