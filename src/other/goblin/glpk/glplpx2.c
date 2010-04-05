/* glplpx2.c (problem querying routines) */

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

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "glplib.h"
#include "glplpx.h"

/*----------------------------------------------------------------------
-- lpx_get_num_rows - determine number of rows.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_num_rows(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_num_rows returns current number of rows in an LP
-- problem object, which the parameter lp points to. */

int lpx_get_num_rows(LPX *lp)
{     int m = lp->m;
      return m;
}

/*----------------------------------------------------------------------
-- lpx_get_num_cols - determine number of columns.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_num_cols(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_num_cols returns current number of columns in an
-- LP problem object, which the parameter lp points to. */

int lpx_get_num_cols(LPX *lp)
{     int n = lp->n;
      return n;
}

/*----------------------------------------------------------------------
-- lpx_get_num_int - determine number of integer columns.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_num_int(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_num_int returns number of columns (structural
-- variables) in the problem object, which are marked as integer. */

int lpx_get_num_int(LPX *lp)
{     int count = 0, j;
      if (lp->clss != LPX_MIP)
         fault("lpx_get_num_int: error -- not a MIP problem");
      for (j = 1; j <= lp->n; j++)
         if (lp->kind[j] == LPX_IV) count++;
      return count;
}

/*----------------------------------------------------------------------
-- lpx_get_num_bin - determine number of binary columns.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_num_bin(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_num_bin returns number of columns (structural
-- variables) in the problem object, which are marked as integer and
-- have zero lower bound and unity upper bound. */

int lpx_get_num_bin(LPX *lp)
{     int count = 0, j, k;
      double trick = 1e-12;
      if (lp->clss != LPX_MIP)
         fault("lpx_get_num_bin: error -- not a MIP problem");
      for (j = 1; j <= lp->n; j++)
      {  if (lp->kind[j] == LPX_IV)
         {  k = lp->m + j;
            if (lp->typx[k] == LPX_DB &&
                fabs(lp->lb[k] * lp->rs[k])       <= trick &&
                fabs(lp->ub[k] * lp->rs[k] - 1.0) <= trick) count++;
         }
      }
      return count;
}

/*----------------------------------------------------------------------
-- lpx_get_num_nz - determine number of constraint coefficients.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_num_nz(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_num_nz returns current number non-zero elements
-- in the constraint matrix that belongs to an LP problem object, which
-- the parameter lp points to. */

int lpx_get_num_nz(LPX *lp)
{     int m = lp->m;
      int *aa_len = lp->A->len;
      int i, count = 0;
      for (i = 1; i <= m; i++) count += aa_len[i];
      return count;
}

/*----------------------------------------------------------------------
-- lpx_get_prob_name - obtain problem name.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- char *lpx_get_prob_name(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_prob_name returns a pointer to a static buffer
-- that contains symbolic name of the problem. However, if the problem
-- has no assigned name, the routine returns NULL. */

char *lpx_get_prob_name(LPX *lp)
{     return
         lp->prob == NULL ? NULL : get_str(lp->buf, lp->prob);
}

/*----------------------------------------------------------------------
-- lpx_get_row_name - obtain row name.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- char *lpx_get_row_name(LPX *lp, int i);
--
-- *Returns*
--
-- The routine lpx_get_row_name returns a pointer to a static buffer
-- that contains symbolic name of the i-th row. However, if the i-th
-- row has no assigned name, the routine returns NULL. */

char *lpx_get_row_name(LPX *lp, int i)
{     if (!(1 <= i && i <= lp->m))
         fault("lpx_get_row_name: i = %d; row number out of range", i);
      return
         lp->name[i] == NULL ? NULL : get_str(lp->buf, lp->name[i]);
}

/*----------------------------------------------------------------------
-- lpx_get_col_name - obtain column name.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- char *lpx_get_col_name(LPX *lp, int j);
--
-- *Returns*
--
-- The routine lpx_get_col_name returns a pointer to a static buffer
-- that contains symbolic name of the j-th column. However, if the j-th
-- column has no assigned name, the routine returns NULL. */

