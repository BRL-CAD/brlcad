/* glpbcs1.c (branch-and-cut framework; driver routine) */

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
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "glpbcs.h"
#include "glplib.h"

/*----------------------------------------------------------------------
-- item_filt - item filter routine.
--
-- This routine is called from the implicit enumeration suite every
-- time when the reference count of some master row/column reaches zero
-- in order to inform the suite routines whether the master row/column
-- should be deleted from the master set or not.
--
-- In the current version of the branch-and-cut framework variables are
-- kept in the workspace until the problem has been completely solved.
-- On the other hand, constraints are deleted from the workspace if the
-- subproblem, for which they were generated, has been deleted. */

static int item_filt(void *info, IESITEM *item)
{     BCS *bcs = info;
      int ret;
      switch (ies_what_item(bcs->tree, item))
      {  case 'R':
            /* master row (constraint) should be deleted */
            ret = 0; break;
         case 'C':
            /* master column (variable) should be kept */
            ret = 1; break;
         default:
            insist(item != item);
      }
      return ret;
}

/*----------------------------------------------------------------------
-- item_hook - item hook routine.
--
-- This routine is called from the implicit enumeration suite if some
-- master row/column is being deleted. It signals to the application
-- procedure about that and then deletes the corresponding variable or
-- constraint from the workspace. */

static void item_hook(void *info, IESITEM *item)
{     BCS *bcs = info;
      BCSVAR *var;
      BCSCON *con;
      switch (ies_what_item(bcs->tree, item))
      {  case 'R':
            /* master row is being deleted */
            con = ies_get_item_link(bcs->tree, item);
            insist(con->flag == BCS_CON_FLAG);
            /* inform the application procedure about that */
            bcs->event = BCS_V_DELCON;
            bcs->this_con = con;
            bcs->appl(bcs->info, bcs);
            bcs->event = BCS_V_UNDEF;
            bcs->this_con = NULL;
            /* delete associated constraint from the workspace */
            con->flag = 0;
            dmp_free_atom(bcs->con_pool, con);
            break;
         case 'C':
            /* master column is being deleted */
            var = ies_get_item_link(bcs->tree, item);
            insist(var->flag == BCS_VAR_FLAG);
            /* inform the application procedure about that */
            bcs->event = BCS_V_DELVAR;
            bcs->this_var = var;
            bcs->appl(bcs->info, bcs);
            bcs->event = BCS_V_UNDEF;
            bcs->this_var = NULL;
            /* delete associated variable from the workspace */
            var->flag = 0;
            dmp_free_atom(bcs->var_pool, var);
            break;
         default:
            insist(item != item);
      }
      return;
}

/*----------------------------------------------------------------------
-- node_hook - node hook routine.
--
-- This routine is called from the implicit enumeration suite if some
-- node problem is being deleted. */

static void node_hook(void *info, IESNODE *node)
{     BCS *bcs = info;
      BCSJOB *job;
      job = ies_get_node_link(bcs->tree, node);
      insist(job->flag == BCS_JOB_FLAG);
      /* delete associated subproblem from the workspace */
      job->flag = 0;
      dmp_free_atom(bcs->job_pool, job);
      return;
}

/*----------------------------------------------------------------------
-- create_wksp - create branch-and-cut workspace.
--
-- This routine creates branch-and-cut workspace and returns a pointer
-- to it. */

static BCS *create_wksp(char *name, int dir,
      void *info, void (*appl)(void *info, BCS *bcs))
{     BCS *bcs;
      if (name != NULL && lpx_check_name(name))
         fault("bcs_driver: invalid problem name");
      if (!(dir == LPX_MIN || dir == LPX_MAX))
         fault("bcs_driver: dir = %d; invalid parameter", dir);
      if (appl == NULL)
         fault("bcs_driver: appl = %p; application procedure not specif"
            "ied", appl);
      /* create and initialize branch-and-cut workspace */
      bcs = umalloc(sizeof(BCS));
      bcs->tree = ies_create_tree();
      bcs->var_pool = dmp_create_pool(sizeof(BCSVAR));
      bcs->con_pool = dmp_create_pool(sizeof(BCSCON));
      bcs->job_pool = dmp_create_pool(sizeof(BCSJOB));
      bcs->info = info;
      bcs->appl = appl;
      bcs->event = BCS_V_UNDEF;
      bcs->br_var = NULL;
      bcs->this_var = NULL;
      bcs->this_con = NULL;
      bcs->found = 0;
      bcs->best = 0.0;
      bcs->t_last = -DBL_MAX;
      /* install IES communication routines */
      ies_set_item_filt(bcs->tree, bcs, item_filt);
      ies_set_item_hook(bcs->tree, bcs, item_hook);
      ies_set_node_hook(bcs->tree, bcs, node_hook);
      /* assign problem name */
      lpx_set_prob_name(ies_get_lp_object(bcs->tree), name);
      /* set optimization direction */
      lpx_set_obj_dir(ies_get_lp_object(bcs->tree), dir);
      return bcs;
}

