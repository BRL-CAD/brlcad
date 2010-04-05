/* glplpx8d.c */

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
#include "glplib.h"
#include "glplpx.h"
#include "glpmpl.h"

/*----------------------------------------------------------------------
-- lpx_read_model - read LP/MIP model written in GNU MathProg language.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- LPX *lpx_read_model(char *model, char *data, char *output);
--
-- *Description*
--
-- The routine lpx_read_model reads and translates LP/MIP model written
-- in the GNU MathProg modeling language.
--
-- The character string model specifies name of input text file, which
-- contains model section and, optionally, data section. This parameter
-- cannot be NULL.
--
-- The character string data specifies name of input text file, which
-- contains data section. If data section follows model section in the
-- same file, the data file is not read and a warning is issued. If the
-- parameter data is NULL, the data file is not used (that assumes the
-- data section follows model section in the same file).
--
-- The character string output specifies name of output text file, to
-- which the output produced by display statements is written. If the
-- parameter output is NULL, the display output is sent to stdout via
-- the routine print.
--
-- *Returns*
--
-- If no errors occurred, the routine returns a pointer to the problem
-- object created. Otherwise the routine returns NULL. */

struct mpl_info
{     /* working info used by the routine mpl_mat */
      MPL *mpl;
      /* translator database */
      int i;
      /* number of current row */
      int t;
      /* pointer to current element in current row */
      int *ndx;
      /* column indices of elements in current row */
      double *val;
      /* numeric values of elements in current row */
};

static double mpl_mat(void *_info, int *i, int *j)
{     /* "read" next element of the constraint matrix */
      struct mpl_info *info = _info;
      double aij;
read: if (info->i == 0 || info->t == 0)
      {  /* either it is the first call or the current row has been
            completely read; the first/next row is needed */
         info->i++;
         if (info->i > mpl_get_num_rows(info->mpl))
         {  /* the entire matrix has been completely read */
            *i = *j = 0;            /* end of file */
            info->i = info->t = 0;  /* rewind */
            return 0.0;
         }
         /* obtain the row of the constraint matrix and set the pointer
            to its tailing element */
         info->t = mpl_get_mat_row(info->mpl, info->i, info->ndx,
            info->val);
         /* the row may be empty, so repeat the check */
         goto read;
      }
      /* obtain the current element */
      *i = info->i;
      *j = info->ndx[info->t];
      aij = info->val[info->t];
      insist(aij != 0.0);
      /* move the pointer to preceding element in the row */
      info->t--;
      /* bring the element to the calling program */
      return aij;
}

