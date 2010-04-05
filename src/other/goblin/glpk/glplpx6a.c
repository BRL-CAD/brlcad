/* glplpx6a.c (simplex-based solver routines) */

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

#include <math.h>
#include <string.h>
#include "glplib.h"
#include "glplpp.h"
#include "glpspx.h"

/*----------------------------------------------------------------------
-- lpx_warm_up - "warm up" the initial basis.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_warm_up(LPX *lp);
--
-- *Description*
--
-- The routine lpx_warm_up "warms up" the initial basis specified by
-- the array lp->tagx. "Warming up" includes (if necessary) reinverting
-- (factorizing) the initial basis matrix, computing the initial basic
-- solution components (values of basic variables, simplex multipliers,
-- reduced costs of non-basic variables), and determining primal and
-- dual statuses of the initial basic solution.
--
-- *Returns*
--
-- The routine lpx_warm_up returns one of the following exit codes:
--
-- LPX_E_OK       the initial basis has been successfully "warmed up".
--
-- LPX_E_EMPTY    the problem has no rows and/or no columns.
--
-- LPX_E_BADB     the initial basis is invalid, because number of basic
--                variables and number of rows are different.
--
-- LPX_E_SING     the initial basis matrix is numerically singular or
--                ill-conditioned.
--
-- Note that additional exit codes may appear in the future versions of
-- this routine. */

int lpx_warm_up(LPX *lp)
{     int m = lp->m;
      int n = lp->n;
      int ret;
      /* check if the problem is empty */
      if (!(m > 0 && n > 0))
      {  ret = LPX_E_EMPTY;
         goto done;
      }
      /* reinvert the initial basis matrix (if necessary) */
      if (lp->b_stat != LPX_B_VALID)
      {  int i, j, k;
         /* invalidate the basic solution */
         lp->p_stat = LPX_P_UNDEF;
         lp->d_stat = LPX_D_UNDEF;
         /* build the arrays posx and indx using the array tagx */
         i = j = 0;
         for (k = 1; k <= m+n; k++)
         {  if (lp->tagx[k] == LPX_BS)
            {  /* x[k] = xB[i] */
               i++;
               if (i > m)
               {  /* too many basic variables */
                  ret = LPX_E_BADB;
                  goto done;
               }
               lp->posx[k] = i, lp->indx[i] = k;
            }
            else
            {  /* x[k] = xN[j] */
               j++;
               if (j > n)
               {  /* too many non-basic variables */
                  ret = LPX_E_BADB;
                  goto done;
               }
               lp->posx[k] = m+j, lp->indx[m+j] = k;
            }
         }
         insist(i == m && j == n);
         /* reinvert the initial basis matrix */
         if (spx_invert(lp) != 0)
         {  /* the basis matrix is singular or ill-conditioned */
            ret = LPX_E_SING;
            goto done;
         }
      }
      /* now the basis is valid */
      insist(lp->b_stat == LPX_B_VALID);
      /* compute the initial primal solution */
      if (lp->p_stat == LPX_P_UNDEF)
      {  spx_eval_bbar(lp);
         if (spx_check_bbar(lp, lp->tol_bnd) == 0.0)
            lp->p_stat = LPX_P_FEAS;
         else
            lp->p_stat = LPX_P_INFEAS;
      }
      /* compute the initial dual solution */
      if (lp->d_stat == LPX_D_UNDEF)
      {  spx_eval_pi(lp);
         spx_eval_cbar(lp);
         if (spx_check_cbar(lp, lp->tol_dj) == 0.0)
            lp->d_stat = LPX_D_FEAS;
         else
            lp->d_stat = LPX_D_INFEAS;
      }
      /* the basis has been successfully "warmed up" */
      ret = LPX_E_OK;
      /* return to the calling program */
done: return ret;
}

/*----------------------------------------------------------------------
-- lpx_prim_opt - find optimal solution (primal simplex).
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_prim_opt(LPX *lp);
--
-- *Description*
--
-- The routine lpx_prim_opt is intended to find optimal solution of an
-- LP problem using the primal simplex method.
--
-- On entry to the routine the initial basis should be "warmed up" and,
-- moreover, the initial basic solution should be primal feasible.
--
-- Structure of this routine can be an example for other variants based
-- on the primal simplex method.
--
-- *Returns*
--
-- The routine lpx_prim_opt returns one of the folloiwng exit codes:
--
-- LPX_E_OK       optimal solution found.
--
-- LPX_E_NOFEAS   the problem has no dual feasible solution, therefore
--                its primal solution is unbounded.
--
-- LPX_E_ITLIM    iterations limit exceeded.
--
-- LPX_E_TMLIM    time limit exceeded.
--
-- LPX_E_BADB     the initial basis is not "warmed up".
--
-- LPX_E_INFEAS   the initial basic solution is primal infeasible.
--
-- LPX_E_INSTAB   numerical instability; the current basic solution got
--                primal infeasible due to excessive round-off errors.
--
-- LPX_E_SING     singular basis; the current basis matrix got singular
--                or ill-conditioned due to improper simplex iteration.
--
-- Note that additional exit codes may appear in the future versions of
-- this routine. */

static void prim_opt_dpy(SPX *spx)
{     /* this auxiliary routine displays information about the current
         basic solution */
      LPX *lp = spx->lp;
      int i, def = 0;
      for (i = 1; i <= lp->m; i++)
         if (lp->typx[lp->indx[i]] == LPX_FX) def++;
      print("*%6d:   objval = %17.9e   infeas = %17.9e (%d)",
         lp->it_cnt, spx_eval_obj(lp), spx_check_bbar(lp, 0.0), def);
      return;
}

