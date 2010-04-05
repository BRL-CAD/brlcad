/* glplpx6b.c (interior point solver routine) */

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

#include <stddef.h>
#include "glpipm.h"
#include "glplib.h"
#include "glplpx.h"

/*----------------------------------------------------------------------
-- lpx_interior - easy-to-use driver to the interior point method.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_interior(LPX *lp);
--
-- *Description*
--
-- The routine lpx_interior is intended to find optimal solution of an
-- LP problem, which is specified by the parameter lp.
--
-- Currently this routine implements an easy variant of the primal-dual
-- interior point method based on Mehrotra's technique.
--
-- This routine transforms the original LP problem to an equivalent LP
-- problem in the standard formulation (all constraints are equalities,
-- all variables are non-negative), calls the routine ipm1 for solving
-- the transformed problem, and then transforms the obtained solution to
-- the solution of the original problem.
--
-- *Returns*
--
-- The routine lpx_interior returns one of the following exit codes:
--
-- LPX_E_OK       the problem has been successfully solved.
--
-- LPX_E_FAULT    the solver can't start the search because either the
--                problem has no rows and/or no columns or some row has
--                non-zero objective coefficient.
--
-- LPX_E_NOFEAS   the problem has no feasible (primal/dual) solution.
--
-- LPX_E_NOCONV   very slow convergence or divergence.
--
-- LPX_E_ITLIM    iterations limit exceeded.
--
-- LPX_E_INSTAB   numerical instability on solving Newtonian system.
--
-- Note that additional exit codes may appear in the future versions of
-- this routine. */

static int *ref; /* int ref[1+lp->m+lp->n]; */
/* this array contains references from variables of the original LP
   problem to the components of the transformed LP problem */

/* the following five components defines the transformed LP problem in
   the standard form:
   minimize    F = c'*x + c[0]
   subject to  A*x = b, x >= 0 */

static int m;
/* number of constraints */

static int n;
/* number of variables */

static MAT *A; /* MAT A[m,n]; */
/* matrix of constraint coefficients */

static double *b; /* double b[1+m]; */
/* vector of right-hand sides */

static double *c; /* double c[1+n]; */
/* vector of coefficients of the objective function and the constant
   term c[0] */

/* the following three arrays defines a solution of the transformed LP
   problem obtained by the interior point solver */

static double *x; /* double x[1+n]; */
/* values of primal variables */

static double *y; /* double y[1+m]; */
/* values of dual variables for equality constraints */

static double *z; /* double z[1+n]; */
/* values of dual variables for non-negativity constraints */

/*----------------------------------------------------------------------
-- calc_mn - determine dimension of the transformed LP problem.
--
-- This routine calculates number of rows (m) and number of columns (n)
-- in the transformed LP. */

