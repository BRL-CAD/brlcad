/* glplpx7.c (simplex table routines) */

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
#include "glpspx.h"

/*----------------------------------------------------------------------
-- lpx_eval_tab_row - compute row of the simplex table.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_eval_tab_row(LPX *lp, int k, int ndx[], double val[]);
--
-- *Description*
--
-- The routine lpx_eval_tab_row computes a row of the current simplex
-- table for the basic variable, which is specified by the number k:
-- if 1 <= k <= m, x[k] is k-th auxiliary variable; if m+1 <= k <= m+n,
-- x[k] is (k-m)-th structural variable, where m is number of rows, and
-- n is number of columns. The current basis must be valid.
--
-- The routine stores column indices and numerical values of non-zero
-- elements of the computed row using sparse format to the locations
-- ndx[1], ..., ndx[len] and val[1], ..., val[len] respectively, where
-- 0 <= len <= n is number of non-zeros returned on exit.
--
-- Element indices stored in the array ndx have the same sense as the
-- index k, i.e. indices 1 to m denote auxiliary variables and indices
-- m+1 to m+n denote structural ones (all these variables are obviously
-- non-basic by the definition).
--
-- The computed row shows how the specified basic variable x[k] = xB[i]
-- depends on non-basic variables:
--
--    xB[i] = alfa[i,1]*xN[1] + alfa[i,2]*xN[2] + ... + alfa[i,n]*xN[n],
--
-- where alfa[i,j] are elements of the simplex table row, xN[j] are
-- non-basic (auxiliary and structural) variables.
--
-- Even if the problem is (internally) scaled, the routine returns the
-- specified simplex table row as if the problem were unscaled.
--
-- *Returns*
--
-- The routine returns number of non-zero elements in the simplex table
-- row stored in the arrays ndx and val.
--
-- *Unscaling*
--
-- Let A~ = (I | -A) is the augmented constraint matrix of the original
-- (unscaled) problem. In the scaled LP problem instead the matrix A the
-- scaled matrix A" = R*A*S is actually used, so
--
--    A~" = (I | A") = (I | R*A*S) = (R*I*inv(R) | R*A*S) =
--                                                                   (1)
--        = R*(I | A)*S~ = R*A~*S~,
--
-- is the scaled augmented constraint matrix, where R and S are diagonal
-- scaling matrices used to scale rows and columns of the matrix A, and
--
--    S~ = diag(inv(R) | S)                                          (2)
--
-- is an augmented diagonal scaling matrix.
--
-- By the definition the simplex table is the matrix
--
--    T = - inv(B) * N,                                              (3)
--
-- where B is the basis matrix that consists of basic columns of the
-- augmented constraint matrix A~, and N is a matrix that consists of
-- non-basic columns of A~. From (1) it follows that
--
--    A~" = (B" | N") = (R*B*SB | R*N*SN),                           (4)
--
-- where SB and SN are parts of the augmented scaling matrix S~ that
-- correspond to basic and non-basic variables respectively. Thus, in
-- the scaled case
--
--    T" = - inv(B") * N" = - inv(R*B*SB) * (R*N*SN) =
--
--       = - inv(SB)*inv(B)*inv(R) * (R*N*SN) =                      (5)
--
--       = inv(SB) * (-inv(B) * N) * SN = inv(SB) * T * SN,
--
-- that allows us expressing the original simplex table T through the
-- scaled simplex table T":
--
--    T = SB * T" * inv(SN).                                         (6)
--
-- The formula (6) is used by the routine for unscaling elements of the
-- computed simplex table row. */

int lpx_eval_tab_row(LPX *lp, int k, int ndx[], double val[])
{     int m = lp->m, n = lp->n;
      int i, j, len;
      double *rho, *row, sb_i, sn_j;
      if (!(1 <= k && k <= m+n))
         fault("lpx_eval_tab_row: k = %d; variable number out of range",
            k);
      if (lp->b_stat != LPX_B_VALID)
         fault("lpx_eval_tab_row: current basis is undefined");
      if (lp->tagx[k] != LPX_BS)
         fault("lpx_eval_tab_row: k = %d; variable should be basic", k);
      i = lp->posx[k]; /* x[k] = xB[i] */
      insist(1 <= i && i <= m);
      /* allocate working arrays */
      rho = ucalloc(1+m, sizeof(double));
      row = ucalloc(1+n, sizeof(double));
      /* compute i-th row of the inverse inv(B) */
      spx_eval_rho(lp, i, rho);
      /* compute i-th row of the simplex table */
      spx_eval_row(lp, rho, row);
      /* unscale and store the required row */
      sb_i = (k <= m ? 1.0 / lp->rs[k] : lp->rs[k]);
      len = 0;
      for (j = 1; j <= n; j++)
      {  if (row[j] != 0.0)
         {  k = lp->indx[m+j]; /* x[k] = xN[j] */
            sn_j = (k <= m ? 1.0 / lp->rs[k] : lp->rs[k]);
            len++;
            ndx[len] = k;
            val[len] = (sb_i / sn_j) * row[j];
         }
      }
      /* free working arrays */
      ufree(rho);
      ufree(row);
      /* return to the calling program */
      return len;
}

/*----------------------------------------------------------------------
-- lpx_eval_tab_col - compute column of the simplex table.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_eval_tab_col(LPX *lp, int k, int ndx[], double val[]);
--
-- *Description*
--
-- The routine lpx_eval_tab_col computes a column of the current simplex
-- table for the non-basic variable, which is specified by the number k:
-- if 1 <= k <= m, x[k] is k-th auxiliary variable; if m+1 <= k <= m+n,
-- x[k] is (k-m)-th structural variable, where m is number of rows, and
-- n is number of columns. The current basis must be valid.
--
-- The routine stores row indices and numerical values of non-zero
-- elements of the computed column using sparse format to the locations
-- ndx[1], ..., ndx[len] and val[1], ..., val[len] respectively, where
-- 0 <= len <= m is number of non-zeros returned on exit.
--
-- Element indices stored in the array ndx have the same sense as the
-- index k, i.e. indices 1 to m denote auxiliary variables and indices
-- m+1 to m+n denote structural ones (all these variables are obviously
-- basic by the definition).
--
-- The computed column shows how basic variables depend on the specified
-- non-basic variable x[k] = xN[j]:
--
--    xB[1] = ... + alfa[1,j]*xN[j] + ...
--    xB[2] = ... + alfa[2,j]*xN[j] + ...
--             . . . . . .
--    xB[m] = ... + alfa[m,j]*xN[j] + ...
--
-- where alfa[i,j] are elements of the simplex table column, xB[i] are
-- basic (auxiliary and structural) variables.
--
-- Even if the problem is (internally) scaled, the routine returns the
-- specified simplex table column as if the problem were unscaled.
--
-- *Returns*
--
-- The routine returns number of non-zero elements in the simplex table
-- column stored in the arrays ndx and val.
--
-- *Unscaling*
--
-- The routine unscales elements of the computed simplex table column
-- using the formula (6), which is derived and explained in description
-- of the routine lpx_eval_tab_row (see above). */

