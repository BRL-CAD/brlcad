/* glpmip1.c */

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
#include <stdio.h>
#include <string.h>
#include "glplib.h"
#include "glpmip.h"

/*----------------------------------------------------------------------
-- dummy_appl - dummy application procedure.
--
-- This routine is a dummy application procedure, which is used when an
-- actual procedure is not specified. It does nothing. */

static void dummy_appl(void *info, MIPTREE *tree)
{     insist(info == info);
      insist(tree == tree);
      return;
}

/*----------------------------------------------------------------------
-- item_hook - MIP solver item hook routine.
--
-- This routine is called from the Implicit Enumeration Suite if some
-- master row/column is being deleted. */

static void item_hook(void *info, IESITEM *item)
{     MIPTREE *tree = info;
      MIPROW *my_row;
      MIPCOL *my_col;
      switch (ies_what_item(tree->tree, item))
      {  case 'R':
            /* master row is being deleted */
            my_row = ies_get_item_link(tree->tree, item);
            dmp_free_atom(tree->row_pool, my_row);
            break;
         case 'C':
            /* master column is being deleted */
            my_col = ies_get_item_link(tree->tree, item);
            dmp_free_atom(tree->col_pool, my_col);
            break;
         default:
            insist(item != item);
      }
      return;
}

/*----------------------------------------------------------------------
-- node_hook - MIP solver node hook routine.
--
-- This routine is called from the Implicit Enumeration Suite if some
-- node problem is being deleted. */

static void node_hook(void *info, IESNODE *node)
{     MIPTREE *tree = info;
      MIPNODE *my_node;
      /* if an active subproblem is being deleted, treat that as if it
         were solved */
      if (node->count < 0)
      {  if (tree->sn_lim > 0) tree->sn_lim--;
         tree->an_cnt--;
         tree->sn_cnt++;
      }
      my_node = ies_get_node_link(tree->tree, node);
      dmp_free_atom(tree->node_pool, my_node);
      return;
}

/*----------------------------------------------------------------------
-- mip_create_tree - create MIP solver workspace.
--
-- *Synopsis*
--
-- #include "glpmip.h"
-- MIPTREE *mip_create_tree(LPX *mip);
--
-- *Description*
--
-- The routine mip_create_tree creates and initializes the MIP solver
-- workspace for solving a MIP problem specified by the parameter mip.
--
-- All data contained in the original MIP problem, are copied into the
-- workspace, so the problem object, which the parameter mip points to,
-- can then be deleted (if necessary).
--
-- On exit from this routine the workspace contains the root subproblem
-- only, which is identical to the original MIP problem.
--
-- *Returns*
--
-- The routine mip_create_tree returns a pointer to the created solver
-- workspace. */