/*----------------------------------------------------------------------
-- new_job - create new subproblem.
--
-- This routine creates a new subproblem and adds it to the implicit
-- enumeration tree.
--
-- The parameter parent specifies an existing subproblem, to which the
-- new subproblem is attached. If the parameter parent is NULL, the root
-- subproblem is created.
--
-- Being created the new subproblem is equivalent to its parent (except
-- the root subproblem, which is equivalent to an empty LP).
--
-- The routine returns a pointer to the created subproblem. */

static BCSJOB *new_job(BCS *bcs, BCSJOB *parent)
{     BCSJOB *job;
      job = dmp_get_atom(bcs->job_pool);
      job->flag = BCS_JOB_FLAG;
      job->node = ies_create_node(bcs->tree, parent == NULL ? NULL :
         parent->node);
      job->node->link = job;
      return job;
}

/*----------------------------------------------------------------------
-- include_vars - include marked variables in current subproblem.
--
-- This routine looks through all variables in the workspace, includes
-- marked ones in the current subproblem, unmarks them, and then returns
-- number of the included variables. */

static int include_vars(BCS *bcs)
{     IESITEM *col, **ccc;
      BCSVAR *var;
      int count, t;
      /* count marked variables */
      count = 0;
      for (col = ies_next_master_col(bcs->tree, NULL); col != NULL;
           col = ies_next_master_col(bcs->tree, col))
      {  var = ies_get_item_link(bcs->tree, col);
         insist(var != NULL);
         if (var->attr & BCS_MARKED)
         {  /* should be missing in the current subproblem */
            insist(ies_get_col_bind(bcs->tree, col) == 0);
            count++;
         }
      }
      /* if there are no marked variables, do nothing */
      if (count == 0) goto done;
      /* build a list of marked variables and unmark them */
      ccc = ucalloc(1+count, sizeof(IESITEM *));
      t = 0;
      for (col = ies_next_master_col(bcs->tree, NULL); col != NULL;
           col = ies_next_master_col(bcs->tree, col))
      {  var = ies_get_item_link(bcs->tree, col);
         insist(var != NULL);
         if (var->attr & BCS_MARKED)
         {  ccc[++t] = col;
            var->attr &= ~BCS_MARKED;
         }
      }
      insist(t == count);
      /* add marked variables to the current subproblem */
      ies_add_cols(bcs->tree, count, ccc);
      /* free the list of marked variables */
      ufree(ccc);
done: /* return to the calling program */
      return count;
}

/*----------------------------------------------------------------------
-- include_cons - include marked constraints in current subproblem.
--
-- This routine looks through all constraints in the workspace, includes
-- marked ones in the current subproblem, unmarks them, and then returns
-- number of the included constraints. */

static int include_cons(BCS *bcs)
{     IESITEM *row, **rrr;
      BCSCON *con;
      int count, t;
      /* count marked constraints */
      count = 0;
      for (row = ies_next_master_row(bcs->tree, NULL); row != NULL;
           row = ies_next_master_row(bcs->tree, row))
      {  con = ies_get_item_link(bcs->tree, row);
         insist(con != NULL);
         if (con->attr & BCS_MARKED)
         {  /* should be missing in the current subproblem */
            insist(ies_get_row_bind(bcs->tree, row) == 0);
            count++;
         }
      }
      /* if there are no marked constraints, do nothing */
      if (count == 0) goto done;
      /* build a list of marked constraints and unmark them */
      rrr = ucalloc(1+count, sizeof(IESITEM *));
      t = 0;
      for (row = ies_next_master_row(bcs->tree, NULL); row != NULL;
           row = ies_next_master_row(bcs->tree, row))
      {  con = ies_get_item_link(bcs->tree, row);
         insist(con != NULL);
         if (con->attr & BCS_MARKED)
         {  rrr[++t] = row;
            con->attr &= ~BCS_MARKED;
         }
      }
      insist(t == count);
      /* add marked constraints to the current subproblem */
      ies_add_rows(bcs->tree, count, rrr);
      /* free the list of marked constraints */
      ufree(rrr);
done: /* return to the calling program */
      return count;
}