int lpx_eval_tab_col(LPX *lp, int k, int ndx[], double val[])
{     int m = lp->m, n = lp->n;
      int i, j, len;
      double *col, sb_i, sn_j;
      if (!(1 <= k && k <= m+n))
         fault("lpx_eval_tab_col: k = %d; variable number out of range",
            k);
      if (lp->b_stat != LPX_B_VALID)
         fault("lpx_eval_tab_col: current basis is undefined");
      if (lp->tagx[k] == LPX_BS)
         fault("lpx_eval_tab_col; k = %d; variable should be non-basic",
            k);
      j = lp->posx[k] - m; /* x[k] = xN[j] */
      insist(1 <= j && j <= n);
      /* allocate working array */
      col = ucalloc(1+m, sizeof(double));
      /* compute j-th column of the simplex table */
      spx_eval_col(lp, j, col, 0);
      /* unscale and store the required column */
      sn_j = (k <= m ? 1.0 / lp->rs[k] : lp->rs[k]);
      len = 0;
      for (i = 1; i <= m; i++)
      {  if (col[i] != 0.0)
         {  k = lp->indx[i]; /* x[k] = xB[i] */
            sb_i = (k <= m ? 1.0 / lp->rs[k] : lp->rs[k]);
            len++;
            ndx[len] = k;
            val[len] = (sb_i / sn_j) * col[i];
         }
      }
      /* free working array */
      ufree(col);
      /* return to the calling program */
      return len;
}

/*----------------------------------------------------------------------
-- lpx_transform_row - transform explicitly specified row.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_transform_row(LPX *lp, int len, int ndx[], double val[]);
--
-- *Description*
--
-- The routine lpx_transform_row performs the same operation as the
-- routine lpx_eval_tab_row, except that the transformed row should be
-- specified explicitly.
--
-- The explicitly specified row may be thought as a linear form
--
--    x = a[1]*x[m+1] + a[2]*x[m+2] + ... + a[n]*x[m+n],             (1)
--
-- where x is an auxiliary variable for this row, a[j] are coefficients
-- of the linear form, x[m+j] are structural variables.
--
-- On entry column indices and numerical values of non-zero elements of
-- the transformed row should be in locations ndx[1], ..., ndx[len] and
-- val[1], ..., val[len], where len is number of non-zero elements.
--
-- This routine uses the system of equality constraints and the current
-- basis in order to express the auxiliary variable x in (1) through the
-- current non-basic variables (as if the transformed row were added to
-- the problem object and the auxiliary variable were basic), i.e. the
-- resultant row has the form:
--
--    x = alfa[1]*xN[1] + alfa[2]*xN[2] + ... + alfa[n]*xN[n],       (2)
--
-- where xN[j] are non-basic (auxiliary or structural) variables, n is
-- number of columns in the specified problem object.
--
-- On exit the routine stores indices and numerical values of non-zero
-- elements of the resultant row (2) in locations ndx[1], ..., ndx[len']
-- and val[1], ..., val[len'], where 0 <= len' <= n is count of non-zero
-- elements in the resultant row returned by the routine. Note that
-- indices of non-basic variables stored in the array ndx correspond to
-- original ordinal numbers of variables: indices 1 to m mean auxiliary
-- variables and indices m+1 to m+n mean structural ones.
--
-- Even if the problem is (internally) scaled, the routine returns the
-- resultant row as if the problem were unscaled.
--
-- *Returns*
--
-- The routine returns len' that is number of non-zero elements in the
-- resultant row stored in the arrays ndx and val.
--
-- *Algorithm*
--
-- The explicitly specified row (1) is transformed in the same way as
-- it were the objective function row. Rewrite (1) as
--
--    x = aB' * xB + aN' * xN,                                       (3)
--
-- where aB is subvector of a at basic variables xB and aN is subvector
-- of a at non-basic variables xN. The simplex table that correspond to
-- the current basis has the form
--
--    xB = [-inv(B) * N] * xN.                                       (4)
--
-- Therefore substituting xB from (4) to (3) we have
--
--    x = aB' * [-inv(B) * N] * xN + aN' * xN =
--                                                                   (5)
--      = (aN' - aB' * inv(B) * N) * xN = alfa' * xN,
--
-- where
--
--    alfa = aN - N' * inv(B') * aB                                  (6)
--
-- is the resultant row computed by the routine. */

int lpx_transform_row(LPX *lp, int len, int ndx[], double val[])
{     int m = lp->m;
      int n = lp->n;
      double *rs = lp->rs;
      int *A_ptr = lp->A->ptr;
      int *A_len = lp->A->len;
      int *A_ndx = lp->A->ndx;
      double *A_val = lp->A->val;
      int *tagx = lp->tagx;
      int *posx = lp->posx;
      int *indx = lp->indx;
      int i, j, k, t, beg, end, ptr;
      double *v, *alfa;
      /* check the explicitly specified row */
      if (!(0 <= len && len <= n))
         fault("lpx_transform_row: len = %d; invalid row length", len);
      for (t = 1; t <= len; t++)
      {  j = ndx[t];
         if (!(1 <= j && j <= n))
            fault("lpx_transform_row: ndx[%d] = %d; column number out o"
               "f range", t, j);
      }
      /* the current basis should be valid */
      if (lp->b_stat != LPX_B_VALID)
         fault("lpx_transform_row: current basis is undefined");
      /* v := aB (scaled) */
      v = ucalloc(1+m, sizeof(double));
      for (i = 1; i <= m; i++) v[i] = 0.0;
      for (t = 1; t <= len; t++)
      {  j = ndx[t];
         if (tagx[m+j] == LPX_BS)
         {  i = posx[m+j]; /* xB[i] = xS[j] */
            v[i] += val[t] * rs[m+j];
         }
      }
      /* v := inv(B') * aB (scaled) */
      spx_btran(lp, v);
      /* alfa := aN (scaled) */
      alfa = ucalloc(1+n, sizeof(double));
      for (j = 1; j <= n; j++) alfa[j] = 0.0;
      for (t = 1; t <= len; t++)
      {  j = ndx[t];
         if (tagx[m+j] != LPX_BS)
         {  k = posx[m+j]; /* xN[k] = xS[j] */
            alfa[k-m] = val[t] * rs[m+j];
         }
      }
      /* alfa := alfa - N' * inv(B') * aB (scaled) */
      for (j = 1; j <= n; j++)
      {  k = indx[m+j]; /* x[k] = xN[j] */
         if (k <= m)
         {  /* x[k] is auxiliary variable */
            alfa[j] -= v[k];
         }
         else
         {  /* x[k] is structural variable */
            beg = A_ptr[k];
            end = beg + A_len[k] - 1;
            for (ptr = beg; ptr <= end; ptr++)
               alfa[j] += v[A_ndx[ptr]] * A_val[ptr];
         }
      }
      /* unscale alfa and store it in sparse format */
      len = 0;
      for (j = 1; j <= n; j++)
      {  if (alfa[j] != 0.0)
         {  k = indx[m+j]; /* x[k] = xN[j] */
            len++;
            ndx[len] = k;
            if (k <= m)
            {  /* x[k] is auxiliary variable */
               val[len] = alfa[j] * rs[k];
            }
            else
            {  /* x[k] is structural variable */
               val[len] = alfa[j] / rs[k];
            }
         }
      }
      /* free working arrays and return */
      ufree(v);
      ufree(alfa);
      return len;
}