MIPTREE *mip_create_tree(LPX *mip,
      void *info, void appl(void *info, MIPTREE *tree))
{     MIPTREE *tree;
      MIPROW *row;
      MIPCOL *col;
      MIPNODE *node;
      IESITEM **ref;
      int m, n, dir, i, j, typx, tagx, t, len, *ndx;
      double lb, ub, coef, temp, *val;
      char *name;
      /* determine the size of the original problem */
      m = lpx_get_num_rows(mip);
      n = lpx_get_num_cols(mip);
      /* check if the original problem is non-empty */
      if (!(m > 0 && n > 0))
         fault("mip_create_tree: problem has no rows/columns");
      /* the original problem should be of MIP class */
      if (lpx_get_class(mip) != LPX_MIP)
         fault("mip_create_tree: problem is not of MIP class");
      /* all auxiliary variables should have zero obj. coefficients */
      for (i = 1; i <= m; i++)
      {  coef = lpx_get_row_coef(mip, i);
         if (coef != 0.0)
            fault("mip_create_tree: i = %d; coef = %g; non-zero obj. co"
               "eff. at auxiliary variable not allowed", i, coef);
      }
      /* all integer variables should have integer bounds */
      for (j = 1; j <= n; j++)
      {  if (lpx_get_col_kind(mip, j) == LPX_IV)
         {  lpx_get_col_bnds(mip, j, &typx, &lb, &ub);
            if (typx == LPX_LO || typx == LPX_DB || typx == LPX_FX)
            {  temp = floor(lb + 0.5);
               if (!(fabs(lb - temp) <= 1e-12 * (1.0 + fabs(lb))))
                  fault("mip_create_tree: j = %d; lb = %g; integer vari"
                     "able has non-integer lower bound", j, lb);
            }
            if (typx == LPX_UP || typx == LPX_DB)
            {  temp = floor(ub + 0.5);
               if (!(fabs(ub - temp) <= 1e-12 * (1.0 + fabs(ub))))
                  fault("mip_create_tree: j = %d; ub = %g; integer vari"
                     "able has non-integer upper bound", j, ub);
            }
         }
      }
      /* optimal basis of the initial LP relaxation should be known */
      if (lpx_get_status(mip) != LPX_OPT)
         fault("mip_create_tree: optimal solution of initial LP relaxat"
            "ion required");
      /* create the solver workspace */
      tree = umalloc(sizeof(MIPTREE));
      tree->row_pool = dmp_create_pool(sizeof(MIPROW));
      tree->col_pool = dmp_create_pool(sizeof(MIPCOL));
      tree->node_pool = dmp_create_pool(sizeof(MIPNODE));
      tree->orig_m = m;
      tree->orig_n = n;
      tree->dir = lpx_get_obj_dir(mip);
      tree->info = info;
      tree->appl = (appl != NULL ? appl : dummy_appl);
      tree->event = MIP_V_UNDEF;
      tree->tree = ies_create_tree();
      tree->glob = NULL;
      tree->curr = NULL;
      tree->m_max = m;
      tree->row = ucalloc(1+m, sizeof(MIPROW *));
      tree->col = ucalloc(1+n, sizeof(MIPCOL *));
      tree->reopt = 0;
      tree->e_code = 0;
      tree->better = 0;
      tree->unsat = 0;
      tree->ii_sum = 0.0;
      tree->del = ucalloc(1+m, sizeof(int));
      tree->nrs = 0;
      tree->j_br = 0;
      tree->heir = 0;
      tree->found = 0;
      tree->best = ucalloc(1+m+n, sizeof(double));
      tree->msg_lev = 3;
      tree->tol_int = 1e-6;
      tree->tol_obj = 1e-7;
      tree->it_lim = -1;
      tree->sn_lim = -1;
      tree->tm_lim = -1.0;
      tree->out_frq = 5.0;
      tree->an_cnt = 1;
      tree->sn_cnt = 0;
      tree->tm_last = -DBL_MAX;
      /* install hook routines */
      ies_set_item_hook(tree->tree, tree, item_hook);
      ies_set_node_hook(tree->tree, tree, node_hook);
      /* copy symbolic name of the original problem */
      name = lpx_get_prob_name(mip);
      lpx_set_prob_name(ies_get_lp_object(tree->tree), name);
      /* copy symbolic name of the objective function */
      name = lpx_get_obj_name(mip);
      lpx_set_prob_name(ies_get_lp_object(tree->tree), name);
      /* set optimization direction */
      dir = lpx_get_obj_dir(mip);
      lpx_set_obj_dir(ies_get_lp_object(tree->tree), dir);
      /* create and revive the root subproblem */
      node = tree->glob = dmp_get_atom(tree->node_pool);
      node->node = ies_create_node(tree->tree, NULL);
      ies_set_node_link(tree->tree, node->node, node);
      node->lp_obj = lpx_get_obj_val(mip);
      node->temp = 0.0;
      ies_revive_node(tree->tree, node->node);
      /* load rows of the original problem into the workspace */
      for (i = 1; i <= m; i++)
      {  row = tree->row[i] = dmp_get_atom(tree->row_pool);
         row->i = i;
         name = lpx_get_row_name(mip, i);
         lpx_get_row_bnds(mip, i, &typx, &lb, &ub);
         row->row = ies_add_master_row(tree->tree, name, typx, lb, ub,
            0.0, 0, NULL, NULL);
         ies_set_item_link(tree->tree, row->row, row);
         row->node = node;
      }
      /* load columns and the constraint matrix of the original problem
         into the workspace */
      ref = ucalloc(1+m, sizeof(IESITEM *));
      ndx = ucalloc(1+m, sizeof(int));
      val = ucalloc(1+m, sizeof(double));
      for (j = 1; j <= n; j++)
      {  col = tree->col[j] = dmp_get_atom(tree->col_pool);
         col->j = j;
         name = lpx_get_col_name(mip, j);
         lpx_get_col_bnds(mip, j, &typx, &lb, &ub);
         coef = lpx_get_col_coef(mip, j);
         len = lpx_get_mat_col(mip, j, ndx, val);
         for (t = 1; t <= len; t++) ref[t] = tree->row[ndx[t]]->row;
         col->col = ies_add_master_col(tree->tree, name, typx, lb, ub,
            coef, len, ref, val);
         ies_set_item_link(tree->tree, col->col, col);
         switch (lpx_get_col_kind(mip, j))
         {  case LPX_CV:
               col->intvar = 0; break;
            case LPX_IV:
               col->intvar = 1; break;
            default:
               insist(mip != mip);
         }
         col->infeas = 0;
      }
      ufree(ref);
      ufree(ndx);
      ufree(val);
      /* include all rows and columns of the original problem into the
         root subproblem */
      ref = ucalloc(1+m, sizeof(IESITEM *));
      for (i = 1; i <= m; i++) ref[i] = tree->row[i]->row;
      ies_add_rows(tree->tree, m, ref);
      ufree(ref);
      ref = ucalloc(1+n, sizeof(IESITEM *));
      for (j = 1; j <= n; j++) ref[j] = tree->col[j]->col;
      ies_add_cols(tree->tree, n, ref);
      ufree(ref);
      /* set the constant term of the objective function */
      ies_set_obj_c0(tree->tree, lpx_get_obj_c0(mip));
      /* copy optimal basis of the initial LP relaxation into the root
         subproblem */
      for (i = 1; i <= m; i++)
      {  lpx_get_row_info(mip, i, &tagx, NULL, NULL);
         ies_set_row_stat(tree->tree, tree->row[i]->row, tagx);
      }
      for (j = 1; j <= n; j++)
      {  lpx_get_col_info(mip, j, &tagx, NULL, NULL);
         ies_set_col_stat(tree->tree, tree->col[j]->col, tagx);
      }
      /* return to the calling program */
      return tree;
}

/*----------------------------------------------------------------------
-- display - display visual information.
--
-- This routine displays some visual information about progress of the
-- search, which includes number of performed simplex iterations, value
-- of the objective function for the best known integer feasible
-- solution, value of the objective function for an optimal solution of
-- LP relaxation for the highest-leveled common ancestor of all active
-- and the current subproblem, which defines the global lower (in case
-- of minimization) or upper (in case of maximization) bound for the
-- original MIP problem, number of subproblems in the active list, and
-- number of subproblems that have been solved. */