int lpx_prim_opt(LPX *lp)
{     /* find optimal solution (primal simplex) */
      SPX *spx = NULL;
      int m = lp->m;
      int n = lp->n;
      int ret;
      double start = utime(), spent = 0.0;
      /* the initial basis should be "warmed up" */
      if (lp->b_stat != LPX_B_VALID ||
          lp->p_stat == LPX_P_UNDEF || lp->d_stat == LPX_D_UNDEF)
      {  ret = LPX_E_BADB;
         goto done;
      }
      /* the initial basic solution should be primal feasible */
      if (lp->p_stat != LPX_P_FEAS)
      {  ret = LPX_E_INFEAS;
         goto done;
      }
      /* if the initial basic solution is dual feasible, nothing to
         search for */
      if (lp->d_stat == LPX_D_FEAS)
      {  ret = LPX_E_OK;
         goto done;
      }
      /* allocate the common block */
      spx = umalloc(sizeof(SPX));
      spx->lp = lp;
      spx->meth = 'P';
      spx->p = 0;
      spx->p_tag = 0;
      spx->q = 0;
      spx->zeta = ucalloc(1+m, sizeof(double));
      spx->ap = ucalloc(1+n, sizeof(double));
      spx->aq = ucalloc(1+m, sizeof(double));
      spx->gvec = ucalloc(1+n, sizeof(double));
      spx->dvec = NULL;
      spx->refsp = (lp->price ? ucalloc(1+m+n, sizeof(int)) : NULL);
      spx->count = 0;
      spx->work = ucalloc(1+m+n, sizeof(double));
      spx->orig_typx = NULL;
      spx->orig_lb = spx->orig_ub = NULL;
      spx->orig_dir = 0;
      spx->orig_coef = NULL;
beg:  /* initialize weights of non-basic variables */
      if (!lp->price)
      {  /* textbook pricing will be used */
         int j;
         for (j = 1; j <= n; j++) spx->gvec[j] = 1.0;
      }
      else
      {  /* steepest edge pricing will be used */
         spx_reset_refsp(spx);
      }
      /* display information about the initial basic solution */
      if (lp->msg_lev >= 2 && lp->it_cnt % lp->out_frq != 0 &&
          lp->out_dly <= spent) prim_opt_dpy(spx);
      /* main loop starts here */
      for (;;)
      {  /* determine the spent amount of time */
         spent = utime() - start;
         /* display information about the current basic solution */
         if (lp->msg_lev >= 2 && lp->it_cnt % lp->out_frq == 0 &&
             lp->out_dly <= spent) prim_opt_dpy(spx);
         /* check if the iterations limit has been exhausted */
         if (lp->it_lim == 0)
         {  ret = LPX_E_ITLIM;
            break;
         }
         /* check if the time limit has been exhausted */
         if (lp->tm_lim >= 0.0 && lp->tm_lim <= spent)
         {  ret = LPX_E_TMLIM;
            break;
         }
         /* choose non-basic variable xN[q] */
         if (spx_prim_chuzc(spx, lp->tol_dj))
         {  /* basic solution components were recomputed; check primal
               feasibility */
            if (spx_check_bbar(lp, lp->tol_bnd) != 0.0)
            {  /* the current solution became primal infeasible due to
                  round-off errors */
               ret = LPX_E_INSTAB;
               break;
            }
         }
         /* if no xN[q] has been chosen, the current basic solution is
            dual feasible and therefore optimal */
         if (spx->q == 0)
         {  ret = LPX_E_OK;
            break;
         }
         /* compute the q-th column of the current simplex table (later
            this column will enter the basis) */
         spx_eval_col(lp, spx->q, spx->aq, 1);
         /* choose basic variable xB[p] */
         if (spx_prim_chuzr(spx, lp->relax * lp->tol_bnd))
         {  /* the basis matrix should be reinverted, because the q-th
               column of the simplex table is unreliable */
            insist("not implemented yet" == NULL);
         }
         /* if no xB[p] has been chosen, the problem is unbounded (has
            no dual feasible solution) */
         if (spx->p == 0)
         {  ret = LPX_E_NOFEAS;
            break;
         }
         /* update values of basic variables */
         spx_update_bbar(spx, NULL);
         if (spx->p > 0)
         {  /* compute the p-th row of the inverse inv(B) */
            spx_eval_rho(lp, spx->p, spx->zeta);
            /* compute the p-th row of the current simplex table */
            spx_eval_row(lp, spx->zeta, spx->ap);
            /* update simplex multipliers */
            spx_update_pi(spx);
            /* update reduced costs of non-basic variables */
            spx_update_cbar(spx, 0);
            /* update weights of non-basic variables */
            if (lp->price) spx_update_gvec(spx);
         }
         /* jump to the adjacent vertex of the LP polyhedron */
         if (spx_change_basis(spx))
         {  /* the basis matrix should be reinverted */
            if (spx_invert(lp) != 0)
            {  /* numerical problems with the basis matrix */
               lp->p_stat = LPX_P_UNDEF;
               lp->d_stat = LPX_D_UNDEF;
               ret = LPX_E_SING;
               goto done;
            }
            /* compute the current basic solution components */
            spx_eval_bbar(lp);
            spx_eval_pi(lp);
            spx_eval_cbar(lp);
            /* check primal feasibility */
            if (spx_check_bbar(lp, lp->tol_bnd) != 0.0)
            {  /* the current solution became primal infeasible due to
                  round-off errors */
               ret = LPX_E_INSTAB;
               break;
            }
         }
#if 0
         /* check accuracy of main solution components after updating
            (for debugging purposes only) */
         {  double ae_bbar = spx_err_in_bbar(spx);
            double ae_pi   = spx_err_in_pi(spx);
            double ae_cbar = spx_err_in_cbar(spx, 0);
            double ae_gvec = lp->price ? spx_err_in_gvec(spx) : 0.0;
            print("bbar: %g; pi: %g; cbar: %g; gvec: %g",
               ae_bbar, ae_pi, ae_cbar, ae_gvec);
            if (ae_bbar > 1e-7 || ae_pi > 1e-7 || ae_cbar > 1e-7 ||
                ae_gvec > 1e-3) fault("solution accuracy too low");
         }
#endif
      }
      /* compute the final basic solution components */
      spx_eval_bbar(lp);
      spx_eval_pi(lp);
      spx_eval_cbar(lp);
      if (spx_check_bbar(lp, lp->tol_bnd) == 0.0)
         lp->p_stat = LPX_P_FEAS;
      else
         lp->p_stat = LPX_P_INFEAS;
      if (spx_check_cbar(lp, lp->tol_dj) == 0.0)
         lp->d_stat = LPX_D_FEAS;
      else
         lp->d_stat = LPX_D_INFEAS;
      /* display information about the final basic solution */
      if (lp->msg_lev >= 2 && lp->it_cnt % lp->out_frq != 0 &&
          lp->out_dly <= spent) prim_opt_dpy(spx);
      /* correct the preliminary diagnosis */
      switch (ret)
      {  case LPX_E_OK:
            /* assumed LPX_P_FEAS and LPX_D_FEAS */
            if (lp->p_stat != LPX_P_FEAS)
               ret = LPX_E_INSTAB;
            else if (lp->d_stat != LPX_D_FEAS)
            {  /* it seems we need to continue the search */
               goto beg;
            }
            break;
         case LPX_E_ITLIM:
         case LPX_E_TMLIM:
            /* assumed LPX_P_FEAS and LPX_D_INFEAS */
            if (lp->p_stat != LPX_P_FEAS)
               ret = LPX_E_INSTAB;
            else if (lp->d_stat == LPX_D_FEAS)
               ret = LPX_E_OK;
            break;
         case LPX_E_NOFEAS:
            /* assumed LPX_P_FEAS and LPX_D_INFEAS */
            if (lp->p_stat != LPX_P_FEAS)
               ret = LPX_E_INSTAB;
            else if (lp->d_stat == LPX_D_FEAS)
               ret = LPX_E_OK;
            else
               lp->d_stat = LPX_D_NOFEAS;
            break;
         case LPX_E_INSTAB:
            /* assumed LPX_P_INFEAS */
            if (lp->p_stat == LPX_P_FEAS)
            {  if (lp->d_stat == LPX_D_FEAS)
                  ret = LPX_E_OK;
               else
               {  /* it seems we need to continue the search */
                  goto beg;
               }
            }
            break;
         default:
            insist(ret != ret);
      }
done: /* deallocate the common block */
      if (spx != NULL)
      {  ufree(spx->zeta);
         ufree(spx->ap);
         ufree(spx->aq);
         ufree(spx->gvec);
         if (lp->price) ufree(spx->refsp);
         ufree(spx->work);
         ufree(spx);
      }
      /* determine the spent amount of time */
      spent = utime() - start;
      /* decrease the time limit by the spent amount */
      if (lp->tm_lim >= 0.0)
      {  lp->tm_lim -= spent;
         if (lp->tm_lim < 0.0) lp->tm_lim = 0.0;
      }
      /* return to the calling program */
      return ret;
}

/*----------------------------------------------------------------------
-- lpx_prim_art - find primal feasible solution (primal simplex).
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_prim_art(LPX *lp);
--
-- *Description*
--
-- The routine lpx_prim_art tries to find primal feasible solution of
-- an LP problem using the method of single artificial variable, which
-- is based on the primal simplex method (see the comments below).
--
-- On entry to the routine the initial basis should be "warmed up".
--
-- *Returns*
--
-- The routine lpx_prim_art returns one of the following exit codes:
--
-- LPX_E_OK       primal feasible solution found.
--
-- LPX_E_NOFEAS   the problem has no primal feasible solution.
--
-- LPX_E_ITLIM    iterations limit exceeded.
--
-- LPX_E_TMLIM    time limit exceeded.
--
-- LPX_E_BADB     the initial basis is not "warmed up".
--
-- LPX_E_INSTAB   numerical instability; the current artificial basic
--                solution (internally constructed by the routine) got
--                primal infeasible due to excessive round-off errors.
--
-- LPX_E_SING     singular basis; the current basis matrix got singular
--                or ill-conditioned due to improper simplex iteration.
--
-- Note that additional exit codes may appear in the future versions of
-- this routine.
--
-- *Algorithm*
--
-- Let the current simplex table be
--
--    xB = A^ * xN,                                                  (1)
--
-- where
--
--    A^ = - inv(B) * N,                                             (2)
--
-- and some basic variables xB violate their (lower or upper) bounds.
-- We can make the current basic solution to be primal feasible if we
-- add some appropriate quantities to each right part of the simplex
-- table:
--
--    xB = A^ * xN + av,                                             (3)
--
-- where
--
--    av[i] = (lB)i - bbar[i] + delta[i], if bbar[i] < (lB)i,        (4)
--
--    av[i] = (uB)i - bbar[i] - delta[i], if bbar[i] > (uB)i,        (5)
--
-- and delta[i] > 0 is a non-negative offset intended to avoid primal
-- degeneracy, because after introducing the vector av basic variable
-- xB[i] is equal to (lB)i + delta[i] or (uB)i - delta[i].
--
-- Formally (3) is equivalent to introducing an artificial variable xv,
-- which is non-basic with the initial value 1 and has the column av as
-- its coefficients in the simplex table:
--
--    xB = A^ * xN + av * xv.                                        (6)
--
-- Multiplying both parts of (6) on B and accounting (2) we have:
--
--    B * xB + N * xN - B * av * xv = 0.                             (7)
--
-- We can consider the column (-B * av) as an additional column of the
-- augmented constraint matrix A~ = (I | -A), or, that is the same, the
-- column (B * av) as an additional column of the original constraint
-- matrix A, which corresponds to the artificial variable xv.
--
-- If the variable xv is non-basic and equal to 1, the artificial basic
-- solution is primal feasible and therefore can be used as an initial
-- solution on the phase I. Thus, in order to find a primal feasible
-- basic solution of the original LP problem, which has no artificial
-- variables, xv should be minimized to zero (if it is impossible, the
-- original problem has no feasible solution). Note also that the value
-- of xv, which is in the range [0,1], can be considered as a measure
-- of primal infeasibility. */

static double orig_objfun(SPX *spx)
{     /* this auxiliary routine computes the objective function value
         for the original LP problem */
      LPX *lp = spx->lp;
      double objval;
      void *t;
      t = lp->coef, lp->coef = spx->orig_coef, spx->orig_coef = t;
      objval = spx_eval_obj(lp);
      t = lp->coef, lp->coef = spx->orig_coef, spx->orig_coef = t;
      return objval;
}

static double orig_infeas(SPX *spx)
{     /* this auxiliary routine computes the infeasibilitiy measure for
         the original LP problem */
      LPX *lp = spx->lp;
      double infeas;
      /* the infeasibility measure is a current value of the artificial
         variable */
      if (lp->tagx[lp->m+lp->n] == LPX_BS)
         infeas = lp->bbar[lp->posx[lp->m+lp->n]];
      else
         infeas = spx_eval_xn_j(lp, lp->posx[lp->m+lp->n] - lp->m);
      return infeas;
}

static void prim_art_dpy(SPX *spx)
{     /* this auxiliary routine displays information about the current
         basic solution */
      LPX *lp = spx->lp;
      int i, def = 0;
      for (i = 1; i <= lp->m; i++)
         if (lp->typx[lp->indx[i]] == LPX_FX) def++;
      print(" %6d:   objval = %17.9e   infeas = %17.9e (%d)",
         lp->it_cnt, orig_objfun(spx), orig_infeas(spx), def);
      return;
}

