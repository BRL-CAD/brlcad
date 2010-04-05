/* glplpx1.c (problem creating and modifying routines) */

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

#include <ctype.h>
#include <math.h>
#include <string.h>
#include "glplib.h"
#include "glplpx.h"

/*----------------------------------------------------------------------
-- lpx_create_prob - create LP problem object.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- LPX *lpx_create_prob(void);
--
-- *Description*
--
-- The routine lpx_create_prob creates an LP problem object.
--
-- Initially the created object corresponds to an "empty" LP problem,
-- which has no rows and no columns.
--
-- *Returns*
--
-- The routine returns a pointer to the created object. */

LPX *lpx_create_prob(void)
{     LPX *lp;
      int m_max = 50, n_max = 100;
      lp = umalloc(sizeof(LPX));
      lp->m_max = m_max;
      lp->n_max = n_max;
      lp->m = lp->n = 0;
      lp->pool = create_str_pool();
      lp->buf = ucalloc(255+1, sizeof(char));
      lp->prob = NULL;
      lp->clss = LPX_LP;
      lp->name = ucalloc(1+m_max+n_max, sizeof(STR *));
      lp->typx = ucalloc(1+m_max+n_max, sizeof(int));
      lp->lb = ucalloc(1+m_max+n_max, sizeof(double));
      lp->ub = ucalloc(1+m_max+n_max, sizeof(double));
      lp->rs = ucalloc(1+m_max+n_max, sizeof(double));
      lp->mark = ucalloc(1+m_max+n_max, sizeof(int));
      lp->obj = NULL;
      lp->dir = LPX_MIN;
      lp->coef = ucalloc(1+m_max+n_max, sizeof(double));
      lp->coef[0] = 0.0;
      lp->A = spm_create();
      lp->b_stat = LPX_B_UNDEF;
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->tagx = ucalloc(1+m_max+n_max, sizeof(int));
      lp->posx = ucalloc(1+m_max+n_max, sizeof(int));
      lp->indx = ucalloc(1+m_max+n_max, sizeof(int));
      lp->inv = NULL;
      lp->bbar = ucalloc(1+m_max, sizeof(double));
      lp->pi = ucalloc(1+m_max, sizeof(double));
      lp->cbar = ucalloc(1+n_max, sizeof(double));
      lp->t_stat = LPX_T_UNDEF;
      lp->pv = NULL;
      lp->dv = NULL;
      lp->kind = NULL;
      lp->i_stat = LPX_I_UNDEF;
      lp->mipx = NULL;
      lpx_reset_parms(lp);
      return lp;
}

/*----------------------------------------------------------------------
-- lpx_realloc_prob - reallocate LP problem object.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_realloc_prob(LPX *lp, int m_max, int n_max);
--
-- *Description*
--
-- The routine lpx_realloc_prob reallocates arrays in an LP problem
-- object, which the parameter lp points to.
--
-- The parameter m_max specifies a new maximal number of rows, and the
-- parameter n_max specifies a new maximal number of columns.
--
-- This routine is intended for auxiliary purposes and should not be
-- used directly. */

void lpx_realloc_prob(LPX *lp, int m_max, int n_max)
{     int m = lp->m;
      int n = lp->n;
      void *temp;
      insist(m_max >= m);
      insist(n_max >= n);
#     define reallocate(type, ptr, max_len, len) \
      temp = ucalloc(max_len, sizeof(type)), \
      memcpy(temp, ptr, (len) * sizeof(type)), \
      ufree(ptr), ptr = temp
      reallocate(STR *, lp->name, 1+m_max+n_max, 1+m+n);
      reallocate(int, lp->typx, 1+m_max+n_max, 1+m+n);
      reallocate(double, lp->lb, 1+m_max+n_max, 1+m+n);
      reallocate(double, lp->ub, 1+m_max+n_max, 1+m+n);
      reallocate(double, lp->rs, 1+m_max+n_max, 1+m+n);
      reallocate(int, lp->mark, 1+m_max+n_max, 1+m+n);
      reallocate(double, lp->coef, 1+m_max+n_max, 1+m+n);
      reallocate(int, lp->tagx, 1+m_max+n_max, 1+m+n);
      reallocate(int, lp->posx, 1+m_max+n_max, 1+m+n);
      reallocate(int, lp->indx, 1+m_max+n_max, 1+m+n);
      reallocate(double, lp->bbar, 1+m_max, 1+m);
      reallocate(double, lp->pi, 1+m_max, 1+m);
      reallocate(double, lp->cbar, 1+n_max, 1+n);
      if (lp->pv != NULL)
         reallocate(double, lp->pv, 1+m_max+n_max, 1+m+n);
      if (lp->dv != NULL)
         reallocate(double, lp->dv, 1+m_max+n_max, 1+m+n);
      if (lp->clss == LPX_MIP)
      {  reallocate(int, lp->kind, 1+n_max, 1+n);
         reallocate(double, lp->mipx, 1+m_max+n_max, 1+m+n);
      }
#     undef reallocate
      lp->m_max = m_max;
      lp->n_max = n_max;
      return;
}

