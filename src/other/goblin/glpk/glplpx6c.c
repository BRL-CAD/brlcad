/* glplpx6c.c (branch-and-bound solver routine) */

/*----------------------------------------------------------------------
-- Copyright (C) 2000, 2001, 2002, 2003 Andrew Makhorin, Department
-- for Applied Informatics, Moscow Aviation Institute, Moscow, Russia.
-- All rights reserved. E-mail: <mao@mai2.rcnet.ru>.
--
-- This file is a part of GLPK (GNU Linear Programming Kit).
--
-- GLPK is free software; you can redistribute it and/or modify it
-- under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2, or (at your option)
-- any later version.
--
-- GLPK is distributed in the hope that it will be useful, but WITHOUT
-- ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
-- or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
-- License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with GLPK; see the file COPYING. If not, write to the Free
-- Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
-- 02111-1307, USA.
----------------------------------------------------------------------*/

#include <float.h>
#include <math.h>
#include <stddef.h>
#include "glplib.h"
#include "glpmip.h"

/*----------------------------------------------------------------------
-- branch_first - choose first branching variable.
--
-- This routine looks up the list of structural variables and chooses
-- the first one, which is of integer kind and has fractional value in
-- an optimal solution of the current LP relaxation.
--
-- This routine also selects a branch to be solved next, where integer
-- infeasibility of the chosen variable is less than in other one. */

static void branch_first(MIPTREE *tree)
{     int n, j;
      double beta;
      insist(tree->j_br == 0);
      insist(tree->heir == 0);
      /* choose a variable to branch on */
      n = tree->orig_n;
      for (j = 1; j <= n; j++)
      {  if (tree->col[j]->infeas)
         {  tree->j_br = j;
            break;
         }
      }
      insist(1 <= tree->j_br && tree->j_br <= n);
      /* obtain a value of the chosen variable */
      ies_get_col_info(tree->tree, tree->col[tree->j_br]->col, NULL,
         &beta, NULL);
      /* select a branch to be solved next */
      if (beta - floor(beta) < ceil(beta) - beta)
         tree->heir = 1; /* down branch */
      else
         tree->heir = 2; /* up branch */
      return;
}

/*----------------------------------------------------------------------
-- branch_last - choose last branching variable.
--
-- This routine looks up the list of structural variables and chooses
-- the last one, which is of integer kind and has fractional value in
-- an optimal solution of the current LP relaxation.
--
-- This routine also selects a branch to be solved next, where integer
-- infeasibility of the chosen variable is less than in other one. */

static void branch_last(MIPTREE *tree)
{     int n, j;
      double beta;
      insist(tree->j_br == 0);
      insist(tree->heir == 0);
      /* choose a variable to branch on */
      n = tree->orig_n;
      for (j = n; j >= 1; j--)
      {  if (tree->col[j]->infeas)
         {  tree->j_br = j;
            break;
         }
      }
      insist(1 <= tree->j_br && tree->j_br <= n);
      /* obtain a value of the chosen variable */
      ies_get_col_info(tree->tree, tree->col[tree->j_br]->col, NULL,
         &beta, NULL);
      /* select a branch to be solved next */
      if (beta - floor(beta) < ceil(beta) - beta)
         tree->heir = 1; /* down branch */
      else
         tree->heir = 2; /* up branch */
      return;
}

/*----------------------------------------------------------------------
-- branch_drtom - choose branch using Driebeek-Tomlin heuristic.
--
-- This routine looks up the list of structural variables and chooses
-- one, which is of integer kind and has fractional value in an optimal
-- solution of the current LP relaxation, using heuristic proposed by
-- Driebeek and Tomlin.
--
-- This routine also selects a branch to be solved next, which has the
-- largest estimated degradation of the objective function.
--
-- *References*
--
-- This routine is based on the heuristic proposed in:
--
-- Driebeek N.J. An algorithm for the solution of mixed-integer
-- programming problems, Management Science, 12: 576-87 (1966)
--
-- and improved in:
--
-- Tomlin J.A. Branch and bound methods for integer and non-convex
-- programming, in J.Abadie (ed.), Integer and Nonlinear Programming,
-- North-Holland, Amsterdam, pp. 437-50 (1970).
--
-- Note that this heuristic is time-expensive, because computing
-- one-step degradation (see the routine below) for each basic integer
-- structural variable requires one BTRAN. */