/*----------------------------------------------------------------------
-- incl_by_red_cost - include variables by reduced costs.
--
-- This routine looks through the variables, which are missing in the
-- current subproblem, and include in the current subproblem that ones,
-- whose reduced costs indicate improving the objective function. This
-- is needed to guarantee that an optimal solution of the current
-- subproblem is dual feasible for all variables in the workspace, not
-- only for variables presented in the subproblem.
--
-- At a time not all variables with dual infeasibility are included in
-- the current subproblem, but not more than nv_max, because after
-- re-optimization reduced costs of the rest variables may be altered.
--
-- If the parameter spec is non-zero, this means that the current
-- subproblem has no primal feasible solution, and that reduced costs
-- of variables correspond to an auxiliary objective function, which is
-- the sum of primal infeasibilities. In this case variables should be
-- included in the subproblem in order to attain primal feasibility.
--
-- The routine returns number of variables included in the subproblem.
-- If at least one variable was included, the current subproblem should
-- be re-optimized. */

static int incl_by_red_cost(BCS *bcs, int nv_max, int spec)
{     BCSVAR *var, **vvv;
      IESITEM *col;
      int dir, nv, t, loc;
      double *ddd, dj, temp;
      insist(nv_max > 0);
      /* determine optimization direction */
      dir = lpx_get_obj_dir(ies_get_lp_object(bcs->tree));
      /* the array vvv contains selected variables, and the array ddd
         contains their reduced costs */
      vvv = ucalloc(1+nv_max, sizeof(BCSVAR *));
      ddd = ucalloc(1+nv_max, sizeof(double));
      /* no variables are selected so far */
      nv = 0;
      /* walk through all variables in the workspace */
      for (col = ies_next_master_col(bcs->tree, NULL); col != NULL;
           col = ies_next_master_col(bcs->tree, col))
      {  var = ies_get_item_link(bcs->tree, col);
         insist(var != NULL);
         /* if the variable is presented in the current subproblem,
            skip it */
         if (ies_get_col_bind(bcs->tree, col) != 0) continue;
         /* if the variable is missing in the current subproblem, it
            should have zero lower bound; thus, it is non-basic at its
            zero bound and can increase (this condition is checked by
            the routine, which adds variables to the workspace) */
         insist(col->typx == LPX_LO || col->typx == LPX_DB);
         insist(col->lb == 0.0);
         /* compute reduced cost (in the case of maximization its sign
            is replaced by opposite one) */
         if (!spec)
         {  /* ordinary case */
            dj = ies_eval_red_cost(bcs->tree, col);
            if (dir == LPX_MAX) dj = - dj;
         }
         else
         {  /* special case (an objective coefficient of the variable
               is temporarily nullified, because it relates to the
               original objective function, however, in this case we
               need reduced cost for the auxiliary objective function,
               which is the sum of primal infeasibilities) */
            temp = col->coef;
            col->coef = 0.0;
            dj = ies_eval_red_cost(bcs->tree, col);
            col->coef = temp;
         }
         /* if the reduced cost has correct sign, skip the variable */
         if (dj >= -1e-7) continue;
         /* the dual feasibility condition is violated, i.e. increasing
            the variable involves improving the objective function */
         if (nv < nv_max)
         {  /* store the variable into the selection list */
            nv++;
            vvv[nv] = var;
            ddd[nv] = dj;
         }
         else
         {  /* there is no room in the selection list; retain variables
               with most negative reduced costs only */
            insist(nv == nv_max);
            /* find a variable with cheapest reduced cost */
            loc = 1;
            for (t = 2; t <= nv; t++)
               if (ddd[loc] > ddd[t]) loc = t;
            /* if the current variable has better reduced cost that the
               found one, store it into the selection list */
            if (ddd[loc] > dj) vvv[loc] = var, ddd[loc] = dj;
         }
      }
      /* include selected variables in the current subproblem */
      if (nv > 0)
      {  for (t = 1; t <= nv; t++) vvv[t]->attr |= BCS_MARKED;
         insist(include_vars(bcs) == nv);
#if 1
         /* check reduced costs by direct computation (other branch of
            the routine ies_eval_red_cost) after the selected variables
            have been included in the current subproblem */
         insist(lpx_warm_up(ies_get_lp_object(bcs->tree)) == LPX_E_OK);
         for (t = 1; t <= nv; t++)
         {  insist(ies_get_col_bind(bcs->tree, vvv[t]->col) != 0);
            dj = ies_eval_red_cost(bcs->tree, vvv[t]->col);
            if (spec) dj -= vvv[t]->col->coef;
            insist(fabs(ddd[t] - dj) <= 1e-5 * (1.0 + fabs(dj)));
         }
#endif
      }
      /* free working arrays */
      ufree(vvv);
      ufree(ddd);
      /* return to the calling program */
      return nv;
}