char *lpx_get_col_name(LPX *lp, int j)
{     if (!(1 <= j && j <= lp->n))
         fault("lpx_get_col_name: j = %d; column number out of range",
            j);
      j += lp->m;
      return
         lp->name[j] == NULL ? NULL : get_str(lp->buf, lp->name[j]);
}

/*----------------------------------------------------------------------
-- glp_get_row_bnds - obtain row bounds.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_get_row_bnds(LPX *lp, int i, int *typx, double *lb,
--    double *ub);
--
-- *Description*
--
-- The routine lpx_get_row_bnds stores the type, lower bound and upper
-- bound of the i-th row to locations, which the parameters typx, lb,
-- and ub point to, respectively.
--
-- If some of the parameters typx, lb, or ub is NULL, the corresponding
-- value is not stored.
--
-- Types and bounds have the following meaning:
--
--     Type          Bounds            Note
--    -------------------------------------------
--    LPX_FR   -inf <  x <  +inf   free variable
--    LPX_LO     lb <= x <  +inf   lower bound
--    LPX_UP   -inf <  x <=  ub    upper bound
--    LPX_DB     lb <= x <=  ub    double bound
--    LPX_FX           x  =  lb    fixed variable
--
-- where x is the corresponding auxiliary variable.
--
-- If the row has no lower bound, *lb is set to zero. If the row has no
-- upper bound, *ub is set to zero. If the row is of fixed type, *lb and
-- *ub are set to the same value. */

void lpx_get_row_bnds(LPX *lp, int i, int *typx, double *lb, double *ub)
{     if (!(1 <= i && i <= lp->m))
         fault("lpx_get_row_bnds: i = %d; row number out of range", i);
      if (typx != NULL) *typx = lp->typx[i];
      if (lb != NULL) *lb = lp->lb[i] / lp->rs[i];
      if (ub != NULL) *ub = lp->ub[i] / lp->rs[i];
      return;
}

/*----------------------------------------------------------------------
-- glp_get_col_bnds - obtain column bounds.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_get_col_bnds(LPX *lp, int j, int *typx, double *lb,
--    double *ub);
--
-- *Description*
--
-- The routine lpx_get_col_bnds stores the type, lower bound and upper
-- bound of the j-th column to locations, which the parameters typx, lb,
-- and ub point to, respectively.
--
-- If some of the parameters typx, lb, or ub is NULL, the corresponding
-- value is not stored.
--
-- Types and bounds have the following meaning:
--
--     Type          Bounds            Note
--    -------------------------------------------
--    LPX_FR   -inf <  x <  +inf   free variable
--    LPX_LO     lb <= x <  +inf   lower bound
--    LPX_UP   -inf <  x <=  ub    upper bound
--    LPX_DB     lb <= x <=  ub    double bound
--    LPX_FX           x  =  lb    fixed variable
--
-- where x is the corresponding structural variable.
--
-- If the column has no lower bound, *lb is set to zero. If the column
-- has no upper bound, *ub is set to zero. If the column is of fixed
-- type, *lb and *ub are set to the same value. */

void lpx_get_col_bnds(LPX *lp, int j, int *typx, double *lb, double *ub)
{     if (!(1 <= j && j <= lp->n))
         fault("lpx_get_col_bnds: j = %d; column number out of range",
            j);
      j += lp->m;
      if (typx != NULL) *typx = lp->typx[j];
      if (lb != NULL) *lb = lp->lb[j] * lp->rs[j];
      if (ub != NULL) *ub = lp->ub[j] * lp->rs[j];
      return;
}

/*----------------------------------------------------------------------
-- lpx_get_class - query problem class.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_class(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_class returns a problem class for the specified
-- problem object:
--
-- LPX_LP  - pure linear programming (LP) problem;
-- LPX_MIP - mixed integer programming (MIP) problem. */

int lpx_get_class(LPX *lp)
{     int clss = lp->clss;
      return clss;
}

/*----------------------------------------------------------------------
-- lpx_get_col_kind - query column kind.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_col_kind(LPX *lp, int j);
--
-- *Returns*
--
-- The routine lpx_get_col_kind returns the kind of j-th structural
-- variable:
--
-- LPX_CV - continuous variable;
-- LPX_IV - integer variable. */