static void branch_drtom(MIPTREE *tree)
{     LPX *lp;
      int m, n, how, j, jj, qq, t, tagx, len, *ndx;
      double alfa, beta, delta_j, delta_q, delta_z, dq, dn_z, up_z,
         degrad = -1.0, *val;
      insist(tree->j_br == 0);
      insist(tree->heir == 0);
      /* access the internal LP object */
      lp = ies_get_lp_object(tree->tree);
      insist(lpx_get_status(lp) == LPX_OPT);
      /* determine number of rows and columns */
      m = lpx_get_num_rows(lp);
      n = lpx_get_num_cols(lp);
      /* allocate working arrays */
      ndx = ucalloc(1+n, sizeof(int));
      val = ucalloc(1+n, sizeof(double));
      /* look up the list of structural variables */
      for (j = 1; j <= n; j++)
      {  /* if j-th variable is either continuous or integer feasible,
            skip it */
         if (!tree->col[j]->infeas) continue;
         /* obtain a value of j-th variable in an optimal solution of
            the current LP relaxation */
         ies_get_col_info(tree->tree, tree->col[j]->col, NULL, &beta,
            NULL);
         /* determine ordinal number of j-th variable in the internal
            LP object */
         jj = ies_get_col_bind(tree->tree, tree->col[j]->col);
         /* j-th variable is integer infeasible, therefore it is basic;
            compute the corresponding row of the simplex table */
         len = lpx_eval_tab_row(lp, m + jj, ndx, val);
         /* the following fragment computes a change in the objective
            function delta Z = new Z - old Z, where old Z is a value of
            the objective function in the current optimal basis, new Z
            is its value in an adjacent basis, for two cases:
            a) when new upper bound ub = floor(x[j]) is introduced for
               j-th variable;
            b) when new lower bound lb = ceil(x[j]) is introduced for
               j-th variable;
            since in both cases the current basic solution remaining
            dual feasible becomes primal infeasible, one implicit dual
            simplex iteration is performed in order to determine the
            change delta Z;
            new Z, which is never better than old Z, is a lower (in
            case of minimization) or an upper (in case of minimization)
            bound of the objective if the LP with modified bounds of
            j-th variable would be solved to optimality (i.e. if j-th
            variable would be chosen to branch on) */
         for (how = -1; how <= +1; how += 2)
         {  /* if how < 0, the new upper bound of x[j] is introduced;
               in this case x[j] should decrease in order to leave the
               basis and go to its new upper bound */
            /* if how > 0, the new lower bound of x[j] is introduced;
               in this case x[j] should increase in order to leave the
               basis and go to its new lower bound */
            /* apply the dual ratio test in order to determine which
               auxiliary or structural variable should enter the basis
               in order to keep dual feasibility */
            qq = lpx_dual_ratio_test(lp, len, ndx, val, how, 1e-8);
            /* if the choice of non-basic variable cannot be made, the
               modified LP being dual unbounded and primal infeasible
               has no primal feasible solution; in this case the change
               delta Z is formally set to infinity */
            if (qq == 0)
            {  delta_z = (tree->dir == LPX_MIN ? +DBL_MAX : -DBL_MAX);
               goto skip;
            }
            /* a row of the current simplex table, which corresponds to
               the chosen non-basic variable x[q], is the following:
                  x[j] = ... + alfa * x[q] + ...
               where alfa is an influence coefficient */
            /* determine the coefficient alfa */
            for (t = 1; t <= len; t++) if (ndx[t] == qq) break;
            insist(1 <= t && t <= len);
            alfa = val[t];
            /* since in the adjacent basis the variable x[j] becomes
               non-basic, knowing its value in the current basis we can
               determine its change delta x[j] = new x[j] - old x[j] */
            delta_j = (how < 0 ? floor(beta) : ceil(beta)) - beta;
            /* and knowing the coefficient alfa we can determine the
               corresponding change x[q] = new x[q] - old x[q], where
               old x[q] is a value of x[q] in the current basis, and
               new x[q] is a value of x[q] in the adjacent basis */
            delta_q = delta_j / alfa;
            /* Tomlin noticed that if the variable x[q] is of integer
               kind, its change cannot be less than one in magnitude */
            if (qq > m)
            {  /* x[q] is structural variable */
               MIPCOL *col;
               col = ies_get_item_link(tree->tree,
                  ies_get_jth_col(tree->tree, qq - m));
               if (col->intvar)
               {  /* x[q] is of integer kind */
                  /* we should avoid rounding delta x[q] if it is close
                     to the nearest integer */
                  if (fabs(delta_q - floor(delta_q + 0.5)) > 1e-3)
                  {  if (delta_q > 0.0)
                        delta_q = ceil(delta_q);  /* +3.14 -> +4 */
                     else
                        delta_q = floor(delta_q); /* -3.14 -> -4 */
                  }
               }
            }
            /* determine reduced cost of x[q] in the current basis */
            if (qq <= m)
               lpx_get_row_info(lp, qq, &tagx, NULL, &dq);
            else
               lpx_get_col_info(lp, qq - m, &tagx, NULL, &dq);
            /* if the current basic solution is dual degenerative,
               some reduced costs that are close to zero may have wrong
               signs; so, correct the sign of d[q], if necessary */
            switch (tree->dir)
            {  case LPX_MIN:
                  if (tagx == LPX_NL && dq < 0.0 ||
                      tagx == LPX_NU && dq > 0.0 ||
                      tagx == LPX_NF) dq = 0.0;
                  break;
               case LPX_MAX:
                  if (tagx == LPX_NL && dq > 0.0 ||
                      tagx == LPX_NU && dq < 0.0 ||
                      tagx == LPX_NF) dq = 0.0;
                  break;
               default:
                  insist(tree->dir != tree->dir);
            }
            /* now knowing the change of x[q] and its reduced cost d[q]
               we can compute the corresponding change in the objective
               function delta Z = new Z - old Z */
            delta_z = dq * delta_q;
skip:       /* new Z is never better than old Z, therefore the change
               delta Z is always non-negative (in case of minimization)
               or non-positive (in case of maximization) */
            switch (tree->dir)
            {  case LPX_MIN: insist(delta_z >= 0.0); break;
               case LPX_MAX: insist(delta_z <= 0.0); break;
            }
            /* save absolute value of delta_z for down and up branches
               respectively */
            if (how < 0)
               dn_z = fabs(delta_z);
            else
               up_z = fabs(delta_z);
         }
         /* following Driebeek-Tomlin heuristic we choose a branching
            variable, which provides the largest degradation of the
            objective function in the corresponding down or up branch,
            using the absolute value of delta z as an estimation of
            such degradation; besides, we select a branch with smaller
            degradation to be solved next, keeping other branch with
            larger degradation in the enumeration tree in the hope to
            reduce number of further backtrackings */
         if (degrad < dn_z || degrad < up_z)
         {  tree->j_br = j;
            if (dn_z < up_z)
            {  /* select down branch to be solved next */
               tree->heir = 1;
               degrad = up_z;
            }
            else
            {  /* select up branch to be solved next */
               tree->heir = 2;
               degrad = dn_z;
            }
            /* if down or up branch has no primal feasible solution,
               we needn't to consider other candidates (in principle,
               that branch could be pruned right now) */
            if (degrad == DBL_MAX) break;
         }
      }
      /* free working arrays */
      ufree(ndx);
      ufree(val);
      /* and return to the mip driver */
      insist(1 <= tree->j_br && tree->j_br <= n);
      insist(tree->heir == 1 || tree->heir == 2);
      return;
}