/*----------------------------------------------------------------------
-- recover_feas - include variables to recover primal feasibility.
--
-- This routine is called when the current subproblem has no feasible
-- solution. It tries to recover primal feasibility by including in the
-- current subproblem the variables, which are missing in it and which
-- may decrease the sum of infeasibilities.
--
-- At a time not all variables with dual infeasibility are included in
-- the current subproblem, but not more than nv_max, because after
-- re-optimization reduced costs of the rest variables may be altered.
--
-- Note that the sum of infeasibilities may decrease non-monotonically,
-- because the phase I implemented in the simplex-based solver may use
-- other measurement of primal infeasibility.
--
-- The routine returns number of variables included in the subproblem.
-- If at least one variable was included, the current subproblem should
-- be re-optimized. */

static int recover_feas(BCS *bcs, int nv_max)
{     LPX *lp = ies_get_lp_object(bcs->tree);
      int m = ies_get_num_rows(bcs->tree);
      int n = ies_get_num_cols(bcs->tree);
      int i, j, k, nv, typx, tagx;
      double *coef, lb, ub, vx, sum = 0.0;
      insist(m == lpx_get_num_rows(lp));
      insist(n == lpx_get_num_cols(lp));
      /* walk through rows and columns of the current subproblem and
         construct an auxiliary objective function, which is the sum of
         primal infeasibilities */
      insist(lpx_get_prim_stat(lp) != LPX_P_UNDEF);
      coef = ucalloc(1+m+n, sizeof(double));
      for (k = 1; k <= m+n; k++)
      {  coef[k] = 0.0;
         /* obtain bounds and a value of the corresponding auxiliary or
            structural variable in the current basic solution */
         if (k <= m)
         {  lpx_get_row_bnds(lp, k, &typx, &lb, &ub);
            lpx_get_row_info(lp, k, &tagx, &vx, NULL);
         }
         else
         {  lpx_get_col_bnds(lp, k-m, &typx, &lb, &ub);
            lpx_get_col_info(lp, k-m, &tagx, &vx, NULL);
         }
         /* if the variable is non-basic, skip it */
         if (tagx != LPX_BS) continue;
         /* if the variable violates its lower bound, it should be
            increased */
         if (typx == LPX_LO || typx == LPX_DB || typx == LPX_FX)
            if (vx < lb) coef[k] = -1.0, sum += lb - vx;
         /* if the variable violates its upper bound, it should be
            decreased */
         if (typx == LPX_UP || typx == LPX_DB || typx == LPX_FX)
            if (vx > ub) coef[k] = +1.0, sum += vx - ub;
      }
#if 0
      print("sum of prim infeas = %g", sum);
#endif
      /* replace temporarily the original objective function by the
         auxiliary one (original objective coefficients are kept in the
         implicit enumeration tree, so they needn't to be saved) */
      lpx_set_obj_c0(lp, 0.0);
      for (i = 1; i <= m; i++) lpx_set_row_coef(lp, i, coef[i]);
      for (j = 1; j <= n; j++) lpx_set_col_coef(lp, j, coef[m+j]);
      ufree(coef);
      /* re-compute basic solution components */
      insist(lpx_warm_up(lp) == LPX_E_OK);
      /* select variables, which are missing in the current subproblem
         and which may decrease the sum of infeasibilities, and include
         them in the subproblem */
      nv = incl_by_red_cost(bcs, nv_max, 1);
      /* restore coefficients of the original objective function */
      lpx_set_obj_c0(lp, bcs->tree->coef[0]);
      for (i = 1; i <= m; i++)
         lpx_set_row_coef(lp, i, bcs->tree->coef[i]);
      for (j = 1; j <= n; j++)
         lpx_set_col_coef(lp, j, bcs->tree->coef[m+j]);
      /* re-compute basic solution components */
      insist(lpx_warm_up(lp) == LPX_E_OK);
      /* return to the calling program */
      return nv;
}

/*----------------------------------------------------------------------
-- check_nonint - assign the non-integer attribute.
--
-- This routine assigns the non-integer attribute to integer variables,
-- whose value in the current basic solution is integer infeasible, and
-- returns number of these variables. The current basic solution should
-- be optimal. */