static void calc_mn(LPX *lp)
{     int k;
      m = n = 0;
      for (k = 1; k <= lp->m; k++)
      {  switch (lp->typx[k])
         {  case LPX_FR:                  break;
            case LPX_LO: m++; n++;        break;
            case LPX_UP: m++; n++;        break;
            case LPX_DB: m += 2; n += 2;  break;
            case LPX_FX: m++;             break;
            default: insist(lp->typx != lp->typx);
         }
      }
      for (k = lp->m+1; k <= lp->m+lp->n; k++)
      {  switch (lp->typx[k])
         {  case LPX_FR: n += 2;          break;
            case LPX_LO: n++;             break;
            case LPX_UP: n++;             break;
            case LPX_DB: m++; n += 2;     break;
            case LPX_FX:                  break;
            default: insist(lp->typx != lp->typx);
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- transform - transform original LP to the standard formulation.
--
-- This routine transforms the original LP problem to the standard
-- formulation. */

static void transform(LPX *lp)
{     int orig_m = lp->m;
      int orig_n = lp->n;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      int *A_ptr = lp->A->ptr;
      int *A_len = lp->A->len;
      int *A_ndx = lp->A->ndx;
      double *A_val = lp->A->val;
      int i, j, k, t, beg, end, ptr;
      double *coef = ucalloc(1+orig_m+orig_n, sizeof(double));
      /* substitute auxiliary variables into the objective function
         row to eliminate non-zero objective coefficients at auxiliary
         variables */
      for (k = 0; k <= orig_m+orig_n; k++) coef[k] = lp->coef[k];
      for (i = 1; i <= orig_m; i++)
      {  beg = A_ptr[i];
         end = beg + A_len[i] - 1;
         for (ptr = beg; ptr <= end; ptr++)
         {  j = orig_m + A_ndx[ptr];
            coef[j] += coef[i] * A_val[ptr];
         }
         coef[i] = 0.0;
      }
      /* initialize components of the transformed LP */
      clear_mat(A);
      for (i = 1; i <= m; i++) b[i] = 0.0;
      c[0] = coef[0];
      for (j = 1; j <= n; j++) c[j] = 0.0;
      /* i and j are, respectively, the number of the current row and
         the number of the current column in the transformed LP */
      i = j = 0;
      /* transform rows (auxiliary variables) */
      for (k = 1; k <= orig_m; k++)
      {  switch (typx[k])
         {  case LPX_FR:
               /* source: -inf < (L.F.) < +inf */
               /* result: ignore free row */
               ref[k] = 0;
               break;
            case LPX_LO:
               /* source: lb <= (L.F.) < +inf */
               /* result: (L.F.) - x' = lb, x' >= 0 */
               i++; j++;
               ref[k] = i;
               new_elem(A, i, j, -1.0);
               b[i] = lb[k];
               break;
            case LPX_UP:
               /* source: -inf < (L.F.) <= ub */
               /* result: (L.F.) + x' = ub, x' >= 0 */
               i++; j++;
               ref[k] = i;
               new_elem(A, i, j, +1.0);
               b[i] = ub[k];
               break;
            case LPX_DB:
               /* source: lb <= (L.F.) <= ub */
               /* result: (L.F.) - x' = lb, x' + x'' = ub - lb */
               i++; j++;
               ref[k] = i;
               new_elem(A, i, j, -1.0);
               b[i] = lb[k];
               i++;
               new_elem(A, i, j, +1.0);
               j++;
               new_elem(A, i, j, +1.0);
               b[i] = ub[k] - lb[k];
               break;
            case LPX_FX:
               /* source: (L.F.) = lb */
               /* result: (L.F.) = lb */
               i++;
               ref[k] = i;
               b[i] = lb[k];
               break;
            default:
               insist(typx != typx);
         }
      }
      /* transform columns (structural variables) */
      for (k = orig_m+1; k <= orig_m+orig_n; k++)
      {  switch (typx[k])
         {  case LPX_FR:
               /* source: -inf < x < +inf */
               /* result: x = x' - x'', x' >= 0, x'' >= 0 */
               j++;
               ref[k] = j;
               beg = A_ptr[k];
               end = beg + A_len[k] - 1;
               for (ptr = beg; ptr <= end; ptr++)
               {  t = ref[A_ndx[ptr]];
                  if (t != 0) new_elem(A, t, j, +A_val[ptr]);
               }
               c[j] = +coef[k];
               j++;
               for (ptr = beg; ptr <= end; ptr++)
               {  t = ref[A_ndx[ptr]];
                  if (t != 0) new_elem(A, t, j, -A_val[ptr]);
               }
               c[j] = -coef[k];
               break;
            case LPX_LO:
               /* source: lb <= x < +inf */
               /* result: x = lb + x', x' >= 0 */
               j++;
               ref[k] = j;
               beg = A_ptr[k];
               end = beg + A_len[k] - 1;
               for (ptr = beg; ptr <= end; ptr++)
               {  t = ref[A_ndx[ptr]];
                  if (t != 0)
                  {  new_elem(A, t, j, A_val[ptr]);
                     b[t] -= A_val[ptr] * lb[k];
                  }
               }
               c[j] = coef[k];
               c[0] += c[j] * lb[k];
               break;
            case LPX_UP:
               /* source: -inf < x <= ub */
               /* result: x = ub - x', x' >= 0 */
               j++;
               ref[k] = j;
               beg = A_ptr[k];
               end = beg + A_len[k] - 1;
               for (ptr = beg; ptr <= end; ptr++)
               {  t = ref[A_ndx[ptr]];
                  if (t != 0)
                  {  new_elem(A, t, j, -A_val[ptr]);
                     b[t] -= A_val[ptr] * ub[k];
                  }
               }
               c[j] = -coef[k];
               c[0] -= c[j] * ub[k];
               break;
            case LPX_DB:
               /* source: lb <= x <= ub */
               /* result: x = lb + x', x' + x'' = ub - lb */
               j++;
               ref[k] = j;
               beg = A_ptr[k];
               end = beg + A_len[k] - 1;
               for (ptr = beg; ptr <= end; ptr++)
               {  t = ref[A_ndx[ptr]];
                  if (t != 0)
                  {  new_elem(A, t, j, A_val[ptr]);
                     b[t] -= A_val[ptr] * lb[k];
                  }
               }
               c[j] = coef[k];
               c[0] += c[j] * lb[k];
               i++;
               new_elem(A, i, j, +1.0);
               j++;
               new_elem(A, i, j, +1.0);
               b[i] = ub[k] - lb[k];
               break;
            case LPX_FX:
               /* source: x = lb */
               /* result: just substitute */
               ref[k] = 0;
               beg = A_ptr[k];
               end = beg + A_len[k] - 1;
               for (ptr = beg; ptr <= end; ptr++)
               {  t = ref[A_ndx[ptr]];
                  if (t != 0) b[t] -= A_val[ptr] * lb[k];
               }
               c[0] += coef[k] * lb[k];
               break;
            default:
               insist(typx != typx);
         }
      }
      insist(i == m && j == n);
      /* change sign of the objective func in case of maximization */
      if (lp->dir == LPX_MAX)
         for (j = 0; j <= n; j++) c[j] = -c[j];
      /* end of transformation */
      ufree(coef);
      return;
}

/*----------------------------------------------------------------------
-- restore - restore solution of the original LP problem.
--
-- This routine restores solution of the original LP problem using the
-- solution of the transformed LP problem obtained by the solver. */

static void restore(LPX *lp)
{     int orig_m = lp->m;
      int orig_n = lp->n;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      int *A_ptr = lp->A->ptr;
      int *A_len = lp->A->len;
      int *A_ndx = lp->A->ndx;
      double *A_val = lp->A->val;
      int dir = lp->dir;
      double *pv = lp->pv;
      double *dv = lp->dv;
      int i, j, k, beg, end, ptr;
      /* compute primal values of structural variables */
      for (k = orig_m+1; k <= orig_m+orig_n; k++)
      {  j = ref[k];
         switch (typx[k])
         {  case LPX_FR:
               /* source: -inf < x < +inf */
               /* result: x = x' - x'', x' >= 0, x'' >= 0 */
               pv[k] = x[j] - x[j+1];
               break;
            case LPX_LO:
               /* source: lb <= x < +inf */
               /* result: x = lb + x', x' >= 0 */
               pv[k] = lb[k] + x[j];
               break;
            case LPX_UP:
               /* source: -inf < x <= ub */
               /* result: x = ub - x', x' >= 0 */
               pv[k] = ub[k] - x[j];
               break;
            case LPX_DB:
               /* source: lb <= x <= ub */
               /* result: x = lb + x', x' + x'' = ub - lb */
               pv[k] = lb[k] + x[j];
               break;
            case LPX_FX:
               /* source: x = lb */
               /* result: just substitute */
               pv[k] = lb[k];
               break;
            default:
               insist(typx != typx);
         }
      }
      /* compute primal values of auxiliary variables */
      /* xR = A * xS */
      for (k = 1; k <= orig_m; k++)
      {  pv[k] = 0.0;
         beg = A_ptr[k];
         end = beg + A_len[k] - 1;
         for (ptr = beg; ptr <= end; ptr++)
            pv[k] += A_val[ptr] * pv[orig_m + A_ndx[ptr]];
      }
      /* compute dual values of auxiliary variables */
      for (k = 1; k <= orig_m; k++)
      {  i = ref[k];
         switch (typx[k])
         {  case LPX_FR:
               insist(i == 0);
               dv[k] = 0.0;
               break;
            case LPX_LO:
            case LPX_UP:
            case LPX_DB:
            case LPX_FX:
               insist(1 <= i && i <= m);
               switch (dir)
               {  case LPX_MIN:
                     dv[k] = + y[i];
                     break;
                  case LPX_MAX:
                     dv[k] = - y[i];
                     break;
                  default:
                     insist(dir != dir);
               }
               break;
            default:
               insist(typx != typx);
         }
      }
      /* compute dual values of structural variables */
      /* dS = cS - A' * (dR - cR) */
      for (k = orig_m+1; k <= orig_m+orig_n; k++)
      {  dv[k] = lp->coef[k];
         beg = A_ptr[k];
         end = beg + A_len[k] - 1;
         for (ptr = beg; ptr <= end; ptr++)
         {  int i = A_ndx[ptr];
            dv[k] -= A_val[ptr] * (dv[i] - lp->coef[i]);
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_interior - easy-to-use driver to the interior point method.
--
-- This is a main driver routine for solving an LP problem specified by
-- the parameter lp by means of the interior point method. */

int lpx_interior(LPX *lp)
{     int orig_m = lp->m;
      int orig_n = lp->n;
      int ret;
#     define prefix "lpx_interior: "
      /* check if the problem is empty */
      if (!(orig_m > 0 && orig_n > 0))
      {  fault(prefix "problem has no rows/columns");
         ret = LPX_E_FAULT;
         goto done;
      }
      /* determine dimension of the transformed LP problem */
      print(prefix "original LP problem has %d rows and %d columns",
         orig_m, orig_n);
      calc_mn(lp);
      print(prefix "transformed LP problem has %d rows and %d columns",
         m, n);
      /* allocate working arrays */
      ref = ucalloc(1+orig_m+orig_n, sizeof(int));
      A = create_mat(m, n);
      b = ucalloc(1+m, sizeof(double));
      c = ucalloc(1+n, sizeof(double));
      x = ucalloc(1+n, sizeof(double));
      y = ucalloc(1+m, sizeof(double));
      z = ucalloc(1+n, sizeof(double));
      /* transform original LP to the standard formulation */
      transform(lp);
      /* solve the transformed LP problem */
      ret = ipm1(A, b, c, x, y, z);
      /* analyze return code reported by the solver */
      switch (ret)
      {  case 0:
            /* optimal solution found */
            ret = LPX_E_OK;
            break;
         case 1:
            /* problem has no feasible (primal or dual) solution */
            ret = LPX_E_NOFEAS;
            break;
         case 2:
            /* no convergence */
            ret = LPX_E_NOCONV;
            break;
         case 3:
            /* iterations limit exceeded */
            ret = LPX_E_ITLIM;
            break;
         case 4:
            /* numerical instability on solving Newtonian system */
            ret = LPX_E_INSTAB;
            break;
         default:
            insist(ret != ret);
      }
      /* set solution status */
      if (ret == LPX_E_OK)
      {  /* solution is optimal */
         lp->t_stat = LPX_T_OPT;
         if (lp->pv == NULL)
            lp->pv = ucalloc(1+lp->m_max+lp->n_max, sizeof(double));
         if (lp->dv == NULL)
            lp->dv = ucalloc(1+lp->m_max+lp->n_max, sizeof(double));
         /* restore solution components */
         restore(lp);
      }
      else
      {  /* solution is undefined */
         lp->t_stat = LPX_T_UNDEF;
         if (lp->pv != NULL) ufree(lp->pv), lp->pv = NULL;
         if (lp->dv != NULL) ufree(lp->dv), lp->dv = NULL;
      }
      /* free working arrays */
      ufree(ref);
      delete_mat(A);
      ufree(b);
      ufree(c);
      ufree(x);
      ufree(y);
      ufree(z);
done: /* return to the calling program */
      return ret;
#     undef prefix
}

/* eof */