/*----------------------------------------------------------------------
-- lpx_transform_col - transform explicitly specified column.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_transform_col(LPX *lp, int len, int ndx[], double val[]);
--
-- *Description*
--
-- The routine lpx_transform_col performs the same operation as the
-- routine lpx_eval_tab_col, except that the transformed column should
-- be specified explicitly.
--
-- The explicitly specified column may be thought as it were added to
-- the original system of equality constraints:
--
--    x[1] = a[1,1]*x[m+1] + ... + a[1,n]*x[m+n] + a[1]*x
--    x[2] = a[2,1]*x[m+1] + ... + a[2,n]*x[m+n] + a[2]*x            (1)
--       .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
--    x[m] = a[m,1]*x[m+1] + ... + a[m,n]*x[m+n] + a[m]*x
--
-- where x[i] are auxiliary variables, x[m+j] are structural variables,
-- x is a structural variable for the explicitly specified column, a[i]
-- are constraint coefficients for x.
--
-- On entry row indices and numerical values of non-zero elements of
-- the transformed column should be in locations ndx[1], ..., ndx[len]
-- and val[1], ..., val[len], where len is number of non-zero elements.
--
-- This routine uses the system of equality constraints and the current
-- basis in order to express the current basic variables through the
-- structural variable x in (1) (as if the transformed column were added
-- to the problem object and the variable x were non-basic):
--
--    xB[1] = ... + alfa[1]*x
--    xB[2] = ... + alfa[2]*x                                        (2)
--       .  .  .  .  .  .
--    xB[m] = ... + alfa[m]*x
--
-- where xB are basic (auxiliary and structural) variables, m is number
-- of rows in the specified problem object.
--
-- On exit the routine stores indices and numerical values of non-zero
-- elements of the resultant column alfa (2) in locations ndx[1], ...,
-- ndx[len'] and val[1], ..., val[len'], where 0 <= len' <= m is count
-- of non-zero element in the resultant column returned by the routine.
-- Note that indices of basic variables stored in the array ndx
-- correspond to original ordinal numbers of variables; indices 1 to m
-- mean auxiliary variables, indices m+1 to m+n mean structural ones.
--
-- Even if the problem is (internally) scaled, the routine returns the
-- resultant column as if the problem were unscaled.
--
-- *Returns*
--
-- The routine returns len' that is number of non-zero elements in the
-- resultant column stored in the arrays ndx and val.
--
-- *Algorithm*
--
-- The explicitly specified column (1) is transformed in the same way
-- as any other column of the constraint matrix using the formula:
--
--    alfa = inv(B) * a,
--
-- where alfa is the resultant column. */

int lpx_transform_col(LPX *lp, int len, int ndx[], double val[])
{     int m = lp->m;
      double *rs = lp->rs;
      int *indx = lp->indx;
      int i, t, k;
      double *alfa;
      /* check the explicitly specified column */
      if (!(0 <= len && len <= m))
         fault("lpx_transform_col: len = %d; invalid column length",
            len);
      for (t = 1; t <= len; t++)
      {  i = ndx[t];
         if (!(1 <= i && i <= m))
            fault("lpx_transform_col: ndx[%d] = %d; row number out of r"
               "ange", t, i);
      }
      /* the current basis should be valid */
      if (lp->b_stat != LPX_B_VALID)
         fault("lpx_transform_col: current basis is undefined");
      /* alfa := a (scaled) */
      alfa = ucalloc(1+m, sizeof(double));
      for (i = 1; i <= m; i++) alfa[i] = 0.0;
      for (t = 1; t <= len; t++)
      {  i = ndx[t];
         alfa[i] += val[t] * rs[i];
      }
      /* alfa := inv(B) * a */
      spx_ftran(lp, alfa, 0);
      /* unscale alfa and store it in sparse format */
      len = 0;
      for (i = 1; i <= m; i++)
      {  if (alfa[i] != 0.0)
         {  k = indx[i]; /* x[k] = xB[i] */
            len++;
            ndx[len] = k;
            if (k <= m)
            {  /* x[k] is auxiliary variable */
               val[len] = alfa[i] / rs[k];
            }
            else
            {  /* x[k] is structural variable */
               val[len] = alfa[i] * rs[k];
            }
         }
      }
      /* free working array and return */
      ufree(alfa);
      return len;
}

/*----------------------------------------------------------------------
-- lpx_prim_ratio_test - perform primal ratio test.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_prim_ratio_test(LPX *lp, int len, int ndx[], double val[],
--    int how, double tol);
--
-- *Description*
--
-- The routine lpx_prim_ratio_test performs the primal ratio test for
-- an explicitly specified column of the simplex table.
--
-- The primal basic solution associated with an LP problem object,
-- which the parameter lp points to, should be feasible. No components
-- of the LP problem object are changed by the routine.
--
-- The explicitly specified column of the simplex table shows how the
-- basic variables xB depend on some non-basic variable y (which is not
-- necessarily presented in the problem object):
--
--    xB[1] = ... + alfa[1]*y + ...
--    xB[2] = ... + alfa[2]*y + ...                                  (*)
--       .  .  .  .  .  .  .  .
--    xB[m] = ... + alfa[m]*y + ...
--
-- The column (*) is specifed on entry to the routine using the sparse
-- format. Ordinal numbers of basic variables xB[i] should be placed in
-- locations ndx[1], ..., ndx[len], where ordinal number 1 to m denote
-- auxiliary variables, and ordinal numbers m+1 to m+n denote structural
-- variables. The corresponding non-zero coefficients alfa[i] should be
-- placed in locations val[1], ..., val[len]. The arrays ndx and val are
-- not changed on exit.
--
-- The parameter how specifies in which direction the variable y changes
-- on entering the basis: +1 means increasing, -1 means decreasing.
--
-- The parameter tol is a relative tolerance (small positive number)
-- used by the routine to skip small alfa[i] of the column (*).
--
-- The routine determines the ordinal number of some basic variable
-- (specified in ndx[1], ..., ndx[len]), which should leave the basis
-- instead the variable y in order to keep primal feasibility, and
-- returns it on exit. If the choice cannot be made (i.e. if the
-- adjacent basic solution is primal unbounded), the routine returns
-- zero.
--
-- *Note*
--
-- If the non-basic variable y is presented in the LP problem object,
-- the column (*) can be computed using the routine lpx_eval_tab_col.
-- Otherwise it can be computed using the routine lpx_transform_col.
--
-- *Returns*
--
-- The routine lpx_prim_ratio_test returns the ordinal number of some
-- basic variable xB[i], which should leave the basis instead the
-- variable y in order to keep primal feasibility. If the adjacent basic
-- solution is primal unbounded and therefore the choice cannot be made,
-- the routine returns zero. */