int lpx_prim_art(LPX *lp)
{     /* find primal feasible solution (primal simplex) */
      SPX *spx = NULL;
      int m = lp->m;
      int n = lp->n;
      int i, j, k, ret, *ndx, final = 0;
      double *av, *col;
      double start = utime(), spent = 0.0;
      /* the initial basis should be "warmed up" */
      if (lp->b_stat != LPX_B_VALID ||
          lp->p_stat == LPX_P_UNDEF || lp->d_stat == LPX_D_UNDEF)
      {  ret = LPX_E_BADB;
         goto done;
      }
      /* if the initial basic solution is primal feasible, nothing to
         search for */
      if (lp->p_stat == LPX_P_FEAS)
      {  ret = LPX_E_OK;
         goto done;
      }
      /* allocate the common block (one extra location in the arrays
         ap, gvec, refsp, work, and coef is reserved for the artificial
         column, which will be introduced to the problem) */
      spx = umalloc(sizeof(SPX));
      spx->lp = lp;
      spx->meth = 'P';
      spx->p = 0;
      spx->p_tag = 0;
      spx->q = 0;
      spx->zeta = ucalloc(1+m, sizeof(double));
      spx->ap = ucalloc(1+n+1, sizeof(double));
      spx->aq = ucalloc(1+m, sizeof(double));
      spx->gvec = ucalloc(1+n+1, sizeof(double));
      spx->dvec = NULL;
      spx->refsp = (lp->price ? ucalloc(1+m+n+1, sizeof(int)) : NULL);
      spx->count = 0;
      spx->work = ucalloc(1+m+n+1, sizeof(double));
      spx->orig_typx = NULL;
      spx->orig_lb = spx->orig_ub = NULL;
      spx->orig_dir = 0;
      spx->orig_coef = ucalloc(1+m+n+1, sizeof(double));
beg:  /* save the original objective function, because it is changed by
         the routine */
      spx->orig_dir = lp->dir;
      memcpy(spx->orig_coef, lp->coef, (1+m+n) * sizeof(double));
      spx->orig_coef[m+n+1] = 0.0;
      /* compute the vector av */
      av = ucalloc(1+m, sizeof(double));
      for (i = 1; i <= m; i++)
      {  double eps = 0.10 * lp->tol_bnd, delta = 100.0, av_i, temp;
         k = lp->indx[i]; /* x[k] = xB[i] */
         av_i = 0.0;
         switch (lp->typx[k])
         {  case LPX_FR:
               /* xB[i] is free variable */
               break;
            case LPX_LO:
               /* xB[i] has lower bound */
               if (lp->bbar[i] < lp->lb[k] - eps)
                  av_i = (lp->lb[k] - lp->bbar[i]) + delta;
               break;
            case LPX_UP:
               /* xB[i] has upper bound */
               if (lp->bbar[i] > lp->ub[k] + eps)
                  av_i = (lp->ub[k] - lp->bbar[i]) - delta;
               break;
            case LPX_DB:
            case LPX_FX:
               /* xB[i] is double-bounded or fixed variable */
               if (lp->bbar[i] < lp->lb[k] - eps)
               {  temp = 0.5 * fabs(lp->lb[k] - lp->ub[k]);
                  if (temp > delta) temp = delta;
                  av_i = (lp->lb[k] - lp->bbar[i]) + temp;
               }
               if (lp->bbar[i] > lp->ub[k] + eps)
               {  temp = 0.5 * fabs(lp->lb[k] - lp->ub[k]);
                  if (temp > delta) temp = delta;
                  av_i = (lp->ub[k] - lp->bbar[i]) - temp;
               }
               break;
            default:
               insist(lp->typx != lp->typx);
         }
         av[i] = av_i;
      }
      /* compute the column B*av */
      ndx = ucalloc(1+m, sizeof(int));
      col = ucalloc(1+m, sizeof(double));
      for (i = 1; i <= m; i++) col[i] = 0.0;
      for (j = 1; j <= m; j++)
      {  int k = lp->indx[j]; /* x[k] = xB[j]; */
         if (k <= m)
         {  /* x[k] is auxiliary variable */
            col[k] += av[j];
         }
         else
         {  /* x[k] is structural variable */
            int ptr = lp->A->ptr[k];
            int end = ptr + lp->A->len[k] - 1;
            for (ptr = ptr; ptr <= end; ptr++)
               col[lp->A->ndx[ptr]] -= lp->A->val[ptr] * av[j];
         }
      }
      /* convert the column B*av to sparse format and "anti-scale" it,
         in order to avoid scaling performed by lpx_set_mat_col */
      k = 0;
      for (i = 1; i <= m; i++)
      {  if (col[i] != 0.0)
         {  k++;
            ndx[k] = i;
            col[k] = col[i] / lp->rs[i];
         }
      }
      /* add the artificial variable and its column to the problem */
      lpx_add_cols(lp, 1), n++;
      lpx_set_col_bnds(lp, n, LPX_DB, 0.0, 1.0);
      lpx_set_mat_col(lp, n, k, ndx, col);
      ufree(av);
      ufree(ndx);
      ufree(col);
      /* set the artificial variable to its upper unity bound in order
         to make the current basic solution primal feasible */
      lp->tagx[m+n] = LPX_NU;
      /* the artificial variable should be minimized to zero */
      lp->dir = LPX_MIN;
      for (k = 0; k <= m+n; k++) lp->coef[k] = 0.0;
      lp->coef[m+n] = 1.0;
      /* since the problem size has been changed, the factorization of
         the basis matrix doesn't exist; reinvert the basis matrix */
      {  int i, j, k;
         i = j = 0;
         for (k = 1; k <= m+n; k++)
         {  if (lp->tagx[k] == LPX_BS)
               i++, lp->posx[k] = i, lp->indx[i] = k;
            else
               j++, lp->posx[k] = m+j, lp->indx[m+j] = k;
         }
         insist(i == m && j == n);
      }
      if (spx_invert(lp) != 0) goto sing;
      /* compute the artificial basic solution components */
      spx_eval_bbar(lp);
      spx_eval_pi(lp);
      spx_eval_cbar(lp);
      /* now the basic solution should be primal feasible */
      insist(spx_check_bbar(lp, lp->tol_bnd) == 0.0);
      /* initialize weights of non-basic variables */
      if (!lp->price)
      {  /* textbook pricing will be used */
         int j;
         for (j = 1; j <= n; j++) spx->gvec[j] = 1.0;
      }
      else
      {  /* steepest edge pricing will be used */
         spx_reset_refsp(spx);
      }
      /* display information about the initial basic solution */
      if (lp->msg_lev >= 2 && lp->it_cnt % lp->out_frq != 0 &&
          lp->out_dly <= spent) prim_art_dpy(spx);
      /* main loop starts here */
      for (;;)
      {  /* determine the spent amount of time */
         spent = utime() - start;
         /* display information about the current basic solution */
         if (lp->msg_lev >= 2 && lp->it_cnt % lp->out_frq == 0 &&
             lp->out_dly <= spent) prim_art_dpy(spx);
         /* we needn't to wait until the artificial variable has left
            the basis */
         if (orig_infeas(spx) < 1e-10)
         {  /* the infeasibility is near to zero, therefore the current
               solution is primal feasible for the original problem */
            ret = LPX_E_OK;
            break;
         }
         /* check if the iterations limit has been exhausted */
         if (lp->it_lim == 0)
         {  ret = LPX_E_ITLIM;
            break;
         }
         /* check if the time limit has been exhausted */
         if (lp->tm_lim >= 0.0 && lp->tm_lim <= spent)
         {  ret = LPX_E_TMLIM;
            break;
         }
         /* choose non-basic variable xN[q] */
         if (spx_prim_chuzc(spx, lp->tol_dj))
         {  /* basic solution components were recomputed; check primal
               feasibility */
            if (spx_check_bbar(lp, lp->tol_bnd) != 0.0)
            {  /* the current solution became primal infeasible due to
                  round-off errors */
               ret = LPX_E_INSTAB;
               break;
            }
         }
         /* if no xN[q] has been chosen, the infeasibility is minimal
            but non-zero; therefore the original problem has no primal
            feasible solution */
         if (spx->q == 0)
         {  ret = LPX_E_NOFEAS;
            break;
         }
         /* compute the q-th column of the current simplex table (later
            this column will enter the basis) */
         spx_eval_col(lp, spx->q, spx->aq, 1);
         /* choose basic variable xB[p] */
         if (spx_prim_chuzr(spx, lp->relax * lp->tol_bnd))
         {  /* the basis matrix should be reinverted, because the q-th
               column of the simplex table is unreliable */
            insist("not implemented yet" == NULL);
         }
         /* the infeasibility can't be negative, therefore the modified
            problem can't have unbounded solution */
         insist(spx->p != 0);
         /* update values of basic variables */
         spx_update_bbar(spx, NULL);
         if (spx->p > 0)
         {  /* compute the p-th row of the inverse inv(B) */
            spx_eval_rho(lp, spx->p, spx->zeta);
            /* compute the p-th row of the current simplex table */
            spx_eval_row(lp, spx->zeta, spx->ap);
            /* update simplex multipliers */
            spx_update_pi(spx);
            /* update reduced costs of non-basic variables */
            spx_update_cbar(spx, 0);
            /* update weights of non-basic variables */
            if (lp->price) spx_update_gvec(spx);
         }
         /* jump to the adjacent vertex of the LP polyhedron */
         if (spx_change_basis(spx))
         {  /* the basis matrix should be reinverted */
            if (spx_invert(lp) != 0)
sing:       {  /* remove the artificial variable from the problem */
               lpx_unmark_all(lp);
               lpx_mark_col(lp, n, 1);
               lpx_del_items(lp);
               /* numerical problems with the basis matrix */
               lp->p_stat = LPX_P_UNDEF;
               lp->d_stat = LPX_D_UNDEF;
               ret = LPX_E_SING;
               goto done;
            }
            /* compute the current basic solution components */
            spx_eval_bbar(lp);
            spx_eval_pi(lp);
            spx_eval_cbar(lp);
            /* check primal feasibility */
            if (spx_check_bbar(lp, lp->tol_bnd) != 0.0)
            {  /* the current solution became primal infeasible due to
                  round-off errors */
               ret = LPX_E_INSTAB;
               break;
            }
         }
#if 0
         /* check accuracy of main solution components after updating
            (for debugging purposes only) */
         {  double ae_bbar = spx_err_in_bbar(spx);
            double ae_pi   = spx_err_in_pi(spx);
            double ae_cbar = spx_err_in_cbar(spx, 0);
            double ae_gvec = lp->price ? spx_err_in_gvec(spx) : 0.0;
            print("bbar: %g; pi: %g; cbar: %g; gvec: %g",
               ae_bbar, ae_pi, ae_cbar, ae_gvec);
            if (ae_bbar > 1e-7 || ae_pi > 1e-7 || ae_cbar > 1e-7 ||
                ae_gvec > 1e-3) fault("solution accuracy too low");
         }
#endif
      }
      /* display information about the final basic solution */
      if (lp->msg_lev >= 2 && lp->it_cnt % lp->out_frq != 0 &&
          lp->out_dly <= spent) prim_art_dpy(spx);
      /* if the artificial variable is still basic, we have to pull it
         from the basis, because the original problem has no artificial
         variables */
      if (lp->tagx[m+n] == LPX_BS)
      {  /* replace the artificial variable by a non-basic variable,
            which has greatest influence coefficient (for the sake of
            numerical stability); it is understood that this operation
            is a dual simplex iteration */
         int j;
         double big;
         spx->p = lp->posx[m+n]; /* x[m+n] = xB[p] */
         insist(1 <= spx->p && spx->p <= m);
         /* the artificial variable will be set on its lower bound */
         spx->p_tag = LPX_NL;
         /* compute the p-th row of the inverse inv(B) */
         spx_eval_rho(lp, spx->p, spx->zeta);
         /* compute the p-th row of the current simplex table */
         spx_eval_row(lp, spx->zeta, spx->ap);
         /* choose non-basic variable xN[q] with greatest (in absolute
            value) influence coefficient */
         spx->q = 0, big = 0.0;
         for (j = 1; j <= n; j++)
         {  if (big < fabs(spx->ap[j]))
               spx->q = j, big = fabs(spx->ap[j]);
         }
         insist(spx->q != 0);
         /* perform forward transformation of the column of xN[q] (to
            prepare it for replacing the artificial column in the basis
            matrix) */
         spx_eval_col(lp, spx->q, spx->aq, 1);
         /* jump to the adjacent basis, where the artificial variable
            is non-basic; note that if the artificial variable being
            basic is close to zero (and therefore the artificial basic
            solution is primal feasible), it is not changed becoming
            non-basic, in which case basic variables are also remain
            unchanged, so the adjacent basis remains primal feasible */
         spx_change_basis(spx);
         insist(lp->tagx[m+n] == LPX_NL);
      }
      /* remove the artificial variable from the problem */
      lpx_unmark_all(lp);
      lpx_mark_col(lp, n, 1);
      lpx_del_items(lp), n--;
      /* restore the original objective function */
      lp->dir = spx->orig_dir;
      memcpy(lp->coef, spx->orig_coef, (1+m+n) * sizeof(double));
      /* since the problem size has been changed, the factorization of
         the basis matrix doesn't exist; reinvert the basis matrix */
      {  int i, j, k;
         i = j = 0;
         for (k = 1; k <= m+n; k++)
         {  if (lp->tagx[k] == LPX_BS)
               i++, lp->posx[k] = i, lp->indx[i] = k;
            else
               j++, lp->posx[k] = m+j, lp->indx[m+j] = k;
         }
         insist(i == m && j == n);
      }
      insist(spx_invert(lp) == 0);
      /* compute the final basic solution components */
      spx_eval_bbar(lp);
      spx_eval_pi(lp);
      spx_eval_cbar(lp);
      if (spx_check_bbar(lp, lp->tol_bnd) == 0.0)
         lp->p_stat = LPX_P_FEAS;
      else
         lp->p_stat = LPX_P_INFEAS;
      if (spx_check_cbar(lp, lp->tol_dj) == 0.0)
         lp->d_stat = LPX_D_FEAS;
      else
         lp->d_stat = LPX_D_INFEAS;
      /* correct the preliminary diagnosis */
      switch (ret)
      {  case LPX_E_OK:
            /* assumed LPX_P_FEAS */
            if (lp->p_stat != LPX_P_FEAS)
               ret = LPX_E_INSTAB;
            break;
         case LPX_E_ITLIM:
         case LPX_E_TMLIM:
            /* assumed LPX_P_INFEAS */
            if (lp->p_stat == LPX_P_FEAS)
               ret = LPX_E_OK;
            break;
         case LPX_E_NOFEAS:
            /* assumed LPX_P_INFEAS */
            if (lp->p_stat == LPX_P_FEAS)
               ret = LPX_E_OK;
            else
            {  /* the problem can have tiny feasible region, which is
                  not always reliably detected by this method; because
                  of that we need to repeat the search before making a
                  final conclusion */
               if (!final)
               {  final = 1;
                  goto beg;
               }
               lp->p_stat = LPX_P_NOFEAS;
            }
            break;
         case LPX_E_INSTAB:
            /* assumed LPX_P_INFEAS */
            if (lp->p_stat == LPX_P_FEAS)
               ret = LPX_E_OK;
            break;
         default:
            insist(ret != ret);
      }
done: /* deallocate the common block */
      if (spx != NULL)
      {  ufree(spx->zeta);
         ufree(spx->ap);
         ufree(spx->aq);
         ufree(spx->gvec);
         if (lp->price) ufree(spx->refsp);
         ufree(spx->work);
         ufree(spx->orig_coef);
         ufree(spx);
      }
      /* determine the spent amount of time */
      spent = utime() - start;
      /* decrease the time limit by the spent amount */
      if (lp->tm_lim >= 0.0)
      {  lp->tm_lim -= spent;
         if (lp->tm_lim < 0.0) lp->tm_lim = 0.0;
      }
      /* return to the calling program */
      return ret;
}