/*----------------------------------------------------------------------
-- btrack_lifo - select active subproblem using LIFO heuristic.
--
-- This routine selects from the active list a subproblem to be solved
-- next using LIFO (Last-In-First-Out) heuristic (i.e. that one, which
-- was added to the list most recently).
--
-- If this heuristic is used, nodes of the enumeration tree are visited
-- in the depth-first-search manner. */

static void btrack_lifo(MIPTREE *tree)
{     IESNODE *node;
      insist(tree->curr == NULL);
      /* since new subproblems are always added to the end of the list,
         the last subproblem is always active */
      node = ies_get_prev_node(tree->tree, NULL);
      insist(node != NULL);
      insist(ies_get_node_count(tree->tree, node) < 0);
      tree->curr = ies_get_node_link(tree->tree, node);
      return;
}

/*----------------------------------------------------------------------
-- btrack_fifo - select active subproblem using FIFO heiristic.
--
-- This routine selects from the active list a subproblem to be solved
-- next using FIFO (First-In-First-Out) heuristic (i.e. that one, which
-- was added to the list most early).
--
-- If this heuristic is used, nodes of the enumeration tree are visited
-- in the breadth-first-search manner. */

static void btrack_fifo(MIPTREE *tree)
{     IESNODE *node;
      insist(tree->curr == NULL);
      /* look up the list from the beginning to the end and select the
         first active subproblem */
      for (node = ies_get_next_node(tree->tree, NULL); node != NULL;
           node = ies_get_next_node(tree->tree, node))
         if (ies_get_node_count(tree->tree, node) < 0) break;
      insist(node != NULL);
      tree->curr = ies_get_node_link(tree->tree, node);
      return;
}