int lpx_prim_ratio_test(LPX *lp, int len, int ndx[], double val[],
      int how, double tol)
{     int m = lp->m;
      int n = lp->n;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      double *rs = lp->rs;
      int *tagx = lp->tagx;
      int *posx = lp->posx;
      double *bbar = lp->bbar;
      int i, k, p, t;
      double alfa_i, abs_alfa_i, big, eps, bbar_i, lb_i, ub_i, temp,
         teta;
      /* the current basic solution should be primal feasible */
      if (lp->p_stat != LPX_P_FEAS)
         fault("lpx_prim_ratio_test: current basic solution is not prim"
            "al feasible");
      /* check if the parameter how is correct */
      if (!(how == +1 || how == -1))
         fault("lpx_prim_ratio_test: how = %d; invalid parameter", how);
      /* compute the largest absolute value of the specified influence
         coefficients */
      big = 0.0;
      for (t = 1; t <= len; t++)
      {  temp = val[t];
         if (temp < 0.0) temp = - temp;
         if (big < temp) big = temp;
      }
      /* compute the absolute tolerance eps used to skip small entries
         of the column */
      if (!(0.0 < tol && tol < 1.0))
         fault("lpx_prim_ratio_test: tol = %g; invalid tolerance", tol);
      eps = tol * (1.0 + big);
      /* initial settings */
      p = 0, teta = DBL_MAX, big = 0.0;
      /* walk through the entries of the specified column */
      for (t = 1; t <= len; t++)
      {  /* get the ordinal number of basic variable */
         k = ndx[t];
         if (!(1 <= k && k <= m+n))
            fault("lpx_prim_ratio_test: ndx[%d] = %d; ordinal number ou"
               "t of range", t, k);
         if (tagx[k] != LPX_BS)
            fault("lpx_prim_ratio_test: ndx[%d] = %d; non-basic variabl"
               "e not allowed", t, k);
         /* determine index of the variable x[k] in the vector xB */
         i = posx[k]; /* x[k] = xB[i] */
         insist(1 <= i && i <= m);
         /* determine unscaled bounds and value of the basic variable
            xB[i] in the current basic solution */
         if (k <= m)
         {  lb_i = lb[k] / rs[k];
            ub_i = ub[k] / rs[k];
            bbar_i = bbar[i] / rs[k];
         }
         else
         {  lb_i = lb[k] * rs[k];
            ub_i = ub[k] * rs[k];
            bbar_i = bbar[i] * rs[k];
         }
         /* determine influence coefficient for the basic variable
            x[k] = xB[i] in the explicitly specified column and turn to
            the case of increasing the variable y in order to simplify
            program logic */
         alfa_i = (how > 0 ? +val[t] : -val[t]);
         abs_alfa_i = (alfa_i > 0.0 ? +alfa_i : -alfa_i);
         /* analyze main cases */
         switch (typx[k])
         {  case LPX_FR:
               /* xB[i] is free variable */
               continue;
            case LPX_LO:
lo:            /* xB[i] has an lower bound */
               if (alfa_i > -eps) continue;
               temp = (lb_i - bbar_i) / alfa_i;
               break;
            case LPX_UP:
up:            /* xB[i] has an upper bound */
               if (alfa_i < +eps) continue;
               temp = (ub_i - bbar_i) / alfa_i;
               break;
            case LPX_DB:
               /* xB[i] has both lower and upper bounds */
               if (alfa_i < 0.0) goto lo; else goto up;
            case LPX_FX:
               /* xB[i] is fixed variable */
               if (abs_alfa_i < eps) continue;
               temp = 0.0;
               break;
            default:
               insist(typx != typx);
         }
         /* if the value of the variable xB[i] violates its lower or
            upper bound (slightly, because the current basis is assumed
            to be primal feasible), temp is negative; we can think this
            happens due to round-off errors and the value is exactly on
            the bound; this allows replacing temp by zero */
         if (temp < 0.0) temp = 0.0;
         /* apply the minimal ratio test */
         if (teta > temp || teta == temp && big < abs_alfa_i)
            p = k, teta = temp, big = abs_alfa_i;
      }
      /* return the ordinal number of the chosen basic variable */
      return p;
}

/*----------------------------------------------------------------------
-- lpx_dual_ratio_test - perform dual ratio test.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_dual_ratio_test(LPX *lp, int len, int ndx[], double val[],
--    int how, double tol);
--
-- *Description*
--
-- The routine lpx_dual_ratio_test performs the dual ratio test for an
-- explicitly specified row of the simplex table.
--
-- The dual basic solution associated with an LP problem object, which
-- the parameter lp points to, should be feasible. No components of the
-- LP problem object are changed by the routine.
--
-- The explicitly specified row of the simplex table is a linear form,
-- which shows how some basic variable y (not necessarily presented in
-- the problem object) depends on non-basic variables xN:
--
--    y = alfa[1]*xN[1] + alfa[2]*xN[2] + ... + alfa[n]*xN[n].       (*)
--
-- The linear form (*) is specified on entry to the routine using the
-- sparse format. Ordinal numbers of non-basic variables xN[j] should be
-- placed in locations ndx[1], ..., ndx[len], where ordinal numbers 1 to
-- m denote auxiliary variables, and ordinal numbers m+1 to m+n denote
-- structural variables. The corresponding non-zero coefficients alfa[j]
-- should be placed in locations val[1], ..., val[len]. The arrays ndx
-- and val are not changed on exit.
--
-- The parameter how specifies in which direction the variable y changes
-- on leaving the basis: +1 means increasing, -1 means decreasing.
--
-- The parameter tol is a relative tolerance (small positive number)
-- used by the routine to skip small alfa[j] of the form (*).
--
-- The routine determines the ordinal number of some non-basic variable
-- (specified in ndx[1], ..., ndx[len]), which should enter the basis
-- instead the variable y in order to keep dual feasibility, and returns
-- it on exit. If the choice cannot be made (i.e. if the adjacent basic
-- solution is dual unbounded), the routine returns zero.
--
-- *Note*
--
-- If the basic variable y is presented in the LP problem object, the
-- row (*) can be computed using the routine lpx_eval_tab_row. Otherwise
-- it can be computed using the routine lpx_transform_row.
--
-- *Returns*
--
-- The routine lpx_dual_ratio_test returns the ordinal number of some
-- non-basic variable xN[j], which should enter the basis instead the
-- variable y in order to keep dual feasibility. If the adjacent basic
-- solution is dual unbounded and therefore the choice cannot be made,
-- the routine returns zero. */

int lpx_dual_ratio_test(LPX *lp, int len, int ndx[], double val[],
      int how, double tol)
{     int m = lp->m;
      int n = lp->n;
      double *rs = lp->rs;
      double dir = (lp->dir == LPX_MIN ? +1.0 : -1.0);
      int *tagx = lp->tagx;
      int *posx = lp->posx;
      double *cbar = lp->cbar;
      int j, k, t, q;
      double alfa_j, abs_alfa_j, big, eps, cbar_j, temp, teta;
      /* the current basic solution should be dual feasible */
      if (lp->d_stat != LPX_D_FEAS)
         fault("lpx_dual_ratio_test: current basic solution is not dual"
            " feasible");
      /* check if the parameter how is correct */
      if (!(how == +1 || how == -1))
         fault("lpx_dual_ratio_test: how = %d; invalid parameter", how);
      /* compute the largest absolute value of the specified influence
         coefficients */
      big = 0.0;
      for (t = 1; t <= len; t++)
      {  temp = val[t];
         if (temp < 0.0) temp = - temp;
         if (big < temp) big = temp;
      }
      /* compute the absolute tolerance eps used to skip small entries
         of the row */
      if (!(0.0 < tol && tol < 1.0))
         fault("lpx_dual_ratio_test: tol = %g; invalid tolerance", tol);
      eps = tol * (1.0 + big);
      /* initial settings */
      q = 0, teta = DBL_MAX, big = 0.0;
      /* walk through the entries of the specified row */
      for (t = 1; t <= len; t++)
      {  /* get ordinal number of non-basic variable */
         k = ndx[t];
         if (!(1 <= k && k <= m+n))
            fault("lpx_dual_ratio_test: ndx[%d] = %d; ordinal number ou"
               "t of range", t, k);
         if (tagx[k] == LPX_BS)
            fault("lpx_dual_ratio_test: ndx[%d] = %d; basic variable no"
               "t allowed", t, k);
         /* determine index of the variable x[k] in the vector xN */
         j = posx[k] - m; /* x[k] = xN[j] */
         insist(1 <= j && j <= n);
         /* determine unscaled reduced cost of the non-basic variable
            x[k] = xN[j] in the current basic solution */
         cbar_j = (k <= m ? cbar[j] * rs[k] : cbar[j] / rs[k]);
         /* determine influence coefficient at the non-basic variable
            x[k] = xN[j] in the explicitly specified row and turn to
            the case of increasing the variable y in order to simplify
            program logic */
         alfa_j = (how > 0 ? +val[t] : -val[t]);
         abs_alfa_j = (alfa_j > 0.0 ? +alfa_j : -alfa_j);
         /* analyze main cases */
         switch (tagx[k])
         {  case LPX_NL:
               /* xN[j] is on its lower bound */
               if (alfa_j < +eps) continue;
               temp = (dir * cbar_j) / alfa_j;
               break;
            case LPX_NU:
               /* xN[j] is on its upper bound */
               if (alfa_j > -eps) continue;
               temp = (dir * cbar_j) / alfa_j;
               break;
            case LPX_NF:
               /* xN[j] is non-basic free variable */
               if (abs_alfa_j < eps) continue;
               temp = 0.0;
               break;
            case LPX_NS:
               /* xN[j] is non-basic fixed variable */
               continue;
            default:
               insist(tagx != tagx);
         }
         /* if the reduced cost of the variable xN[j] violates its zero
            bound (slightly, because the current basis is assumed to be
            dual feasible), temp is negative; we can think this happens
            due to round-off errors and the reduced cost is exact zero;
            this allows replacing temp by zero */
         if (temp < 0.0) temp = 0.0;
         /* apply the minimal ratio test */
         if (teta > temp || teta == temp && big < abs_alfa_j)
            q = k, teta = temp, big = abs_alfa_j;
      }
      /* return the ordinal number of the chosen non-basic variable */
      return q;
}