int lpx_get_col_kind(LPX *lp, int j)
{     if (lp->clss != LPX_MIP)
         fault("lpx_get_col_kind: error -- not a MIP problem");
      if (!(1 <= j && j <= lp->n))
         fault("lpx_get_col_kind: j = %d; column number out of range",
            j);
      return lp->kind[j];
}

/*----------------------------------------------------------------------
-- lpx_get_obj_name - obtain objective function name.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- char *lpx_get_obj_name(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_obj_name returns a pointer to a static buffer
-- that contains a symbolic name of the objective function. However,
-- if the objective function has no assigned name, the routine returns
-- NULL. */

char *lpx_get_obj_name(LPX *lp)
{     return
         lp->obj == NULL ? NULL : get_str(lp->buf, lp->obj);
}

/*----------------------------------------------------------------------
-- lpx_get_obj_dir - determine optimization direction.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_obj_dir(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_obj_dir returns a flag that defines optimization
-- direction (i.e. sense of the objective function):
--
-- LPX_MIN - the objective function has to be minimized;
-- LPX_MAX - the objective function has to be maximized. */

int lpx_get_obj_dir(LPX *lp)
{     int dir = lp->dir;
      return dir;
}

/*----------------------------------------------------------------------
-- lpx_get_obj_c0 - obtain constant term of the obj. function.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_get_obj_c0(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_obj_c0 return a constant term of the objective
-- function for an LP problem, which the parameter lp points to. */

double lpx_get_obj_c0(LPX *lp)
{     double c0 = lp->coef[0];
      return c0;
}

/*----------------------------------------------------------------------
-- lpx_get_row_coef - obtain row objective coefficient.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_get_row_coef(LPX *lp, int i);
--
-- *Returns*
--
-- The routine lpx_get_row_coef returns a coefficient of the objective
-- function at the i-th auxiliary variable (row). */

double lpx_get_row_coef(LPX *lp, int i)
{     if (!(1 <= i && i <= lp->m))
         fault("lpx_get_row_coef: i = %d; row number out of range", i);
      return lp->coef[i] * lp->rs[i];
}

/*----------------------------------------------------------------------
-- lpx_get_col_coef - obtain column objective coefficient.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_get_col_coef(LPX *lp, int j);
--
-- *Returns*
--
-- The routine lpx_get_col_coef returns a coefficient of the objective
-- function at the j-th structural variable (column). */

double lpx_get_col_coef(LPX *lp, int j)
{     if (!(1 <= j && j <= lp->n))
         fault("lpx_get_col_coef: j = %d; column number out of range",
            j);
      j += lp->m;
      return lp->coef[j] / lp->rs[j];
}

/*----------------------------------------------------------------------
-- lpx_get_mat_row - get row of the constraint matrix.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_mat_row(LPX *lp, int i, int ndx[], double val[]);
--
-- *Description*
--
-- The routine lpx_get_mat_row scans (non-zero) elements of the i-th
-- row of the constraint matrix and stores their column indices and
-- values to locations ndx[1], ..., ndx[len] and val[1], ..., val[len]
-- respectively, where 0 <= len <= n is number of elements in the i-th
-- row, n is number of columns. It is allowed to specify val as NULL,
-- in which case only column indices are stored.
--
-- *Returns*
--
-- The routine returns len, which is number of stored elements (length
-- of the i-th row). */

int lpx_get_mat_row(LPX *lp, int i, int ndx[], double val[])
{     int m = lp->m;
      double *rs = lp->rs;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int beg, len, t;
      double rs_i;
      if (!(1 <= i && i <= m))
         fault("lpx_get_mat_row: i = %d; row number out of range", i);
      beg = aa_ptr[i];
      len = aa_len[i];
      memcpy(&ndx[1], &sv_ndx[beg], len * sizeof(int));
      if (val != NULL)
      {  memcpy(&val[1], &sv_val[beg], len * sizeof(double));
         rs_i = rs[i];
         for (t = 1; t <= len; t++)
            val[t] /= (rs_i * rs[m + ndx[t]]);
      }
      return len;
}