/*----------------------------------------------------------------------
-- btrack_bestp - select active subproblem using best proj. heuristic.
--
-- This routine selects from the active list a subproblem to be solved
-- next. If no integer feasible solution has been found, the routine
-- selects the subproblem, whose parent subproblem has the best value
-- of the objective function, in which case the tree is expored in the
-- best-first-search manner. If some integer feasible solution has been
-- found yet, the routine selects an active subproblem using the best
-- projection heuristic. */

static void btrack_bestp(MIPTREE *tree)
{     MIPNODE *my_node;
      IESNODE *node, *parent;
      double dir = (tree->dir == LPX_MIN ? +1.0 : -1.0);
      insist(tree->curr == NULL);
      /* access the root subproblem (it is always the first subproblem
         in the list) */
      node = ies_get_next_node(tree->tree, NULL);
      insist(node != NULL);
      my_node = ies_get_node_link(tree->tree, node);
      /* if the root subproblem is active, select it, because it is the
         only subproblem in the tree */
      if (ies_get_node_count(tree->tree, node) < 0)
      {  tree->curr = my_node;
         goto done;
      }
      if (!tree->found)
      {  /* no integer feasible solution has been found */
         double best = DBL_MAX;
         for (node = ies_get_next_node(tree->tree, node); node != NULL;
              node = ies_get_next_node(tree->tree, node))
         {  if (ies_get_node_count(tree->tree, node) < 0)
            {  parent = node->up;
               insist(parent != NULL);
               my_node = ies_get_node_link(tree->tree, parent);
               if (best > dir * my_node->lp_obj)
               {  tree->curr = ies_get_node_link(tree->tree, node);
                  best = dir * my_node->lp_obj;
               }
            }
         }
      }
      else
      {  /* some integer feasible solution has been found */
         double best = DBL_MAX;
         double root_obj = my_node->lp_obj;
         double root_sum = my_node->temp; /* ii_sum */
         for (node = ies_get_next_node(tree->tree, node); node != NULL;
              node = ies_get_next_node(tree->tree, node))
         {  if (ies_get_node_count(tree->tree, node) < 0)
            {  double deg, val;
               parent = node->up;
               insist(parent != NULL);
               my_node = ies_get_node_link(tree->tree, parent);
               /* deg estimates degradation of the objective function
                  per unit of the sum of integer infeasibilities */
               insist(root_sum > 0.0);
               deg = (dir * (tree->best[0] - root_obj)) / root_sum;
               /* val estimates optimal value of the objective function
                  if the sum of integer infeasibilities were zero */
               insist(my_node->temp > 0.0);
               val = dir * my_node->lp_obj + deg * my_node->temp;
               /* select the active subproblem with the best estimated
                  optimal value of the objective function */
               if (best > val)
               {  tree->curr = ies_get_node_link(tree->tree, node);
                  best = val;
               }
            }
         }
      }
done: /* return to the mip driver */
      insist(tree->curr != NULL);
      return;
}