static int check_nonint(BCS *bcs)
{     int n = ies_get_num_cols(bcs->tree);
      BCSVAR *var;
      IESITEM *col;
      int count, j, typx, tagx;
      double lb, ub, vx, temp;
      /* the current solution should be optimal */
      insist(lpx_get_status(ies_get_lp_object(bcs->tree)) == LPX_OPT);
      /* variables, which are missing in the current subproblem, have
         zero value by the definition, so only variables presented in
         the current subproblem should be examined */
      count = 0;
      for (j = 1; j <= n; j++)
      {  /* obtain pointer to the j-th column */
         col = ies_get_jth_col(bcs->tree, j);
         /* obtain pointer to the corresponding variable */
         var = ies_get_item_link(bcs->tree, col);
         /* if the variable is not of integer kind, skip it */
         if (!(var->attr & BCS_INTEGER)) continue;
         /* obtain current bounds of the variable */
         ies_get_col_bnds(bcs->tree, col, &typx, &lb, &ub);
         /* check and round lower bound to cancel the scaling effect */
         if (typx == LPX_LO || typx == LPX_DB || typx == LPX_FX)
         {  temp = floor(lb + 0.5);
            insist(fabs(lb - temp) <= 1e-12);
            lb = temp;
         }
         /* check and round upper bound to cancel the scaling effect */
         if (typx == LPX_UP || typx == LPX_DB)
         {  temp = floor(ub + 0.5);
            insist(fabs(ub - temp) <= 1e-12);
            ub = temp;
         }
         /* if the variable is fixed, its value is integer (because the
            solution is primal feasible), so skip it */
         if (typx == LPX_FX) continue;
         /* obtain the current status and value of the variable */
         ies_get_col_info(bcs->tree, col, &tagx, &vx, NULL);
         /* if the variable is non-basic, its value is integer (because
            its bounds are integer), so skip it */
         if (tagx != LPX_BS) continue;
         /* if the variable is close to its lower bound, its value can
            be considered as integer, so skip it */
         if (typx == LPX_LO || typx == LPX_DB)
            if (vx <= lb + 1e-6) continue;
         /* if the variable is close to its upper bound, its value can
            be considered as integer, so skip it */
         if (typx == LPX_UP || typx == LPX_DB)
            if (vx >= ub - 1e-6) continue;
         /* if the variable is close to the nearest integer point, its
            value can be considered as integer, so skip it */
         temp = floor(vx + 0.5);
         if (fabs(vx - temp) <= 1e-6) continue;
         /* the variable has a fractional value */
         var->attr |= BCS_NONINT;
         count++;
      }
      return count;
}

/*----------------------------------------------------------------------
-- clear_nonint - clear the non-integer attribute.
--
-- This routine clears the non-integer attribute previosuly assigned by
-- the routine check_nonint. */

static void clear_nonint(BCS *bcs)
{     int n = ies_get_num_cols(bcs->tree);
      BCSVAR *var;
      IESITEM *col;
      int j;
      /* only variables presented in the current subproblem can have
         the non-integer attribute */
      for (j = 1; j <= n; j++)
      {  /* obtain pointer to the j-th column */
         col = ies_get_jth_col(bcs->tree, j);
         /* obtain pointer to the corresponding variable */
         var = ies_get_item_link(bcs->tree, col);
         /* clear the non-integer attribute */
         var->attr &= ~BCS_NONINT;
      }
      return;
}

/*----------------------------------------------------------------------
-- choose_branch - choose branching variable.
--
-- This routine chooses a variable for branchng, which is an integer
-- variable, whose value in a basic solution of the current subproblem
-- is integer infeasible (fractional). Candidates for branching have
-- the non-integer attribute assigned by the routine check_nonint.
--
-- In the current implementation the routine uses an easiest branching
-- heuristic choosing the first appropriate variable.
--
-- The routine returns a pointer to the chosen variable. */

static BCSVAR *choose_branch(BCS *bcs)
{     int n = ies_get_num_cols(bcs->tree);
      BCSVAR *var;
      IESITEM *col;
      int j;
      /* only variables presented in the current subproblem can have
         the non-integer attribute */
      var = NULL;
#if 0
      for (j = 1; j <= n; j++)
#else
      for (j = n; j >= 1; j--)
#endif
      {  /* obtain pointer to the j-th column */
         col = ies_get_jth_col(bcs->tree, j);
         /* obtain pointer to the corresponding variable */
         var = ies_get_item_link(bcs->tree, col);
         /* choose the first appropriate variable */
         if (var->attr & BCS_NONINT) break;
      }
      insist(var != NULL);
      return var;
}