static void display(MIPTREE *tree)
{     LPX *lp = ies_get_lp_object(tree->tree);
      int it_cnt;
      char mip_obj[50], lp_obj[50];
      it_cnt = lpx_get_int_parm(lp, LPX_K_ITCNT);
      if (!tree->found)
         strcpy(mip_obj, "not found yet");
      else
         sprintf(mip_obj, "%17.9e", tree->best[0]);
      if (tree->glob == NULL)
         strcpy(lp_obj, "tree is empty");
      else
         sprintf(lp_obj, "%17.9e", tree->glob->lp_obj);
      print("+%6d: mip = %17s; lp = %17s (%d, %d)",
         it_cnt, mip_obj, lp_obj, tree->an_cnt, tree->sn_cnt);
      tree->tm_last = utime();
      return;
}

/*----------------------------------------------------------------------
-- rebuild_pointers - rebuild arrays of pointers to rows and columns.
--
-- This routine rebuilds the arrays of pointers to rows and columns of
-- the current subproblem. (These two arrays are used to map original
-- ordinal numbers of rows and columns to internal ordinal numbers used
-- in the internal LP object.) */

static void rebuild_pointers(MIPTREE *tree)
{     LPX *lp = ies_get_lp_object(tree->tree);
      int m = lpx_get_num_rows(lp);
      int n = lpx_get_num_cols(lp);
      MIPROW *row;
      MIPCOL *col;
      IESITEM *item;
      int i, j;
      /* rebuild the array of pointers to rows */
      insist(m <= tree->m_max);
      for (i = 1; i <= m; i++) tree->row[i] = NULL;
      for (i = 1; i <= m; i++)
      {  item = ies_get_ith_row(tree->tree, i);
         row = ies_get_item_link(tree->tree, item);
         insist(1 <= row->i && row->i <= m);
         insist(tree->row[row->i] == NULL);
         tree->row[row->i] = row;
      }
      /* rebuild the array of pointers to columns */
      insist(n == tree->orig_n);
      for (j = 1; j <= n; j++) tree->col[j] = NULL;
      for (j = 1; j <= n; j++)
      {  item = ies_get_jth_col(tree->tree, j);
         col = ies_get_item_link(tree->tree, item);
         insist(1 <= col->j && col->j <= n);
         insist(tree->col[col->j] == NULL);
         tree->col[col->j] = col;
      }
      return;
}

/*----------------------------------------------------------------------
-- solve_subproblem - solve LP relaxation of current subproblem.
--
-- This routine solves LP relaxation of the current subproblem (i.e.
-- the current subproblem where all integer variables are considered as
-- continuous). */

static void solve_subproblem(MIPTREE *tree)
{     LPX *lp = ies_get_lp_object(tree->tree);
      int it_cnt0, it_cnt1;
      lpx_set_int_parm(lp, LPX_K_MSGLEV, 2);
      /* since an initial basis is assumed to be dual feasible, try to
         use the dual simplex (note that the simplex solver can switch
         to the primal simplex in case of numerical instability) */
      lpx_set_int_parm(lp, LPX_K_DUAL, 1);
      /* if the best known integer feasible solution exists, use it as
         a bound of the objective function */
      lpx_set_real_parm(lp, LPX_K_OBJLL, -DBL_MAX);
      lpx_set_real_parm(lp, LPX_K_OBJUL, +DBL_MAX);
      if (tree->found)
      {  switch (tree->dir)
         {  case LPX_MIN:
               lpx_set_real_parm(lp, LPX_K_OBJUL, tree->best[0]);
               break;
            case LPX_MAX:
               lpx_set_real_parm(lp, LPX_K_OBJLL, tree->best[0]);
               break;
            default:
               insist(tree->dir != tree->dir);
         }
      }
      /* delay output from the simplex method for first 10 seconds */
      lpx_set_real_parm(lp, LPX_K_OUTDLY, 10.0);
      /* find solution of the LP relaxation */
      it_cnt0 = lpx_get_int_parm(lp, LPX_K_ITCNT);
      tree->e_code = ies_solve_node(tree->tree);
      it_cnt1 = lpx_get_int_parm(lp, LPX_K_ITCNT);
      /* decrease the simplex iterations limit by the spent amount */
      if (tree->it_lim >= 0)
      {  tree->it_lim -= (it_cnt1 - it_cnt0);
         if (tree->it_lim < 0) tree->it_lim = 0;
      }
      return;
}

/*----------------------------------------------------------------------
-- is_better - check if given obj. value is better than best known one.
--
-- This routine returns non-zero if a given value of the objective
-- function is better than its value in the best known integer feasible
-- solution. Otherwise the routine returns zero. */

static int is_better(MIPTREE *tree, double obj)
{     int better = 1;
      if (tree->found)
      {  double eps = tree->tol_obj * (1.0 + fabs(tree->best[0]));
         switch (tree->dir)
         {  case LPX_MIN:
               if (obj > tree->best[0] - eps) better = 0;
               break;
            case LPX_MAX:
               if (obj < tree->best[0] + eps) better = 0;
               break;
            default:
               insist(tree->dir != tree->dir);
         }
      }
      return better;
}

/*----------------------------------------------------------------------
-- check_lp_status - check status of current basic solution.
--
-- This routine checks status of a basic solution of the LP relaxation
-- of the current subproblem. Depending on whether the current solution
-- is better or not than the best known integer feasible solution the
-- routine sets the flag tree->better and also stores the corresponding
-- value of the objective function to the current subproblem node. */