/*----------------------------------------------------------------------
-- lpx_eval_activity - compute activity of explicitly specified row.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_eval_activity(LPX *lp, int len, int ndx[], double val[]);
--
-- *Description*
--
-- The routine lpx_eval_activity computes activity (primal value) of the
-- explicitly specified row as if this row were presented in the problem
-- object and the corresponding auxiliary variable were basic.
--
-- The explicitly specified row may be thought as a linear form
--
--    x = a[1]*x[m+1] + a[2]*x[m+2] + ... + a[n]*x[m+n],             (*)
--
-- where x is an auxiliary variable for this row, a[j] are coefficients
-- of the linear form, x[m+j] are structural variables.
--
-- Column indices and numerical values of non-zero elements of the row
-- should be placed in locations ndx[1], ..., ndx[len] and val[1], ...,
-- val[len], respectively, where len is number of non-zero elements. The
-- arrays ndx and val are not changed by the routine.
--
-- *Returns*
--
-- The routine lpx_eval_activity returns activity (primal value) of the
-- explicitly specified row.
--
-- *Algorithm*
--
-- Activity of the row (*), which is a value of the auxiliary variable
-- x, is computed by substituting values of structural variables for the
-- current basic solution. */

double lpx_eval_activity(LPX *lp, int len, int ndx[], double val[])
{     int n = lp->n;
      int j, t;
      double sum, vx;
      /* check the explicitly specified row */
      if (!(0 <= len && len <= n))
         fault("lpx_eval_activity: len = %d; invalid row length", len);
      for (t = 1; t <= len; t++)
      {  j = ndx[t];
         if (!(1 <= j && j <= n))
            fault("lpx_eval_activity: ndx[%d] = %d; column number out o"
               "f range", t, j);
      }
      /* the current primal basic solution should exist */
      if (lp->p_stat == LPX_P_UNDEF)
         fault("lpx_eval_activity: current primal basic solution not ex"
            "ist");
      /* substitute the current values of structural variable into the
         linear form */
      sum = 0.0;
      for (t = 1; t <= len; t++)
      {  if (val[t] != 0.0)
         {  j = ndx[t];
            lpx_get_col_info(lp, j, NULL, &vx, NULL);
            sum += val[t] * vx;
         }
      }
      return sum;
}

/*----------------------------------------------------------------------
-- lpx_eval_red_cost - compute red. cost of explicitly specified column.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_eval_red_cost(LPX *lp, int len, int ndx[], double val[]);
--
-- *Description*
--
-- The routine lpx_eval_red_cost computes reduced cost (dual value) of
-- the explicitly specified column as if this column were presented in
-- the problem object and the corresponding structural variable were
-- non-basic.
--
-- The explicitly specified column may be thought as it were added to
-- the original system of equality constraints:
--
--    x[1] = a[1,1]*x[m+1] + ... + a[1,n]*x[m+n] + a[1]*x
--    x[2] = a[2,1]*x[m+1] + ... + a[2,n]*x[m+n] + a[2]*x            (*)
--       .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
--    x[m] = a[m,1]*x[m+1] + ... + a[m,n]*x[m+n] + a[m]*x
--
-- where x[i] are auxiliary variables, x[m+j] are structural variables,
-- x is a structural variable for the explicitly specified column, a[i]
-- are constraint coefficients for x.
--
-- Row indices and numerical values of non-zero elements of the column
-- should be placed in locations ndx[1], ..., ndx[len] and val[1], ...,
-- val[len], respectively, where len is number of non-zero elements. The
-- array ndx and val are not changed by the routine.
--
-- *Returns*
--
-- The routine lpx_eval_red_cost returns reduced cost (dual value) of
-- the explicitly specified column computed in the assumption that this
-- column has zero objective coefficient. If the objective coefficient
-- of the specified column is non-zero, it should be added to the value
-- returned by the routine.
--
-- *Algorithm*
--
-- By the definition reduced cost d of the structural variable x (*) is
--
--    d = c + pi * a,                                               (**)
--
-- where c is the objective coefficient of the variable x, pi is the
-- vector of simplex multipliers (which are Lagrange multipliers for
-- equality constraints), a is the column of the constraint matrix for
-- the variable x.
--
-- *Unscaling*
--
-- Let A~ = (I | -A) is the augmented constraint matrix of the original
-- (unscaled) problem. In the scaled LP problem instead the matrix A the
-- scaled matrix A" = R*A*S is actually used, so
--
--    A~" = (I | A") = (I | R*A*S) = (R*I*inv(R) | R*A*S) =
--                                                                   (1)
--        = R*(I | A)*S~ = R*A~*S~,
--
-- is the scaled augmented constraint matrix, where R and S are diagonal
-- scaling matrices used to scale rows and columns of the matrix A, and
--
--    S~ = diag(inv(R) | S)                                          (2)
--
-- is an augmented diagonal scaling matrix.
--
-- By the definition the vector pi is
--
--    pi = inv(B') * cB,                                             (3)
--
-- where cB is a subvector of objective coefficients at basic variables,
-- B' is a matrix transposed to the basis matrix B.
--
-- For scaled components of the objective function we have
--
--    cR" = inv(R) * cR,
--                                                                   (4)
--    cS" = S * cS,
--
-- so
--
--    cB" = SB~ * cB,                                                (5)
--
-- where SB~ is a submatrix of the matrix S~ (2), which corresponds to
-- basic variables. Therefore from (3) it follows:
--
--    pi" = inv(SB~*B'*R) * (SB~*cB) =
--
--        = inv(R) * inv(B') * inv(SB~) * sB~ * cB =                 (6)
--
--        = inv(R) * inv(B') * cB = inv(R) * pi,
--
-- that allows expressing us the original vector pi through the scaled
-- vector pi" actually stored in the problem object:
--
--    pi = R * pi".                                                  (7)
--
-- The formula (7) is used by the routine for unscaling components of
-- the vector pi. */

double lpx_eval_red_cost(LPX *lp, int len, int ndx[], double val[])
{     int m = lp->m;
      double *rs = lp->rs;
      double *pi = lp->pi;
      int i, t;
      double dj;
      /* check the explicitly specified column */
      if (!(0 <= len && len <= m))
         fault("lpx_eval_red_cost: len = %d; invalid column length",
            len);
      for (t = 1; t <= len; t++)
      {  i = ndx[t];
         if (!(1 <= i && i <= m))
            fault("lpx_eval_red_cost: ndx[%d] = %d; row number out of r"
               "ange", t, i);
      }
      /* the current dual basic solution should exist */
      if (lp->d_stat == LPX_D_UNDEF)
         fault("lpx_eval_red_cost: current dual basic solution not exis"
            "t");
      /* compute reduced cost dj = pi * A[j] */
      dj = 0.0;
      for (t = 1; t <= len; t++)
      {  i = ndx[t];
         dj += (rs[i] * pi[i]) * val[t];
      }
      return dj;
}