/*----------------------------------------------------------------------
-- new_bound - set new lower/upper bound of branching variable.
--
-- This routine sets a new lower (which = 'L') or upper (which = 'U')
-- bound of the branching variable, which the parameter var points to.
--
-- Note that the new lower/upper bound is set locally for the current
-- subproblem and inherited by only subproblems derived from it. */

static void new_bound(BCS *bcs, BCSVAR *var, int which, double bnd)
{     int typx;
      double lb, ub, temp;
      /* the branching variable should be in the current subproblem */
      insist(ies_get_col_bind(bcs->tree, var->col) != 0);
      /* obtain current local bounds of the branching variable */
      ies_get_col_bnds(bcs->tree, var->col, &typx, &lb, &ub);
      /* in the current implementation the branching variable can be
         only double-bounded */
      insist(typx == LPX_DB);
      /* check and round lower bound to cancel the scaling effect */
      temp = floor(lb + 0.5);
      insist(fabs(lb - temp) <= 1e-12);
      lb = temp;
      /* check and round upper bound to cancel the scaling effect */
      temp = floor(ub + 0.5);
      insist(fabs(ub - temp) <= 1e-12);
      ub = temp;
      /* new bound should be integral */
      insist(bnd == floor(bnd + 0.5));
      /* determine new type and bounds of the branching variable */
      switch (which)
      {  case 'L':
            /* set new lower bound */
            insist(bnd >= lb + 1.0);
            insist(bnd <= ub - 1.0 || bnd == ub);
            typx = (bnd == ub ? LPX_FX : LPX_DB);
            lb = bnd;
            break;
         case 'U':
            /* set new upper bound */
            insist(bnd <= ub - 1.0);
            insist(bnd >= lb + 1.0 || bnd == lb);
            typx = (bnd == lb ? LPX_FX : LPX_DB);
            ub = bnd;
            break;
         default:
            insist(which != which);
      }
      /* set new type and bounds of the branching variable */
      ies_set_col_bnds(bcs->tree, var->col, typx, lb, ub);
      return;
}

/*----------------------------------------------------------------------
-- choose_job - choose active subproblem.
--
-- This routine chooses a subproblem from the active list, from which
-- the search should be continued.
--
-- In the current implementation the routine uses an easiest heuristic
-- for backtracking choosing the active subproblem most recently added
-- to the list (depth first search).
--
-- The routine returns a pointer to the chosen subproblem or NULL, if
-- the active list is empty. */

static BCSJOB *choose_job(BCS *bcs)
{     BCSJOB *job;
      IESNODE *node;
      job = NULL;
      for (node = ies_get_prev_node(bcs->tree, NULL); node != NULL;
           node = ies_get_prev_node(bcs->tree, node))
      {  if (ies_get_node_count(bcs->tree, node) < 0)
         {  /* negative count means the node problem is active */
            job = ies_get_node_link(bcs->tree, node);
            insist(job != NULL);
            break;
         }
      }
      return job;
}

/*----------------------------------------------------------------------
-- display - display visual information.
--
-- This routine displays visual information, which includes number of
-- performed simplex iteration, value of the objective function for the
-- best known solution, and level of the subproblem in the enumeration
-- tree, which is currently being solved. */

static void display(BCS *bcs)
{     IESNODE *node;
      int itcnt, level, m, n, nz;
      char sol[50];
      /* obtain number of performed simplex iterations */
      itcnt = lpx_get_int_parm(ies_get_lp_object(bcs->tree),
         LPX_K_ITCNT);
      /* obtain pointer to the current node problem */
      node = ies_get_this_node(bcs->tree);
      /* determine level of the current node problem (negative value
         means the current node problem doesn't exist) */
      if (node == NULL)
      {  level = -1;
         m = n = nz = 0;
      }
      else
      {  LPX *lp = ies_get_lp_object(bcs->tree);
         level = ies_get_node_level(bcs->tree, node);
         m = lpx_get_num_rows(lp);
         n = lpx_get_num_cols(lp);
         nz = lpx_get_num_nz(lp);
      }
      /* determine the best known integer feasible solution */
      if (!bcs->found)
         strcpy(sol, "not found yet");
      else
         sprintf(sol, "%17.9e", bcs->best);
      /* display visual information */
      print("+%6d:   intsol = %17s   (lev %d; lp %d, %d, %d)",
         itcnt, sol, level, m, n, nz);
      /* note the time */
      bcs->t_last = utime();
      /* return to the calling program */
      return;
}