/*----------------------------------------------------------------------
-- appl_proc - event-driven application procedure.
--
-- This routine is called from the mip driver at certain points of the
-- optimization process. */

static void appl_proc(void *info, MIPTREE *tree)
{     LPX *mip = info;
      /* process the current event */
      switch (tree->event)
      {  case MIP_V_SELECT:
            /* active subproblem selection required */
            switch (lpx_get_int_parm(mip, LPX_K_BTRACK))
            {  case 0:
                  /* select using LIFO heuristic */
                  btrack_lifo(tree);
                  break;
               case 1:
                  /* select using FIFO heuristic */
                  btrack_fifo(tree);
                  break;
               case 2:
                  /* select using best projection heuristic */
                  btrack_bestp(tree);
                  break;
               default:
                  insist(mip != mip);
            }
            break;
         case MIP_V_ENDLP:
            /* the simplex has finished solving LP relaxation of the
               current subproblem */
            /* save the sum of integer infeasibilities used in the best
               projection heuristic */
            tree->curr->temp = tree->ii_sum;
            break;
         case MIP_V_BINGO:
            /* better integer feasible solution has been found */
            /* copy components of this solution to the original problem
               object */
            {  int k;
               mip->i_stat = LPX_I_FEAS;
               for (k = 1; k <= tree->orig_m + tree->orig_n; k++)
                  mip->mipx[k] = tree->best[k];
            }
            break;
         case MIP_V_BRANCH:
            /* branching variable selection required */
            switch (lpx_get_int_parm(mip, LPX_K_BRANCH))
            {  case 0:
                  /* branch on first appropriate variable */
                  branch_first(tree);
                  break;
               case 1:
                  /* branch on last appropriate variable */
                  branch_last(tree);
                  break;
               case 2:
                  /* choose branching variable using the heuristic by
                     Driebeek and Tomlin */
                  branch_drtom(tree);
                  break;
               default:
                  insist(mip != mip);
            }
            break;
         default:
            /* ignore other events */
            break;
      }
      /* return to the mip driver */
      return;
}

/*----------------------------------------------------------------------
-- lpx_integer - easy-to-use driver to the branch-and-bound method.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_integer(LPX *mip);
--
-- *Description*
--
-- The routine lpx_integer is intended to solve a MIP problem, which is
-- specified by the parameter mip.
--
-- On entry the problem object should contain an optimal solution of LP
-- relaxation, which can be obtained by the simplex method.
--
-- *Returns*
--
-- The routine lpx_integer returns one of the following exit codes:
--
-- LPX_E_OK       the MIP problem has been successfully solved.
--
-- LPX_E_FAULT    the solver can't start the search because either
--                the problem is not of MIP class, or
--                the problem object doesn't contain optimal solution
--                for LP relaxation, or
--                some integer variable has non-integer lower or upper
--                bound, or
--                some row has non-zero objective coefficient.
--
-- LPX_E_ITLIM    iterations limit exceeded.
--
-- LPX_E_TMLIM    time limit exceeded.
--
-- LPX_E_SING     an error occurred on solving LP relaxation of some
--                subproblem.
--
-- Note that additional exit codes may appear in the future versions of
-- this routine. */