/*----------------------------------------------------------------------
-- lpx_reduce_form - reduce linear form.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_reduce_form(LPX *lp, int len, int ndx[], double val[],
--    double work[]);
--
-- *Description*
--
-- The routine lpx_reduce_form reduces the specified linear form:
--
--    f = a[1]*x[1] + a[2]*x[2] + ... + a[m+n]*x[m+n],               (1)
--
-- which includes auxiliary and structural variables, by substituting
-- auxiliary variables x[1], ..., x[m] from equality constraints of the
-- specified LP object, so the resultant linear form becomes expressed
-- only through structural variables:
--
--    f' = a'[1]*x[m+1] + a'[2]*x[m+2] + ... + a'[n]*x[m+n].         (2)
--
-- On entry indices and numerical values of the coefficients of the
-- specified linear form (1) should be placed in locations ndx[1], ...,
-- ndx[len] and val[1], ..., val[len], respectively, where indices from
-- 1 to m denote auxiliary variables, indices from m+1 to m+n denote
-- structural variables.
--
-- On exit the routine stores column indices and coefficients of the
-- resultant linear form (2) in locations ndx[1], ..., ndx[len'] and
-- val[1], ..., val[len'], respectively, where 0 <= len' <= n is number
-- of non-zero coefficients returned by the routine.
--
-- The working array work should have at least 1+n locations, where n
-- is number of columns in the LP object. If work is NULL, the working
-- array is allocated and freed internally by the routine.
--
-- *Returns*
--
-- The routine returns len', which is number of non-zero coefficients
-- in the resultant linear form. */

int lpx_reduce_form(LPX *lp, int len, int ndx[], double val[],
      double _work[])
{     int m = lp->m;
      int n = lp->n;
      double *rs = lp->rs;
      int *A_ptr = lp->A->ptr;
      int *A_len = lp->A->len;
      int *A_ndx = lp->A->ndx;
      double *A_val = lp->A->val;
      int i, j, k, t, beg, end, ptr;
      double aij, *work = _work;
      /* allocate working array */
      if (_work == NULL) work = ucalloc(1+n, sizeof(double));
      for (j = 1; j <= n; j++) work[j] = 0.0;
      /* walk through terms of the given linear form */
      for (t = 1; t <= len; t++)
      {  k = ndx[t];
         if (!(1 <= k && k <= m+n))
            fault("lpx_reduce_form: ndx[%d] = %d; ordinal number out of"
               " range", t, k);
         if (k <= m)
         {  /* x[k] is auxiliary variable */
            i = k;
            /* substitute x[i] = a[i,1]*x[m+1] + ... + a[i,n]*x[m+n] */
            beg = A_ptr[i];
            end = beg + A_len[i] - 1;
            for (ptr = beg; ptr <= end; ptr++)
            {  j = A_ndx[ptr];
               aij = A_val[ptr] / (rs[i] * rs[m+j]); /* unscale */
               work[j] += val[t] * aij;
            }
         }
         else
         {  /* x[k] is structural variable */
            j = k - m;
            work[j] += val[t];
         }
      }
      /* convert the resultant linear form to sparse format */
      len = 0;
      for (j = 1; j <= n; j++)
      {  if (work[j] != 0.0)
         {  len++;
            ndx[len] = j;
            val[len] = work[j];
         }
      }
      /* free working array */
      if (_work == NULL) ufree(work);
      /* return to the calling program */
      return len;
}

/*----------------------------------------------------------------------
-- lpx_mixed_gomory - generate Gomory's mixed integer cut.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_mixed_gomory(LPX *lp, int kind[], int len, int ndx[],
--    double val[]);
--
-- *Description*
--
-- The routine lpx_mixed_gomory generates a Gomory's mixed integer cut
-- for a given row of the simplex table.
--
-- The given row of the simplex table should be explicitly specified in
-- the form:
--
--    y = alfa[1]*xN[1] + alfa[2]*xN[2] + ... + alfa[n]*xN[n]        (1)
--
-- where y is some basic structural variable of integer kind, whose
-- value in the current basic solution is assumed to be fractional,
-- xN[1], ..., xN[n] are non-basic variables, alfa[1], ..., alfa[n] are
-- influence coefficient.
--
-- On entry indices (ordinal numbers) of non-basic variables, which
-- have non-zero influence coefficients, should be placed in locations
-- ndx[1], ..., ndx[len], where indices from 1 to m denote auxiliary
-- variables and indices from m+1 to m+n denote structural variables,
-- and the corresponding influence coefficients should be placed in
-- locations val[1], ..., val[len]. These arrays can be computed using
-- the routine lpx_eval_tab_row.
--
-- The array kind specifies kinds of structural variables. The location
-- kind[0] is not used. If the flag kind[j], 1 <= j <= n, is zero, the
-- j-th structural variable (not the variable xN[j]!) is continuous. If
-- it is non-zero, the j-th structural variable is integer. This array
-- is not changed by the routine.
--
-- The routine generates the Gomory's mixed integer cut in the form of
-- an inequality constraint:
--
--    a[1]*x[m+1] + a[2]*x[m+2] + ... + a[n]*x[m+n] >= b,            (2)
--
-- where x[m+1], ..., x[m+n] are structural variables, a[1], ..., a[n]
-- are constraint coefficients, b is a right-hand side.
--
-- On exit the routine stores indices of structural variables and the
-- corresponding non-zero constraint coefficients for the inequality
-- constraint (2) in locations ndx[1], ..., ndx[len'] and val[1], ...,
-- val[len'], where 0 <= len' <= n is returned by the routine. The
-- right-hand side b is stored to the location val[0], and the location
-- ndx[0] is set to 0. (Should note than on exit indices of structural
-- variables stored in the array ndx are in the range from 1 to n.)
--
-- The working array work should have at least 1+n locations, where n
-- is number of columns in the LP object. If work is NULL, the working
-- array is allocated and freed internally by the routine.
--
-- *Returns*
--
-- If the cutting plane has been successfully generated, the routine
-- returns 0 <= len' <= n, which is number of non-zero coefficients in
-- the inequality constraint (2). In case of errors the routine returns
-- one of the following negative codes:
--
-- -1 - in the row (1) there is a free non-basic variable with non-zero
--      influence coefficient;
-- -2 - value of the basic variable y defined by the row (1) is near to
--      a closest integer point, so the cutting plane may be unreliable.
--
-- *References*
--
-- 1. R.E.Gomory. An algorithm for the mixed integer problem. Tech.Rep.
--    RM-2597, The RAND Corp. (1960).
--
-- 2. H.Marchand, A.Martin, R.Weismantel, L.Wolsey. Cutting Planes in
--    Integer and Mixed Integer Programming. CORE, October 1999. */