/*----------------------------------------------------------------------
-- lpx_dual_opt - find optimal solution (dual simplex).
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_dual_opt(LPX *lp);
--
-- *Description*
--
-- The routine lpx_dual_opt is intended to find optimal solution of an
-- LP problem using the dual simplex method.
--
-- On entry to the routine the initial basis should be "warmed up" and,
-- moreover, the initial basic solution should be dual feasible.
--
-- Structure of this routine can be an example for other variants based
-- on the dual simplex method.
--
-- *Returns*
--
-- The routine lpx_dual_opt returns one of the following exit codes:
--
-- LPX_E_OK       optimal solution found.
--
-- LPX_E_NOFEAS   the problem has no primal feasible solution.
--
-- LPX_E_OBJLL    the objective function has reached its lower limit
--                and continues decreasing.
--
-- LPX_E_OBJUL    the objective function has reached its upper limit
--                and continues increasing.
--
-- LPX_E_ITLIM    iterations limit exceeded.
--
-- LPX_E_TMLIM    time limit exceeded.
--
-- LPX_E_BADB     the initial basis is not "warmed up".
--
-- LPX_E_INFEAS   the initial basic solution is dual infeasible.
--
-- LPX_E_INSTAB   numerical instability; the current basic solution got
--                primal infeasible due to excessive round-off errors.
--
-- LPX_E_SING     singular basis; the current basis matrix got singular
--                or ill-conditioned due to improper simplex iteration.
--
-- Note that additional exit codes may appear in the future versions of
-- this routine. */

static void dual_opt_dpy(SPX *spx)
{     /* this auxiliary routine displays information about the current
         basic solution */
      LPX *lp = spx->lp;
      int i, def = 0;
      for (i = 1; i <= lp->m; i++)
         if (lp->typx[lp->indx[i]] == LPX_FX) def++;
      print("|%6d:   objval = %17.9e   infeas = %17.9e (%d)",
         lp->it_cnt, spx_eval_obj(lp), spx_check_bbar(lp, 0.0), def);
      return;
}