/*----------------------------------------------------------------------
-- lpx_add_rows - add new rows to LP problem object.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_add_rows(LPX *lp, int nrs);
--
-- *Description*
--
-- The routine lpx_add_rows adds nrs rows (constraints) to LP problem
-- object, which the parameter lp points to. New rows are always added
-- to the end of the row list, therefore the numbers of existing rows
-- remain unchanged.
--
-- Being added each a new row is free (unbounded) and has no constraint
-- coefficients. */

void lpx_add_rows(LPX *lp, int nrs)
{     int m = lp->m;
      int n = lp->n;
      STR **name = lp->name;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      double *rs = lp->rs;
      int *mark = lp->mark;
      double *coef = lp->coef;
      int *tagx = lp->tagx;
      int m_new, i;
      if (nrs < 1)
         fault("lpx_add_rows: nrs = %d; invalid parameter", nrs);
      /* determine new number of rows in the problem */
      m_new = m + nrs;
      /* reallocate the arrays (if necessary) */
      if (lp->m_max < m_new)
      {  int m_max = lp->m_max;
         while (m_max < m_new) m_max += m_max;
         lpx_realloc_prob(lp, m_max, lp->n_max);
         name = lp->name;
         typx = lp->typx;
         lb = lp->lb;
         ub = lp->ub;
         rs = lp->rs;
         mark = lp->mark;
         coef = lp->coef;
         tagx = lp->tagx;
      }
      /* we need a free place in the arrays, which contain information
         about rows and columns (i.e. which have m+n locations) */
      memmove(&name[m_new+1], &name[m+1], n * sizeof(STR *));
      memmove(&typx[m_new+1], &typx[m+1], n * sizeof(int));
      memmove(&lb[m_new+1], &lb[m+1], n * sizeof(double));
      memmove(&ub[m_new+1], &ub[m+1], n * sizeof(double));
      memmove(&rs[m_new+1], &rs[m+1], n * sizeof(double));
      memmove(&mark[m_new+1], &mark[m+1], n * sizeof(int));
      memmove(&coef[m_new+1], &coef[m+1], n * sizeof(double));
      memmove(&tagx[m_new+1], &tagx[m+1], n * sizeof(int));
      /* initialize new rows */
      for (i = m+1; i <= m_new; i++)
      {  /* the new row has no symbolic name */
         name[i] = NULL;
         /* the new row is free */
         typx[i] = LPX_FR, lb[i] = ub[i] = 0.0;
         /* the new row is not scaled */
         rs[i] = 1.0;
         /* the new row is unmarked */
         mark[i] = 0;
         /* the new row has zero objective coefficient */
         coef[i] = 0.0;
         /* the new row is basic */
         tagx[i] = LPX_BS;
      }
      /* set new number of rows */
      lp->m = m_new;
      /* also add nrs empty rows to the constraint matrix */
      spm_add_rows(lp->A, nrs);
      /* invalidate the factorization and the basic solution */
      lp->b_stat = LPX_B_UNDEF;
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_add_cols - add new columns to LP problem object.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_add_cols(LPX *lp, int ncs);
--
-- *Description*
--
-- The routine lpx_add_cols adds ncs columns (structural variables) to
-- LP problem object, which the parameter lp points to. New columns are
-- always added to the end of the column list, therefore the numbers of
-- existing columns remain unchanged.
--
-- Being added each a new column is fixed at zero and has no constraint
-- coefficients. */

void lpx_add_cols(LPX *lp, int ncs)
{     int m = lp->m;
      int n = lp->n;
      int clss = lp->clss;
      STR **name = lp->name;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      double *rs = lp->rs;
      int *mark = lp->mark;
      double *coef = lp->coef;
      int *tagx = lp->tagx;
      int *kind = lp->kind;
      int n_new, j;
      if (ncs < 1)
         fault("lpx_add_cols: ncs = %d; invalid parameter", ncs);
      /* determine new number of columns in the problem */
      n_new = n + ncs;
      /* reallocate the arrays (if necessary) */
      if (lp->n_max < n_new)
      {  int n_max = lp->n_max;
         while (n_max < n_new) n_max += n_max;
         lpx_realloc_prob(lp, lp->m_max, n_max);
         name = lp->name;
         typx = lp->typx;
         lb = lp->lb;
         ub = lp->ub;
         rs = lp->rs;
         mark = lp->mark;
         coef = lp->coef;
         tagx = lp->tagx;
         kind = lp->kind;
      }
      /* initialize new columns */
      for (j = m+n+1; j <= m+n_new; j++)
      {  /* the new column has no symbolic name */
         name[j] = NULL;
         /* the new column is fixed at zero */
         typx[j] = LPX_FX, lb[j] = ub[j] = 0.0;
         /* the new column is not scaled */
         rs[j] = 1.0;
         /* the new column is unmarked */
         mark[j] = 0;
         /* the new column has zero objective coefficient */
         coef[j] = 0.0;
         /* the new column is non-basic */
         tagx[j] = LPX_NS;
         /* the new column is continuous */
         if (clss == LPX_MIP) kind[j-m] = LPX_CV;
      }
      /* set new number of columns */
      lp->n = n_new;
      /* also add ncs empty columns to the constraint matrix */
      spm_add_cols(lp->A, ncs);
      /* invalidate the factorization and the basic solution */
      lp->b_stat = LPX_B_UNDEF;
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_check_name - check correctness of symbolic name.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_check_name(char *name);
--
-- *Description*
--
-- The routine lpx_check_name is intended to check if a given symbolic
-- name is correct.
--
-- A symbolic name is considered as correct, if it consists of 1 up to
-- 255 graphic characters.
--
-- *Returns*
--
-- If the symbolic name is correct, the routine lpx_check_name returns
-- zero. Otherwise the routine returns non-zero. */

int lpx_check_name(char *name)
{     int t;
      for (t = 0; name[t] != '\0'; t++)
         if (t == 255 || !isgraph((unsigned char)name[t])) return 1;
      return 0;
}

/*----------------------------------------------------------------------
-- lpx_set_prob_name - assign (change) problem name.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_prob_name(LPX *lp, char *name);
--
-- *Description*
--
-- The routine lpx_set_prob_name assigns a given symbolic name to the
-- problem object, which the parameter lp points to.
--
-- If the parameter name is NULL, the routine just erases an existing
-- name of the problem object. */

void lpx_set_prob_name(LPX *lp, char *name)
{     if (name == NULL)
      {  if (lp->prob != NULL)
         {  delete_str(lp->prob);
            lp->prob = NULL;
         }
      }
      else
      {  if (lpx_check_name(name))
            fault("lpx_set_prob_name: invalid problem name");
         if (lp->prob == NULL) lp->prob = create_str(lp->pool);
         set_str(lp->prob, name);
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_row_name - assign (change) row name.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_row_name(LPX *lp, int i, char *name);
--
-- *Description*
--
-- The routine lpx_set_row_name assigns a given symbolic name to the
-- i-th row (auxiliary variable).
--
-- If the parameter name is NULL, the routine just erases an existing
-- name of the i-th row. */

void lpx_set_row_name(LPX *lp, int i, char *name)
{     if (!(1 <= i && i <= lp->m))
         fault("lpx_set_row_name: i = %d; row number out of range", i);
      if (name == NULL)
      {  if (lp->name[i] != NULL)
         {  delete_str(lp->name[i]);
            lp->name[i] = NULL;
         }
      }
      else
      {  if (lpx_check_name(name))
            fault("lpx_set_row_name: i = %d; invalid row name", i);
         if (lp->name[i] == NULL) lp->name[i] = create_str(lp->pool);
         set_str(lp->name[i], name);
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_col_name - assign (change) column name.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_col_name(LPX *lp, int j, char *name);
--
-- *Description*
--
-- The routine lpx_set_col_name assigns a given symbolic name to the
-- j-th column (structural variable).
--
-- If the parameter name is NULL, the routine just erases an existing
-- name of the j-th column. */

void lpx_set_col_name(LPX *lp, int j, char *name)
{     if (!(1 <= j && j <= lp->n))
         fault("lpx_set_col_name: j = %d; column number out of range",
            j);
      j += lp->m;
      if (name == NULL)
      {  if (lp->name[j] != NULL)
         {  delete_str(lp->name[j]);
            lp->name[j] = NULL;
         }
      }
      else
      {  if (lpx_check_name(name))
            fault("lpx_set_col_name: j = %d; invalid column name", j);
         if (lp->name[j] == NULL) lp->name[j] = create_str(lp->pool);
         set_str(lp->name[j], name);
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_row_bnds - set (change) row bounds.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_row_bnds(LPX *lp, int i, int typx, double lb,
--    double ub);
--
-- *Description*
--
-- The routine lpx_set_row_bnds sets (changes) type and bounds of the
-- i-th row.
--
-- Parameters typx, lb, and ub specify respectively the type, the lower
-- bound, and the upper bound, which should be set for the i-th row:
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
-- If the row has no lower bound, the parameter lb is ignored. If the
-- row has no upper bound, the parameter ub is ignored. If the row is
-- of fixed type, the parameter lb is used, and the parameter ub is
-- ignored. */

void lpx_set_row_bnds(LPX *lp, int i, int typx, double lb, double ub)
{     if (!(1 <= i && i <= lp->m))
         fault("lpx_set_row_bnds: i = %d; row number out of range", i);
      lp->typx[i] = typx;
      switch (typx)
      {  case LPX_FR:
            lp->lb[i] = lp->ub[i] = 0.0;
            if (lp->tagx[i] != LPX_BS) lp->tagx[i] = LPX_NF;
            break;
         case LPX_LO:
            lp->lb[i] = lb * lp->rs[i], lp->ub[i] = 0.0;
            if (lp->tagx[i] != LPX_BS) lp->tagx[i] = LPX_NL;
            break;
         case LPX_UP:
            lp->lb[i] = 0.0, lp->ub[i] = ub * lp->rs[i];
            if (lp->tagx[i] != LPX_BS) lp->tagx[i] = LPX_NU;
            break;
         case LPX_DB:
            lp->lb[i] = lb * lp->rs[i], lp->ub[i] = ub * lp->rs[i];
            if (lp->tagx[i] != LPX_BS)
               lp->tagx[i] = (fabs(lb) <= fabs(ub) ? LPX_NL : LPX_NU);
            break;
         case LPX_FX:
            lp->lb[i] = lp->ub[i] = lb * lp->rs[i];
            if (lp->tagx[i] != LPX_BS) lp->tagx[i] = LPX_NS;
            break;
         default:
            fault("lpx_set_row_bnds: typx = %d; invalid row type",
               typx);
      }
      /* invalidate the basic solution */
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_col_bnds - set (change) column bounds.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_col_bnds(LPX *lp, int j, int typx, double lb,
--    double ub);
--
-- *Description*
--
-- The routine lpx_set_col_bnds sets (changes) type and bounds of the
-- j-th column.
--
-- Parameters typx, lb, and ub specify respectively the type, the lower
-- bound, and the upper bound, which should be set for the j-th column:
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
-- If the column has no lower bound, the parameter lb is ignored. If
-- the column has no upper bound, the parameter ub is ignored. If the
-- column is of fixed type, the parameter lb is used, and the parameter
-- ub is ignored. */

void lpx_set_col_bnds(LPX *lp, int j, int typx, double lb, double ub)
{     if (!(1 <= j && j <= lp->n))
         fault("lpx_set_col_bnds: j = %d; column number out of range",
            j);
      j += lp->m;
      lp->typx[j] = typx;
      switch (typx)
      {  case LPX_FR:
            lp->lb[j] = lp->ub[j] = 0.0;
            if (lp->tagx[j] != LPX_BS) lp->tagx[j] = LPX_NF;
            break;
         case LPX_LO:
            lp->lb[j] = lb / lp->rs[j], lp->ub[j] = 0.0;
            if (lp->tagx[j] != LPX_BS) lp->tagx[j] = LPX_NL;
            break;
         case LPX_UP:
            lp->lb[j] = 0.0, lp->ub[j] = ub / lp->rs[j];
            if (lp->tagx[j] != LPX_BS) lp->tagx[j] = LPX_NU;
            break;
         case LPX_DB:
            lp->lb[j] = lb / lp->rs[j], lp->ub[j] = ub / lp->rs[j];
            if (lp->tagx[j] != LPX_BS)
               lp->tagx[j] = (fabs(lb) <= fabs(ub) ? LPX_NL : LPX_NU);
            break;
         case LPX_FX:
            lp->lb[j] = lp->ub[j] = lb / lp->rs[j];
            if (lp->tagx[j] != LPX_BS) lp->tagx[j] = LPX_NS;
            break;
         default:
            fault("lpx_set_col_bnds: typx = %d; invalid column type",
               typx);
      }
      /* invalidate the basic solution */
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_class - set (change) problem class.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_class(LPX *lp, int clss);
--
-- *Description*
--
-- The routine lpx_set_class sets (changes) the class of the problem
-- object as specified by the parameter clss:
--
-- LPX_LP  - pure linear programming (LP) problem;
-- LPX_MIP - mixed integer programming (MIP) problem. */

void lpx_set_class(LPX *lp, int clss)
{     switch (clss)
      {  case LPX_LP:
            if (lp->clss == LPX_MIP)
            {  /* deallocate MIP data segment */
               ufree(lp->kind), lp->kind = NULL;
               ufree(lp->mipx), lp->mipx = NULL;
            }
            break;
         case LPX_MIP:
            if (lp->clss == LPX_LP)
            {  /* allocate and initialize MIP data segment */
               int j;
               lp->kind = ucalloc(1+lp->n_max, sizeof(int));
               lp->mipx = ucalloc(1+lp->m_max+lp->n_max, sizeof(double))
                  ;
               for (j = 1; j <= lp->n; j++) lp->kind[j] = LPX_CV;
               lp->i_stat = LPX_I_UNDEF;
            }
            break;
         default:
            fault("lpx_set_class: clss = %d; invalid parameter", clss);
      }
      /* set (change) problem class */
      lp->clss = clss;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_col_kind - set (change) column kind.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_col_kind(LPX *lp, int j, int kind);
--
-- *Description*
--
-- The routine lpx_set_col_kind sets (changes) the kind of the j-th
-- column (structural variable) as specified by the parameter kind:
--
-- LPX_CV - continuous variable;
-- LPX_IV - integer variable. */

void lpx_set_col_kind(LPX *lp, int j, int kind)
{     if (lp->clss != LPX_MIP)
         fault("lpx_set_col_kind: error -- not a MIP problem");
      if (!(1 <= j && j <= lp->n))
         fault("lpx_set_col_kind: j = %d; column number out of range",
            j);
      if (!(kind == LPX_CV || kind == LPX_IV))
         fault("lpx_set_col_kind: kind = %d; invalid parameter", kind);
      lp->kind[j] = kind;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_obj_name - assign (change) objective function name.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_obj_name(LPX *lp, char *name);
--
-- *Description*
--
-- The routine lpx_set_obj_name assigns a given symbolic name to the
-- objective function of the specified problem object.
--
-- If the parameter name is NULL, the routine just erases an existing
-- name of the objective function. */

void lpx_set_obj_name(LPX *lp, char *name)
{     if (name == NULL)
      {  if (lp->obj != NULL)
         {  delete_str(lp->obj);
            lp->obj = NULL;
         }
      }
      else
      {  if (lpx_check_name(name))
            fault("lpx_set_obj_name: invalid objective function name");
         if (lp->obj == NULL) lp->obj = create_str(lp->pool);
         set_str(lp->obj, name);
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_obj_dir - set (change) optimization direction.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_obj_dir(LPX *lp, int dir);
--
-- *Description*
--
-- The routine lpx_set_obj_dir sets (changes) optimization direction
-- (i.e. sense of the objective function), which is specified by the
-- parameter dir:
--
-- LPX_MIN - the objective function should be minimized;
-- LPX_MAX - the objective function should be maximized. */

void lpx_set_obj_dir(LPX *lp, int dir)
{     if (!(dir == LPX_MIN || dir == LPX_MAX))
         fault("lpx_set_obj_dir: dir = %d; invalid parameter");
      lp->dir = dir;
      /* invalidate the basic solution */
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_obj_c0 - set (change) constant term of the obj. function.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_obj_c0(LPX *lp, double c0);
--
-- *Description*
--
-- The routine lpx_set_obj_c0 sets (changes) a constant term of the
-- objective function for an LP problem object, which the parameter lp
-- points to. */

void lpx_set_obj_c0(LPX *lp, double c0)
{     lp->coef[0] = c0;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_row_coef - set (change) row objective coefficient.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_row_coef(LPX *lp, int i, double coef);
--
-- *Description*
--
-- The routine lpx_set_row_coef sets (changes) a coefficient of the
-- objective function at the i-th auxiliary variable (row). */

void lpx_set_row_coef(LPX *lp, int i, double coef)
{     if (!(1 <= i && i <= lp->m))
         fault("lpx_set_row_coef: i = %d; row number out of range", i);
      lp->coef[i] = coef / lp->rs[i];
      /* invalidate the basic solution */
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_col_coef - set (change) column objective coefficient.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_col_coef(LPX *lp, int j, double coef);
--
-- *Description*
--
-- The routine lpx_set_col_coef sets (changes) a coefficient of the
-- objective function at the j-th structural variable (column). */

void lpx_set_col_coef(LPX *lp, int j, double coef)
{     if (!(1 <= j && j <= lp->n))
         fault("lpx_set_col_coef: j = %d; column number out of range",
            j);
      j += lp->m;
      lp->coef[j] = coef * lp->rs[j];
      /* invalidate the basic solution */
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_row_stat - set (change) row status.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_row_stat(LPX *lp, int i, int stat);
--
-- *Description*
--
-- The routine lpx_set_row_stat sets (changes) status of the i-th row
-- (auxiliary variable) as specified by the parameter stat:
--
-- LPX_BS   - make the row basic (make the constraint inactive);
-- LPX_NL   - make the row non-basic (make the constraint active);
-- LPX_NU   - make the row non-basic and set it to the upper bound; if
--            the row is not double-bounded, this status is equivalent
--            to LPX_NL (only in the case of this routine);
-- LPX_NF   - the same as LPX_NL (only in the case of this routine);
-- LPX_NS   - the same as LPX_NL (only in the case of this routine). */

void lpx_set_row_stat(LPX *lp, int i, int stat)
{     if (!(1 <= i && i <= lp->m))
         fault("lpx_set_row_stat: i = %d; row number out of range", i);
      if (!(stat == LPX_BS || stat == LPX_NL || stat == LPX_NU ||
            stat == LPX_NF || stat == LPX_NS))
         fault("lpx_set_row_stat: stat = %d; invalid parameter", stat);
      if (stat != LPX_BS)
      {  switch (lp->typx[i])
         {  case LPX_FR:
               stat = LPX_NF; break;
            case LPX_LO:
               stat = LPX_NL; break;
            case LPX_UP:
               stat = LPX_NU; break;
            case LPX_DB:
               if (stat != LPX_NL) stat = LPX_NU; break;
            case LPX_FX:
               stat = LPX_NS; break;
         }
      }
      if (lp->tagx[i] != stat)
      {  /* if the basis is changed, invalidate the factorization */
         if (lp->tagx[i] == LPX_BS && stat != LPX_BS ||
             lp->tagx[i] != LPX_BS && stat == LPX_BS)
            lp->b_stat = LPX_B_UNDEF;
         /* invalidate the current basic solution */
         lp->p_stat = LPX_P_UNDEF;
         lp->d_stat = LPX_D_UNDEF;
         /* set new status of the row */
         lp->tagx[i] = stat;
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_col_stat - set (change) column status.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_col_stat(LPX *lp, int j, int stat);
--
-- *Description*
--
-- The routine lpx_set_col_stat sets (changes) status of the j-th column
-- (structural variable) as specified by the parameter stat:
--
-- LPX_BS   - make the column basic;
-- LPX_NL   - make the column non-basic;
-- LPX_NU   - make the column non-basic and set it to the upper bound;
--            if the column is not of double-bounded type, this status
--            is the same as LPX_NL (only in the case of this routine);
-- LPX_NF   - the same as LPX_NL (only in the case of this routine);
-- LPX_NS   - the same as LPX_NL (only in the case of this routine). */

void lpx_set_col_stat(LPX *lp, int j, int stat)
{     if (!(1 <= j && j <= lp->n))
         fault("lpx_set_col_stat: j = %d; column number out of range",
            j);
      if (!(stat == LPX_BS || stat == LPX_NL || stat == LPX_NU ||
            stat == LPX_NF || stat == LPX_NS))
         fault("lpx_set_col_stat: stat = %d; invalid parameter", stat);
      j += lp->m;
      if (stat != LPX_BS)
      {  switch (lp->typx[j])
         {  case LPX_FR:
               stat = LPX_NF; break;
            case LPX_LO:
               stat = LPX_NL; break;
            case LPX_UP:
               stat = LPX_NU; break;
            case LPX_DB:
               if (stat != LPX_NL) stat = LPX_NU; break;
            case LPX_FX:
               stat = LPX_NS; break;
         }
      }
      if (lp->tagx[j] != stat)
      {  /* if the basis is changed, invalidate the factorization */
         if (lp->tagx[j] == LPX_BS && stat != LPX_BS ||
             lp->tagx[j] != LPX_BS && stat == LPX_BS)
            lp->b_stat = LPX_B_UNDEF;
         /* invalidate the current basic solution */
         lp->p_stat = LPX_P_UNDEF;
         lp->d_stat = LPX_D_UNDEF;
         /* set new status of the row */
         lp->tagx[j] = stat;
      }
      return;
}

/*----------------------------------------------------------------------
-- lpx_load_mat - load constraint matrix.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_load_mat(LPX *lp,
--    void *info, double (*mat)(void *info, int *i, int *j));
--
-- *Description*
--
-- The routine lpx_load_mat loads non-zero elements of the constraint
-- matrix (i.e. constraint coefficients) from a file specified by the
-- formal routine mat. All existing contents of the constraint matrix
-- is destroyed.
--
-- The parameter info is a transit pointer passed to the formal routine
-- mat (see below).
--
-- The formal routine mat specifies a set of non-zero elements, which
-- should be loaded into the matrix. The routine lpx_load_mat calls the
-- routine mat in order to obtain a next non-zero element a[i,j]. In
-- response the routine mat should store row and column indices of this
-- next element to the locations *i and *j and return a numerical value
-- of the element. Elements may be enumerated in arbirary order. Note
-- that zero elements and multiplets (i.e. elements with identical row
-- and column indices) are not allowed. If there is no next element, the
-- routine mat should store zero to both locations *i and *j and then
-- "rewind" the file in order to begin enumerating again from the first
-- element. */

struct mat_info
{     /* transit information passed to the routine mat */
      LPX *lp;
      void *info;
      double (*mat)(void *info, int *i, int *j);
};

static double mat(void *_info, int *i, int *j)
{     /* this intermediate routine is used to scale data loaded in the
         constraint matrix */
      struct mat_info *info = _info;
      LPX *lp = info->lp;
      int m = lp->m;
      int n = lp->n;
      double aij = info->mat(info->info, i, j);
      if (!(*i == 0 && *j == 0))
      {  if (!(1 <= *i && *i <= m))
            fault("lpx_load_mat: i = %d; invalid row number", *i);
         if (!(1 <= *j && *j <= n))
            fault("lpx_load_mat: j = %d; invalid column number", *j);
         if (aij == 0.0)
            fault("lpx_load_mat: i = %d, j = %d; zero coefficient not a"
               "llowed", *i, *j);
         aij *= (lp->rs[*i] * lp->rs[m + *j]);
      }
      return aij;
}

void lpx_load_mat(LPX *lp,
      void *_info, double (*_mat)(void *info, int *i, int *j))
{     struct mat_info info;
      /* load scaled data in the constraint matrix */
      info.lp = lp;
      info.info = _info;
      info.mat = _mat;
      spm_load_data(lp->A, &info, mat);
      /* invalidate the factorization and the basic solution */
      lp->b_stat = LPX_B_UNDEF;
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_load_mat3 - load constraint matrix.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_load_mat3(LPX *lp, int nz, int rn[], int cn[], double a[]);
--
-- *Description*
--
-- The routine lpx_load_mat3 loads non-zero elements of the constraint
-- matrix (i.e. constraint coefficients) from the arrays rn, cn, and a.
-- All existing contents of the constraint matrix is destroyed.
--
-- The parameter nz specifies number of loaded non-zero coefficients.
--
-- A particular constraint coefficient should be specified as a triplet
-- (rn[k], cn[k], a[k]), k = 1, ..., nz, where rn[k] is row index, cn[k]
-- is column index, and a[k] is its numerical value. Coefficients may be
-- enumerated in arbitrary order. Note that zero coefficients as well as
-- multiplets (i.e. coefficients with identical row and column indices)
-- are not allowed. */

struct mat3_info
{     /* transit information passed to the routine mat3 */
      int nz;
      int *rn;
      int *cn;
      double *a;
      int ptr;
};

static double mat3(void *_info, int *i, int *j)
{     /* "reads" a next element of the constraint matrix */
      struct mat3_info *info = _info;
      info->ptr++;
      if (info->ptr > info->nz)
      {  *i = *j = 0;
         info->ptr = 0;
         return 0.0;
      }
      *i = info->rn[info->ptr];
      *j = info->cn[info->ptr];
      return info->a[info->ptr];
}

void lpx_load_mat3(LPX *lp, int nz, int rn[], int cn[], double a[])
{     struct mat3_info info;
      info.nz = nz;
      info.rn = rn;
      info.cn = cn;
      info.a = a;
      info.ptr = 0;
      lpx_load_mat(lp, &info, mat3);
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_mat_row - set (replace) row of the constraint matrix.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_mat_row(LPX *lp, int i, int len, int ndx[],
--    double val[]);
--
-- *Description*
--
-- The routine lpx_set_mat_row sets (replaces) the i-th row of the
-- constraint matrix for the LP problem object, which the parameter lp
-- points to.
--
-- Column indices and numerical values of new non-zero coefficients of
-- the i-th row should be placed in the locations ndx[1], ..., ndx[len]
-- and val[1], ..., val[len] respectively, where 0 <= len <= n is the
-- new length of the i-th row, n is number of columns.
--
-- Note that zero coefficients and multiplets (i.e. coefficients with
-- identical column indices) are not allowed. */

void lpx_set_mat_row(LPX *lp, int i, int len, int ndx[], double val[])
{     int m = lp->m;
      int n = lp->n;
      int i_beg, i_end, i_ptr, j;
      if (!(1 <= i && i <= m))
         fault("lpx_set_mat_row: i = %d; row number out of range", i);
      if (!(0 <= len && len <= n))
         fault("lpx_set_mat_row: len = %d; invalid row length", len);
      /* if the old i-th row has non-zero constraint coefficients in
         any basic columns, invalidate the factorization */
      i_beg = lp->A->ptr[i];
      i_end = i_beg + lp->A->len[i] - 1;
      for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
      {  j = lp->A->ndx[i_ptr];
         if (lp->tagx[m+j] == LPX_BS)
         {  lp->b_stat = LPX_B_UNDEF;
            break;
         }
      }
      /* replace the i-th row of the constraint matrix */
      spm_set_row(lp->A, i, len, ndx, val, &lp->rs[0], &lp->rs[m]);
      /* if the new i-th row has non-zero constraint coefficients in
         any basic columns, invalidate the factorization */
      i_beg = lp->A->ptr[i];
      i_end = i_beg + lp->A->len[i] - 1;
      for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
      {  j = lp->A->ndx[i_ptr];
         if (lp->tagx[m+j] == LPX_BS)
         {  lp->b_stat = LPX_B_UNDEF;
            break;
         }
      }
      /* invalidate the basic solution */
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_set_mat_col - set (replace) column of the constraint matrix.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_set_mat_col(LPX *lp, int j, int len, int ndx[],
--    double val[]);
--
-- *Description*
--
-- The routine lpx_set_mat_col sets (replaces) the j-th column of the
-- constraint matrix for the LP problem object, which the parameter lp
-- points to.
--
-- Row indices and numerical values of new non-zero coefficients of the
-- j-th column should be placed in the locations ndx[1], ..., ndx[len]
-- and val[1], ..., val[len] respectively, where 0 <= len <= m is the
-- new length of the j-th column, m is number of rows.
--
-- Note that zero coefficients and multiplets (i.e. coefficients with
-- identical row indices) are not allowed. */

void lpx_set_mat_col(LPX *lp, int j, int len, int ndx[], double val[])
{     int m = lp->m;
      int n = lp->n;
      if (!(1 <= j && j <= n))
         fault("lpx_set_mat_col: j = %d; column number out of range",
            j);
      if (!(0 <= len && len <= m))
         fault("lpx_set_mat_col: len = %d; invalid column length", len);
      /* replace the j-th column of the constraint matrix */
      spm_set_col(lp->A, j, len, ndx, val, &lp->rs[0], &lp->rs[m]);
      /* if the j-th column is basic, invalidate the factorization */
      if (lp->tagx[m+j] == LPX_BS) lp->b_stat = LPX_B_UNDEF;
      /* invalidate the basic solution */
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_unmark_all - unmark all rows and columns.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_unmark_all(LPX *lp);
--
-- *Description*
--
-- The routine lpx_unmark_all resets marks of all rows and columns of
-- the LP problem object to zero. */

void lpx_unmark_all(LPX *lp)
{     int m = lp->m;
      int n = lp->n;
      int *mark = lp->mark;
      int k;
      for (k = 1; k <= m+n; k++) mark[k] = 0;
      return;
}

/*----------------------------------------------------------------------
-- lpx_mark_row - assign mark to row.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_mark_row(LPX *lp, int i, int mark);
--
-- *Description*
--
-- The routine lpx_mark_row assigns an integer mark to the i-th row.
--
-- The sense of marking depends on what operation will be performed on
-- the LP problem object. */

void lpx_mark_row(LPX *lp, int i, int mark)
{     if (!(1 <= i && i <= lp->m))
         fault("lpx_mark_row: i = %d; row number out of range", i);
      lp->mark[i] = mark;
      return;
}

/*----------------------------------------------------------------------
-- lpx_mark_col - assign mark to column.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_mark_col(LPX *lp, int j, int mark);
--
-- The routine lpx_mark_col assigns an integer mark to the j-th column.
--
-- The sense of marking depends on what operation will be performed on
-- the LP problem object. */

void lpx_mark_col(LPX *lp, int j, int mark)
{     if (!(1 <= j && j <= lp->n))
         fault("lpx_mark_col: j = %d; column number out of range", j);
      lp->mark[lp->m + j] = mark;
      return;
}

/*----------------------------------------------------------------------
-- lpx_clear_mat - clear rows and columns of the constraint matrix.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_clear_mat(LPX *lp);
--
-- *Description*
--
-- The routine lpx_clear_mat clears (nullifies) marked rows and columns
-- of the constraint matrix.
--
-- On exit the routine remains the row and column marks unchanged. */

void lpx_clear_mat(LPX *lp)
{     int m = lp->m;
      int n = lp->n;
      int i, i_beg, i_end, i_ptr, j;
      /* if at least one non-zero coefficient in marked rows or columns
         belong to the basic matrix, invalidate the factorization */
      for (i = 1; i <= m; i++)
      {  if (lp->mark[i])
         {  i_beg = lp->A->ptr[i];
            i_end = i_beg + lp->A->len[i] - 1;
            for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
            {  if (lp->tagx[m + lp->A->ndx[i_ptr]] == LPX_BS)
               {  lp->b_stat = LPX_B_UNDEF;
                  goto skip;
               }
            }
         }
      }
      for (j = m+1; j <= m+n; j++)
      {  if (lp->mark[j] && lp->tagx[j] == LPX_BS)
         {  lp->b_stat = LPX_B_UNDEF;
            break;
         }
      }
skip: /* clear marked rows of the constraint matrix */
      spm_clear_rows(lp->A, &lp->mark[0]);
      /* clear marked columns of the constraint matrix */
      spm_clear_cols(lp->A, &lp->mark[m]);
      /* invalidate the basic solution */
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_del_items - delete rows and columns from LP problem object.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_del_items(LPX *lp);
--
-- *Description*
--
-- The routine lpx_del_items deletes all marked rows and columns from
-- an LP problem object, which the parameter lp points to. */

void lpx_del_items(LPX *lp)
{     int m = lp->m;
      int n = lp->n;
      int clss = lp->clss;
      STR **name = lp->name;
      int *typx = lp->typx;
      double *lb = lp->lb;
      double *ub = lp->ub;
      double *rs = lp->rs;
      int *mark = lp->mark;
      double *coef = lp->coef;
      int *tagx = lp->tagx;
      int *kind = lp->kind;
      int m_new, n_new, k, k_new;
      /* delete marked rows and columns from the LP problem object */
      m_new = n_new = 0;
      for (k = 1; k <= m+n; k++)
      {  if (mark[k])
         {  /* the row/column should be deleted */
            if (name[k] != NULL) delete_str(name[k]);
         }
         else
         {  /* the row/column should be kept */
            if (k <= m) m_new++; else n_new++;
            k_new = m_new + n_new;
            name[k_new] = name[k];
            typx[k_new] = typx[k];
            lb[k_new] = lb[k];
            ub[k_new] = ub[k];
            rs[k_new] = rs[k];
            coef[k_new] = coef[k];
            tagx[k_new] = tagx[k];
            if (clss == LPX_MIP && k > m) kind[n_new] = kind[k-m];
         }
      }
      /* delete marked rows from the constraint matrix */
      if (m_new < m) spm_del_rows(lp->A, &mark[0]);
      /* delete marked columns from the constraint matrix */
      if (n_new < n) spm_del_cols(lp->A, &mark[m]);
      /* set new problem size */
      lp->m = m_new;
      lp->n = n_new;
      /* clear marks of rows and columns that are still in LP */
      lpx_unmark_all(lp);
      /* invalidate the factorization and the basic solution */
      lp->b_stat = LPX_B_UNDEF;
      lp->p_stat = LPX_P_UNDEF;
      lp->d_stat = LPX_D_UNDEF;
      lp->t_stat = LPX_T_UNDEF;
      lp->i_stat = LPX_I_UNDEF;
      return;
}

/*----------------------------------------------------------------------
-- lpx_delete_prob - delete LP problem object.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- void lpx_delete_prob(LPX *lp);
--
-- *Description*
--
-- The routine lpx_delete_prob deletes an LP problem object, which the
-- parameter lp points to, freeing all the memory allocated to it. */

void lpx_delete_prob(LPX *lp)
{     dmp_delete_pool(lp->pool);
      ufree(lp->buf);
      ufree(lp->name);
      ufree(lp->typx);
      ufree(lp->lb);
      ufree(lp->ub);
      ufree(lp->rs);
      ufree(lp->mark);
      ufree(lp->coef);
      spm_delete(lp->A);
      ufree(lp->tagx);
      ufree(lp->posx);
      ufree(lp->indx);
      if (lp->inv != NULL) inv_delete(lp->inv);
      ufree(lp->bbar);
      ufree(lp->pi);
      ufree(lp->cbar);
      if (lp->pv != NULL) ufree(lp->pv);
      if (lp->dv != NULL) ufree(lp->dv);
      if (lp->clss == LPX_MIP)
      {  ufree(lp->kind);
         ufree(lp->mipx);
      }
      ufree(lp);
      return;
}

/* eof */