static void check_lp_status(MIPTREE *tree)
{     LPX *lp;
      int lp_stat;
      double lp_obj;
      lp = ies_get_lp_object(tree->tree);
      lp_stat = lpx_get_status(lp);
      insist(lp_stat != LPX_UNDEF);
      lp_obj = lpx_get_obj_val(lp);
      switch (tree->e_code)
      {  case LPX_E_OK:
            /* normal termination of the simplex search */
            switch (lp_stat)
            {  case LPX_OPT:
                  /* optimal solution was found */
                  tree->better = is_better(tree, lp_obj);
                  break;
               case LPX_NOFEAS:
                  /* LP relaxation has no primal feasible solution */
                  switch (tree->dir)
                  {  case LPX_MIN:
                        lp_obj = +DBL_MAX;
                        break;
                     case LPX_MAX:
                        lp_obj = -DBL_MAX;
                        break;
                     default:
                        insist(tree->dir != tree->dir);
                  }
                  tree->better = 0;
                  break;
               default:
                  /* no other statuses are possible */
                  insist(lp_stat != lp_stat);
            }
            break;
         case LPX_E_OBJLL:
            /* the simplex search was prematurely terminated because
               a current value of the objective function became less
               than for the best known integer feasible solution and
               continued decreasing (only in case of maximization) */
            insist(tree->dir == LPX_MAX);
            tree->better = 0;
            break;
         case LPX_E_OBJUL:
            /* the simplex search was prematurely terminated because
               a current value of the objective function became greater
               than for the best known integer feasible solution and
               continued increasing (only in case of minimization) */
            insist(tree->dir == LPX_MIN);
            tree->better = 0;
            break;
         default:
            /* no other exit codes are possible */
            insist(tree != tree);
      }
      /* store the corresponding value of the objective function to the
         current subproblem node */
      tree->curr->lp_obj = lp_obj;
      return;
}

/*----------------------------------------------------------------------
-- check_integrality - check integrality of current basic solution.
--
-- This routine checks integer feasibility of a basic solution of the
-- current subproblem.
--
-- The routine marks each structural variable (column) of integer kind
-- whose value in a basic solution of the current subproblem is integer
-- infeasible (i.e. fractional) and stores the number of such variables
-- to tree->unsat. It also computes the sum of integer infeasibilities
-- and store it to tree->ii_sum. */

static void check_integrality(MIPTREE *tree)
{     MIPCOL *col;
      int j, typx, tagx;
      double lb, ub, vx, temp, t1, t2;
      /* walk through all columns (structural variables) */
      tree->unsat = 0;
      tree->ii_sum = 0.0;
      for (j = 1; j <= tree->orig_n; j++)
      {  /* retrieve a pointer to the j-th variable (column) */
         col = tree->col[j];
         insist(col->j == j);
         /* clear non-integrality flag */
         col->infeas = 0;
         /* if the variable is not of integer kind, skip it */
         if (!col->intvar) continue;
         /* obtain bounds of the variable in the current subproblem */
         ies_get_col_bnds(tree->tree, col->col, &typx, &lb, &ub);
         /* if the variable is fixed, its value is integer feasible
            (because the solution is primal feasible) */
         if (typx == LPX_FX) continue;
         /* obtain the current status and value of the variable */
         ies_get_col_info(tree->tree, col->col, &tagx, &vx, NULL);
         /* if the variable is non-basic, its value is integer feasible
            (because its bounds are integral) */
         if (tagx != LPX_BS) continue;
         /* check if the variable is close to its lower bound */
         if (typx == LPX_LO || typx == LPX_DB)
         {  /* round off the bound to cancel the scaling effect */
            temp = floor(lb + 0.5);
            insist(fabs(lb - temp) <= 1e-12 * (1.0 + fabs(lb)));
            lb = temp;
            /* if the variable is close to its lower bound, its value
               can be considered as integer feasible */
            if (vx <= lb + tree->tol_int * (1.0 + fabs(lb))) continue;
         }
         /* check if the variable is close to its upper bound */
         if (typx == LPX_UP || typx == LPX_DB)
         {  /* round off the bound to cancel the scaling effect */
            temp = floor(ub + 0.5);
            insist(fabs(ub - temp) <= 1e-12 * (1.0 + fabs(ub)));
            ub = temp;
            /* if the variable is close to its upper bound, its value
               can be considered as integer feasible */
            if (vx >= ub - tree->tol_int * (1.0 + fabs(ub))) continue;
         }
         /* if the variable is close to the nearest integer point, its
            value can be considered as integer feasible */
         temp = fabs(vx - floor(vx + 0.5));
         if (temp <= tree->tol_int * (1.0 + fabs(vx))) continue;
         /* the j-th variable is integer infeasible */
         col->infeas = 1;
         tree->unsat++;
         t1 = vx - floor(vx);
         t2 = ceil(vx) - vx;
         tree->ii_sum += (t1 <= t2 ? t1 : t2);
      }
      return;
}

/*----------------------------------------------------------------------
-- apply_changes - apply changes to modify current subproblem.
--
-- This procedure applies accumulated changes (made by the solver or by
-- the application procedure) to the current subproblem. If any changes
-- have been made, the routine returns non-zero that signals the solver
-- to perform re-optimization. Otherwise, if the current subproblem has
-- not been changed, the routine returns zero, in which case the solver
-- continues processing of the current subproblem.
--
-- Currently only two kinds of changes are implemented: adding rows and
-- deleting rows.
--
-- This routine assumes that pointers to new rows that should be added
-- are stored in locations tree->row[m+1], ..., tree->row[m+tree->nrs],
-- where m is number of rows in the current subproblem, nrs is number of
-- the new rows, and existing rows that should be deleted are marked in
-- locations tree->del[i], 1 <= i <= m. */