int lpx_mixed_gomory(LPX *lp, int kind[], int len, int ndx[],
      double val[], double _work[])
{     int m = lp->m;
      int n = lp->n;
      double *lb = lp->lb;
      double *ub = lp->ub;
      double *rs = lp->rs;
      int *tagx = lp->tagx;
      int *posx = lp->posx;
      int *indx = lp->indx;
      int j, k, t;
      double *a, a_j, b, *alfa, alfa_j, beta, lb_k, ub_k, f0, fj;
      double *work = _work;
      /* allocate working array */
      if (_work == NULL) work = ucalloc(1+n, sizeof(double));
      /* on entry the specified row of the simplex table has the form:
         y = alfa[1]*xN[1] + ... + alfa[n]*xN[n];
         convert this row to the form:
         y + alfa'[1]*xN'[1] + ... + alfa'[n]*xN'[n] = beta,
         where all new (stroked) non-basic variables are non-negative
         (this is not needed for y, because it has integer bounds and
         only fractional part of beta is used); note that beta is the
         value of y in the current basic solution */
      alfa = work;
      for (j = 1; j <= n; j++) alfa[j] = 0.0;
      beta = 0.0;
      for (t = 1; t <= len; t++)
      {  /* get index of some non-basic variable x[k] = xN[j] */
         k = ndx[t];
         if (!(1 <= k && k <= m+n))
            fault("lpx_mixed_gomory: ndx[%d] = %d; variable number out "
               "of range", t, k);
         if (tagx[k] == LPX_BS)
            fault("lpx_mixed_gomory: ndx[%d] = %d; variable should be n"
               "on-basic", t, k);
         j = posx[k] - m; /* x[k] = xN[j] */
         insist(1 <= j && j <= n);
         insist(alfa[j] == 0.0);
         /* get the original influence coefficient alfa[j] */
         alfa_j = val[t];
         /* obtain unscaled bounds of the variable x[k] = xN[j] */
         if (k <= m)
         {  lb_k = lb[k] / rs[k];
            ub_k = ub[k] / rs[k];
         }
         else
         {  lb_k = lb[k] * rs[k];
            ub_k = ub[k] * rs[k];
         }
         /* perform conversion */
         switch (tagx[k])
         {  case LPX_NL:
               /* xN[j] is on its lower bound */
               /* substitute xN[j] = lb[k] + xN'[j] */
               alfa[j] = - alfa_j;
               beta += alfa_j * lb_k;
               break;
            case LPX_NU:
               /* xN[j] is on its upper bound */
               /* substitute xN[j] = ub[k] - xN'[j] */
               alfa[j] = + alfa_j;
               beta += alfa_j * ub_k;
               break;
            case LPX_NF:
               /* xN[j] is free non-basic variable */
               return -1;
            case LPX_NS:
               /* xN[j] is fixed non-basic variable */
               /* substitute xN[j] = lb[k] */
               alfa[j] = 0.0;
               beta += alfa_j * lb_k;
               break;
            default:
               insist(tagx != tagx);
         }
      }
      /* now the converted row of the simplex table has the form:
         y + alfa'[1]*xN'[1] + ... + alfa'[n]*xN'[n] = beta,
         where all xN'[j] >= 0; generate Gomory's mixed integer cut in
         the form of inequality:
         a'[1]*xN'[1] + ... + a'[n]*xN'[n] >= b' */
      a = work;
      /* f0 is fractional part of beta, where beta is the value of the
         variable y in the current basic solution; if f0 is close to
         zero or to one, i.e. if y is near to a closest integer point,
         the corresponding cutting plane may be unreliable */
      f0 = beta - floor(beta);
      if (!(0.00001 <= f0 && f0 <= 0.99999)) return -2;
      for (j = 1; j <= n; j++)
      {  alfa_j = alfa[j];
         if (alfa_j == 0.0)
         {  a[j] = 0.0;
            continue;
         }
         k = indx[m+j]; /* x[k] = xN[j] */
         insist(1 <= k && k <= m+n);
         if (k > m && kind[k-m])
         {  /* xN[j] is integer */
            fj = alfa_j - floor(alfa_j);
            if (fj <= f0)
               a[j] = fj;
            else
               a[j] = (f0 / (1.0 - f0)) * (1.0 - fj);
         }
         else
         {  /* xN[j] is continuous */
            if (alfa_j > 0.0)
               a[j] = alfa_j;
            else
               a[j] = - (f0 / (1.0 - f0)) * alfa_j;
         }
      }
      b = f0;
      /* now the generated cutting plane has the form of an inequality:
         a'[1]*xN'[1] + ... + a'[n]*xN'[n] >= b';
         convert this inequality back to the form expressed through the
         original non-basic variables:
         a[1]*xN[1] + ... + a[n]*xN[n] >= b */
      len = 0;
      for (j = 1; j <= n; j++)
      {  /* get constraint coefficient at xN[j] */
         a_j = a[j];
         if (a_j == 0.0) continue;
         k = indx[m+j]; /* x[k] = xN[j] */
         /* obtain unscaled bounds of the variable x[k] = xN[j] */
         if (k <= m)
         {  lb_k = lb[k] / rs[k];
            ub_k = ub[k] / rs[k];
         }
         else
         {  lb_k = lb[k] * rs[k];
            ub_k = ub[k] * rs[k];
         }
         /* perform conversion */
         len++;
         ndx[len] = k;
         switch (tagx[k])
         {  case LPX_NL:
               /* xN[j] is on its lower bound */
               /* substitute xN'[j] = xN[j] - lb[k] */
               val[len] = + a_j;
               b += a_j * lb_k;
               break;
            case LPX_NU:
               /* xN[j] is on its upper bound */
               /* substitute xN'[j] = ub[k] - xN[j] */
               val[len] = - a_j;
               b -= a_j * ub_k;
               break;
            default:
               insist(tagx != tagx);
         }
      }
      /* substitute auxiliary (non-basic) variables to the generated
         inequality constraint a[1]*xN[1] + ... + a[n]*xN[n] >= b using
         the equality constraints of the specified LP problem object in
         order to express the generated constraint through structural
         variables only */
      len = lpx_reduce_form(lp, len, ndx, val, work);
      /* store the right-hand side */
      ndx[0] = 0, val[0] = b;
      /* free working array */
      if (_work == NULL) ufree(work);
      /* return to the calling program */
      return len;
}