int lpx_dual_opt(LPX *lp)
{     /* find optimal solution (dual simplex) */
      SPX *spx = NULL;
      int m = lp->m;
      int n = lp->n;
      int ret;
      double start = utime(), spent = 0.0, obj;
      /* the initial basis should be "warmed up" */
      if (lp->b_stat != LPX_B_VALID ||
          lp->p_stat == LPX_P_UNDEF || lp->d_stat == LPX_D_UNDEF)
      {  ret = LPX_E_BADB;
         goto done;
      }
      /* the initial basic solution should be dual feasible */
      if (lp->d_stat != LPX_D_FEAS)
      {  ret = LPX_E_INFEAS;
         goto done;
      }
      /* if the initial basic solution is primal feasible, nothing to
         search for */
      if (lp->p_stat == LPX_P_FEAS)
      {  ret = LPX_E_OK;
         goto done;
      }
      /* allocate common block */
      spx = umalloc(sizeof(SPX));
      spx->lp = lp;
      spx->meth = 'D';
      spx->p = 0;
      spx->p_tag = 0;
      spx->q = 0;
      spx->zeta = ucalloc(1+m, sizeof(double));
      spx->ap = ucalloc(1+n, sizeof(double));
      spx->aq = ucalloc(1+m, sizeof(double));
      spx->gvec = NULL;
      spx->dvec = ucalloc(1+m, sizeof(double));
      spx->refsp = (lp->price ? ucalloc(1+m+n, sizeof(double)) : NULL);
      spx->count = 0;
      spx->work = ucalloc(1+m+n, sizeof(double));
      spx->orig_typx = NULL;
      spx->orig_lb = spx->orig_ub = NULL;
      spx->orig_dir = 0;
      spx->orig_coef = NULL;
beg:  /* compute initial value of the objective function */
      obj = spx_eval_obj(lp);
      /* initialize weights of basic variables */
      if (!lp->price)
      {  /* textbook pricing will be used */
         int i;
         for (i = 1; i <= m; i++) spx->dvec[i] = 1.0;
      }
      else
      {  /* steepest edge pricing will be used */
         spx_reset_refsp(spx);
      }
      /* display information about the initial basic solution */
      if (lp->msg_lev >= 2 && lp->it_cnt % lp->out_frq != 0 &&
          lp->out_dly <= spent) dual_opt_dpy(spx);
      /* main loop starts here */
      for (;;)
      {  /* determine the spent amount of time */
         spent = utime() - start;
         /* display information about the current basic solution */
         if (lp->msg_lev >= 2 && lp->it_cnt % lp->out_frq == 0 &&
             lp->out_dly <= spent) dual_opt_dpy(spx);
         /* if the objective function should be minimized, check if it
            has reached its upper bound */
         if (lp->dir == LPX_MIN && obj >= lp->obj_ul)
         {  ret = LPX_E_OBJUL;
            break;
         }
         /* if the objective function should be maximized, check if it
            has reached its lower bound */
         if (lp->dir == LPX_MAX && obj <= lp->obj_ll)
         {  ret = LPX_E_OBJLL;
            break;
         }
         /* check if the iterations limit has been exhausted */
         if (lp->it_lim == 0)
         {  ret = LPX_E_ITLIM;
            break;
         }
         /* check if the time limit has been exhausted */
         if (lp->tm_lim >= 0.0 && lp->tm_lim <= spent)
         {  ret = LPX_E_TMLIM;
            break;
         }
         /* choose basic variable */
         spx_dual_chuzr(spx, lp->tol_bnd);
         /* if no xB[p] has been chosen, the current basic solution is
            primal feasible and therefore optimal */
         if (spx->p == 0)
         {  ret = LPX_E_OK;
            break;
         }
         /* compute the p-th row of the inverse inv(B) */
         spx_eval_rho(lp, spx->p, spx->zeta);
         /* compute the p-th row of the current simplex table */
         spx_eval_row(lp, spx->zeta, spx->ap);
         /* choose non-basic variable xN[q] */
         if (spx_dual_chuzc(spx, lp->relax * lp->tol_dj))
         {  /* the basis matrix should be reinverted, because the p-th
               row of the simplex table is unreliable */
            insist("not implemented yet" == NULL);
         }
         /* if no xN[q] has been chosen, there is no primal feasible
            solution (the dual problem has unbounded solution) */
         if (spx->q == 0)
         {  ret = LPX_E_NOFEAS;
            break;
         }
         /* compute the q-th column of the current simplex table (later
            this column will enter the basis) */
         spx_eval_col(lp, spx->q, spx->aq, 1);
         /* update values of basic variables and value of the objective
            function */
         spx_update_bbar(spx, &obj);
         /* update simplex multipliers */
         spx_update_pi(spx);
         /* update reduced costs of non-basic variables */
         spx_update_cbar(spx, 0);
         /* update weights of basic variables */
         if (lp->price) spx_update_dvec(spx);
         /* if xB[p] is fixed variable, adjust its non-basic tag */
         if (lp->typx[lp->indx[spx->p]] == LPX_FX) spx->p_tag = LPX_NS;
         /* jump to the adjacent vertex of the LP polyhedron */
         if (spx_change_basis(spx))
         {  /* the basis matrix should be reinverted */
            if (spx_invert(lp) != 0)
            {  /* numerical problems with the basis matrix */
               lp->p_stat = LPX_P_UNDEF;
               lp->d_stat = LPX_D_UNDEF;
               ret = LPX_E_SING;
               goto done;
            }
            /* compute the current basic solution components */
            spx_eval_bbar(lp);
            obj = spx_eval_obj(lp);
            spx_eval_pi(lp);
            spx_eval_cbar(lp);
            /* check dual feasibility */
            if (spx_check_cbar(lp, lp->tol_dj) != 0.0)
            {  /* the current solution became dual infeasible due to
                  round-off errors */
               ret = LPX_E_INSTAB;
               break;
            }
         }
#if 0
         /* check accuracy of main solution components after updating
            (for debugging purposes only) */
         {  double ae_bbar = spx_err_in_bbar(spx);
            double ae_pi   = spx_err_in_pi(spx);
            double ae_cbar = spx_err_in_cbar(spx, 0);
            double ae_dvec = lp->price ? spx_err_in_dvec(spx) : 0.0;
            print("bbar: %g; pi: %g; cbar: %g; dvec: %g",
               ae_bbar, ae_pi, ae_cbar, ae_dvec);
            if (ae_bbar > 1e-9 || ae_pi > 1e-9 || ae_cbar > 1e-9 ||
                ae_dvec > 1e-3)
               insist("solution accuracy too low" == NULL);
         }
#endif
      }
      /* compute the final basic solution components */
      spx_eval_bbar(lp);
      obj = spx_eval_obj(lp);
      spx_eval_pi(lp);
      spx_eval_cbar(lp);
      if (spx_check_bbar(lp, lp->tol_bnd) == 0.0)
         lp->p_stat = LPX_P_FEAS;
      else
         lp->p_stat = LPX_P_INFEAS;
      if (spx_check_cbar(lp, lp->tol_dj) == 0.0)
         lp->d_stat = LPX_D_FEAS;
      else
         lp->d_stat = LPX_D_INFEAS;
      /* display information about the final basic solution */
      if (lp->msg_lev >= 2 && lp->it_cnt % lp->out_frq != 0 &&
          lp->out_dly <= spent) dual_opt_dpy(spx);
      /* correct the preliminary diagnosis */
      switch (ret)
      {  case LPX_E_OK:
            /* assumed LPX_P_FEAS and LPX_D_FEAS */
            if (lp->d_stat != LPX_D_FEAS)
               ret = LPX_E_INSTAB;
            else if (lp->p_stat != LPX_P_FEAS)
            {  /* it seems we need to continue the search */
               goto beg;
            }
            break;
         case LPX_E_OBJLL:
         case LPX_E_OBJUL:
            /* assumed LPX_P_INFEAS and LPX_D_FEAS */
            if (lp->d_stat != LPX_D_FEAS)
               ret = LPX_E_INSTAB;
            else if (lp->p_stat == LPX_P_FEAS)
               ret = LPX_E_OK;
            else if (lp->dir == LPX_MIN && obj < lp->obj_ul ||
                     lp->dir == LPX_MAX && obj > lp->obj_ll)
            {  /* it seems we need to continue the search */
               goto beg;
            }
            break;
         case LPX_E_ITLIM:
         case LPX_E_TMLIM:
            /* assumed LPX_P_INFEAS and LPX_D_FEAS */
            if (lp->d_stat != LPX_D_FEAS)
               ret = LPX_E_INSTAB;
            else if (lp->p_stat == LPX_P_FEAS)
               ret = LPX_E_OK;
            break;
         case LPX_E_NOFEAS:
            /* assumed LPX_P_INFEAS and LPX_D_FEAS */
            if (lp->d_stat != LPX_D_FEAS)
               ret = LPX_E_INSTAB;
            else if (lp->p_stat == LPX_P_FEAS)
               ret = LPX_E_OK;
            else
               lp->p_stat = LPX_P_NOFEAS;
            break;
         case LPX_E_INSTAB:
            /* assumed LPX_D_INFEAS */
            if (lp->d_stat == LPX_D_FEAS)
            {  if (lp->p_stat == LPX_P_FEAS)
                  ret = LPX_E_OK;
               else
               {  /* it seems we need to continue the search */
                  goto beg;
               }
            }
            break;
         default:
            insist(ret != ret);
      }
done: /* deallocate the common block */
      if (spx != NULL)
      {  ufree(spx->zeta);
         ufree(spx->ap);
         ufree(spx->aq);
         ufree(spx->dvec);
         if (lp->price) ufree(spx->refsp);
         ufree(spx->work);
         ufree(spx);
      }
      /* determine the spent amount of time */
      spent = utime() - start;
      /* decrease the time limit by the spent amount */
      if (lp->tm_lim >= 0.0)
      {  lp->tm_lim -= spent;
         if (lp->tm_lim < 0.0) lp->tm_lim = 0.0;
      }
      /* return to the calling program */
      return ret;
}

/*----------------------------------------------------------------------
-- lpx_simplex - easy-to-use driver to the simplex method.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_simplex(LPX *lp);
--
-- *Description*
--
-- The routine lpx_simplex is intended to find optimal solution of an
-- LP problem, which is specified by the parameter lp.
--
-- Currently this routine implements an easy variant of the two-phase
-- primal simplex method, where on the phase I the routine searches for
-- a primal feasible solution, and on the phase II for an optimal one.
-- (However, if the initial basic solution is primal infeasible, but
-- dual feasible, the dual simplex method may be used; see the control
-- parameter LPX_K_DUAL.)
--
-- *Returns*
--
-- If the LP presolver is not used, the routine lpx_simplex returns one
-- of the following exit codes:
--
-- LPX_E_OK       the LP problem has been successfully solved.
--
-- LPX_E_FAULT    either the LP problem has no rows and/or columns, or
--                the initial basis is invalid, or the basis matrix is
--                singular or ill-conditioned.
--
-- LPX_E_OBJLL    the objective function has reached its lower limit
--                and continues decreasing.
--
-- LPX_E_OBJUL    the objective function has reached its upper limit
--                and continues increasing.
--
-- LPX_E_ITLIM    iterations limit exceeded.
--
-- LPX_E_TMLIM    time limit exceeded.
--
-- LPX_E_SING     the basis matrix becomes singular or ill-conditioned
--                due to improper simplex iteration.
--
-- If the LP presolver is used, the routine lpx_simplex returns one of
-- the following exit codes:
--
-- LPX_E_OK       optimal solution of the LP problem has been found.
--
-- LPX_E_FAULT    the LP problem has no rows and/or columns.
--
-- LPX_E_NOPFS    the LP problem has no primal feasible solution.
--
-- LPX_E_NODFS    the LP problem has no dual feasible solution.
--
-- LPX_E_NOSOL    the presolver cannot recover undefined or non-optimal
--                solution.
--
-- LPX_E_ITLIM    same as above.
--
-- LPX_E_TMLIM    same as above.
--
-- LPX_E_SING     same as above.
--
-- Note that additional exit codes may appear in the future versions of
-- this routine. */