static int apply_changes(MIPTREE *tree)
{     LPX *lp;
      int m, drs, nrs, i;
      /* get access to the internal LP object */
      lp = ies_get_lp_object(tree->tree);
      /* determine number of existing rows in the current subproblem */
      m = lpx_get_num_rows(lp);
      /* determine number of existing rows that should be deleted */
      drs = 0;
      for (i = 1; i <= m; i++)
         if (tree->del[i]) drs++;
      /* determine number of new rows that should be added */
      nrs = tree->nrs;
      /* delete marked rows */
      if (drs > 0)
      {  int m_new = 0;
         lpx_unmark_all(lp);
         for (i = 1; i <= m; i++)
         {  insist(tree->row[i]->i == i);
            if (tree->del[i])
            {  /* i-th row should be deleted */
               /* it cannot be an inherited row */
               insist(tree->row[i]->node == tree->curr);
               /* it also cannot be a row of the original problem */
               insist(i > tree->orig_m);
               /* set mark of the corresponding row in the internal LP
                  object */
               lpx_mark_row(lp,
                  ies_get_row_bind(tree->tree, tree->row[i]->row), 1);
            }
            else
            {  /* i-th row should be kept */
               m_new++;
               tree->row[m_new] = tree->row[i];
               tree->row[m_new]->i = m_new;
            }
         }
         /* move pointers to new rows */
         for (i = 1; i <= nrs; i++)
         {  int i_new = m_new + i;
            tree->row[i_new] = tree->row[m+i];
            tree->row[i_new]->i = i_new;
         }
         /* delete marked rows from the internal LP object */
         ies_del_items(tree->tree);
         /* determine new number of rows in the current subproblem */
         m = m_new;
         insist(m == lpx_get_num_rows(lp));
      }
      /* add new rows */
      if (nrs > 0)
      {  IESITEM **ref = ucalloc(1+nrs, sizeof(void *));
         for (i = 1; i <= nrs; i++)
         {  int i_new = m + i;
            insist(tree->row[i_new]->i == i_new);
            ref[i] = tree->row[i_new]->row;
         }
         ies_add_rows(tree->tree, tree->nrs, ref);
         ufree(ref);
      }
      /* return non-zero if the current subproblem has been changed */
      return drs > 0 || nrs > 0;
}

/*----------------------------------------------------------------------
-- record_solution - record better integer feasible solution.
--
-- This routine records an optimal basic solution of LP relaxation of
-- the current subproblem, which being integer feasible is better than
-- the best known integer feasible solution. */

static void record_solution(MIPTREE *tree)
{     int m = tree->orig_m;
      int n = tree->orig_n;
      int i, j;
      double temp;
      tree->found = 1;
      tree->best[0] = tree->curr->lp_obj;
      for (i = 1; i <= m; i++)
      {  ies_get_row_info(tree->tree, tree->row[i]->row, NULL, &temp,
            NULL);
         tree->best[i] = temp;
      }
      for (j = 1; j <= n; j++)
      {  ies_get_col_info(tree->tree, tree->col[j]->col, NULL, &temp,
            NULL);
         /* a value of the integer column must be integral */
         if (tree->col[j]->intvar) temp = floor(temp + 0.5);
         tree->best[m+j] = temp;
      }
      return;
}

/*----------------------------------------------------------------------
-- cleanup_the_tree - prune hopeless branches from the tree.
--
-- This routine is called when a better integer feasible solution has
-- been found. It prunes hopeless branches from the enumeration tree,
-- i.e. the branches, whose terminal (active) subproblems being solved
-- to the end would have an optimal solution not better than the best
-- known integer feasible solution. */

static void cleanup_the_tree(MIPTREE *tree)
{     MIPNODE *my_node;
      IESNODE *node, *next_node;
      insist(tree->found);
      for (node = ies_get_next_node(tree->tree, NULL); node != NULL;
         node = next_node)
      {  /* deleting some active problem node may involve deleting its
            parents recursively; however, all its parents being created
            *before* it are always *precede* it in the node list, so
            the next problem node is never affected by such deletion */
         next_node = ies_get_next_node(tree->tree, node);
         /* retrieve a pointer to the corresponding subproblem */
         my_node = ies_get_node_link(tree->tree, node);
         /* if estimation of an optimal solution of LP relaxation of
            an active subproblem is worse than the best known integer
            feasible solution, prune the corresponding branch */
         if (ies_get_node_count(tree->tree, node) < 0 &&
            !is_better(tree, my_node->lp_obj))
            ies_prune_branch(tree->tree, node);
      }
      return;
}

/*----------------------------------------------------------------------
-- find_common_ancestor - find new highest-leveled common ancestor.
--
-- This routine is called when some subproblems were deleted from the
-- enumeration tree. It finds a new highest-leveled common ancestor of
-- all subproblems presented in the tree and stores a pointer to that
-- subproblem to tree->glob (if the tree is empty, tree->glob is set to
-- NULL).
--
-- Since new node problems are always appended to the end of the linked
-- list and this ordering is not changed on deleting nodes, the desired
-- node, obviously, is the first one (if walk from the head of the list
-- toward its tail), which has either more than one child or no childs
-- (in the latter case it corresponds to an active subproblem). */