/*----------------------------------------------------------------------
-- lpx_get_mat_col - get column of the constraint matrix.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_mat_col(LPX *lp, int j, int ndx[], double val[]);
--
-- *Description*
--
-- The routine lpx_get_mat_col scans (non-zero) elements of the j-th
-- column of the constraint matrix and stores their row indices and
-- values to locations ndx[1], ..., ndx[len] and val[1], ..., val[len]
-- respectively, where 0 <= len <= m is number of elements in the j-th
-- column, m is number of rows. It is allowed to specify val as NULL,
-- in which case only row indices are stored.
--
-- *Returns*
--
-- The routine returns len, which is number of stored elements (length
-- of the j-th column). */

int lpx_get_mat_col(LPX *lp, int j, int ndx[], double val[])
{     int m = lp->m;
      int n = lp->n;
      double *rs = lp->rs;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int beg, len, t;
      double rs_j;
      if (!(1 <= j && j <= n))
         fault("lpx_get_mat_col: j = %d; column number out of range",
            j);
      j += m;
      beg = aa_ptr[j];
      len = aa_len[j];
      memcpy(&ndx[1], &sv_ndx[beg], len * sizeof(int));
      if (val != NULL)
      {  memcpy(&val[1], &sv_val[beg], len * sizeof(double));
         rs_j = rs[j];
         for (t = 1; t <= len; t++)
            val[t] /= (rs[ndx[t]] * rs_j);
      }
      return len;
}

/*----------------------------------------------------------------------
-- lpx_get_row_mark - determine row mark.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_row_mark(LPX *lp, int i);
--
-- *Returns*
--
-- The routine lpx_get_row_mark returns an integer mark assigned to the
-- i-th row by the routine lpx_mark_row. */

int lpx_get_row_mark(LPX *lp, int i)
{     if (!(1 <= i && i <= lp->m))
         fault("lpx_get_row_mark: i = %d; row number out of range", i);
      return lp->mark[i];
}

/*----------------------------------------------------------------------
-- lpx_get_col_mark - determine column mark.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_col_mark(LPX *lp, int j);
--
-- *Returns*
--
-- The routine lpx_get_col_mark returns an integer mark assigned to the
-- j-th column by the routine lpx_mark_col. */

int lpx_get_col_mark(LPX *lp, int j)
{     if (!(1 <= j && j <= lp->n))
         fault("lpx_get_col_mark: j = %d; column number out of range",
            j);
      return lp->mark[lp->m + j];
}

/*----------------------------------------------------------------------
-- lpx_get_status - query basic solution status.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_status(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_status reports the status of the current basic
-- solution obtained for an LP problem object, which the parameter lp
-- points to:
--
-- LPX_OPT      - solution is optimal;
-- LPX_FEAS     - solution is feasible;
-- LPX_INFEAS   - solution is infeasible;
-- LPX_NOFEAS   - problem has no feasible solution;
-- LPX_UNBND    - problem has unbounded solution;
-- LPX_UNDEF    - solution is undefined. */

int lpx_get_status(LPX *lp)
{     int p_stat = lp->p_stat;
      int d_stat = lp->d_stat;
      int status;
      switch (p_stat)
      {  case LPX_P_UNDEF:
            status = LPX_UNDEF;
            break;
         case LPX_P_FEAS:
            switch (d_stat)
            {  case LPX_D_UNDEF:
                  status = LPX_FEAS;
                  break;
               case LPX_D_FEAS:
                  status = LPX_OPT;
                  break;
               case LPX_D_INFEAS:
                  status = LPX_FEAS;
                  break;
               case LPX_D_NOFEAS:
                  status = LPX_UNBND;
                  break;
               default:
                  insist(d_stat != d_stat);
            }
            break;
         case LPX_P_INFEAS:
            status = LPX_INFEAS;
            break;
         case LPX_P_NOFEAS:
            status = LPX_NOFEAS;
            break;
         default:
            insist(p_stat != p_stat);
      }
      return status;
}

/*----------------------------------------------------------------------
-- lpx_get_prim_stat - query primal status of basic solution.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_prim_stat(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_prim_stat reports the primal status of the basic
-- solution obtained by the solver for an LP problem object, which the
-- parameter lp points to:
--
-- LPX_P_UNDEF  - the primal status is undefined;
-- LPX_P_FEAS   - the solution is primal feasible;
-- LPX_P_INFEAS - the solution is primal infeasible;
-- LPX_P_NOFEAS - no primal feasible solution exists. */