#define prefix "lpx_simplex: "

static int simplex1(LPX *lp)
{     /* base driver that doesn't use the presolver */
      int ret;
      /* check that each double-bounded variable has correct lower and
         upper bounds */
      {  int k;
         for (k = 1; k <= lp->m + lp->n; k++)
         {  if (lp->typx[k] == LPX_DB && lp->lb[k] >= lp->ub[k])
            {  if (lp->msg_lev >= 1)
                  print(prefix "double-bounded variable %d has invalid "
                     "bounds", k);
               ret = LPX_E_FAULT;
               goto done;
            }
         }
      }
      /* "warm up" the initial basis */
      ret = lpx_warm_up(lp);
      switch (ret)
      {  case LPX_E_OK:
            break;
         case LPX_E_EMPTY:
            if (lp->msg_lev >= 1)
               print(prefix "problem has no rows/columns");
            ret = LPX_E_FAULT;
            goto done;
         case LPX_E_BADB:
            if (lp->msg_lev >= 1)
               print(prefix "initial basis is invalid");
            ret = LPX_E_FAULT;
            goto done;
         case LPX_E_SING:
            if (lp->msg_lev >= 1)
               print(prefix "initial basis is singular");
            ret = LPX_E_FAULT;
            goto done;
         default:
            insist(ret != ret);
      }
      /* if the initial basic solution is optimal (i.e. primal and dual
         feasible), nothing to search for */
      if (lp->p_stat == LPX_P_FEAS && lp->d_stat == LPX_D_FEAS)
      {  if (lp->msg_lev >= 2 && lp->out_dly == 0.0)
            print("!%6d:   objval = %17.9e   infeas = %17.9e",
               lp->it_cnt, lpx_get_obj_val(lp), 0.0);
         if (lp->msg_lev >= 3)
            print("OPTIMAL SOLUTION FOUND");
         ret = LPX_E_OK;
         goto done;
      }
      /* if the initial basic solution is primal infeasible, but dual
         feasible, the dual simplex method may be used */
      if (lp->d_stat == LPX_D_FEAS && lp->dual) goto dual;
feas: /* phase I: find a primal feasible basic solution */
      ret = lpx_prim_art(lp);
      switch (ret)
      {  case LPX_E_OK:
            goto opt;
         case LPX_E_NOFEAS:
            if (lp->msg_lev >= 3)
               print("PROBLEM HAS NO FEASIBLE SOLUTION");
            ret = LPX_E_OK;
            goto done;
         case LPX_E_ITLIM:
            if (lp->msg_lev >= 3)
               print("ITERATION LIMIT EXCEEDED; SEARCH TERMINATED");
            goto done;
         case LPX_E_TMLIM:
            if (lp->msg_lev >= 3)
               print("TIME LIMIT EXCEEDED; SEARCH TERMINATED");
            goto done;
         case LPX_E_INSTAB:
            if (lp->msg_lev >= 2)
               print(prefix "numerical instability (primal simplex, pha"
                  "se I)");
            goto feas;
         case LPX_E_SING:
            if (lp->msg_lev >= 1)
            {  print(prefix "numerical problems with basis matrix");
               print(prefix "sorry, basis recovery procedure not implem"
                  "ented yet");
            }
            goto done;
         default:
            insist(ret != ret);
      }
opt:  /* phase II: find an optimal basic solution (primal simplex) */
      ret = lpx_prim_opt(lp);
      switch (ret)
      {  case LPX_E_OK:
            if (lp->msg_lev >= 3)
               print("OPTIMAL SOLUTION FOUND");
            goto done;
         case LPX_E_NOFEAS:
            if (lp->msg_lev >= 3)
               print("PROBLEM HAS UNBOUNDED SOLUTION");
            ret = LPX_E_OK;
            goto done;
         case LPX_E_ITLIM:
            if (lp->msg_lev >= 3)
               print("ITERATIONS LIMIT EXCEEDED; SEARCH TERMINATED");
            goto done;
         case LPX_E_TMLIM:
            if (lp->msg_lev >= 3)
               print("TIME LIMIT EXCEEDED; SEARCH TERMINATED");
            goto done;
         case LPX_E_INSTAB:
            if (lp->msg_lev >= 2)
               print(prefix "numerical instability (primal simplex, pha"
                  "se II)");
            goto feas;
         case LPX_E_SING:
            if (lp->msg_lev >= 1)
            {  print(prefix "numerical problems with basis matrix");
               print(prefix "sorry, basis recovery procedure not implem"
                  "ented yet");
            }
            goto done;
         default:
            insist(ret != ret);
      }
dual: /* phase II: find an optimal basic solution (dual simplex) */
      ret = lpx_dual_opt(lp);
      switch (ret)
      {  case LPX_E_OK:
            if (lp->msg_lev >= 3)
               print("OPTIMAL SOLUTION FOUND");
            goto done;
         case LPX_E_NOFEAS:
            if (lp->msg_lev >= 3)
               print("PROBLEM HAS NO FEASIBLE SOLUTION");
            ret = LPX_E_OK;
            goto done;
         case LPX_E_OBJLL:
            if (lp->msg_lev >= 3)
               print("OBJECTIVE LOWER LIMIT REACHED; SEARCH TERMINATED")
                  ;
            goto done;
         case LPX_E_OBJUL:
            if (lp->msg_lev >= 3)
               print("OBJECTIVE UPPER LIMIT REACHED; SEARCH TERMINATED")
                  ;
            goto done;
         case LPX_E_ITLIM:
            if (lp->msg_lev >= 3)
               print("ITERATIONS LIMIT EXCEEDED; SEARCH TERMINATED");
            goto done;
         case LPX_E_TMLIM:
            if (lp->msg_lev >= 3)
               print("TIME LIMIT EXCEEDED; SEARCH TERMINATED");
            goto done;
         case LPX_E_INSTAB:
            if (lp->msg_lev >= 2)
               print(prefix "numerical instability (dual simplex)");
            goto feas;
         case LPX_E_SING:
            if (lp->msg_lev >= 1)
            {  print(prefix "numerical problems with basis matrix");
               print(prefix "sorry, basis recovery procedure not implem"
                  "ented yet");
            }
            goto done;
         default:
            insist(ret != ret);
      }
done: /* return to the calling program */
      return ret;
}

static int simplex2(LPX *orig)
{     /* extended driver that uses the presolver */
      LPP *lpp;
      LPX *prob;
      int k, ret;
      if (orig->msg_lev >= 3)
      {  int m = lpx_get_num_rows(orig);
         int n = lpx_get_num_cols(orig);
         int nnz = lpx_get_num_nz(orig);
         print(prefix "original LP has %d row%s, %d column%s, %d non-ze"
            "ro%s", m, m == 1 ? "" : "s", n, n == 1 ? "" : "s",
            nnz, nnz == 1 ? "" : "s");
      }
      /* the problem must have at least one row and one column */
      if (!(orig->m > 0 && orig->n > 0))
      {  if (orig->msg_lev >= 1)
            print(prefix "problem has no rows/columns");
         return LPX_E_FAULT;
      }
      /* check that each double-bounded variable has correct lower and
         upper bounds */
      for (k = 1; k <= orig->m + orig->n; k++)
      {  if (orig->typx[k] == LPX_DB && orig->lb[k] >= orig->ub[k])
         {  if (orig->msg_lev >= 1)
               print(prefix "double-bounded variable %d has invalid bou"
                  "nds", k);
            return LPX_E_FAULT;
         }
      }
      /* create LP presolver workspace */
      lpp = lpp_create_wksp();
      /* load the original problem into LP presolver workspace */
      lpp_load_orig(lpp, orig);
      /* perform LP presolve analysis */
      ret = lpp_presolve(lpp);
      switch (ret)
      {  case 0:
            /* presolving has been successfully completed */
            break;
         case 1:
            /* the original problem is primal infeasible */
            if (orig->msg_lev >= 3)
               print("PROBLEM HAS NO PRIMAL FEASIBLE SOLUTION");
            lpp_delete_wksp(lpp);
            return LPX_E_NOPFS;
         case 2:
            /* the original problem is dual infeasible */
            if (orig->msg_lev >= 3)
               print("PROBLEM HAS NO DUAL FEASIBLE SOLUTION");
            lpp_delete_wksp(lpp);
            return LPX_E_NODFS;
         default:
            insist(ret != ret);
      }
      /* if the resultant problem is empty, it has an empty solution,
         which is optimal */
      if (lpp->row_ptr == NULL || lpp->col_ptr == NULL)
      {  insist(lpp->row_ptr == NULL);
         insist(lpp->col_ptr == NULL);
         if (orig->msg_lev >= 3)
         {  print("Objective value = %.10g",
               lpp->orig_dir == LPX_MIN ? + lpp->c0 : - lpp->c0);
            print("OPTIMAL SOLUTION FOUND BY LP PRESOLVER");
         }
         /* allocate recovered solution segment */
         lpp_alloc_sol(lpp);
         goto post;
      }
      /* build resultant LP problem object */
      prob = lpp_build_prob(lpp);
      if (orig->msg_lev >= 3)
      {  int m = lpx_get_num_rows(prob);
         int n = lpx_get_num_cols(prob);
         int nnz = lpx_get_num_nz(prob);
         print(prefix "presolved LP has %d row%s, %d column%s, %d non-z"
            "ero%s", m, m == 1 ? "" : "s", n, n == 1 ? "" : "s",
            nnz, nnz == 1 ? "" : "s");
      }
      /* inherit some control parameters from the original object */
      prob->msg_lev = orig->msg_lev;
      prob->scale = orig->scale;
      prob->sc_ord = orig->sc_ord;
      prob->sc_max = orig->sc_max;
      prob->sc_eps = orig->sc_eps;
      prob->dual = orig->dual;
      prob->price = orig->price;
      prob->relax = orig->relax;
      prob->tol_bnd = orig->tol_bnd;
      prob->tol_dj = orig->tol_dj;
      prob->tol_piv = orig->tol_piv;
      prob->round = 0;
      prob->it_lim = orig->it_lim;
      prob->it_cnt = orig->it_cnt;
      prob->tm_lim = orig->tm_lim;
      prob->out_frq = orig->out_frq;
      prob->out_dly = orig->out_dly;
      /* scale the resultant problem */
      lpx_scale_prob(prob);
      /* build advanced initial basis */
      lpx_adv_basis(prob);
      /* try to solve the resultant problem */
      ret = simplex1(prob);
      /* copy back statistics about resources spent by the solver */
      orig->it_cnt = prob->it_cnt;
      orig->it_lim = prob->it_lim;
      orig->tm_lim = prob->tm_lim;
      /* check if an optimal solution has been found */
      if (!(ret == LPX_E_OK &&
            prob->p_stat == LPX_P_FEAS && prob->d_stat == LPX_D_FEAS))
      {  if (orig->msg_lev >= 3)
            print(prefix "cannot recover undefined or non-optimal solut"
               "ion");
         if (ret == LPX_E_OK)
         {  if (prob->p_stat == LPX_P_NOFEAS)
               ret = LPX_E_NOPFS;
            else if (prob->d_stat == LPX_D_NOFEAS)
               ret = LPX_E_NODFS;
         }
         lpx_delete_prob(prob);
         lpp_delete_wksp(lpp);
         return ret;
      }
      /* allocate recovered solution segment */
      lpp_alloc_sol(lpp);
      /* load basic solution of the resultant problem into LP presolver
         workspace */
      lpp_load_sol(lpp, prob);
      /* the resultant problem object is no longer needed */
      lpx_delete_prob(prob);
post: /* perform LP postsolve processing */
      lpp_postsolve(lpp);
      /* unload recovered basic solution and store it into the original
         problem object */
      lpp_unload_sol(lpp, orig);
      /* delete LP presolver workspace */
      lpp_delete_wksp(lpp);
      /* the original problem has been successfully solved */
      return LPX_E_OK;
}

