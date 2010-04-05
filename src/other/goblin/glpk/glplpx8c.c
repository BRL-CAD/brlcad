/* glplpx8c.c */

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
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "glplib.h"
#include "glplpt.h"
#include "glplpx.h"

/*----------------------------------------------------------------------
-- lpx_read_lpt - read LP/MIP problem using CPLEX LP format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- LPX *lpx_read_lpt(char *fname);
--
-- *Description*
--
-- The routine lpx_read_lpt reads LP/MIP problem data using CPLEX LP
-- format from a text file whose name is the character string fname.
--
-- *Returns*
--
-- If no error occurred, the routine returns a pointer to the created
-- problem object. Otherwise the routine returns NULL. */

struct lpt_info
{     /* transit information passed to the routine lpt_mat */
      LPT *lpt;
      /* LP/MIP problem data block */
      LPTROW *row;
      /* pointer to the current row (constraint) */
      LPTLFE *lfe;
      /* pointer to the next term of the corresponding linear form */
};

static double lpt_mat(void *_info, int *i, int *j)
{     /* "read" a next element of the constraint matrix */
      struct lpt_info *info = _info;
      double aij;
read: if (info->row == NULL || info->lfe == NULL)
      {  /* either it is the first call or the current row has been
            completely read; choose the first/next row */
         if (info->row == NULL)
            info->row = info->lpt->first_row;
         else
            info->row = info->row->next;
         if (info->row == NULL)
         {  /* the entire matrix has been completely read */
            *i = *j = 0; /* end of file */
            return 0.0;
         }
         /* set the pointer to the first element of the row */
         info->lfe = info->row->ptr;
         /* the row may be empty, so repeat the check */
         goto read;
      }
      /* obtain the next element from the current row */
      *i = info->row->i;
      *j = info->lfe->col->j;
      aij = info->lfe->coef;
      /* advance the pointer */
      info->lfe = info->lfe->next;
      /* skip the element if it has zero value */
      if (aij == 0.0) goto read;
      /* bring the next element to the calling program */
      return aij;
}

LPX *lpx_read_lpt(char *fname)
{     LPX *lpx;
      LPT *lpt; LPTROW *row; LPTCOL *col; LPTLFE *lfe;
      int i, j;
      /* read LP/MIP problem in CPLEX LP format */
      lpt = lpt_read_prob(fname);
      if (lpt == NULL) return NULL;
      /* create problem object */
      lpx = lpx_create_prob();
      lpx_set_prob_name(lpx, "PROBLEM");
      lpx_add_rows(lpx, lpt->m);
      lpx_add_cols(lpx, lpt->n);
      /* process the objective function */
      lpx_set_obj_name(lpx, lpt->name);
      switch (lpt->sense)
      {  case LPT_MIN:
            lpx_set_obj_dir(lpx, LPX_MIN);
            break;
         case LPT_MAX:
            lpx_set_obj_dir(lpx, LPX_MAX);
            break;
         default:
            insist(lpt->sense != lpt->sense);
      }
      for (lfe = lpt->obj; lfe != NULL; lfe = lfe->next)
         lpx_set_col_coef(lpx, lfe->col->j, lfe->coef);
      /* process rows (constraints) */
      i = 0;
      for (row = lpt->first_row; row != NULL; row = row->next)
      {  i++;
         lpx_set_row_name(lpx, i, row->name);
         insist(row->i == i);
         switch (row->sense)
         {  case LPT_LE:
               lpx_set_row_bnds(lpx, i, LPX_UP, 0.0, row->rhs);
               break;
            case LPT_GE:
               lpx_set_row_bnds(lpx, i, LPX_LO, row->rhs, 0.0);
               break;
            case LPT_EQ:
               lpx_set_row_bnds(lpx, i, LPX_FX, row->rhs, row->rhs);
               break;
            default:
               insist(row->sense != row->sense);
         }
      }
      insist(i == lpt->m);
      /* process columns (variables) */
      j = 0;
      for (col = lpt->first_col; col != NULL; col = col->next)
      {  j++;
         lpx_set_col_name(lpx, j, col->name);
         insist(col->j == j);
         switch (col->kind)
         {  case LPT_CON:
               break;
            case LPT_INT:
            case LPT_BIN:
               lpx_set_class(lpx, LPX_MIP);
               lpx_set_col_kind(lpx, j, LPX_IV);
               break;
            default:
               insist(col->kind != col->kind);
         }
         if (col->lb == -DBL_MAX && col->ub == +DBL_MAX)
            lpx_set_col_bnds(lpx, j, LPX_FR, 0.0, 0.0);
         else if (col->ub == +DBL_MAX)
            lpx_set_col_bnds(lpx, j, LPX_LO, col->lb, 0.0);
         else if (col->lb == -DBL_MAX)
            lpx_set_col_bnds(lpx, j, LPX_UP, 0.0, col->ub);
         else if (col->lb != col->ub)
            lpx_set_col_bnds(lpx, j, LPX_DB, col->lb, col->ub);
         else
            lpx_set_col_bnds(lpx, j, LPX_FX, col->lb, col->ub);
      }
      insist(j == lpt->n);
      /* load the constraint matrix */
      {  struct lpt_info info;
         info.lpt = lpt;
         info.row = NULL;
         info.lfe = NULL;
         lpx_load_mat(lpx, &info, lpt_mat);
      }
      /* deallocate the problem block */
      lpt_free_prob(lpt);
      /* return to the calling program */
      return lpx;
}