/*----------------------------------------------------------------------
-- bcs_delete_wksp - delete branch-and-cut workspace.
--
-- This routine deletes the branch-and-cut workspace freeing all the
-- memory allocated to it. */

static void bcs_delete_wksp(BCS *bcs)
{     ies_delete_tree(bcs->tree);
      dmp_delete_pool(bcs->var_pool);
      dmp_delete_pool(bcs->con_pool);
      dmp_delete_pool(bcs->job_pool);
      ufree(bcs);
      return;
}

/*----------------------------------------------------------------------
-- bcs_driver - branch-and-cut driver.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- int bcs_driver(char *name, int dir,
--    void *info, void (*appl)(void *info, BCS *bcs));
--
-- *Description*
--
-- The routine bcs_driver is a branch-and-cut driver, which manages the
-- process of solving the optimization problem.
--
-- The character string name is a symbolic name (1 to 255 arbitrary
-- graphic characters) assigned to the problem. If this parameter NULL,
-- no name is assigned.
--
-- The parameter dir specifies the optimization direction:
--
-- LPX_MIN - the objective function should be minimized;
-- LPX_MAX - the objective function should be maximized.
--
-- The parameter info is a transit pointer passed to the application
-- procedure.
--
-- The parameter appl should specify an entry point to the event-driven
-- application procedure supplied by the user. This procedure is called
-- by the branch-and-cut driver at certain points of the optimization
-- process in order to perform application specific actions (variables
-- and constraints generation, etc.). The first parameter passed to this
-- procedure is the transit pointer passed to the branch-and-cut driver
-- routine. The second parameter passed to this procedure is a pointer
-- to the branch-and-cut workspace. It should be used by the application
-- procedure on subsequent calls to the framework routines.
--
-- *Returns*
--
-- In the current implementation the routine bcs_driver always returns
-- zero code. */