#undef prefix

int lpx_simplex(LPX *lp)
{     /* driver routine to the simplex method */
      return !lp->presol ? simplex1(lp) : simplex2(lp);
}

/*----------------------------------------------------------------------
-- lpx_check_kkt - check Karush-Kuhn-Tucker conditions.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_check_kkt(LPX *lp, int scaled, LPXKKT *kkt);
--
-- *Description*
--
-- The routine lpx_check_kkt checks Karush-Kuhn-Tucker conditions for
-- the current basic solution specified by an LP problem object, which
-- the parameter lp points to. Both primal and dual components of the
-- basic solution should be defined.
--
-- If the parameter scaled is zero, the conditions are checked for the
-- original, unscaled LP problem. Otherwise, if the parameter scaled is
-- non-zero, the routine checks the conditions for an internally scaled
-- LP problem.
--
-- The parameter kkt is a pointer to the structure LPXKKT, to which the
-- routine stores the results of checking (for details see below).
--
-- The routine performs all computations using only components of the
-- given LP problem and the current basic solution.
--
-- *Background*
--
-- The first condition checked by the routine is:
--
--    xR - A * xS = 0,                                          (KKT.PE)
--
-- where xR is the subvector of auxiliary variables (rows), xS is the
-- subvector of structural variables (columns), A is the constraint
-- matrix. This condition expresses the requirement that all primal
-- variables must satisfy to the system of equality constraints of the
-- original LP problem. In case of exact arithmetic this condition is
-- satisfied for any basic solution; however, if the arithmetic is
-- inexact, it shows how accurate the primal basic solution is, that
-- depends on accuracy of a representation of the basis matrix.
--
-- The second condition checked by the routines is:
--
--    l[k] <= x[k] <= u[k]  for all k = 1, ..., m+n,            (KKT.PB)
--
-- where x[k] is auxiliary or structural variable, l[k] and u[k] are,
-- respectively, lower and upper bounds of the variable x[k] (including
-- cases of infinite bounds). This condition expresses the requirement
-- that all primal variables must satisfy to bound constraints of the
-- original LP problem. Since in case of basic solution all non-basic
-- variables are placed on their bounds, actually the condition (KKT.PB)
-- is checked for basic variables only. If the primal basic solution
-- has sufficient accuracy, this condition shows primal feasibility of
-- the solution.
--
-- The third condition checked by the routine is:
--
--    grad Z = c = (A~)' * pi + d,
--
-- where Z is the objective function, c is the vector of objective
-- coefficients, (A~)' is a matrix transposed to the expanded constraint
-- matrix A~ = (I | -A), pi is a vector of Lagrange multiplers that
-- correspond to equality constraints of the original LP problem, d is
-- a vector of Lagrange multipliers that correspond to bound constraints
-- for all (i.e. auxiliary and structural) variables of the original LP
-- problem. Geometrically the third condition expresses the requirement
-- that the gradient of the objective function must belong to the
-- orthogonal complement of a linear subspace defined by the equality
-- and active bound constraints, i.e. that the gradient must be a linear
-- combination of normals to the constraint planes, where Lagrange
-- multiplers pi and d are coefficients of that linear combination. To
-- eliminate the vector pi the third condition can be rewritten as:
--
--    (  I  )      ( dR )   ( cR )
--    (     ) pi + (    ) = (    ),
--    ( -A' )      ( dS )   ( cS )
--
-- or, equivalently:
--
--          pi + dR = cR,
--
--    -A' * pi + dS = cS.
--
-- Substituting the vector pi from the first equation into the second
-- one we have:
--
--    A' * (dR - cR) + (dS - cS) = 0,                           (KKT.DE)
--
-- where dR is the subvector of reduced costs of auxiliary variables
-- (rows), dS is the subvector of reduced costs of structural variables
-- (columns), cR and cS are, respectively, subvectors of objective
-- coefficients at auxiliary and structural variables, A' is a matrix
-- transposed to the constraint matrix of the original LP problem. In
-- case of exact arithmetic this condition is satisfied for any basic
-- solution; however, if the arithmetic is inexact, it shows how
-- accurate the dual basic solution is, that depends on accuracy of a
-- representation of the basis matrix.
--
-- The last, fourth condition checked by the routine is:
--
--           d[k] = 0,    if x[k] is basic or free non-basic
--
--      0 <= d[k] < +inf, if x[k] is non-basic on its lower
--                        (minimization) or upper (maximization)
--                        bound
--                                                              (KKT.DB)
--    -inf < d[k] <= 0,   if x[k] is non-basic on its upper
--                        (minimization) or lower (maximization)
--                        bound
--
--    -inf < d[k] < +inf, if x[k] is non-basic fixed
--
-- for all k = 1, ..., m+n, where d[k] is a reduced cost (i.e. Lagrange
-- multiplier) of auxiliary or structural variable x[k]. Geometrically
-- this condition expresses the requirement that constraints of the
-- original problem must "hold" the point preventing its movement along
-- the antigradient (in case of minimization) or the gradient (in case
-- of maximization) of the objective function. Since in case of basic
-- solution reduced costs of all basic variables are placed on their
-- (zero) bounds, actually the condition (KKT.DB) is checked for
-- non-basic variables only. If the dual basic solution has sufficient
-- accuracy, this condition shows dual feasibility of the solution.
--
-- Should note that the complete set of Karush-Kuhn-Tucker conditions
-- also includes the fifth, so called complementary slackness condition,
-- which expresses the requirement that at least either a primal
-- variable x[k] or its dual conterpart d[k] must be on its bound for
-- all k = 1, ..., m+n. However, being always satisfied for any basic
-- solution by definition that condition is not checked by the routine.
--
-- To check the first condition (KKT.PE) the routine computes a vector
-- of residuals
--
--    g = xR - A * xS,
--
-- determines components of this vector that correspond to largest
-- absolute and relative errors:
--
--    pe_ae_max = max |g[i]|,
--
--    pe_re_max = max |g[i]| / (1 + |xR[i]|),
--
-- and stores these quantities and corresponding row indices to the
-- structure LPXKKT.
--
-- To check the second condition (KKT.PB) the routine computes a vector
-- of residuals
--
--           ( 0,            if lb[k] <= x[k] <= ub[k]
--           |
--    h[k] = < x[k] - lb[k], if x[k] < lb[k]
--           |
--           ( x[k] - ub[k], if x[k] > ub[k]
--
-- for all k = 1, ..., m+n, determines components of this vector that
-- correspond to largest absolute and relative errors:
--
--    pb_ae_max = max |h[k]|,
--
--    pb_re_max = max |h[k]| / (1 + |x[k]|),
--
-- and stores these quantities and corresponding variable indices to
-- the structure LPXKKT.
--
-- To check the third condition (KKT.DE) the routine computes a vector
-- of residuals
--
--    u = A' * (dR - cR) + (dS - cS),
--
-- determines components of this vector that correspond to largest
-- absolute and relative errors:
--
--    de_ae_max = max |u[j]|,
--
--    de_re_max = max |u[j]| / (1 + |dS[j] - cS[j]|),
--
-- and stores these quantities and corresponding column indices to the
-- structure LPXKKT.
--
-- To check the fourth condition (KKT.DB) the routine computes a vector
-- of residuals
--
--           ( 0,    if d[k] has correct sign
--    v[k] = <
--           ( d[k], if d[k] has wrong sign
--
-- for all k = 1, ..., m+n, determines components of this vector that
-- correspond to largest absolute and relative errors:
--
--    db_ae_max = max |v[k]|,
--
--    db_re_max = max |v[k]| / (1 + |d[k] - c[k]|),
--
-- and stores these quantities and corresponding variable indices to
-- the structure LPXKKT. */