int lpx_integer(LPX *mip)
{     int m = lpx_get_num_rows(mip);
      int n = lpx_get_num_cols(mip);
      MIPTREE *tree;
      LPX *lp;
      int i, j, k, ret;
#     define prefix "lpx_integer: "
      /* the problem must be of MIP class */
      if (lpx_get_class(mip) != LPX_MIP)
      {  print(prefix "problem is not of MIP class");
         ret = LPX_E_FAULT;
         goto done;
      }
      /* an optimal solution of LP relaxation must be known */
      if (lpx_get_status(mip) != LPX_OPT)
      {  print(prefix "optimal solution of LP relaxation required");
         ret = LPX_E_FAULT;
         goto done;
      }
      /* objective coefficients at auxiliary variables must be zero */
      for (i = 1; i <= m; i++)
      {  if (lpx_get_row_coef(mip, i) != 0.0)
         {  print(prefix "row %d has non-zero obj. coefficient", i);
            ret = LPX_E_FAULT;
            goto done;
         }
      }
      /* bounds of all integer variables must be integral */
      for (j = 1; j <= n; j++)
      {  int typx;
         double lb, ub, temp;
         if (lpx_get_col_kind(mip, j) != LPX_IV) continue;
         lpx_get_col_bnds(mip, j, &typx, &lb, &ub);
         if (typx == LPX_LO || typx == LPX_DB || typx == LPX_FX)
         {  temp = floor(lb + 0.5);
            if (fabs(lb - temp) <= 1e-12 * (1.0 + fabs(lb))) lb = temp;
            if (lb != floor(lb))
            {  print(prefix "integer column %d has non-integer lower bo"
                  "und %g", j, lb);
               ret = LPX_E_FAULT;
               goto done;
            }
         }
         if (typx == LPX_UP || typx == LPX_DB)
         {  temp = floor(ub + 0.5);
            if (fabs(ub - temp) <= 1e-12 *(1.0 + fabs(ub))) ub = temp;
            if (ub != floor(ub))
            {  print(prefix "integer column %d has non-integer upper bo"
                  "und %g", j, ub);
               ret = LPX_E_FAULT;
               goto done;
            }
         }
      }
      /* it seems all is ok */
      if (mip->msg_lev >= 2)
         print("Integer optimization begins...");
      /* create the mip solver workspace */
      tree = mip_create_tree(mip, mip, appl_proc);
      /* access the internal LP object */
      lp = ies_get_lp_object(tree->tree);
      /* reflect some control parameters and statistics */
      lp->msg_lev = mip->msg_lev;
      lp->scale = mip->scale;
      lp->sc_ord = mip->sc_ord;
      lp->sc_max = mip->sc_max;
      lp->sc_eps = mip->sc_eps;
      lp->dual = mip->dual;
      lp->price = mip->price;
      lp->relax = mip->relax;
      lp->tol_bnd = mip->tol_bnd;
      lp->tol_dj = mip->tol_dj;
      lp->tol_piv = mip->tol_piv;
      lp->it_cnt = mip->it_cnt;
      tree->msg_lev = mip->msg_lev;
      tree->tol_int = mip->tol_int;
      tree->tol_obj = mip->tol_obj;
      tree->it_lim = mip->it_lim;
      tree->tm_lim = mip->tm_lim;
      /* if the original problem is scaled, scale the internal LP */
      {  int scaled = 0;
         for (k = 1; k <= m+n; k++)
         {  if (mip->rs[k] != 1.0)
            {  scaled = 1;
               break;
            }
         }
         if (scaled) lpx_scale_prob(lp);
      }
      /* reset the status of the mip solution */
      mip->i_stat = LPX_I_UNDEF;
      /* solve the problem */
      ret = mip_driver(tree);
      /* reflect statistics about the spent resources */
      mip->it_cnt = lp->it_cnt;
      mip->it_lim = tree->it_lim;
      mip->tm_lim = tree->tm_lim;
      /* analyze exit code reported by the mip driver */
      switch (ret)
      {  case MIP_E_OK:
            /* the search is finished */
            if (mip->i_stat == LPX_I_FEAS) mip->i_stat = LPX_I_OPT;
            ret = LPX_E_OK;
            break;
         case MIP_E_ITLIM:
            /* iterations limit exceeded */
            ret = LPX_E_ITLIM;
            break;
         case MIP_E_TMLIM:
            /* time limit exceeded */
            ret = LPX_E_TMLIM;
            break;
         case MIP_E_ERROR:
            /* unable to solve LP relaxation */
            ret = LPX_E_SING;
            break;
         default:
            insist(ret != ret);
      }
      /* delete the mip solver workspace */
      mip_delete_tree(tree);
done: /* return to the calling program */
      return ret;
#     undef prefix
}

/* eof */