/*----------------------------------------------------------------------
-- lpx_write_lpt - write problem data using CPLEX LP format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_write_lpt(LPX *lp, char *fname);
--
-- *Description*
--
-- The routine lpx_write_lpt writes data from a problem object, which
-- the parameter lp points to, to an output text file, whose name is the
-- character string fname, using the CPLEX LP format.
--
-- *Returns*
--
-- If the operation was successful, the routine returns zero. Otherwise
-- the routine prints an error message and returns non-zero. */

static int check_name(char *name)
{     char *set = "!\"#$%&()/,.;?@_`'{}|~"; /* may appear in names */
      int t;
      if (isdigit((unsigned char)name[0])) return 1;
      if (name[0] == '.') return 1;
      for (t = 0; name[t] != '\0'; t++)
      {  if (t == 16) return 1;
         if (!isalnum((unsigned char)name[t]) &&
            strchr(set, (unsigned char)name[t]) == NULL) return 1;
      }
      return 0; /* name is ok */
}

static char *row_name(LPX *lp, int i, char rname[16+1])
{     char *str = NULL;
      if (lpx_get_int_parm(lp, LPX_K_LPTORIG))
      {  if (i == 0)
            str = lpx_get_obj_name(lp);
         else
            str = lpx_get_row_name(lp, i);
         if (str != NULL && check_name(str)) str = NULL;
      }
      if (str == NULL)
      {  if (i == 0)
            strcpy(rname, "obj");
         else
            sprintf(rname, "r(%d)", i);
      }
      else
         strcpy(rname, str);
      return rname;
}

static char *col_name(LPX *lp, int j, char cname[16+1])
{     char *str = NULL;
      if (lpx_get_int_parm(lp, LPX_K_LPTORIG))
      {  str = lpx_get_col_name(lp, j);
         if (str != NULL && check_name(str)) str = NULL;
      }
      if (str == NULL)
         sprintf(cname, "x(%d)", j);
      else
         strcpy(cname, str);
      return cname;
}