int lpx_get_prim_stat(LPX *lp)
{     int p_stat = lp->p_stat;
      return p_stat;
}

/*----------------------------------------------------------------------
-- lpx_get_dual_stat - query dual status of basic solution.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_dual_stat(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_dual_stat reports the dual status of the basic
-- solution obtained by the solver for an LP problem object, which the
-- parameter lp points to:
--
-- LPX_D_UNDEF  - the dual status is undefined;
-- LPX_D_FEAS   - the solution is dual feasible;
-- LPX_D_INFEAS - the solution is dual infeasible;
-- LPX_D_NOFEAS - no dual feasible solution exists. */

int lpx_get_dual_stat(LPX *lp)
{     int d_stat = lp->d_stat;
      return d_stat;
}

/*----------------------------------------------------------------------
-- lpx_get_row_info - obtain row solution information.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_get_row_info(LPX *lp, int i, int *tagx, double *vx,
--    double *dx);
--
-- *Description*
--
-- The routine lpx_get_row_info stores status, primal value and dual
-- value of the i-th auxiliary variable (row) to locations, which the
-- parameters tagx, vx, and dx point to, respectively.
--
-- The status code has the following meaning:
--
-- LPX_BS - basic variable;
-- LPX_NL - non-basic variable on its lower bound;
-- LPX_NU - non-basic variable on its upper bound;
-- LPX_NF - non-basic free (unbounded) variable;
-- LPX_NS - non-basic fixed variable.
--
-- If some of pointers tagx, vx, or dx is NULL, the corresponding value
-- is not stored. */