static void find_common_ancestor(MIPTREE *tree)
{     IESNODE *glob, *node;
      glob = NULL;
      for (node = ies_get_next_node(tree->tree, NULL); node != NULL;
         node = ies_get_next_node(tree->tree, node))
      {  glob = node;
         insist(node->up == node->prev);
         if (ies_get_node_count(tree->tree, node) != 1) break;
      }
      if (glob == NULL)
         tree->glob = NULL;
      else
         tree->glob = ies_get_node_link(tree->tree, glob);
      return;
}

/*----------------------------------------------------------------------
-- set_new_bound - set new lower/upper bound for given column.
--
-- This routine sets a new lower (which = 'L') or upper (which = 'U')
-- bound for a given column (structural variable choosen to branch on),
-- specified by the pointer col. */

static void set_new_bound(MIPTREE *tree, MIPCOL *col, int which,
      double bound)
{     int typx;
      double lb, ub, temp;
      /* retrieve current type and bounds of the column */
      ies_get_col_bnds(tree->tree, col->col, &typx, &lb, &ub);
      /* round off the lower bound to cancel the scaling effect */
      if (typx == LPX_LO || typx == LPX_DB)
      {  temp = floor(lb + 0.5);
         insist(fabs(lb - temp) <= 1e-12 * (1.0 + fabs(lb)));
         lb = temp;
      }
      /* round off the upper bound to cancel the scaling effect */
      if (typx == LPX_UP || typx == LPX_DB)
      {  temp = floor(ub + 0.5);
         insist(fabs(ub - temp) <= 1e-12 * (1.0 + fabs(ub)));
         ub = temp;
      }
      /* the new bound must be integral */
      insist(bound == floor(bound));
      /* determine new type and bounds of the column */
      switch (which)
      {  case 'L':
            /* change (increase) lower bound */
            switch (typx)
            {  case LPX_FR:
                  typx = LPX_LO;
                  break;
               case LPX_LO:
                  insist(bound >= lb + 1.0);
                  break;
               case LPX_UP:
                  insist(bound <= ub);
                  typx = LPX_DB;
                  break;
               case LPX_DB:
                  insist(bound >= lb + 1.0);
                  insist(bound <= ub);
                  break;
               default:
                  insist(typx != typx);
            }
            lb = bound;
            if (typx == LPX_DB && lb == ub) typx = LPX_FX;
            break;
         case 'U':
            /* change (decrease) upper bound */
            switch (typx)
            {  case LPX_FR:
                  typx = LPX_UP;
                  break;
               case LPX_LO:
                  insist(bound >= lb);
                  typx = LPX_DB;
                  break;
               case LPX_UP:
                  insist(bound <= ub - 1.0);
                  break;
               case LPX_DB:
                  insist(bound >= lb);
                  insist(bound <= ub - 1.0);
                  break;
               default:
                  insist(typx != typx);
            }
            ub = bound;
            if (typx == LPX_DB && lb == ub) typx = LPX_FX;
            break;
         default:
            insist(which != which);
      }
      /* set new type and bounds of the column */
      ies_set_col_bnds(tree->tree, col->col, typx, lb, ub);
      return;
}

/*----------------------------------------------------------------------
-- create_branches - create up and down branches.
--
-- This routine creates two subproblems, which are identical to the
-- current subproblem except that in the first one called down branch
-- an upper bound of the branching variable is decreased, and in the
-- second one called up branch a lower bound of the branching variable
-- is increased. Depending on the flag tree->heir this routine also
-- selects a subproblem, from which the search should be continued. */

static void create_branches(MIPTREE *tree)
{     LPX *lp = ies_get_lp_object(tree->tree);
      MIPCOL *col;
      MIPNODE *dn_node, *up_node;
      int j_br, pass;
      double vx, lp_obj;
      /* the branching column (structural variable) must be chosen */
      j_br = tree->j_br;
      insist(1 <= j_br && j_br <= tree->orig_n);
      col = tree->col[j_br];
      insist(col->intvar);
      insist(col->infeas);
      /* retrieve a value of the branching variable in an optimal basic
         solution of the LP relaxation of the current subproblem */
      ies_get_col_info(tree->tree, col->col, NULL, &vx, NULL);
      /* retrieve a value of the objective function in an optimal basic
         solution of the LP relaxation of the current subproblem;
         currently this value is used as an estimation of a lower (in
         case of minimization) or an upper (in case of maximization)
         bound of an optimal value the objective function for both down
         and up branches */
      lp_obj = lpx_get_obj_val(lp);
      /* create the branches (the down branch on the first pass and the
         up branch on the second pass) */
      for (pass = 1; pass <= 2; pass++)
      {  MIPNODE *node;
         node = dmp_get_atom(tree->node_pool);
         node->node = ies_create_node(tree->tree, tree->curr->node);
         ies_set_node_link(tree->tree, node->node, node);
         node->lp_obj = lp_obj;
         node->temp = 0.0;
         ies_revive_node(tree->tree, node->node);
         switch (pass)
         {  case 1:
               dn_node = node;
               set_new_bound(tree, col, 'U', floor(vx));
               break;
            case 2:
               up_node = node;
               set_new_bound(tree, col, 'L', ceil(vx));
               break;
            default:
               insist(pass != pass);
         }
      }
      /* two new active subproblems have appeared in the tree */
      tree->an_cnt += 2;
      /* select a subproblem to continue the search */
      switch (tree->heir)
      {  case 0:
            /* it will be selected using a backtracking heuristic */
            tree->curr = NULL;
            break;
         case 1:
            /* the down branch */
            tree->curr = dn_node;
            break;
         case 2:
            /* the up branch */
            tree->curr = up_node;
            break;
         default:
            insist(tree->heir != tree->heir);
      }
      return;
}