int lpx_write_lpt(LPX *lp, char *fname)
{     FILE *fp;
      int nrows, ncols, i, j, t, len, typx, flag, kind, *ndx;
      double lb, ub, *obj, *val;
      char line[72+1], term[72+1], rname[16+1], cname[16+1];
      print("lpx_write_lpt: writing problem data to `%s'...", fname);
      /* open the output text file */
      fp = ufopen(fname, "w");
      if (fp == NULL)
      {  print("lpx_write_lpt: can't create `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      /* determine number of rows and number of columns */
      nrows = lpx_get_num_rows(lp);
      ncols = lpx_get_num_cols(lp);
      /* the problem should contain at least one row and one column */
      if (!(nrows > 0 && ncols > 0))
         fault("lpx_write_lpt: problem has no rows/columns");
      /* write problem name */
      {  char *name = lpx_get_prob_name(lp);
         if (name == NULL) name = "Unknown";
         fprintf(fp, "\\* Problem: %s *\\\n", name);
         fprintf(fp, "\n");
      }
      /* allocate working arrays */
      ndx = ucalloc(1+ncols, sizeof(int));
      val = ucalloc(1+ncols, sizeof(double));
      /* prepare coefficients of the objective function */
      obj = ucalloc(1+ncols, sizeof(double));
      for (j = 1; j <= ncols; j++)
         obj[j] = lpx_get_col_coef(lp, j);
      for (i = 1; i <= nrows; i++)
      {  double ci = lpx_get_row_coef(lp, i);
         if (ci != 0.0)
         {  len = lpx_get_mat_row(lp, i, ndx, val);
            for (t = 1; t <= len; t++)
               obj[ndx[t]] += ci * val[t];
         }
      }
      for (j = 1, len = 0; j <= ncols; j++)
      {  if (fabs(obj[j]) < 1e-12) continue;
         len++, ndx[len] = j, val[len] = obj[j];
      }
      ufree(obj);
      /* write the objective function definition and the constraints
         section */
      for (i = 0; i <= nrows; i++)
      {  if (i == 0)
         {  switch (lpx_get_obj_dir(lp))
            {  case LPX_MIN:
                  fprintf(fp, "Minimize\n");
                  break;
               case LPX_MAX:
                  fprintf(fp, "Maximize\n");
                  break;
               default:
                  insist(lp != lp);
            }
         }
         else if (i == 1)
         {  double temp = lpx_get_obj_c0(lp);
            if (temp != 0.0)
               fprintf(fp, "\\* constant term = %.12g *\\\n", temp);
            fprintf(fp, "\n");
            fprintf(fp, "Subject To\n");
         }
         row_name(lp, i, rname);
         if (i > 0)
         {  lpx_get_row_bnds(lp, i, &typx, &lb, &ub);
            if (typx == LPX_FR) continue;
            len = lpx_get_mat_row(lp, i, ndx, val);
         }
         flag = 0;
more:    if (!flag)
            sprintf(line, " %s:", rname);
         else
            sprintf(line, " %*s ", strlen(rname), "");
         for (t = 1; t <= len; t++)
         {  col_name(lp, ndx[t], cname);
            if (fabs(val[t] - 1.0) < 1e-12)
               sprintf(term, " + %s", cname);
            else if (fabs(val[t] + 1.0) < 1e-12)
               sprintf(term, " - %s", cname);
            else if (val[t] > 0.0)
               sprintf(term, " + %.12g %s", + val[t], cname);
            else if (val[t] < 0.0)
               sprintf(term, " - %.12g %s", - val[t], cname);
            else
               continue;
            if (strlen(line) + strlen(term) > 72)
               fprintf(fp, "%s\n", line), line[0] = '\0';
            strcat(line, term);
         }
         if (len == 0)
         {  /* empty row */
            sprintf(term, " 0 %s", col_name(lp, 1, cname));
            strcat(line, term);
         }
         if (i > 0)
         {  switch (typx)
            {  case LPX_LO:
               case LPX_DB:
                  sprintf(term, " >= %.12g", lb);
                  break;
               case LPX_UP:
                  sprintf(term, " <= %.12g", ub);
                  break;
               case LPX_FX:
                  sprintf(term, " = %.12g", lb);
                  break;
               default:
                  insist(typx != typx);
            }
            if (strlen(line) + strlen(term) > 72)
               fprintf(fp, "%s\n", line), line[0] = '\0';
            strcat(line, term);
         }
         fprintf(fp, "%s\n", line);
         if (i > 0 && typx == LPX_DB)
         {  /* double-bounded row needs a copy for its upper bound */
            flag = 1;
            typx = LPX_UP;
            goto more;
         }
      }
      /* free working arrays */
      ufree(ndx);
      ufree(val);
      /* write the bounds section */
      flag = 0;
      for (j = 1; j <= ncols; j++)
      {  col_name(lp, j, cname);
         lpx_get_col_bnds(lp, j, &typx, &lb, &ub);
         if (typx == LPX_LO && lb == 0.0) continue;
         if (!flag)
         {  fprintf(fp, "\n");
            fprintf(fp, "Bounds\n");
            flag = 1;
         }
         switch (typx)
         {  case LPX_FR:
		         fprintf(fp, " %s free\n", cname);
               break;
            case LPX_LO:
               fprintf(fp, " %s >= %.12g\n", cname, lb);
               break;
            case LPX_UP:
               fprintf(fp, " %s <= %.12g\n", cname, ub);
               break;
            case LPX_DB:
               fprintf(fp, " %.12g <= %s <= %.12g\n", lb, cname, ub);
               break;
            case LPX_FX:
               fprintf(fp, " %s = %.12g\n", cname, lb);
               break;
            default:
               insist(typx != typx);
         }
      }
      /* write the general section */
      if (lpx_get_class(lp) == LPX_MIP)
      {  flag = 0;
         for (j = 1; j <= ncols; j++)
         {  kind = lpx_get_col_kind(lp, j);
            if (kind == LPX_CV) continue;
            insist(kind == LPX_IV);
            if (!flag)
            {  fprintf(fp, "\n");
               fprintf(fp, "Generals\n");
               flag = 1;
            }
            fprintf(fp, " %s\n", col_name(lp, j, cname));
         }
      }
      /* write the end keyword */
      fprintf(fp, "\n");
      fprintf(fp, "End\n");
      /* close the output text file */
      fflush(fp);
      if (ferror(fp))
      {  print("lpx_write_lpt: can't write to `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      ufclose(fp);
      /* return to the calling program */
      return 0;
fail: /* the operation failed */
      if (fp != NULL) ufclose(fp);
      return 1;
}

/* eof */