void lpx_get_row_info(LPX *lp, int i, int *tagx, double *vx, double *dx)
{     int m = lp->m;
      int n = lp->n;
      int tagx_i, t;
      double vx_i, dx_i;
      if (!(1 <= i && i <= m))
         fault("lpx_get_row_info: i = %d; row number out of range", i);
      /* obtain the status */
      tagx_i = lp->tagx[i];
      if (tagx != NULL) *tagx = tagx_i;
      /* obtain the primal value */
      if (vx != NULL)
      {  if (lp->p_stat == LPX_P_UNDEF)
         {  /* the primal value is undefined */
            vx_i = 0.0;
         }
         else
         {  if (tagx_i == LPX_BS)
            {  /* basic variable */
               t = lp->posx[i]; /* x[i] = xB[t] */
               insist(1 <= t && t <= m);
               vx_i = lp->bbar[t];
               /* round the primal value (if required) */
               if (lp->round && fabs(vx_i) <= lp->tol_bnd) vx_i = 0.0;
            }
            else
            {  /* non-basic variable */
               switch (tagx_i)
               {  case LPX_NL:
                     vx_i = lp->lb[i]; break;
                  case LPX_NU:
                     vx_i = lp->ub[i]; break;
                  case LPX_NF:
                     vx_i = 0.0; break;
                  case LPX_NS:
                     vx_i = lp->lb[i]; break;
                  default:
                     insist(tagx_i != tagx_i);
               }
            }
            /* unscale the primal value */
            vx_i /= lp->rs[i];
         }
         *vx = vx_i;
      }
      /* obtain the dual value */
      if (dx != NULL)
      {  if (lp->d_stat == LPX_D_UNDEF)
         {  /* the dual value is undefined */
            dx_i = 0.0;
         }
         else
         {  if (tagx_i == LPX_BS)
            {  /* basic variable */
               dx_i = 0.0;
            }
            else
            {  /* non-basic variable */
               t = lp->posx[i] - m; /* x[i] = xN[t] */
               insist(1 <= t && t <= n);
               dx_i = lp->cbar[t];
               /* round the dual value (if required) */
               if (lp->round && fabs(dx_i) <= lp->tol_dj) dx_i = 0.0;
            }
            /* unscale the dual value */
            dx_i *= lp->rs[i];
         }
         *dx = dx_i;
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_get_col_info - obtain column solution information.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_get_col_info(LPX *lp, int j, int *tagx, double *vx,
--    double *dx);
--
-- *Description*
--
-- The routine lpx_get_col_info stores status, primal value and dual
-- value of the j-th structural variable (column) to locations, which
-- the parameters tagx, vx, and dx point to, respectively.
--
-- The status code has the following meaning:
--
-- LPX_BS - basic variable;
-- LPX_NL - non-basic variable on its lower bound;
-- LPX_NU - non-basic variable on its upper bound;
-- LPX_NF - non-basic free (unbounded) variable;
-- LPX_NS - non-basic fixed variable.
--
-- If some of pointers tagx, vx, or dx is NULL, the corresponding value
-- is not stored. */

void lpx_get_col_info(LPX *lp, int j, int *tagx, double *vx, double *dx)
{     int m = lp->m;
      int n = lp->n;
      int tagx_j, t;
      double vx_j, dx_j;
      if (!(1 <= j && j <= n))
         fault("lpx_get_col_info: j = %d; column number out of range",
            j);
      j += m;
      /* obtain the status */
      tagx_j = lp->tagx[j];
      if (tagx != NULL) *tagx = tagx_j;
      /* obtain the primal value */
      if (vx != NULL)
      {  if (lp->p_stat == LPX_P_UNDEF)
         {  /* the primal value is undefined */
            vx_j = 0.0;
         }
         else
         {  if (tagx_j == LPX_BS)
            {  /* basic variable */
               t = lp->posx[j]; /* x[j] = xB[t] */
               insist(1 <= t && t <= m);
               vx_j = lp->bbar[t];
               /* round the primal value (if required) */
               if (lp->round && fabs(vx_j) <= lp->tol_bnd) vx_j = 0.0;
            }
            else
            {  /* non-basic variable */
               switch (tagx_j)
               {  case LPX_NL:
                     vx_j = lp->lb[j]; break;
                  case LPX_NU:
                     vx_j = lp->ub[j]; break;
                  case LPX_NF:
                     vx_j = 0.0; break;
                  case LPX_NS:
                     vx_j = lp->lb[j]; break;
                  default:
                     insist(tagx_j != tagx_j);
               }
            }
            /* unscale the primal value */
            vx_j *= lp->rs[j];
         }
         *vx = vx_j;
      }
      /* obtain the dual value */
      if (dx != NULL)
      {  if (lp->d_stat == LPX_D_UNDEF)
         {  /* the dual value is undefined */
            dx_j = 0.0;
         }
         else
         {  if (tagx_j == LPX_BS)
            {  /* basic variable */
               dx_j = 0.0;
            }
            else
            {  /* non-basic variable */
               t = lp->posx[j] - m; /* x[i] = xN[t] */
               insist(1 <= t && t <= n);
               dx_j = lp->cbar[t];
               /* round the dual value (if required) */
               if (lp->round && fabs(dx_j) <= lp->tol_dj) dx_j = 0.0;
            }
            /* unscale the dual value */
            dx_j /= lp->rs[j];
         }
         *dx = dx_j;
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_get_obj_val - obtain value of the objective function.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_get_obj_val(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_obj_val returns the current value of the
-- objective function for an LP problem object, which the parameter lp
-- points to. */

double lpx_get_obj_val(LPX *lp)
{     int m = lp->m;
      int n = lp->n;
      int i, j;
      double sum, coef, vx;
      sum = lpx_get_obj_c0(lp);
      for (i = 1; i <= m; i++)
      {  coef = lpx_get_row_coef(lp, i);
         if (coef != 0.0)
         {  lpx_get_row_info(lp, i, NULL, &vx, NULL);
            sum += coef * vx;
         }
      }
      for (j = 1; j <= n; j++)
      {  coef = lpx_get_col_coef(lp, j);
         if (coef != 0.0)
         {  lpx_get_col_info(lp, j, NULL, &vx, NULL);
            sum += coef * vx;
         }
      }
      return sum;
}

/*----------------------------------------------------------------------
-- lpx_get_ips_stat - query status of interior point solution.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_ips_stat(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_ips_stat reports the status of interior point
-- solution obtained by the solver for an LP problem object, which the
-- parameter lp points to:
--
-- LPX_T_UNDEF  - the interior point solution is undefined;
-- LPX_T_OPT    - the interior point solution is optimal.
--
-- Note that additional status codes may appear in the future versions
-- of this routine. */

int lpx_get_ips_stat(LPX *lp)
{     int t_stat = lp->t_stat;
      return t_stat;
}

/*----------------------------------------------------------------------
-- lpx_get_ips_row - obtain row interior point solution.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_get_ips_row(LPX *lp, int i, double *vx, double *dx);
--
-- *Description*
--
-- The routine lpx_get_ips_row stores primal and dual interior point
-- values of the i-th auxiliary variable (row) to locations, which the
-- parameters vx and dx point to, respectively. If some of pointers vx
-- or dx is NULL, the corresponding value is not stored. */

void lpx_get_ips_row(LPX *lp, int i, double *vx, double *dx)
{     double vx_i, dx_i;
      if (!(1 <= i && i <= lp->m))
         fault("lpx_get_ips_row: i = %d; row number out of range", i);
      switch (lp->t_stat)
      {  case LPX_T_UNDEF:
            vx_i = dx_i = 0.0;
            break;
         case LPX_T_OPT:
            /* obtain primal and dual values */
            vx_i = lp->pv[i];
            dx_i = lp->dv[i];
#if 1
            /* round them (if required) */
            if (lp->round)
            {  if (fabs(vx_i) <= 1e-8) vx_i = 0.0;
               if (fabs(dx_i) <= 1e-8) dx_i = 0.0;
            }
#endif
            /* and unscale these values */
            vx_i /= lp->rs[i];
            dx_i *= lp->rs[i];
            break;
         default:
            insist(lp->t_stat != lp->t_stat);
      }
      if (vx != NULL) *vx = vx_i;
      if (dx != NULL) *dx = dx_i;
      return;
}

/*----------------------------------------------------------------------
-- lpx_get_ips_col - obtain column interior point solution.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_get_ips_col(LPX *lp, int j, double *vx, double *dx);
--
-- *Description*
--
-- The routine lpx_get_ips_col stores primal and dual interior point
-- values of the j-th structural variable (column) to locations, which
-- the parameters vx and dx point to, respectively. If some of pointers
-- vx or dx is NULL, the corresponding value is not stored. */

void lpx_get_ips_col(LPX *lp, int j, double *vx, double *dx)
{     double vx_j, dx_j;
      if (!(1 <= j && j <= lp->n))
         fault("lpx_get_ips_col: j = %d; column number out of range",
            j);
      j += lp->m;
      switch (lp->t_stat)
      {  case LPX_T_UNDEF:
            vx_j = dx_j = 0.0;
            break;
         case LPX_T_OPT:
            /* obtain primal and dual values */
            vx_j = lp->pv[j];
            dx_j = lp->dv[j];
#if 1
            /* round them (if required) */
            if (lp->round)
            {  if (fabs(vx_j) <= 1e-8) vx_j = 0.0;
               if (fabs(dx_j) <= 1e-8) dx_j = 0.0;
            }
#endif
            /* and unscale these values */
            vx_j *= lp->rs[j];
            dx_j /= lp->rs[j];
            break;
         default:
            insist(lp->t_stat != lp->t_stat);
      }
      if (vx != NULL) *vx = vx_j;
      if (dx != NULL) *dx = dx_j;
      return;
}

/*----------------------------------------------------------------------
-- lpx_get_ips_obj - obtain interior point value of the obj. function.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_get_ips_obj(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_ips_obj returns an interior point value of the
-- objective function. */

double lpx_get_ips_obj(LPX *lp)
{     int m = lp->m;
      int n = lp->n;
      int i, j;
      double sum, coef, vx;
      sum = lpx_get_obj_c0(lp);
      for (i = 1; i <= m; i++)
      {  coef = lpx_get_row_coef(lp, i);
         if (coef != 0.0)
         {  lpx_get_ips_row(lp, i, &vx, NULL);
            sum += coef * vx;
         }
      }
      for (j = 1; j <= n; j++)
      {  coef = lpx_get_col_coef(lp, j);
         if (coef != 0.0)
         {  lpx_get_ips_col(lp, j, &vx, NULL);
            sum += coef * vx;
         }
      }
      return sum;
}

/*----------------------------------------------------------------------
-- lpx_get_mip_stat - query status of MIP solution.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_get_mip_stat(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_mip_stat reports the status of a MIP solution
-- found by the solver for a MIP problem object, which the parameter lp
-- points to:
--
-- LPX_I_UNDEF  - the status is undefined (either the problem has not
--                been solved yet or no integer feasible solution has
--                been found yet).
--
-- LPX_I_OPT    - the solution is integer optimal.
--
-- LPX_I_FEAS   - the solution is integer feasible but its optimality
--                (or non-optimality) has not been proven, perhaps due
--                to premature termination of the search.
--
-- LPX_I_NOFEAS - the problem has no integer feasible solution (proven
--                by the solver). */

int lpx_get_mip_stat(LPX *lp)
{     if (lp->clss != LPX_MIP)
         fault("lpx_get_mip_stat: error -- not a MIP problem");
      return lp->i_stat;
}

/*----------------------------------------------------------------------
-- lpx_get_mip_row - obtain row activity for MIP solution.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_get_mip_row(LPX *lp, int i);
--
-- *Returns*
--
-- The routine returns a value of the i-th auxiliary variable (row) for
-- a MIP solution contained in the specified problem object. */

double lpx_get_mip_row(LPX *lp, int i)
{     double vx;
      if (lp->clss != LPX_MIP)
         fault("lpx_get_mip_row: error -- not a MIP problem");
      if (!(1 <= i && i <= lp->m))
         fault("lpx_get_mip_row: i = %d; row number out of range", i);
      if (!(lp->i_stat == LPX_I_OPT || lp->i_stat == LPX_I_FEAS))
         vx = 0.0;
      else
      {  vx = lp->mipx[i];
         if (lp->round)
         {  double eps = lp->tol_bnd / lp->rs[i];
            if (fabs(vx) <= eps) vx = 0.0;
         }
      }
      return vx;
}

/*----------------------------------------------------------------------
-- lpx_get_mip_col - obtain column activity for MIP solution.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_get_mip_col(LPX *lp, int j);
--
-- *Returns*
--
-- The routine returns a value of the j-th structural variable (column)
-- for a MIP solution contained in the specified problem object. */

double lpx_get_mip_col(LPX *lp, int j)
{     double vx;
      if (lp->clss != LPX_MIP)
         fault("lpx_get_mip_col: error -- not a MIP problem");
      if (!(1 <= j && j <= lp->n))
         fault("lpx_get_mip_col: j = %d; column number out of range",
            j);
      if (!(lp->i_stat == LPX_I_OPT || lp->i_stat == LPX_I_FEAS))
         vx = 0.0;
      else
      {  vx = lp->mipx[lp->m+j];
         if (lp->kind[j] == LPX_IV)
            insist(vx == floor(vx));
         else if (lp->round)
         {  double eps = lp->tol_bnd * lp->rs[lp->m+j];
            if (fabs(vx) <= eps) vx = 0.0;
         }
      }
      return vx;
}

/*----------------------------------------------------------------------
-- lpx_get_mip_obj - obtain value of the obj. func. for MIP solution.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- double lpx_get_mip_obj(LPX *lp);
--
-- *Returns*
--
-- The routine lpx_get_mip_obj returns a value of the obj. function for
-- a MIP solution contained in the specified problem object. */

double lpx_get_mip_obj(LPX *lp)
{     int i, j;
      double sum, coef;
      if (lp->clss != LPX_MIP)
         fault("lpx_get_mip_obj: error -- not a MIP problem");
      sum = lpx_get_obj_c0(lp);
      for (i = 1; i <= lp->m; i++)
      {  coef = lpx_get_row_coef(lp, i);
         if (coef != 0.0) sum += coef * lpx_get_mip_row(lp, i);
      }
      for (j = 1; j <= lp->n; j++)
      {  coef = lpx_get_col_coef(lp, j);
         if (coef != 0.0) sum += coef * lpx_get_mip_col(lp, j);
      }
      return sum;
}

/* eof */