void lpx_check_kkt(LPX *lp, int scaled, LPXKKT *kkt)
{     int m = lp->m;
      int n = lp->n;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      double *rs = lp->rs;
      int dir = lp->dir;
      double *coef = lp->coef;
      int *A_ptr = lp->A->ptr;
      int *A_len = lp->A->len;
      int *A_ndx = lp->A->ndx;
      double *A_val = lp->A->val;
      int *tagx = lp->tagx;
      int *posx = lp->posx;
      int *indx = lp->indx;
      double *bbar = lp->bbar;
      double *cbar = lp->cbar;
      int beg, end, i, j, k, t;
      double cR_i, cS_j, c_k, xR_i, xS_j, x_k, dR_i, dS_j, d_k;
      double g_i, h_k, u_j, v_k, temp;
      if (lp->p_stat == LPX_P_UNDEF)
         fault("lpx_check_kkt: primal basic solution is undefined");
      if (lp->d_stat == LPX_D_UNDEF)
         fault("lpx_check_kkt: dual basic solution is undefined");
      /*--------------------------------------------------------------*/
      /* compute largest absolute and relative errors and corresponding
         row indices for the condition (KKT.PE) */
      kkt->pe_ae_max = 0.0, kkt->pe_ae_row = 0;
      kkt->pe_re_max = 0.0, kkt->pe_re_row = 0;
      for (i = 1; i <= m; i++)
      {  /* determine xR[i] */
         if (tagx[i] == LPX_BS)
            xR_i = bbar[posx[i]];
         else
            xR_i = spx_eval_xn_j(lp, posx[i] - m);
         /* g[i] := xR[i] */
         g_i = xR_i;
         /* g[i] := g[i] - (i-th row of A) * xS */
         beg = A_ptr[i];
         end = beg + A_len[i] - 1;
         for (t = beg; t <= end; t++)
         {  j = m + A_ndx[t]; /* a[i,j] != 0 */
            /* determine xS[j] */
            if (tagx[j] == LPX_BS)
               xS_j = bbar[posx[j]];
            else
               xS_j = spx_eval_xn_j(lp, posx[j] - m);
            /* g[i] := g[i] - a[i,j] * xS[j] */
            g_i -= A_val[t] * xS_j;
         }
         /* unscale xR[i] and g[i] (if required) */
         if (!scaled)
            xR_i /= rs[i], g_i /= rs[i];
         /* determine absolute error */
         temp = fabs(g_i);
         if (kkt->pe_ae_max < temp)
            kkt->pe_ae_max = temp, kkt->pe_ae_row = i;
         /* determine relative error */
         temp /= (1.0 + fabs(xR_i));
         if (kkt->pe_re_max < temp)
            kkt->pe_re_max = temp, kkt->pe_re_row = i;
      }
      /* estimate the solution quality */
      if (kkt->pe_re_max <= 1e-9)
         kkt->pe_quality = 'H';
      else if (kkt->pe_re_max <= 1e-6)
         kkt->pe_quality = 'M';
      else if (kkt->pe_re_max <= 1e-3)
         kkt->pe_quality = 'L';
      else
         kkt->pe_quality = '?';
      /*--------------------------------------------------------------*/
      /* compute largest absolute and relative errors and corresponding
         variable indices for the condition (KKT.PB) */
      kkt->pb_ae_max = 0.0, kkt->pb_ae_ind = 0;
      kkt->pb_re_max = 0.0, kkt->pb_re_ind = 0;
      for (i = 1; i <= m; i++)
      {  /* x[k] = xB[i] */
         k = indx[i];
         /* determine x[k] */
         x_k = bbar[i];
         /* compute h[k] */
         h_k = 0.0;
         switch (typx[k])
         {  case LPX_FR:
               break;
            case LPX_LO:
               if (x_k < lb[k]) h_k = x_k - lb[k];
               break;
            case LPX_UP:
               if (x_k > ub[k]) h_k = x_k - ub[k];
               break;
            case LPX_DB:
            case LPX_FX:
               if (x_k < lb[k]) h_k = x_k - lb[k];
               if (x_k > ub[k]) h_k = x_k - ub[k];
               break;
            default:
               insist(typx != typx);
         }
         /* unscale x[k] and h[k] (if required) */
         if (!scaled)
         {  if (k <= m)
               x_k /= rs[k], h_k /= rs[k];
            else
               x_k *= rs[k], h_k *= rs[k];
         }
         /* determine absolute error */
         temp = fabs(h_k);
         if (kkt->pb_ae_max < temp)
            kkt->pb_ae_max = temp, kkt->pb_ae_ind = k;
         /* determine relative error */
         temp /= (1.0 + fabs(x_k));
         if (kkt->pb_re_max < temp)
            kkt->pb_re_max = temp, kkt->pb_re_ind = k;
      }
      /* estimate the solution quality */
      if (kkt->pb_re_max <= 1e-9)
         kkt->pb_quality = 'H';
      else if (kkt->pb_re_max <= 1e-6)
         kkt->pb_quality = 'M';
      else if (kkt->pb_re_max <= 1e-3)
         kkt->pb_quality = 'L';
      else
         kkt->pb_quality = '?';
      /*--------------------------------------------------------------*/
      /* compute largest absolute and relative errors and corresponding
         column indices for the condition (KKT.DE) */
      kkt->de_ae_max = 0.0, kkt->de_ae_col = 0;
      kkt->de_re_max = 0.0, kkt->de_re_col = 0;
      for (j = m+1; j <= m+n; j++)
      {  /* determine cS[j] */
         cS_j = coef[j];
         /* determine dS[j] */
         if (tagx[j] == LPX_BS)
            dS_j = 0.0;
         else
            dS_j = cbar[lp, posx[j] - m];
         /* u[j] := dS[j] - cS[j] */
         u_j = dS_j - cS_j;
         /* u[j] := u[j] + (j-th column of A) * (dR - cR) */
         beg = A_ptr[j];
         end = beg + A_len[j] - 1;
         for (t = beg; t <= end; t++)
         {  i = A_ndx[t]; /* a[i,j] != 0 */
            /* determine cR[i] */
            cR_i = coef[i];
            /* determine dR[i] */
            if (tagx[i] == LPX_BS)
               dR_i = 0.0;
            else
               dR_i= cbar[lp, posx[i] - m];
            /* u[j] := u[j] + a[i,j] * (dR[i] - cR[i]) */
            u_j += A_val[t] * (dR_i - cR_i);
         }
         /* unscale cS[j], dS[j], and u[j] (if required) */
         if (!scaled)
            cS_j /= rs[j], dS_j /= rs[j], u_j /= rs[j];
         /* determine absolute error */
         temp = fabs(u_j);
         if (kkt->de_ae_max < temp)
            kkt->de_ae_max = temp, kkt->de_ae_col = j - m;
         /* determine relative error */
         temp /= (1.0 + fabs(dS_j - cS_j));
         if (kkt->de_re_max < temp)
            kkt->de_re_max = temp, kkt->de_re_col = j - m;
      }
      /* estimate the solution quality */
      if (kkt->de_re_max <= 1e-9)
         kkt->de_quality = 'H';
      else if (kkt->de_re_max <= 1e-6)
         kkt->de_quality = 'M';
      else if (kkt->de_re_max <= 1e-3)
         kkt->de_quality = 'L';
      else
         kkt->de_quality = '?';
      /*--------------------------------------------------------------*/
      /* compute largest absolute and relative errors and corresponding
         variable indices for the condition (KKT.DB) */
      kkt->db_ae_max = 0.0, kkt->db_ae_ind = 0;
      kkt->db_re_max = 0.0, kkt->db_re_ind = 0;
      for (j = m+1; j <= m+n; j++)
      {  /* d[k] = xN[j] */
         k = indx[j];
         /* determine c[k] */
         c_k = coef[k];
         /* determine d[k] */
         d_k = cbar[j-m];
         /* compute v[k] */
         v_k = 0.0;
         switch (tagx[k])
         {  case LPX_NL:
               switch (dir)
               {  case LPX_MIN:
                     if (d_k < 0.0) v_k = d_k;
                     break;
                  case LPX_MAX:
                     if (d_k > 0.0) v_k = d_k;
                     break;
                  default:
                     insist(dir != dir);
               }
               break;
            case LPX_NU:
               switch (dir)
               {  case LPX_MIN:
                     if (d_k > 0.0) v_k = d_k;
                     break;
                  case LPX_MAX:
                     if (d_k < 0.0) v_k = d_k;
                     break;
                  default:
                     insist(dir != dir);
               }
               break;
            case LPX_NF:
               v_k = d_k;
               break;
            case LPX_NS:
               break;
            default:
               insist(tagx != tagx);
         }
         /* unscale c[k], d[k], and v[k] (if required) */
         if (!scaled)
         {  if (k <= m)
               c_k *= rs[k], d_k *= rs[k], v_k *= rs[k];
            else
               c_k /= rs[k], d_k /= rs[k], v_k /= rs[k];
         }
         /* determine absolute error */
         temp = fabs(v_k);
         if (kkt->db_ae_max < temp)
            kkt->db_ae_max = temp, kkt->db_ae_ind = k;
         /* determine relative error */
         temp /= (1.0 + fabs(d_k - c_k));
         if (kkt->db_re_max < temp)
            kkt->db_re_max = temp, kkt->db_re_ind = k;
      }
      /* estimate the solution quality */
      if (kkt->db_re_max <= 1e-9)
         kkt->db_quality = 'H';
      else if (kkt->db_re_max <= 1e-6)
         kkt->db_quality = 'M';
      else if (kkt->db_re_max <= 1e-3)
         kkt->db_quality = 'L';
      else
         kkt->db_quality = '?';
      /* complementary slackness is always satisfied by definition for
         any basic solution, so not checked */
      kkt->cs_ae_max = 0.0, kkt->cs_ae_ind = 0;
      kkt->cs_re_max = 0.0, kkt->cs_re_ind = 0;
      kkt->cs_quality = 'H';
      return;
}

/* eof */