int bcs_driver(char *name, int dir,
      void *info, void (*appl)(void *info, BCS *bcs))
{     BCS *bcs;
      BCSJOB *job, *child;
      LPX *lp;
      int nonint;
      double vx;
      /* create the branch-and-cut workspace */
      bcs = create_wksp(name, dir, info, appl);
      /* obtain a pointer to the internal LP object */
      lp = ies_get_lp_object(bcs->tree);
      /* create and revive the root subproblem (initially is empty) */
      job = new_job(bcs, NULL);
      ies_revive_node(bcs->tree, job->node);
      /* build an initial set of variables and constraints */
      bcs->event = BCS_V_INIT;
      appl(bcs->info, bcs);
      bcs->event = BCS_V_UNDEF;
      if (include_vars(bcs) == 0)
         fault("bcs_driver: root subproblem has no variables");
      if (include_cons(bcs) == 0)
         fault("bcs_driver: root subproblem has no constraints");
      /* solve initial LP relaxation */
      print("Solving initial LP relaxation...");
      lpx_scale_prob(lp);
      lpx_adv_basis(lp);
      insist(ies_solve_node(bcs->tree) == LPX_E_OK);
      /* solution of the initial LP relaxation should be optimal */
      insist(lpx_get_status(lp) == LPX_OPT);
      /* all is ready for integer optimization */
      print("Integer optimization begins...");
loop: /* main loop starts here */
      /* display visual information about the search status */
      if (utime() - bcs->t_last >= 2.9) display(bcs);
      /* solve the current subproblem */
      lpx_set_int_parm(lp, LPX_K_MSGLEV, 2);
      lpx_set_real_parm(lp, LPX_K_OUTDLY, 5.0);
      insist(ies_solve_node(bcs->tree) == LPX_E_OK);
      /* if the current subproblem has no primal feasible solution, try
         including additional variables to attain primal feasibility */
      if (lpx_get_status(bcs->tree->lp) == LPX_NOFEAS)
      {  if (recover_feas(bcs, 10) > 0)
         {  /* some variables have been included; re-optimize */
            goto loop;
         }
         /* otherwise the current subproblem has been fathomed */
         goto fath;
      }
      /* otherwise the solution should be optimal */
      insist(lpx_get_status(bcs->tree->lp) == LPX_OPT);
      /* be sure that the optimal solution of the current subproblem is
         dual feasible for all variables in the workspace, not only for
         variables presented in the subproblem */
      if (incl_by_red_cost(bcs, 10, 0) > 0)
      {  /* some variables have been included; re-optimize */
         goto loop;
      }
      /* if the optimal solution is not better than the best known one,
         the current subproblem has been fathomed */
      if (bcs->found)
      {  double obj = lpx_get_obj_val(lp);
         double eps = 1e-7 * fabs(bcs->best);
         switch (dir)
         {  case LPX_MIN:
               if (obj > bcs->best - eps) goto fath;
               break;
            case LPX_MAX:
               if (obj < bcs->best + eps) goto fath;
               break;
            default:
               insist(dir != dir);
         }
      }
      /* be sure that the current subproblem includes all essential
         (application specific) constraints */
      bcs->event = BCS_V_GENCON;
      appl(bcs->info, bcs);
      bcs->event = BCS_V_UNDEF;
      if (include_cons(bcs) > 0)
      {  /* some constraints have been included; re-optimize */
         goto loop;
      }
      /* determine number of integer variables, which have fractional
         values, and assign the non-integer attribute to them */
      nonint = check_nonint(bcs);
      /* if the optimal solution is integer feasible, it is a new best
         solution */
      if (nonint == 0)
      {  bcs->found = 1;
         bcs->best = lpx_get_obj_val(lp);
         display(bcs);
         /* inform the application about that */
         bcs->event = BCS_V_BINGO;
         appl(bcs->info, bcs);
         bcs->event = BCS_V_UNDEF;
         /* the current subproblem has been fathomed */
         goto fath;
      }
      /* try to cut off the fractional point using application specific
         cutting planes */
      bcs->event = BCS_V_GENCUT;
      appl(bcs->info, bcs);
      bcs->event = BCS_V_UNDEF;
      if (include_cons(bcs) > 0)
      {  /* some cutting planes have been generated; re-optimize */
         /* don't forget to clear the non-integer attributes */
         clear_nonint(bcs);
         goto loop;
      }
      /* the solution is still integer infeasible; choose a branching
         variable */
      bcs->event = BCS_V_BRANCH;
      bcs->br_var = NULL;
      appl(bcs->info, bcs);
      bcs->event = BCS_V_UNDEF;
      /* if no branching variable has been chosen by the application
         procedure, choose something appropriate */
      if (bcs->br_var == NULL) bcs->br_var = choose_branch(bcs);
      /* at this point the branching variable has to be chosen */
      insist(bcs->br_var != NULL);
      insist(bcs->br_var->attr & BCS_NONINT);
      /* clear the non-integer attributes */
      clear_nonint(bcs);
      /* obtain a value of the branching variable in the optimal basic
         solution of the current subproblem */
      ies_get_col_info(bcs->tree, bcs->br_var->col, NULL, &vx, NULL);
      /* derive one subproblem for the down branch, where the branching
         variable is rounded down (i.e. its upper bound is decreased) */
      child = new_job(bcs, job);
      ies_revive_node(bcs->tree, child->node);
      new_bound(bcs, bcs->br_var, 'U', floor(vx));
      /* derive other subproblem for the up branch, where the branching
         variable is rounded up (i.e. its lower bound is increased) */
      child = new_job(bcs, job);
      ies_revive_node(bcs->tree, child->node);
      new_bound(bcs, bcs->br_var, 'L', ceil(vx));
      /* the latter child subproblem becomes the current one */
      job = child;
      /* continue the search */
      goto loop;
fath: /* the current subproblem has been fathomed; prune the branch */
      ies_prune_branch(bcs->tree, job->node);
      /* choose some subproblem from the active list, from which the
         search can be continued */
      job = choose_job(bcs);
      /* if the active list is empty, terminate the search */
      if (job == NULL) goto done;
      /* revive the chosen subproblem */
      ies_revive_node(bcs->tree, job->node);
      /* and continue the search */
      goto loop;
done: /* all subproblems have been fathomed */
      display(bcs);
      if (!bcs->found)
         print("PROBLEM HAS NO INTEGER FEASIBLE SOLUTION");
      else
         print("INTEGER OPTIMAL SOLUTION FOUND");
      /* delete all variables from the workspace */
      for (;;)
      {  IESITEM *col = ies_next_master_col(bcs->tree, NULL);
         if (col == NULL) break;
         ies_del_master_col(bcs->tree, col);
      }
      /* inform the application procedure about terminating */
      bcs->event = BCS_V_TERM;
      appl(bcs->info, bcs);
      bcs->event = BCS_V_UNDEF;
      /* check if all memory pools are empty */
      insist(bcs->var_pool->count == 0);
      insist(bcs->con_pool->count == 0);
      insist(bcs->job_pool->count == 0);
      /* delete the branch-and-cut workspace */
      bcs_delete_wksp(bcs);
      /* return to the calling program */
      return 0;
}

/* eof */