/*----------------------------------------------------------------------
-- mip_driver - branch-and-bound driver.
--
-- *Synopsis*
--
-- #include "glpmip.h"
-- int mip_driver(MIPTREE *tree);
--
-- *Description*
--
-- The routine mip_solver is a branch-and-bound driver that manages the
-- process of solving the MIP problem.
--
-- The parameter tree is a pointer to the MIP solver workspace, which
-- should be created by using the routine mip_create_tree. This object
-- contains all information about the original MIP problem as well as
-- all data structures used by the solver for internal administration.
-- On exit from the driver routine the workspace remains valid, thus it
-- is possible to continue the search by subsequent calls to the driver
-- routine.
--
-- *Returns*
--
-- The routine mip_driver returns one of the following exit codes:
--
-- MIP_E_OK       the search is finished;
--
-- MIP_E_ITLIM    iterations limit exceeded;
--
-- MIP_E_SPLIM    subproblems limit exceeded;
--
-- MIP_E_TMLIM    time limit exceeded;
--
-- MIP_E_ERROR    an error occurred on solving the LP relaxation of the
--                current subproblem.
--
-- Note that additional exit codes may appear in the future versions of
-- this routine. */

int mip_driver(MIPTREE *tree)
{     LPX *lp;
      int ret;
      double start = utime(), spent;
      /* obtain a pointer to the internal LP object */
      lp = ies_get_lp_object(tree->tree);
      /* display status of the search on entry to the solver */
      if (tree->msg_lev >= 2) display(tree);
loop: /* main loop starts here */
      /* determine the spent amount of time */
      spent = utime() - start;
      /* if the enumeration tree is empty, the search is finished */
      if (ies_get_next_node(tree->tree, NULL) == NULL)
      {  insist(tree->an_cnt == 0);
         /* at this point all memory pools must be empty */
         insist(tree->row_pool->count == 0);
         insist(tree->col_pool->count == 0);
         insist(tree->node_pool->count == 0);
         /* the search has been successfully completed */
         ret = MIP_E_OK;
         goto done;
      }
      /* check if the iterations limit has been exhausted */
      if (tree->it_lim == 0)
      {  ret = MIP_E_ITLIM;
         goto done;
      }
      /* check if the subproblems limit has been exhausted */
      if (tree->sn_lim == 0)
      {  ret = MIP_E_SNLIM;
         goto done;
      }
      /* check if the time limit has been exhausted */
      if (tree->tm_lim >= 0.0 && tree->tm_lim <= spent)
      {  ret = MIP_E_TMLIM;
         goto done;
      }
      /* display current progress of the search */
      if (tree->msg_lev >= 2 &&
         utime() - tree->tm_last >= tree->out_frq - 0.001)
            display(tree);
      /* if the current subproblem has not been selected yet, ask the
         application procedure to do that */
      if (tree->curr == NULL)
      {  tree->event = MIP_V_SELECT;
         tree->appl(tree->info, tree);
         tree->event = MIP_V_UNDEF;
      }
      /* if the current subproblem is still not selected, select the
         most recent active subproblem by default (this corresponds to
         selection using the LIFO heuristic) */
      if (tree->curr == NULL)
      {  IESNODE *node;
         node = ies_get_prev_node(tree->tree, NULL);
         insist(node != NULL);
         tree->curr = ies_get_node_link(tree->tree, node);
      }
      /* now the current subproblem must be selected and be active */
      insist(tree->curr != NULL);
      insist(ies_get_node_count(tree->tree, tree->curr->node) < 0);
      /* revive the current subproblem (if it was already revived, this
         operation doesn't take any expenses) */
      ies_revive_node(tree->tree, tree->curr->node);
      /* rebuilds the arrays of pointers to rows and columns of the
         current subproblem, which are needed to map original ordinal
         numbers of rows and columns to internal ordinal numbers used
         in the internal LP object */
      rebuild_pointers(tree);
      /* if the current subproblem is solved for the first time, inform
         the application procedure about that */
      if (tree->reopt == 0)
      {  tree->event = MIP_V_BEGSUB;
         tree->appl(tree->info, tree);
         tree->event = MIP_V_UNDEF;
      }
      /* tell the application procedure that LP optimization begins */
      tree->event = MIP_V_BEGLP;
      tree->appl(tree->info, tree);
      tree->event = MIP_V_UNDEF;
      /* find an optimal solution of the LP relaxation of the current
         subproblem by means of the simplex method */
      solve_subproblem(tree);
      /* increase the re-optimization count */
      tree->reopt++;
      /* check exit code reported by the simplex solver */
      switch (tree->e_code)
      {  case LPX_E_OK:
         case LPX_E_OBJLL:
         case LPX_E_OBJUL:
            break;
         default:
            ret = MIP_E_ERROR;
            goto done;
      }
      /* check status of a basic solution of the LP relaxation of the
         current subproblem */
      check_lp_status(tree);
      /* checks integer feasibility of a basic solution of the current
         subproblem */
      check_integrality(tree);
      /* tell the application procedure that LP optimization ended */
      {  int m = lpx_get_num_rows(lp), i;
         for (i = 1; i <= m; i++) tree->del[i] = 0;
         tree->nrs = 0;
         tree->event = MIP_V_ENDLP;
         tree->appl(tree->info, tree);
         tree->event = MIP_V_UNDEF;
      }
      /* if the current subproblem was modified, apply the changes and
         go back to re-optimization */
      if (apply_changes(tree)) goto loop;
      /* decide what to do with the current subproblem */
      if (!tree->better)
      {  /* either the current subproblem has no feasible solution or
            its optimal solution is not better then the best known one;
            so it is fathomed and will be removed from the enumeration
            tree */
         tree->event = MIP_V_REJECT;
         tree->appl(tree->info, tree);
         tree->event = MIP_V_UNDEF;
      }
      else if (tree->unsat == 0)
      {  /* an optimal basic solution of the LP relaxation is integer
            feasible and better than the best known one, so record this
            new better solution */
         record_solution(tree);
         if (tree->msg_lev >= 2) display(tree);
         /* the current subproblem is also fathomed and will be removed
            from the enumeration tree */
         tree->event = MIP_V_BINGO;
         tree->appl(tree->info, tree);
         tree->event = MIP_V_UNDEF;
      }
      else
      {  /* an optimal basic solution of the LP relaxation is better
            that the best known integer feasible solution, however it
            is integer infeasible, so branching is needed */
         /* ask the application procedure to choose an integer column
            (structural variable) with fractional value to branch on */
         tree->j_br = 0;
         tree->heir = 0;
         tree->event = MIP_V_BRANCH;
         tree->appl(tree->info, tree);
         tree->event = MIP_V_UNDEF;
         /* if no branching variable has been chosen, choose the first
            suitable one */
         if (tree->j_br == 0)
         {  int n = tree->orig_n, j;
            for (j = 1; j <= n; j++)
            {  if (tree->col[j]->infeas)
               {  tree->j_br = j;
                  break;
               }
            }
         }
      }
      /* tell the application procedure that the current subproblem has
         been completely processed */
      tree->event = MIP_V_ENDSUB;
      tree->appl(tree->info, tree);
      tree->event = MIP_V_UNDEF;
      /* reset the re-optimization count */
      tree->reopt = 0;
      /* either delete the current subproblem or perform branching */
      if (!tree->better || tree->unsat == 0)
      {  /* the current subproblem is fathomed; prune the corresponding
            branch of the enumeration tree */
         ies_prune_branch(tree->tree, tree->curr->node);
         /* now the current subproblem is undefined */
         tree->curr = NULL;
         /* in case if a new best integer feasible solution has just
            been found, prune hopeless branches, i.e. the branches,
            whose terminal (active) subproblems being solved to the end
            would have a solution not better than the new best one */
         if (tree->better) cleanup_the_tree(tree);
         /* since some subproblems were removed from the tree, find a
            new highest-leveled common ancestor of all subproblems that
            are still in the tree */
         find_common_ancestor(tree);
      }
      else
      {  /* the current subproblem being solved becomes non-active */
         tree->an_cnt--;
         tree->sn_cnt++;
         if (tree->sn_lim > 0) tree->sn_lim--;
         /* create down and up branches; some of these two subroblems
            can be selected as a new current subproblem, and if none is
            selected, a backtracking heuristic will be used) */
         create_branches(tree);
      }
      /* continue the search */
      goto loop;
done: /* display status of the search on exit from the solver */
      if (tree->msg_lev >= 2) display(tree);
      /* determine the spent amount of time */
      spent = utime() - start;
      /* decrease the time limit by the spent amount */
      if (tree->tm_lim >= 0.0)
      {  tree->tm_lim -= spent;
         if (tree->tm_lim < 0.0) tree->tm_lim = 0.0;
      }
      /* display an explanatory message */
      switch (ret)
      {  case MIP_E_OK:
            if (tree->msg_lev >= 3)
            {  if (tree->found)
                  print("INTEGER OPTIMAL SOLUTION FOUND");
               else
                  print("PROBLEM HAS NO INTEGER FEASIBLE SOLUTION");
            }
            break;
         case MIP_E_ITLIM:
            if (tree->msg_lev >= 3)
               print("ITERATIONS LIMIT EXCEEDED; SEARCH TERMINATED");
            break;
         case MIP_E_SNLIM:
            if (tree->msg_lev >= 3)
               print("SUBPROBLEMS LIMIT EXCEEDED; SEARCH TERMINATED");
            break;
         case MIP_E_TMLIM:
            if (tree->msg_lev >= 3)
               print("TIME LIMIT EXCEEDED; SEARCH TERMINATED");
            break;
         case MIP_E_ERROR:
            if (tree->msg_lev >= 1)
               print("mip_driver: unable to solve LP relaxation of curr"
                  "ent subproblem");
            break;
         default:
            insist(ret != ret);
      }
      /* return to the application program */
      return ret;
}

/*----------------------------------------------------------------------
-- mip_delete_tree - delete MIP solver workspace.
--
-- *Synopsis*
--
-- #include "glpmip.h"
-- void mip_delete_tree(MIPTREE *tree);
--
-- *Description*
--
-- The routine mip_delete_tree deletes the MIP solver workspace, which
-- the parameter tree points to, and frees all the memory allocated to
-- this object. */

void mip_delete_tree(MIPTREE *tree)
{     dmp_delete_pool(tree->row_pool);
      dmp_delete_pool(tree->col_pool);
      dmp_delete_pool(tree->node_pool);
      ies_delete_tree(tree->tree);
      ufree(tree->row);
      ufree(tree->col);
      ufree(tree->del);
      ufree(tree->best);
      ufree(tree);
      return;
}

/* eof */