LPX *lpx_read_model(char *model, char *data, char *output)
{     struct mpl_info _info, *info = &_info;
      LPX *lp = NULL;
      MPL *mpl;
      int ret, m, n, i, j, *ndx;
      double *val;
      /* create and initialize the translator database */
      mpl = mpl_initialize();
      /* read model section and optional data section */
      ret = mpl_read_model(mpl, model);
      if (ret == 4) goto done;
      insist(ret == 1 || ret == 2);
      /* read data section, if necessary */
      if (data != NULL)
      {  if (ret == 1)
         {  ret = mpl_read_data(mpl, data);
            if (ret == 4) goto done;
            insist(ret == 2);
         }
         else
            print("lpx_read_model: data section already read; file `%s'"
               " ignored", data);
      }
      /* generate model */
      ret = mpl_generate(mpl, output);
      if (ret == 4) goto done;
      insist(ret == 3);
      /* build problem instance */
      lp = lpx_create_prob();
      /* set problem name */
      lpx_set_prob_name(lp, mpl_get_prob_name(mpl));
      /* build rows (constraints) */
      m = mpl_get_num_rows(mpl);
      if (m > 0) lpx_add_rows(lp, m);
      for (i = 1; i <= m; i++)
      {  int type;
         double lb, ub;
         /* set row name */
         lpx_set_row_name(lp, i, mpl_get_row_name(mpl, i));
         /* set row bounds */
         type = mpl_get_row_bnds(mpl, i, &lb, &ub);
         switch (type)
         {  case MPL_FR: type = LPX_FR; break;
            case MPL_LO: type = LPX_LO; break;
            case MPL_UP: type = LPX_UP; break;
            case MPL_DB: type = LPX_DB; break;
            case MPL_FX: type = LPX_FX; break;
            default: insist(type != type);
         }
         if (type == LPX_DB && fabs(lb - ub) < 1e-9 * (1.0 + fabs(lb)))
         {  type = LPX_FX;
            if (fabs(lb) <= fabs(ub)) ub = lb; else lb = ub;
         }
         lpx_set_row_bnds(lp, i, type, lb, ub);
         /* warn about non-zero constant term */
         if (mpl_get_row_c0(mpl, i) != 0.0)
            print("lpx_read_model: row %s; constant term %.12g ignored",
               mpl_get_row_name(mpl, i), mpl_get_row_c0(mpl, i));
      }
      /* build columns (variables) */
      n = mpl_get_num_cols(mpl);
      if (n > 0) lpx_add_cols(lp, n);
      for (j = 1; j <= n; j++)
      {  int kind, type;
         double lb, ub;
         /* set column name */
         lpx_set_col_name(lp, j, mpl_get_col_name(mpl, j));
         /* set column kind */
         kind = mpl_get_col_kind(mpl, j);
         switch (kind)
         {  case MPL_NUM:
               break;
            case MPL_INT:
            case MPL_BIN:
               lpx_set_class(lp, LPX_MIP);
               lpx_set_col_kind(lp, j, LPX_IV);
               break;
            default:
               insist(kind != kind);
         }
         /* set column bounds */
         type = mpl_get_col_bnds(mpl, j, &lb, &ub);
         switch (type)
         {  case MPL_FR: type = LPX_FR; break;
            case MPL_LO: type = LPX_LO; break;
            case MPL_UP: type = LPX_UP; break;
            case MPL_DB: type = LPX_DB; break;
            case MPL_FX: type = LPX_FX; break;
            default: insist(type != type);
         }
         if (kind == MPL_BIN)
         {  if (type == LPX_FR || type == LPX_UP || lb < 0.0) lb = 0.0;
            if (type == LPX_FR || type == LPX_LO || ub > 1.0) ub = 1.0;
            type = LPX_DB;
         }
         if (type == LPX_DB && fabs(lb - ub) < 1e-9 * (1.0 + fabs(lb)))
         {  type = LPX_FX;
            if (fabs(lb) <= fabs(ub)) ub = lb; else lb = ub;
         }
         lpx_set_col_bnds(lp, j, type, lb, ub);
      }
      /* allocate working arrays */
      ndx = ucalloc(1+n, sizeof(int));
      val = ucalloc(1+n, sizeof(double));
      /* load the constraint matrix */
      info->mpl = mpl;
      info->i = info->t = 0;
      info->ndx = ndx;
      info->val = val;
      lpx_load_mat(lp, info, mpl_mat);
      /* build objective function (the first objective is used) */
      for (i = 1; i <= m; i++)
      {  int kind;
         kind = mpl_get_row_kind(mpl, i);
         if (kind == MPL_MIN || kind == MPL_MAX)
         {  int len, t;
            /* set objective name */
            lpx_set_obj_name(lp, mpl_get_row_name(mpl, i));
            /* set objective coefficients */
            len = mpl_get_mat_row(mpl, i, ndx, val);
            for (t = 1; t <= len; t++)
               lpx_set_col_coef(lp, ndx[t], val[t]);
            /* set constant term */
            lpx_set_obj_c0(lp, mpl_get_row_c0(mpl, i));
            /* set optimization direction */
            lpx_set_obj_dir(lp, kind == MPL_MIN ? LPX_MIN : LPX_MAX);
            break;
         }
      }
      /* free working arrays */
      ufree(ndx);
      ufree(val);
done: /* free resources used by the model translator */
      mpl_terminate(mpl);
      return lp;
}

/* eof */