/*----------------------------------------------------------------------
-- lpx_estim_obj_chg - estimate obj. changes for down- and up-branch.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_estim_obj_chg(LPX *lp, int k, double dn_val, double up_val,
--    double *dn_chg, double *up_chg, int ndx[], double val[]);
--
-- *Description*
--
-- The routine lpx_estim_chg computes estimations of the changes in the
-- objective function for the down-branch and the up-branch by means of
-- one implicit iteration of the dual simplex for each branch.
--
-- The current basic solution of an LP problem, which the parameter lp
-- points to, should be optimal.
--
-- The parameter k is the number of some variable x[k], for which the
-- routine has to compute the estimations. If 1 <= k <= m, the variable
-- x[k] is auxiliary, if m+1 <= k <= m+n, it is structural (where m and
-- n are numbers of rows and columns, respectively). The variable x[k]
-- should be basic in the current basic solution.
--
-- The parameter dn_val is a new *upper* bound of the variable x[k] in
-- the down-branch, and the parameter up_val is a new *lower* bound of
-- the variable x[k] in the up-branch. These new bounds should satisfy
-- to dn_val <= beta[k] <= up_val, where beta[k] is a value of x[k] in
-- the current basic solution.
--
-- The down-branch is equivalent to the original LP problem provided
-- with the additional constraint x[k] <= dn_val. And, analogously, the
-- up-branch is equivalent to the original LP problem provided with the
-- additional constraint x[k] >= up_val. These additional constraints
-- are included implicitly, so the original LP problem is not changed.
--
-- Estimation of the change in the objective function is computed as
-- the difference between a value, which the objective function would
-- have in an adjacent basis as the result of one iteration of the dual
-- simplex, and the value, which it has in the current basic solution.
-- The current basic solution is optimal and the adjacent basis is dual
-- feasible, so the actual change on solving the branch to optimality
-- is always not less (in the case of minimization) or not greater (in
-- the case of maximization) than its estimation.
--
-- If some branch has no primal feasible solution, its esimation is set
-- to +DBL_MAX (for minimization) or to -DBL_MAX (for maximization).
--
-- The routine stores computed estimations for the down-branch and the
-- up-branch to locations, which the parameters dn_chg and up_chg point
-- to, respectively.
--
-- The working arrays ndx and val should have at least 1+n locations,
-- where n is number of columns in the problem. If some of these arrays
-- is specified as NULL, it is allocated and freed by the routine.
--
-- *Algorithm*
--
-- Let x[k] be the specified variable, which leaves the current basis
-- and decreasing goes to its new upper bound (for the down-branch) or
-- increasing goes to its new lower bound (for the up-branch). Let also
-- x[s] be a non-basic (auxiliary or structural) variable chosen by the
-- dual ratio test to enter the basis in order to keep dual feasibility
-- of the adjacent basis. The dependence between x[k] and x[s] is given
-- by the corresponding row of the simplex table:
--
--    x[k] = ... + alfa * x[s] + ...                                 (1)
--
-- where alfa is an influence coefficient.
--
-- Let
--
--                                     dn_val - beta[k] (dn-branch)
--    delta[k] = beta'[k] - beta[k] =                                (2)
--                                     up_val - beta[k] (up-branch)
--
-- be the change of x[k], and
--
--    delta[s] = beta'[s] - beta[s]                                  (3)
--
-- be the change of x[s], where beta is a value of the corresponding
-- variable in the current (optimal) basis, and beta' is a value in the
-- adjacent (dual feasible) basis.
--
-- From (1) it follows that
--
--    delta[k] = alfa * delta[s],                                    (4)
--
-- or
--
--    delta[s] = delta[k] / alfa.                                    (5)
--
-- Thus, using the reduced cost d[s] of the non-basic variable x[s] in
-- the current basis it is possible to find the corresponding change in
-- the objective function:
--
--    delta[Z] = Z' - Z = d[s] * delta[s],                           (6)
--
-- which is the estimation of the actual change if the branch would be
-- solved to optimality. May note that Z' (an objective value in the
-- adjacent basis) is always *not better* than Z (an objective value in
-- the current basis). */

void lpx_estim_obj_chg(LPX *lp, int k, double dn_val, double up_val,
      double *dn_chg, double *up_chg, int _ndx[], double _val[])
{     int m, n, dir, s, t, len, tagx, *ndx = _ndx;
      double alfa, vx, dx, tol, eps, *val = _val;
      /* the current basic solution should be optimal */
      if (lpx_get_status(lp) != LPX_OPT)
         fault("lpx_estim_obj_chg: basic solution is not optimal");
      /* determine number of rows and columns */
      m = lpx_get_num_rows(lp);
      n = lpx_get_num_cols(lp);
      /* check if the number k is correct */
      if (!(1 <= k && k <= m+n))
         fault("lpx_estim_obj_chg: k = %d; number of variable out of ra"
            "nge", k);
      /* obtain the status and value of the variable x[k] */
      if (k <= m)
         lpx_get_row_info(lp, k, &tagx, &vx, NULL);
      else
         lpx_get_col_info(lp, k-m, &tagx, &vx, NULL);
      /* the variable x[k] should be basic */
      if (tagx != LPX_BS)
         fault("lpx_estim_obj_chg: k = %d; variable is not basic", k);
      /* check correctness of new bounds of the variable x[k] */
      if (!(dn_val <= vx))
         fault("lpx_estim_obj_chg: dn_val = %g; vx = %g; invalid bound "
            "for down-branch", dn_val, vx);
      if (!(up_val >= vx))
         fault("lpx_estim_obj_chg: up_val = %g; vx = %g; invalid bound "
            "for up-branch", up_val, vx);
      /* allocate working arrays */
      if (_ndx == NULL) ndx = ucalloc(1+n, sizeof(int));
      if (_val == NULL) val = ucalloc(1+n, sizeof(double));
      /* determine optimization direction */
      dir = lpx_get_obj_dir(lp);
      /* obtain a relative tolerance used to check pivots */
      tol = lpx_get_real_parm(lp, LPX_K_TOLPIV);
      /* compute the row of the simplex table, which correspond to the
         variable x[k] */
      len = lpx_eval_tab_row(lp, k, ndx, val);
      /*** estimate obj. change for the down-branch ***/
      /* perform dual ratio test to determine which non-basic variable
         should enter the basis if the variable x[k] decreasing leaves
         the basis and goes to its new upper bound */
      s = lpx_dual_ratio_test(lp, len, ndx, val, -1, tol);
      if (s == 0)
      {  /* the down-branch has no primal feasible solution */
         *dn_chg = (dir == LPX_MIN ? +DBL_MAX : -DBL_MAX);
      }
      else
      {  /* determine the corresponding influence coefficient */
         for (t = 1; t <= len; t++) if (ndx[t] == s) break;
         insist(t <= len);
         alfa = val[t];
         /* obtain the current reduced cost of the variable x[s] */
         if (s <= m)
            lpx_get_row_info(lp, s, NULL, NULL, &dx);
         else
            lpx_get_col_info(lp, s-m, NULL, NULL, &dx);
         /* compute the change of the objective function */
         *dn_chg = dx * ((dn_val - vx) / alfa);
      }
      /*** estimate obj. change for the up-branch ***/
      /* perform dual ratio test to determine which non-basic variable
         should enter the basis if the variable x[k] increasing leaves
         the basis and goes to its new lower bound */
      s = lpx_dual_ratio_test(lp, len, ndx, val, +1, tol);
      if (s == 0)
      {  /* the up-branch has no primal feasible solution */
         *up_chg = (dir == LPX_MIN ? +DBL_MAX : -DBL_MAX);
      }
      else
      {  /* determine the corresponding influence coefficient */
         for (t = 1; t <= len; t++) if (ndx[t] == s) break;
         insist(t <= len);
         alfa = val[t];
         /* obtain the current reduced cost of the variable x[s] */
         if (s <= m)
            lpx_get_row_info(lp, s, NULL, NULL, &dx);
         else
            lpx_get_col_info(lp, s-m, NULL, NULL, &dx);
         /* compute the change of the objective function */
         *up_chg = dx * ((up_val - vx) / alfa);
      }
      /* if the adjacent basic solution of the down- or up-branch is
         dual degenerative, the estimations may have wrong sign (due to
         wrong sign of the reduced cost of x[s], which in this case has
         to be close to zero) */
      eps = 1e-6 * (1.0 + fabs(lpx_get_obj_val(lp)));
      switch (dir)
      {  case LPX_MIN:
            /* in the case of minimization both estimations should be
               non-negative */
            insist(*dn_chg >= -eps);
            insist(*up_chg >= -eps);
            if (*dn_chg < 0.0) *dn_chg = 0.0;
            if (*up_chg < 0.0) *up_chg = 0.0;
            break;
         case LPX_MAX:
            /* in the case of maximization both estimations should be
               non-positive */
            insist(*dn_chg <= +eps);
            insist(*up_chg <= +eps);
            if (*dn_chg > 0.0) *dn_chg = 0.0;
            if (*up_chg > 0.0) *up_chg = 0.0;
            break;
         default:
            insist(dir != dir);
      }
      /* free working arrays */
      if (_ndx == NULL) ufree(ndx);
      if (_val == NULL) ufree(val);
      /* return to the calling program */
      return;
}

/* eof */
