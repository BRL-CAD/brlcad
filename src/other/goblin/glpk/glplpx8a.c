/* glplpx8a.c (additional utility routines) */

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
#include <stdlib.h>
#include <string.h>
#include "glpavl.h"
#include "glplib.h"
#include "glplpx.h"
#include "glpmps.h"

/*----------------------------------------------------------------------
-- lpx_read_mps - read in problem data using MPS format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- LPX *lpx_read_mps(char *fname);
--
-- *Description*
--
-- The routine lpx_read_mps reads LP/MIP problem data using MPS format
-- from a text file whose name is the character string fname.
--
-- *Returns*
--
-- If no error occurred, the routine returns a pointer to the created
-- problem object. Otherwise the routine returns NULL. */

struct mps_info
{     /* transit information passed to the routine mps_mat */
      MPS *mps;
      /* MPS data block */
      int j;
      /* current column number */
      MPSCQE *cqe;
      /* pointer to the next element in the current column list */
};

static double mps_mat(void *_info, int *i, int *j)
{     /* "reads" a next element of the constraint matrix */
      struct mps_info *info = _info;
      double aij;
read: if (info->j == 0 || info->cqe == NULL)
      {  /* either it is the first call or the current column has been
            completely read; set the first/next column */
         info->j++;
         if (info->j > info->mps->n_col)
         {  /* the entire matrix has been completely read */
            *i = *j = 0; /* end of file */
            info->j = 0; /* rewind */
            return 0.0;
         }
         /* set the pointer to the first element of the column */
         info->cqe = info->mps->col[info->j]->ptr;
         /* the column may be empty, so repeat the check */
         goto read;
      }
      /* obtain the next element from the current column */
      *i = info->cqe->ind;
      *j = info->j;
      aij = info->cqe->val;
      /* advance the pointer */
      info->cqe = info->cqe->next;
      /* skip the element if it has zero value */
      if (aij == 0.0) goto read;
      /* bring the next element to the calling program */
      return aij;
}

LPX *lpx_read_mps(char *fname)
{     LPX *lp = NULL;
      MPS *mps;
      int m, n, obj, i, j;
      /* read MPS data block */
      mps = load_mps(fname);
      if (mps == NULL) goto fail;
      m = mps->n_row;
      n = mps->n_col;
      obj = 0;
      /* create problem object */
      lp = lpx_create_prob();
      lpx_set_prob_name(lp, mps->name);
      lpx_add_rows(lp, m);
      lpx_add_cols(lp, n);
      /* process ROW section */
      for (i = 1; i <= m; i++)
      {  MPSROW *row;
         int typx;
         row = mps->row[i];
         lpx_set_row_name(lp, i, row->name);
         if (strcmp(row->type, "N") == 0)
         {  typx = LPX_FR;
            if (obj == 0)
            {  obj = i;
               lpx_set_obj_name(lp, row->name);
            }
         }
         else if (strcmp(row->type, "L") == 0)
            typx = LPX_UP;
         else if (strcmp(row->type, "G") == 0)
            typx = LPX_LO;
         else if (strcmp(row->type, "E") == 0)
            typx = LPX_FX;
         else
         {  print("lpx_read_mps: row `%s' has unknown type `%s'",
               row->name, row->type);
            goto fail;
         }
         lpx_set_row_bnds(lp, i, typx, 0.0, 0.0);
      }
      if (obj == 0)
      {  print("lpx_read_mps: objective function row not found");
         goto fail;
      }
      /* process COLUMN section */
      for (j = 1; j <= n; j++)
      {  MPSCOL *col;
         MPSCQE *cqe;
         col = mps->col[j];
         lpx_set_col_name(lp, j, col->name);
         lpx_set_col_bnds(lp, j, LPX_LO, 0.0, 0.0);
         if (col->flag)
         {  lpx_set_class(lp, LPX_MIP);
            lpx_set_col_kind(lp, j, LPX_IV);
         }
         for (cqe = col->ptr; cqe != NULL; cqe = cqe->next)
            if (cqe->ind == obj) lpx_set_col_coef(lp, j, cqe->val);
      }
      /* load the constraint matrix */
      {  struct mps_info info;
         info.mps = mps;
         info.j = 0;
         info.cqe = NULL;
         lpx_load_mat(lp, &info, mps_mat);
      }
      /* process RHS section */
      if (mps->n_rhs > 0)
      {  MPSCQE *cqe;
         for (cqe = mps->rhs[1]->ptr; cqe != NULL; cqe = cqe->next)
         {  int typx;
            double lb, ub;
            lpx_get_row_bnds(lp, cqe->ind, &typx, NULL, NULL);
            switch (typx)
            {  case LPX_FR:
                  /* if it is the objective function row, the specified
                     right-hand side is considered as a constant term of
                     the objective function; for other free rows the rhs
                     is ignored */
                  if (cqe->ind == obj) lpx_set_obj_c0(lp, cqe->val);
                  lb = ub = 0.0;
                  break;
               case LPX_LO:
                  lb = cqe->val, ub = 0.0;
                  break;
               case LPX_UP:
                  lb = 0.0, ub = cqe->val;
                  break;
               case LPX_FX:
                  lb = ub = cqe->val;
                  break;
               default:
                  insist(typx != typx);
            }
            lpx_set_row_bnds(lp, cqe->ind, typx, lb, ub);
         }
      }
      /* process RANGES section */
      if (mps->n_rng > 0)
      {  MPSCQE *cqe;
         for (cqe = mps->rng[1]->ptr; cqe != NULL; cqe = cqe->next)
         {  int typx;
            double lb, ub;
            lpx_get_row_bnds(lp, cqe->ind, &typx, &lb, &ub);
            switch (typx)
            {  case LPX_FR:
                  print("lpx_read_mps: range vector entry refers to row"
                     " `%s' of N type", mps->row[cqe->ind]->name);
                  goto fail;
               case LPX_LO:
                  ub = lb + fabs(cqe->val);
                  break;
               case LPX_UP:
                  lb = ub - fabs(cqe->val);
                  break;
               case LPX_FX:
                  if (cqe->val >= 0.0)
                     ub += fabs(cqe->val);
                  else
                     lb -= fabs(cqe->val);
                  break;
               default:
                  insist(typx != typx);
            }
            lpx_set_row_bnds(lp, cqe->ind, lb == ub ? LPX_FX : LPX_DB,
               lb, ub);
         }
      }
      /* process BOUNDS section */
      if (mps->n_bnd > 0)
      {  MPSBQE *bqe;
         for (bqe = mps->bnd[1]->ptr; bqe != NULL; bqe = bqe->next)
         {  int typx;
            double lb, ub;
            lpx_get_col_bnds(lp, bqe->ind, &typx, &lb, &ub);
            if (typx == LPX_FR || typx == LPX_UP) lb = -DBL_MAX;
            if (typx == LPX_FR || typx == LPX_LO) ub = +DBL_MAX;
            if (strcmp(bqe->type, "LO") == 0)
               lb = bqe->val;
            else if (strcmp(bqe->type, "UP") == 0)
               ub = bqe->val;
            else if (strcmp(bqe->type, "FX") == 0)
               lb = ub = bqe->val;
            else if (strcmp(bqe->type, "FR") == 0)
               lb = -DBL_MAX, ub = +DBL_MAX;
            else if (strcmp(bqe->type, "MI") == 0)
               lb = -DBL_MAX;
            else if (strcmp(bqe->type, "PL") == 0)
               ub = +DBL_MAX;
            else if (strcmp(bqe->type, "UI") == 0)
            {  /* integer structural variable with upper bound */
               lpx_set_class(lp, LPX_MIP);
               lpx_set_col_kind(lp, bqe->ind, LPX_IV);
               ub = bqe->val;
            }
            else if (strcmp(bqe->type, "BV") == 0)
            {  /* binary structural variable */
               lpx_set_class(lp, LPX_MIP);
               lpx_set_col_kind(lp, bqe->ind, LPX_IV);
               lb = 0.0, ub = 1.0;
            }
            else
            {  print("lpx_read_mps: bound vector entry for column `%s' "
                  "has unknown type `%s'",
               mps->col[bqe->ind]->name, bqe->type);
               goto fail;
            }
            if (lb == -DBL_MAX && ub == +DBL_MAX)
               typx = LPX_FR;
            else if (ub == +DBL_MAX)
               typx = LPX_LO;
            else if (lb == -DBL_MAX)
               typx = LPX_UP;
            else if (lb != ub)
               typx = LPX_DB;
            else
               typx = LPX_FX;
            lpx_set_col_bnds(lp, bqe->ind, typx, lb, ub);
         }
      }
      /* deallocate MPS data block */
      free_mps(mps);
      /* return to the calling program */
      return lp;
fail: /* the operation failed */
      if (lp != NULL) lpx_delete_prob(lp);
      if (mps != NULL) free_mps(mps);
      return NULL;
}

/*----------------------------------------------------------------------
-- lpx_write_mps - write problem data using MPS format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_write_mps(LPX *lp, char *fname);
--
-- *Description*
--
-- The routine lpx_write_mps writes data from a problem object, which
-- the parameter lp points to, to an output text file, whose name is the
-- character string fname, using the MPS format.
--
-- *Returns*
--
-- If the operation was successful, the routine returns zero. Otherwise
-- the routine prints an error message and returns non-zero. */

static char *row_name(LPX *lp, int i, char rname[8+1])
{     char *str = NULL;
      if (lpx_get_int_parm(lp, LPX_K_MPSORIG))
      {  if (i == 0)
            str = lpx_get_obj_name(lp);
         else
            str = lpx_get_row_name(lp, i);
         if (str != NULL && strlen(str) > 8) str = NULL;
      }
      if (str == NULL)
         sprintf(rname, "R%07d", i);
      else
         strcpy(rname, str);
      return rname;
}

static char *col_name(LPX *lp, int j, char cname[8+1])
{     char *str = NULL;
      if (lpx_get_int_parm(lp, LPX_K_MPSORIG))
      {  str = lpx_get_col_name(lp, j);
         if (str != NULL && strlen(str) > 8) str = NULL;
      }
      if (str == NULL)
         sprintf(cname, "C%07d", j);
      else
         strcpy(cname, str);
      return cname;
}

static char *mps_numb(double val, char numb[12+1])
{     int n;
      char str[255+1], *e;
      for (n = 12; n >= 6; n--)
      {  if (val != 0.0 && fabs(val) < 0.002)
#if 0
            sprintf(str, "%.*E", n, val);
#else
            /* n is number of desired decimal places, but in case of E
               format precision means number of digits that follow the
               decimal point */
            sprintf(str, "%.*E", n - 1, val);
#endif
         else
            sprintf(str, "%.*G", n, val);
         insist(strlen(str) <= 255);
         e = strchr(str, 'E');
         if (e != NULL) sprintf(e+1, "%d", atoi(e+1));
         if (strlen(str) <= 12) return strcpy(numb, str);
      }
      fault("lpx_write_mps: can't convert floating point number '%g' to"
         " character string", val);
      return NULL; /* make the compiler happy */
}

int lpx_write_mps(LPX *lp, char *fname)
{     FILE *fp;
      int wide = lpx_get_int_parm(lp, LPX_K_MPSWIDE);
      int frei = lpx_get_int_parm(lp, LPX_K_MPSFREE);
      int skip = lpx_get_int_parm(lp, LPX_K_MPSSKIP);
      int marker = 0; /* intorg/intend marker count */
      int mip, make_obj, nrows, ncols, i, j, flag, *ndx;
      double *obj, *val;
      char rname[8+1], cname[8+1], vname[8+1], numb[12+1];
      print("lpx_write_mps: writing problem data to `%s'...", fname);
      /* open the output text file */
      fp = fopen(fname, "w");
      if (fp == NULL)
      {  print("lpx_write_mps: can't create `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      /* determine whether the problem is LP or MIP */
      mip = (lpx_get_class(lp) == LPX_MIP);
      /* determine number of rows and number of columns */
      nrows = lpx_get_num_rows(lp);
      ncols = lpx_get_num_cols(lp);
      /* the problem should contain at least one row and one column */
      if (!(nrows > 0 && ncols > 0))
         fault("lpx_write_mps: problem has no rows/columns");
      /* determine if the routine should output the objective row */
      make_obj = lpx_get_int_parm(lp, LPX_K_MPSOBJ);
      if (make_obj == 2)
      {  for (i = 1; i <= nrows; i++)
         {  int typx;
            lpx_get_row_bnds(lp, i, &typx, NULL, NULL);
            if (typx == LPX_FR)
            {  make_obj = 0;
               break;
            }
         }
      }
      /* write comment cards (if required) */
      if (lpx_get_int_parm(lp, LPX_K_MPSINFO))
      {  char *name = lpx_get_prob_name(lp);
         if (name == NULL) name = "UNKNOWN";
         fprintf(fp, "* Problem:    %.31s\n", name);
         fprintf(fp, "* Class:      %s\n", !mip ? "LP" : "MIP");
         fprintf(fp, "* Rows:       %d\n", nrows);
         if (!mip)
            fprintf(fp, "* Columns:    %d\n", ncols);
         else
            fprintf(fp, "* Columns:    %d (%d integer, %d binary)\n",
               ncols, lpx_get_num_int(lp), lpx_get_num_bin(lp));
         fprintf(fp, "* Non-zeros:  %d\n", lpx_get_num_nz(lp));
         fprintf(fp, "*\n");
      }
      /* write NAME indicator card */
      {  char *name = lpx_get_prob_name(lp);
         if (name == NULL)
            fprintf(fp, "NAME\n");
         else
            fprintf(fp, "NAME          %.8s\n", name);
      }
      /* write ROWS section */
      fprintf(fp, "ROWS\n");
      if (make_obj)
         fprintf(fp, " %c  %s\n", 'N', row_name(lp, 0, rname));
      for (i = 1; i <= nrows; i++)
      {  int typx;
         lpx_get_row_bnds(lp, i, &typx, NULL, NULL);
         switch (typx)
         {  case LPX_FR: typx = 'N'; break;
            case LPX_LO: typx = 'G'; break;
            case LPX_UP: typx = 'L'; break;
            case LPX_DB: typx = 'E'; break;
            case LPX_FX: typx = 'E'; break;
            default: insist(typx != typx);
         }
         fprintf(fp, " %c  %s\n", typx, row_name(lp, i, rname));
      }
      /* prepare coefficients of the objective function */
      obj = ucalloc(1+ncols, sizeof(double));
      obj[0] = lpx_get_obj_c0(lp);
      for (j = 1; j <= ncols; j++)
         obj[j] = lpx_get_col_coef(lp, j);
      ndx = ucalloc(1+ncols, sizeof(int));
      val = ucalloc(1+ncols, sizeof(double));
      for (i = 1; i <= nrows; i++)
      {  double ci = lpx_get_row_coef(lp, i);
         if (ci != 0.0)
         {  int len, t;
            len = lpx_get_mat_row(lp, i, ndx, val);
            for (t = 1; t <= len; t++)
               obj[ndx[t]] += ci * val[t];
         }
      }
      ufree(ndx);
      ufree(val);
      for (j = 0; j <= ncols; j++)
         if (fabs(obj[j]) < 1e-12) obj[j] = 0.0;
      /* write COLUMNS section */
      fprintf(fp, "COLUMNS\n");
      ndx = ucalloc(1+nrows, sizeof(int));
      val = ucalloc(1+nrows, sizeof(double));
      for (j = 1; j <= ncols; j++)
      {  int nl = 1, iv, len, t;
         col_name(lp, j, cname);
         iv = (mip && lpx_get_col_kind(lp, j) == LPX_IV);
         if (iv && marker % 2 == 0)
         {  /* open new intorg/intend group */
            marker++;
            fprintf(fp, "    M%07d  'MARKER'                 'INTORG'\n"
               , marker);
         }
         else if (!iv && marker % 2 == 1)
         {  /* close the current intorg/intend group */
            marker++;
            fprintf(fp, "    M%07d  'MARKER'                 'INTEND'\n"
               , marker);
         }
         /* obtain j-th column of the constraint matrix */
         len = lpx_get_mat_col(lp, j, ndx, val);
         ndx[0] = 0;
         val[0] = (make_obj ? obj[j] : 0.0);
         if (len == 0 && val[0] == 0.0 && !skip)
            fprintf(fp, "    %-8s  %-8s  %12s   $ empty column\n",
               cname, row_name(lp, 1, rname), mps_numb(0.0, numb));
         for (t = val[0] != 0.0 ? 0 : 1; t <= len; t++)
         {  if (nl)
               fprintf(fp, "    %-8s  ", cname);
            else
               fprintf(fp, "   ");
            fprintf(fp, "%-8s  %12s",
               row_name(lp, ndx[t], rname), mps_numb(val[t], numb));
            if (wide) nl = 1 - nl;
            if (nl) fprintf(fp, "\n");
            if (frei) strcpy(cname, "");
         }
         if (!nl) fprintf(fp, "\n");
      }
      if (marker % 2 == 1)
      {  /* close the last intorg/intend group (if not closed) */
         marker++;
         fprintf(fp, "    M%07d  'MARKER'                 'INTEND'\n",
            marker);
      }
      ufree(ndx);
      ufree(val);
      /* write RHS section */
      flag = 0;
      {  int nl = 1;
         strcpy(vname, frei ? "" : "RHS1");
         for (i = make_obj ? 0 : 1; i <= nrows; i++)
         {  int typx;
            double lb, ub, rhs;
            if (i == 0)
               typx = LPX_FR, lb = ub = 0.0;
            else
               lpx_get_row_bnds(lp, i, &typx, &lb, &ub);
            switch (typx)
            {  case LPX_FR:
                  rhs = (make_obj && i == 0 ? obj[0] : 0.0); break;
               case LPX_LO:
                  rhs = lb; break;
               case LPX_UP:
                  rhs = ub; break;
               case LPX_DB:
                  rhs = (ub > 0.0 ? lb : ub); break;
               case LPX_FX:
                  rhs = lb; break;
               default:
                  insist(typx != typx);
            }
            if (rhs == 0.0) continue;
            if (!flag) fprintf(fp, "RHS\n"), flag = 1;
            if (nl)
                fprintf(fp, "    %-8s  ", vname);
            else
                fprintf(fp, "   ");
            fprintf(fp, "%-8s  %12s",
               row_name(lp, i, rname), mps_numb(rhs, numb));
            if (wide) nl = 1 - nl;
            if (nl) fprintf(fp, "\n");
         }
         if (!nl) fprintf(fp, "\n");
      }
      ufree(obj);
      /* write RANGES section */
      flag = 0;
      {  int nl = 1;
         strcpy(vname, frei ? "" : "RNG1");
         for (i = 1; i <= nrows; i++)
         {  int typx;
            double lb, ub, rng;
            lpx_get_row_bnds(lp, i, &typx, &lb, &ub);
            if (typx != LPX_DB) continue;
            if (!flag) fprintf(fp, "RANGES\n"), flag = 1;
            if (nl)
                fprintf(fp, "    %-8s  ", vname);
            else
                fprintf(fp, "   ");
            rng = (ub > 0.0 ? ub - lb : lb - ub);
            fprintf(fp, "%-8s  %12s",
               row_name(lp, i, rname), mps_numb(rng, numb));
            if (wide) nl = 1 - nl;
            if (nl) fprintf(fp, "\n");
         }
         if (!nl) fprintf(fp, "\n");
      }
      /* write BOUNDS section */
      flag = 0;
      {  strcpy(vname, frei ? "" : "BND1");
         for (j = 1; j <= ncols; j++)
         {  int typx;
            double lb, ub;
            lpx_get_col_bnds(lp, j, &typx, &lb, &ub);
            if (typx == LPX_LO && lb == 0.0) continue;
            if (!flag) fprintf(fp, "BOUNDS\n"), flag = 1;
            switch (typx)
            {  case LPX_FR:
                  fprintf(fp, " FR %-8s  %-8s\n", vname,
                     col_name(lp, j, cname));
                  break;
               case LPX_LO:
                  fprintf(fp, " LO %-8s  %-8s  %12s\n", vname,
                     col_name(lp, j, cname), mps_numb(lb, numb));
                  break;
               case LPX_UP:
                  fprintf(fp, " MI %-8s  %-8s\n", vname,
                     col_name(lp, j, cname));
                  fprintf(fp, " UP %-8s  %-8s  %12s\n", vname,
                     col_name(lp, j, cname), mps_numb(ub, numb));
                  break;
               case LPX_DB:
                  if (lb != 0.0)
                  fprintf(fp, " LO %-8s  %-8s  %12s\n", vname,
                     col_name(lp, j, cname), mps_numb(lb, numb));
                  fprintf(fp, " UP %-8s  %-8s  %12s\n", vname,
                     col_name(lp, j, cname), mps_numb(ub, numb));
                  break;
               case LPX_FX:
                  fprintf(fp, " FX %-8s  %-8s  %12s\n", vname,
                     col_name(lp, j, cname), mps_numb(lb, numb));
                  break;
               default:
                  insist(typx != typx);
            }
         }
      }
      /* write ENDATA indicator card */
      fprintf(fp, "ENDATA\n");
      /* close the output text file */
      fflush(fp);
      if (ferror(fp))
      {  print("lpx_write_mps: can't write to `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      fclose(fp);
      /* return to the calling program */
      return 0;
fail: /* the operation failed */
      if (fp != NULL) fclose(fp);
      return 1;
}

struct dsa
{     /* dynamic storage area used by the routine lpx_read_bas */
      char *fname;
      /* name of input text file */
      FILE *fp;
      /* stream assigned to input text file */
      int count;
      /* card count */
      char card[80+1];
      /* card image buffer */
      char f1[2+1], f2[31+1], f3[31+1], f4[12+1], f5[31+1], f6[12+1];
      /* card fields */
};

/*----------------------------------------------------------------------
-- read_card - read the next card.
--
-- This routine reads the next 80-column card from the input text file
-- and places its image into the character string card. If the card was
-- read successfully, the routine returns zero, otherwise non-zero. */

static int read_card(struct dsa *dsa)
{     int k, c;
loop: dsa->count++;
      memset(dsa->card, ' ', 80), dsa->card[80] = '\0';
      k = 0;
      for (;;)
      {  c = fgetc(dsa->fp);
         if (ferror(dsa->fp))
         {  print("%s:%d: read error - %s", dsa->fname, dsa->count,
               strerror(errno));
            return 1;
         }
         if (feof(dsa->fp))
         {  if (k == 0)
               print("%s:%d: unexpected eof", dsa->fname, dsa->count);
            else
               print("%s:%d: missing final LF", dsa->fname, dsa->count);
            return 1;
         }
         if (c == '\r') continue;
         if (c == '\n') break;
         if (iscntrl(c))
         {  print("%s:%d: invalid control character 0x%02X", dsa->fname,
               dsa->count, c);
            return 1;
         }
         if (k == 80)
         {  print("%s:%d: card image too long", dsa->fname, dsa->count);
            return 1;
         }
         dsa->card[k++] = (char)c;
      }
      /* asterisk in the leftmost column means comment */
      if (dsa->card[0] == '*') goto loop;
      return 0;
}

/*----------------------------------------------------------------------
-- split_card - split data card to separate fields.
--
-- This routine splits the current data card to separate fileds f1, f2,
-- f3, f4, f5, and f6. If the data card has correct format, the routine
-- returns zero, otherwise non-zero. */

static int split_card(struct dsa *dsa)
{     /* col. 1: blank */
      if (memcmp(dsa->card+0, " ", 1))
fail: {  print("%s:%d: invalid data card", dsa->fname, dsa->count);
         return 1;
      }
      /* col. 2-3: field 1 (code) */
      memcpy(dsa->f1, dsa->card+1, 2);
      dsa->f1[2] = '\0'; strspx(dsa->f1);
      /* col. 4: blank */
      if (memcmp(dsa->card+3, " ", 1)) goto fail;
      /* col. 5-12: field 2 (name) */
      memcpy(dsa->f2, dsa->card+4, 8);
      dsa->f2[8] = '\0'; strspx(dsa->f2);
      /* col. 13-14: blanks */
      if (memcmp(dsa->card+12, "  ", 2)) goto fail;
      /* col. 15-22: field 3 (name) */
      memcpy(dsa->f3, dsa->card+14, 8);
      dsa->f3[8] = '\0'; strspx(dsa->f3);
      if (dsa->f3[0] == '$')
      {  /* from col. 15 to the end of the card is a comment */
         dsa->f3[0] = dsa->f4[0] = dsa->f5[0] = dsa->f6[0] = '\0';
         goto done;
      }
      /* col. 23-24: blanks */
      if (memcmp(dsa->card+22, "  ", 2)) goto fail;
      /* col. 25-36: field 4 (number) */
      memcpy(dsa->f4, dsa->card+24, 12);
      dsa->f4[12] = '\0'; strspx(dsa->f4);
      /* col. 37-39: blanks */
      if (memcmp(dsa->card+36, "   ", 3)) goto fail;
      /* col. 40-47: field 5 (name) */
      memcpy(dsa->f5, dsa->card+39,  8);
      dsa->f5[8]  = '\0'; strspx(dsa->f5);
      if (dsa->f5[0] == '$')
      {  /* from col. 40 to the end of the card is a comment */
         dsa->f5[0] = dsa->f6[0] = '\0';
         goto done;
      }
      /* col. 48-49: blanks */
      if (memcmp(dsa->card+47, "  ", 2)) goto fail;
      /* col. 50-61: field 6 (number) */
      memcpy(dsa->f6, dsa->card+49, 12);
      dsa->f6[12] = '\0'; strspx(dsa->f6);
      /* col. 62-71: blanks */
      if (memcmp(dsa->card+61, "          ", 10)) goto fail;
done: return 0;
}

/*----------------------------------------------------------------------
-- lpx_read_bas - read predefined basis using MPS format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_read_bas(LPX *lp, char *fname);
--
-- *Description*
--
-- The routine lpx_read_bas reads a predefined basis prepared in MPS
-- format for an LP problem object, which the parameter lp points to,
-- from a text file, whose name is the character string fname.
--
-- *Returns*
--
-- If the operation was successful, the routine returns zero. Otherwise
-- the routine prints an error message and returns non-zero. */

int lpx_read_bas(LPX *lp, char *fname)
{     DMP *pool;
      AVLTREE *t_row, *t_col;
      AVLNODE *node;
      int i, j, ret = 0;
      struct dsa _dsa, *dsa = &_dsa;
      print("lpx_read_bas: reading predefined basis from `%s'...",
         fname);
      /* open the input text file */
      dsa->fp = ufopen(fname, "r");
      if (dsa->fp == NULL)
      {  print("lpx_read_bas: unable to open `%s' - %s", fname,
            strerror(errno));
         return 1;
      }
      /* initialize dynamic storage area */
      dsa->fname = fname;
      dsa->count = 0;
      /* build serach trees for rows and columns */
      pool = dmp_create_pool(8+1);
      t_row = avl_create_tree(NULL, avl_strcmp);
      t_col = avl_create_tree(NULL, avl_strcmp);
      for (i = 1; i <= lp->m; i++)
         avl_insert_by_key(t_row,
            row_name(lp, i, dmp_get_atom(pool)))->type = i;
      for (j = 1; j <= lp->n; j++)
         avl_insert_by_key(t_col,
            col_name(lp, j, dmp_get_atom(pool)))->type = j;
      /* process NAME indicator card */
      if (read_card(dsa))
      {  ret = 1;
         goto fini;
      }
      if (memcmp(dsa->card, "NAME ", 5) != 0)
      {  print("%s:%d: NAME indicator card missing", fname, dsa->count);
         ret = 1;
         goto fini;
      }
      /* build the "standard" basis, where all rows are basic and all
         columns are non-basic */
      lpx_std_basis(lp);
      /* process data cards */
      for (;;)
      {  if (read_card(dsa))
         {  ret = 1;
            goto fini;
         }
         if (dsa->card[0] != ' ') break;
         if (split_card(dsa))
         {  ret = 1;
            goto fini;
         }
         /* check indicator in the field f1 */
         if (!(strcmp(dsa->f1, "XL") == 0 ||
               strcmp(dsa->f1, "XU") == 0 ||
               strcmp(dsa->f1, "LL") == 0 ||
               strcmp(dsa->f1, "UL") == 0))
         {  print("%s:%d: invalid indicator `%s' in filed 1", fname,
               dsa->count, dsa->f1);
            ret = 1;
            goto fini;
         }
         /* determine number of column specified in the filed 2 */
         node = avl_find_by_key(t_col, dsa->f2);
         if (node == NULL)
         {  print("%s:%d: column `%s' not found", fname, dsa->count,
               dsa->f2);
            ret = 1;
            goto fini;
         }
         j = node->type;
         /* determine number of row specified in the field 3 */
         if (dsa->f1[0] == 'X')
         {  node = avl_find_by_key(t_row, dsa->f3);
            if (node == NULL)
            {  print("%s:%d: row `%s' not found", fname, dsa->count,
                  dsa->f3);
               ret = 1;
               goto fini;
            }
            i = node->type;
         }
         else
         {  /* ignore the field 3 */
            i = 0;
         }
         /* apply a "patch" */
         if (dsa->f1[0] == 'X')
         {  lpx_set_col_stat(lp, j, LPX_BS);
            lpx_set_row_stat(lp, i,
               dsa->f1[1] == 'L' ? LPX_NL : LPX_NU);
         }
         else
            lpx_set_col_stat(lp, j,
               dsa->f1[0] == 'L' ? LPX_NL : LPX_NU);
      }
      /* process ENDATA indicator card */
      if (memcmp(dsa->card, "ENDATA ", 7) != 0)
      {  print("%s:%d: invalid indicator card", fname, dsa->count);
         goto fini;
      }
      print("lpx_read_bas: %d cards were read", dsa->count);
fini: /* delete search trees */
      avl_delete_tree(t_row);
      avl_delete_tree(t_col);
      dmp_delete_pool(pool);
      /* close the input text file */
      ufclose(dsa->fp);
      /* return to the calling program */
      return ret;
}

/*----------------------------------------------------------------------
-- lpx_write_bas - write current basis using MPS format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_write_bas(LPX *lp, char *fname);
--
-- *Description*
--
-- The routine lpx_write_bas writes the current basis information from
-- a problem object, which the parameter lp points to, to a text file,
-- whose name is the character string fname, using MPS format.
--
-- *Returns*
--
-- If the operation was successful, the routine returns zero. Otherwise
-- the routine prints an error message and returns non-zero. */

int lpx_write_bas(LPX *lp, char *fname)
{     FILE *fp;
      int nrows, ncols, i, j, rtype, ctype, rstat, cstat;
      char rname[8+1], cname[8+1];
      print("lpx_write_bas: writing current basis to `%s'...", fname);
      /* open the output text file */
      fp = fopen(fname, "w");
      if (fp == NULL)
      {  print("lpx_write_bas: can't create `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      /* determine number of rows and number of columns */
      nrows = lpx_get_num_rows(lp);
      ncols = lpx_get_num_cols(lp);
      /* the problem should contain at least one row and one column */
      if (!(nrows > 0 && ncols > 0))
         fault("lpx_write_bas: problem has no rows/columns");
      /* the current basis should be defined */
      if (lp->b_stat != LPX_B_VALID)
         fault("lpx_write_bas: current basis is undefined");
      /* write comment cards (if required) */
      if (lpx_get_int_parm(lp, LPX_K_MPSINFO))
      {  int dir, status, round;
         double obj;
         char *name;
         /* problem name and statistics */
         name = lpx_get_prob_name(lp);
         if (name == NULL) name = "UNKNOWN";
         fprintf(fp, "* Problem:    %.31s\n", name);
         fprintf(fp, "* Rows:       %d\n", nrows);
         fprintf(fp, "* Columns:    %d\n", ncols);
         fprintf(fp, "* Non-zeros:  %d\n", lpx_get_num_nz(lp));
         /* solution status */
         status = lpx_get_status(lp);
         fprintf(fp, "* Status:     %s\n",
            status == LPX_OPT    ? "OPTIMAL" :
            status == LPX_FEAS   ? "FEASIBLE" :
            status == LPX_INFEAS ? "INFEASIBLE (INTERMEDIATE)" :
            status == LPX_NOFEAS ? "INFEASIBLE (FINAL)" :
            status == LPX_UNBND  ? "UNBOUNDED" :
            status == LPX_UNDEF  ? "UNDEFINED" : "???");
         /* objective function */
         name = lpx_get_obj_name(lp);
         dir = lpx_get_obj_dir(lp);
         round = lp->round, lp->round = 1;
         obj = lpx_get_obj_val(lp);
         lp->round = round;
         fprintf(fp, "* Objective:  %s%s%.10g %s\n",
            name == NULL ? "" : name,
            name == NULL ? "" : " = ", obj,
            dir == LPX_MIN ? "(MINimum)" :
            dir == LPX_MAX ? "(MAXimum)" : "(???)");
         fprintf(fp, "*\n");
      }
      /* write NAME indicator card */
      {  char *name = lpx_get_prob_name(lp);
         if (name == NULL)
            fprintf(fp, "NAME\n");
         else
            fprintf(fp, "NAME          %.8s\n", name);
      }
      /* write information about which columns should be made basic
         and which rows should be made non-basic */
      i = j = 0;
      for (;;)
      {  /* find a next non-basic row */
         for (i++; i <= nrows; i++)
         {  lpx_get_row_info(lp, i, &rstat, NULL, NULL);
            if (rstat != LPX_BS) break;
         }
         /* find a next basic column */
         for (j++; j <= ncols; j++)
         {  lpx_get_col_info(lp, j, &cstat, NULL, NULL);
            if (cstat == LPX_BS) break;
         }
         /* if all non-basic rows and basic columns have been written,
            break the loop */
         if (i > nrows && j > ncols) break;
         /* since the basis is valid, there must be nor extra non-basic
            rows nor extra basic columns */
         insist(i <= nrows && j <= ncols);
         /* write the pair (basic column, non-basic row) */
         lpx_get_row_bnds(lp, i, &rtype, NULL, NULL);
         fprintf(fp, " %s %-8s  %s\n",
            (rtype == LPX_DB && rstat == LPX_NU) ? "XU" : "XL",
            col_name(lp, j, cname), row_name(lp, i, rname));
      }
      /* write information about statuses of double-bounded non-basic
         columns */
      for (j = 1; j <= ncols; j++)
      {  lpx_get_col_bnds(lp, j, &ctype, NULL, NULL);
         lpx_get_col_info(lp, j, &cstat, NULL, NULL);
         if (ctype == LPX_DB && cstat != LPX_BS)
            fprintf(fp, " %s %s\n",
               cstat == LPX_NU ? "UL" : "LL", col_name(lp, j, cname));
      }
      /* write ENDATA indcator card */
      fprintf(fp, "ENDATA\n");
      /* close the output text file */
      fflush(fp);
      if (ferror(fp))
      {  print("lpx_write_bas: can't write to `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      fclose(fp);
      /* return to the calling program */
      return 0;
fail: /* the operation failed */
      if (fp != NULL) fclose(fp);
      return 1;
}

/*----------------------------------------------------------------------
-- lpx_print_prob - write problem data in plain text format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_print_prob(LPX *lp, char *fname);
--
-- *Description*
--
-- The routine lpx_print_prob writes data from a problem object, which
-- the parameter lp points to, to an output text file, whose name is the
-- character string fname, in plain text format.
--
-- *Returns*
--
-- If the operation is successful, the routine returns zero. Otherwise
-- the routine prints an error message and returns non-zero. */

#define row_name row_name1
#define col_name col_name1

static char *row_name(LPX *lp, int i, char name[255+1])
{     char *str;
      str = lpx_get_row_name(lp, i);
      if (str == NULL)
         sprintf(name, "R.%d", i);
      else
         strcpy(name, str);
      return name;
}

static char *col_name(LPX *lp, int j, char name[255+1])
{     char *str;
      str = lpx_get_col_name(lp, j);
      if (str == NULL)
         sprintf(name, "C.%d", j);
      else
         strcpy(name, str);
      return name;
}

int lpx_print_prob(LPX *lp, char *fname)
{     FILE *fp;
      int m, n, mip, i, j, len, t, type, *ndx;
      double coef, lb, ub, *val;
      char *str, name[255+1];
      print("lpx_write_prob: writing problem data to `%s'...", fname);
      fp = ufopen(fname, "w");
      if (fp == NULL)
      {  print("lpx_write_prob: unable to create `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      m = lpx_get_num_rows(lp);
      n = lpx_get_num_cols(lp);
      mip = (lpx_get_class(lp) == LPX_MIP);
      str = lpx_get_prob_name(lp);
      fprintf(fp, "Problem:    %s\n", str == NULL ? "(unnamed)" : str);
      fprintf(fp, "Class:      %s\n", !mip ? "LP" : "MIP");
      fprintf(fp, "Rows:       %d\n", m);
      if (!mip)
         fprintf(fp, "Columns:    %d\n", n);
      else
         fprintf(fp, "Columns:    %d (%d integer, %d binary)\n",
            n, lpx_get_num_int(lp), lpx_get_num_bin(lp));
      fprintf(fp, "Non-zeros:  %d\n", lpx_get_num_nz(lp));
      fprintf(fp, "\n");
      fprintf(fp, "*** OBJECTIVE FUNCTION ***\n");
      fprintf(fp, "\n");
      switch (lpx_get_obj_dir(lp))
      {  case LPX_MIN:
            fprintf(fp, "Minimize:");
            break;
         case LPX_MAX:
            fprintf(fp, "Maximize:");
            break;
         default:
            insist(lp != lp);
      }
      str = lpx_get_obj_name(lp);
      fprintf(fp, " %s\n", str == NULL ? "(unnamed)" : str);
      coef = lpx_get_obj_c0(lp);
      if (coef != 0.0)
         fprintf(fp, "%*.*g %s\n", DBL_DIG+7, DBL_DIG, coef,
            "(constant term)");
      for (i = 1; i <= m; i++)
      {  coef = lpx_get_row_coef(lp, i);
         if (coef != 0.0)
            fprintf(fp, "%*.*g %s\n", DBL_DIG+7, DBL_DIG, coef,
               row_name(lp, i, name));
      }
      for (j = 1; j <= n; j++)
      {  coef = lpx_get_col_coef(lp, j);
         if (coef != 0.0)
            fprintf(fp, "%*.*g %s\n", DBL_DIG+7, DBL_DIG, coef,
               col_name(lp, j, name));
      }
      fprintf(fp, "\n");
      fprintf(fp, "*** ROWS (CONSTRAINTS) ***\n");
      ndx = ucalloc(1+n, sizeof(int));
      val = ucalloc(1+n, sizeof(double));
      for (i = 1; i <= m; i++)
      {  fprintf(fp, "\n");
         fprintf(fp, "Row %d: %s", i, row_name(lp, i, name));
         lpx_get_row_bnds(lp, i, &type, &lb, &ub);
         switch (type)
         {  case LPX_FR:
               fprintf(fp, " free");
               break;
            case LPX_LO:
               fprintf(fp, " >= %.*g", DBL_DIG, lb);
               break;
            case LPX_UP:
               fprintf(fp, " <= %.*g", DBL_DIG, ub);
               break;
            case LPX_DB:
               fprintf(fp, " >= %.*g <= %.*g", DBL_DIG, lb, DBL_DIG,
                  ub);
               break;
            case LPX_FX:
               fprintf(fp, " = %.*g", DBL_DIG, lb);
               break;
            default:
               insist(type != type);
         }
         fprintf(fp, "\n");
         coef = lpx_get_row_coef(lp, i);
         if (coef != 0.0)
            fprintf(fp, "%*.*g %s\n", DBL_DIG+7, DBL_DIG, coef,
               "(objective)");
         len = lpx_get_mat_row(lp, i, ndx, val);
         for (t = 1; t <= len; t++)
            fprintf(fp, "%*.*g %s\n", DBL_DIG+7, DBL_DIG, val[t],
               col_name(lp, ndx[t], name));
      }
      ufree(ndx);
      ufree(val);
      fprintf(fp, "\n");
      fprintf(fp, "*** COLUMNS (VARIABLES) ***\n");
      ndx = ucalloc(1+m, sizeof(int));
      val = ucalloc(1+m, sizeof(double));
      for (j = 1; j <= n; j++)
      {  fprintf(fp, "\n");
         fprintf(fp, "Col %d: %s", j, col_name(lp, j, name));
         if (mip)
         {  switch (lpx_get_col_kind(lp, j))
            {  case LPX_CV:
                  break;
               case LPX_IV:
                  fprintf(fp, " integer");
                  break;
               default:
                  insist(lp != lp);
            }
         }
         lpx_get_col_bnds(lp, j, &type, &lb, &ub);
         switch (type)
         {  case LPX_FR:
               fprintf(fp, " free");
               break;
            case LPX_LO:
               fprintf(fp, " >= %.*g", DBL_DIG, lb);
               break;
            case LPX_UP:
               fprintf(fp, " <= %.*g", DBL_DIG, ub);
               break;
            case LPX_DB:
               fprintf(fp, " >= %.*g <= %.*g", DBL_DIG, lb, DBL_DIG,
                  ub);
               break;
            case LPX_FX:
               fprintf(fp, " = %.*g", DBL_DIG, lb);
               break;
            default:
               insist(type != type);
         }
         fprintf(fp, "\n");
         coef = lpx_get_col_coef(lp, j);
         if (coef != 0.0)
            fprintf(fp, "%*.*g %s\n", DBL_DIG+7, DBL_DIG, coef,
               "(objective)");
         len = lpx_get_mat_col(lp, j, ndx, val);
         for (t = 1; t <= len; t++)
            fprintf(fp, "%*.*g %s\n", DBL_DIG+7, DBL_DIG, val[t],
               row_name(lp, ndx[t], name));
      }
      ufree(ndx);
      ufree(val);
      fprintf(fp, "\n");
      fprintf(fp, "End of output\n");
      fflush(fp);
      if (ferror(fp))
      {  print("lpx_write_prob: write error on `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      fclose(fp);
      return 0;
fail: if (fp != NULL) ufclose(fp);
      return 1;
}

#undef row_name
#undef col_name

/*----------------------------------------------------------------------
-- lpx_print_sol - write LP problem solution in printable format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_print_sol(LPX *lp, char *fname);
--
-- *Description*
--
-- The routine lpx_print_sol writes the current basic solution of an LP
-- problem, which is specified by the pointer lp, to a text file, whose
-- name is the character string fname, in printable format.
--
-- Information reported by the routine lpx_print_sol is intended mainly
-- for visual analysis.
--
-- *Returns*
--
-- If the operation was successful, the routine returns zero. Otherwise
-- the routine prints an error message and returns non-zero. */

int lpx_print_sol(LPX *lp, char *fname)
{     FILE *fp;
      int what, round;
      print("lpx_print_sol: writing LP problem solution to `%s'...",
         fname);
      fp = fopen(fname, "w");
      if (fp == NULL)
      {  print("lpx_print_sol: can't create `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      /* problem name */
      {  char *name;
         name = lpx_get_prob_name(lp);
         if (name == NULL) name = "";
         fprintf(fp, "%-12s%s\n", "Problem:", name);
      }
      /* number of rows (auxiliary variables) */
      {  int nr;
         nr = lpx_get_num_rows(lp);
         fprintf(fp, "%-12s%d\n", "Rows:", nr);
      }
      /* number of columns (structural variables) */
      {  int nc;
         nc = lpx_get_num_cols(lp);
         fprintf(fp, "%-12s%d\n", "Columns:", nc);
      }
      /* number of non-zeros (constraint coefficients) */
      {  int nz;
         nz = lpx_get_num_nz(lp);
         fprintf(fp, "%-12s%d\n", "Non-zeros:", nz);
      }
      /* solution status */
      {  int status;
         status = lpx_get_status(lp);
         fprintf(fp, "%-12s%s\n", "Status:",
            status == LPX_OPT    ? "OPTIMAL" :
            status == LPX_FEAS   ? "FEASIBLE" :
            status == LPX_INFEAS ? "INFEASIBLE (INTERMEDIATE)" :
            status == LPX_NOFEAS ? "INFEASIBLE (FINAL)" :
            status == LPX_UNBND  ? "UNBOUNDED" :
            status == LPX_UNDEF  ? "UNDEFINED" : "???");
      }
      /* objective function */
      {  char *name;
         int dir;
         double obj;
         name = lpx_get_obj_name(lp);
         dir = lpx_get_obj_dir(lp);
         round = lp->round, lp->round = 1;
         obj = lpx_get_obj_val(lp);
         lp->round = round;
         fprintf(fp, "%-12s%s%s%.10g %s\n", "Objective:",
            name == NULL ? "" : name,
            name == NULL ? "" : " = ", obj,
            dir == LPX_MIN ? "(MINimum)" :
            dir == LPX_MAX ? "(MAXimum)" : "(???)");
      }
      /* main sheet */
      for (what = 1; what <= 2; what++)
      {  int mn, ij;
         fprintf(fp, "\n");
         fprintf(fp, "   No. %-12s St   Activity     Lower bound   Uppe"
            "r bound    Marginal\n",
            what == 1 ? "  Row name" : "Column name");
         fprintf(fp, "------ ------------ -- ------------- ------------"
            "- ------------- -------------\n");
         mn = (what == 1 ? lpx_get_num_rows(lp) : lpx_get_num_cols(lp));
         for (ij = 1; ij <= mn; ij++)
         {  char *name;
            int typx, tagx;
            double lb, ub, vx, dx;
            if (what == 1)
            {  name = lpx_get_row_name(lp, ij);
               if (name == NULL) name = "";
               lpx_get_row_bnds(lp, ij, &typx, &lb, &ub);
               round = lp->round, lp->round = 1;
               lpx_get_row_info(lp, ij, &tagx, &vx, &dx);
               lp->round = round;
            }
            else
            {  name = lpx_get_col_name(lp, ij);
               if (name == NULL) name = "";
               lpx_get_col_bnds(lp, ij, &typx, &lb, &ub);
               round = lp->round, lp->round = 1;
               lpx_get_col_info(lp, ij, &tagx, &vx, &dx);
               lp->round = round;
            }
            /* row/column ordinal number */
            fprintf(fp, "%6d ", ij);
            /* row column/name */
            if (strlen(name) <= 12)
               fprintf(fp, "%-12s ", name);
            else
               fprintf(fp, "%s\n%20s", name, "");
            /* row/column status */
            fprintf(fp, "%s ",
               tagx == LPX_BS ? "B " :
               tagx == LPX_NL ? "NL" :
               tagx == LPX_NU ? "NU" :
               tagx == LPX_NF ? "NF" :
               tagx == LPX_NS ? "NS" : "??");
            /* row/column primal activity */
            fprintf(fp, "%13.6g ", vx);
            /* row/column lower bound */
            if (typx == LPX_LO || typx == LPX_DB || typx == LPX_FX)
               fprintf(fp, "%13.6g ", lb);
            else
               fprintf(fp, "%13s ", "");
            /* row/column upper bound */
            if (typx == LPX_UP || typx == LPX_DB)
               fprintf(fp, "%13.6g ", ub);
            else if (typx == LPX_FX)
               fprintf(fp, "%13s ", "=");
            else
               fprintf(fp, "%13s ", "");
            /* row/column dual activity */
            if (tagx != LPX_BS)
            {  if (dx == 0.0)
                  fprintf(fp, "%13s", "< eps");
               else
                  fprintf(fp, "%13.6g", dx);
            }
            /* end of line */
            fprintf(fp, "\n");
         }
      }
      fprintf(fp, "\n");
#if 1
      if (lpx_get_prim_stat(lp) != LPX_P_UNDEF &&
          lpx_get_dual_stat(lp) != LPX_D_UNDEF)
      {  int m = lpx_get_num_rows(lp);
         LPXKKT kkt;
         fprintf(fp, "Karush-Kuhn-Tucker optimality conditions:\n\n");
         lpx_check_kkt(lp, 1, &kkt);
         fprintf(fp, "KKT.PE: max.abs.err. = %.2e on row %d\n",
            kkt.pe_ae_max, kkt.pe_ae_row);
         fprintf(fp, "        max.rel.err. = %.2e on row %d\n",
            kkt.pe_re_max, kkt.pe_re_row);
         switch (kkt.pe_quality)
         {  case 'H':
               fprintf(fp, "        High quality\n");
               break;
            case 'M':
               fprintf(fp, "        Medium quality\n");
               break;
            case 'L':
               fprintf(fp, "        Low quality\n");
               break;
            default:
               fprintf(fp, "        PRIMAL SOLUTION IS WRONG\n");
               break;
         }
         fprintf(fp, "\n");
         fprintf(fp, "KKT.PB: max.abs.err. = %.2e on %s %d\n",
            kkt.pb_ae_max, kkt.pb_ae_ind <= m ? "row" : "column",
            kkt.pb_ae_ind <= m ? kkt.pb_ae_ind : kkt.pb_ae_ind - m);
         fprintf(fp, "        max.rel.err. = %.2e on %s %d\n",
            kkt.pb_re_max, kkt.pb_re_ind <= m ? "row" : "column",
            kkt.pb_re_ind <= m ? kkt.pb_re_ind : kkt.pb_re_ind - m);
         switch (kkt.pb_quality)
         {  case 'H':
               fprintf(fp, "        High quality\n");
               break;
            case 'M':
               fprintf(fp, "        Medium quality\n");
               break;
            case 'L':
               fprintf(fp, "        Low quality\n");
               break;
            default:
               fprintf(fp, "        PRIMAL SOLUTION IS INFEASIBLE\n");
               break;
         }
         fprintf(fp, "\n");
         fprintf(fp, "KKT.DE: max.abs.err. = %.2e on column %d\n",
            kkt.de_ae_max, kkt.de_ae_col);
         fprintf(fp, "        max.rel.err. = %.2e on column %d\n",
            kkt.de_re_max, kkt.de_re_col);
         switch (kkt.de_quality)
         {  case 'H':
               fprintf(fp, "        High quality\n");
               break;
            case 'M':
               fprintf(fp, "        Medium quality\n");
               break;
            case 'L':
               fprintf(fp, "        Low quality\n");
               break;
            default:
               fprintf(fp, "        DUAL SOLUTION IS WRONG\n");
               break;
         }
         fprintf(fp, "\n");
         fprintf(fp, "KKT.DB: max.abs.err. = %.2e on %s %d\n",
            kkt.db_ae_max, kkt.db_ae_ind <= m ? "row" : "column",
            kkt.db_ae_ind <= m ? kkt.db_ae_ind : kkt.db_ae_ind - m);
         fprintf(fp, "        max.rel.err. = %.2e on %s %d\n",
            kkt.db_re_max, kkt.db_re_ind <= m ? "row" : "column",
            kkt.db_re_ind <= m ? kkt.db_re_ind : kkt.db_re_ind - m);
         switch (kkt.db_quality)
         {  case 'H':
               fprintf(fp, "        High quality\n");
               break;
            case 'M':
               fprintf(fp, "        Medium quality\n");
               break;
            case 'L':
               fprintf(fp, "        Low quality\n");
               break;
            default:
               fprintf(fp, "        DUAL SOLUTION IS INFEASIBLE\n");
               break;
         }
         fprintf(fp, "\n");
      }
#endif
      fprintf(fp, "End of output\n");
      fflush(fp);
      if (ferror(fp))
      {  print("lpx_print_sol: can't write to `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      fclose(fp);
      return 0;
fail: if (fp != NULL) fclose(fp);
      return 1;
}

/*----------------------------------------------------------------------
-- lpx_print_ips - write interior point solution in printable format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_print_ips(LPX *lp, char *fname);
--
-- *Description*
--
-- The routine lpx_print_ips writes the current interior point solution
-- of an LP problem, which the parameter lp points to, to a text file,
-- whose name is the character string fname, in printable format.
--
-- Information reported by the routine lpx_print_ips is intended mainly
-- for visual analysis.
--
-- *Returns*
--
-- If the operation was successful, the routine returns zero. Otherwise
-- the routine prints an error message and returns non-zero. */

int lpx_print_ips(LPX *lp, char *fname)
{     FILE *fp;
      int what, round;
      print("lpx_print_ips: writing LP problem solution to `%s'...",
         fname);
      fp = fopen(fname, "w");
      if (fp == NULL)
      {  print("lpx_print_ips: can't create `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      /* problem name */
      {  char *name;
         name = lpx_get_prob_name(lp);
         if (name == NULL) name = "";
         fprintf(fp, "%-12s%s\n", "Problem:", name);
      }
      /* number of rows (auxiliary variables) */
      {  int nr;
         nr = lpx_get_num_rows(lp);
         fprintf(fp, "%-12s%d\n", "Rows:", nr);
      }
      /* number of columns (structural variables) */
      {  int nc;
         nc = lpx_get_num_cols(lp);
         fprintf(fp, "%-12s%d\n", "Columns:", nc);
      }
      /* number of non-zeros (constraint coefficients) */
      {  int nz;
         nz = lpx_get_num_nz(lp);
         fprintf(fp, "%-12s%d\n", "Non-zeros:", nz);
      }
      /* solution status */
      {  int status;
         status = lpx_get_ips_stat(lp);
         fprintf(fp, "%-12s%s\n", "Status:",
            status == LPX_T_UNDEF  ? "INTERIOR UNDEFINED" :
            status == LPX_T_OPT    ? "INTERIOR OPTIMAL" : "???");
      }
      /* objective function */
      {  char *name;
         int dir;
         double obj;
         name = lpx_get_obj_name(lp);
         dir = lpx_get_obj_dir(lp);
         round = lp->round, lp->round = 1;
         obj = lpx_get_ips_obj(lp);
         lp->round = round;
         fprintf(fp, "%-12s%s%s%.10g %s\n", "Objective:",
            name == NULL ? "" : name,
            name == NULL ? "" : " = ", obj,
            dir == LPX_MIN ? "(MINimum)" :
            dir == LPX_MAX ? "(MAXimum)" : "(???)");
      }
      /* main sheet */
      for (what = 1; what <= 2; what++)
      {  int mn, ij;
         fprintf(fp, "\n");
         fprintf(fp, "   No. %-12s      Activity     Lower bound   Uppe"
            "r bound    Marginal\n",
            what == 1 ? "  Row name" : "Column name");
         fprintf(fp, "------ ------------    ------------- ------------"
            "- ------------- -------------\n");
         mn = (what == 1 ? lpx_get_num_rows(lp) : lpx_get_num_cols(lp));
         for (ij = 1; ij <= mn; ij++)
         {  char *name;
            int typx /*, tagx */;
            double lb, ub, vx, dx;
            if (what == 1)
            {  name = lpx_get_row_name(lp, ij);
               if (name == NULL) name = "";
               lpx_get_row_bnds(lp, ij, &typx, &lb, &ub);
               round = lp->round, lp->round = 1;
               lpx_get_ips_row(lp, ij, &vx, &dx);
               lp->round = round;
            }
            else
            {  name = lpx_get_col_name(lp, ij);
               if (name == NULL) name = "";
               lpx_get_col_bnds(lp, ij, &typx, &lb, &ub);
               round = lp->round, lp->round = 1;
               lpx_get_ips_col(lp, ij, &vx, &dx);
               lp->round = round;
            }
            /* row/column ordinal number */
            fprintf(fp, "%6d ", ij);
            /* row column/name */
            if (strlen(name) <= 12)
               fprintf(fp, "%-12s ", name);
            else
               fprintf(fp, "%s\n%20s", name, "");
            /* two positions are currently not used */
            fprintf(fp, "   ");
            /* row/column primal activity */
            fprintf(fp, "%13.6g ", vx);
            /* row/column lower bound */
            if (typx == LPX_LO || typx == LPX_DB || typx == LPX_FX)
               fprintf(fp, "%13.6g ", lb);
            else
               fprintf(fp, "%13s ", "");
            /* row/column upper bound */
            if (typx == LPX_UP || typx == LPX_DB)
               fprintf(fp, "%13.6g ", ub);
            else if (typx == LPX_FX)
               fprintf(fp, "%13s ", "=");
            else
               fprintf(fp, "%13s ", "");
            /* row/column dual activity */
            fprintf(fp, "%13.6g", dx);
            /* end of line */
            fprintf(fp, "\n");
         }
      }
      fprintf(fp, "\n");
      fprintf(fp, "End of output\n");
      fflush(fp);
      if (ferror(fp))
      {  print("lpx_print_ips: can't write to `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      fclose(fp);
      return 0;
fail: if (fp != NULL) fclose(fp);
      return 1;
}

/*----------------------------------------------------------------------
-- lpx_print_mip - write MIP problem solution in printable format.
--
-- *Synopsis*
--
-- #include "glplpx.h"
-- int lpx_print_mip(LPX *lp, char *fname);
--
-- *Description*
--
-- The routine lpx_print_mip writes a best known integer solution of
-- a MIP problem, which is specified by the pointer lp, to a text file,
-- whose name is the character string fname, in printable format.
--
-- Information reported by the routine lpx_print_mip is intended mainly
-- for visual analysis.
--
-- *Returns*
--
-- If the operation was successful, the routine returns zero. Otherwise
-- the routine prints an error message and returns non-zero. */

int lpx_print_mip(LPX *lp, char *fname)
{     FILE *fp;
      int what, round;
      if (lpx_get_class(lp) != LPX_MIP)
         fault("lpx_print_mip: error -- not a MIP problem");
      print("lpx_print_mip: writing MIP problem solution to `%s'...",
         fname);
      fp = fopen(fname, "w");
      if (fp == NULL)
      {  print("lpx_print_mip: can't create `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      /* problem name */
      {  char *name;
         name = lpx_get_prob_name(lp);
         if (name == NULL) name = "";
         fprintf(fp, "%-12s%s\n", "Problem:", name);
      }
      /* number of rows (auxiliary variables) */
      {  int nr;
         nr = lpx_get_num_rows(lp);
         fprintf(fp, "%-12s%d\n", "Rows:", nr);
      }
      /* number of columns (structural variables) */
      {  int nc, nc_int, nc_bin;
         nc = lpx_get_num_cols(lp);
         nc_int = lpx_get_num_int(lp);
         nc_bin = lpx_get_num_bin(lp);
         fprintf(fp, "%-12s%d (%d integer, %d binary)\n", "Columns:",
            nc, nc_int, nc_bin);
      }
      /* number of non-zeros (constraint coefficients) */
      {  int nz;
         nz = lpx_get_num_nz(lp);
         fprintf(fp, "%-12s%d\n", "Non-zeros:", nz);
      }
      /* solution status */
      {  int status;
         status = lpx_get_mip_stat(lp);
         fprintf(fp, "%-12s%s\n", "Status:",
            status == LPX_I_UNDEF  ? "INTEGER UNDEFINED" :
            status == LPX_I_OPT    ? "INTEGER OPTIMAL" :
            status == LPX_I_FEAS   ? "INTEGER NON-OPTIMAL" :
            status == LPX_I_NOFEAS ? "INTEGER EMPTY" : "???");
      }
      /* objective function */
      {  char *name;
         int dir;
         double mip_obj, lp_obj;
         name = lpx_get_obj_name(lp);
         dir = lpx_get_obj_dir(lp);
         round = lp->round, lp->round = 1;
         mip_obj = lpx_get_mip_obj(lp);
         lp_obj = lpx_get_obj_val(lp);
         lp->round = round;
         fprintf(fp, "%-12s%s%s%.10g %s %.10g (LP)\n", "Objective:",
            name == NULL ? "" : name,
            name == NULL ? "" : " = ", mip_obj,
            dir == LPX_MIN ? "(MINimum)" :
            dir == LPX_MAX ? "(MAXimum)" : "(???)", lp_obj);
      }
      /* main sheet */
      for (what = 1; what <= 2; what++)
      {  int mn, ij;
         fprintf(fp, "\n");
         fprintf(fp, "   No. %-12s      Activity     Lower bound   Uppe"
            "r bound\n",
            what == 1 ? "  Row name" : "Column name");
         fprintf(fp, "------ ------------    ------------- ------------"
            "- -------------\n");
         mn = (what == 1 ? lpx_get_num_rows(lp) : lpx_get_num_cols(lp));
         for (ij = 1; ij <= mn; ij++)
         {  char *name;
            int kind, typx;
            double lb, ub, vx;
            if (what == 1)
            {  name = lpx_get_row_name(lp, ij);
               if (name == NULL) name = "";
               kind = LPX_CV;
               lpx_get_row_bnds(lp, ij, &typx, &lb, &ub);
               round = lp->round, lp->round = 1;
               vx = lpx_get_mip_row(lp, ij);
               lp->round = round;
            }
            else
            {  name = lpx_get_col_name(lp, ij);
               if (name == NULL) name = "";
               kind = lpx_get_col_kind(lp, ij);
               lpx_get_col_bnds(lp, ij, &typx, &lb, &ub);
               round = lp->round, lp->round = 1;
               vx = lpx_get_mip_col(lp, ij);
               lp->round = round;
            }
            /* row/column ordinal number */
            fprintf(fp, "%6d ", ij);
            /* row column/name */
            if (strlen(name) <= 12)
               fprintf(fp, "%-12s ", name);
            else
               fprintf(fp, "%s\n%20s", name, "");
            /* row/column kind */
            fprintf(fp, "%s  ",
               kind == LPX_CV ? " " : kind == LPX_IV ? "*" : "?");
            /* row/column primal activity */
            fprintf(fp, "%13.6g", vx);
            /* row/column lower and upper bounds */
            switch (typx)
            {  case LPX_FR:
                  break;
               case LPX_LO:
                  fprintf(fp, " %13.6g", lb);
                  break;
               case LPX_UP:
                  fprintf(fp, " %13s %13.6g", "", ub);
                  break;
               case LPX_DB:
                  fprintf(fp, " %13.6g %13.6g", lb, ub);
                  break;
               case LPX_FX:
                  fprintf(fp, " %13.6g %13s", lb, "=");
                  break;
               default:
                  insist(typx != typx);
            }
            /* end of line */
            fprintf(fp, "\n");
         }
      }
      fprintf(fp, "\n");
      fprintf(fp, "End of output\n");
      fflush(fp);
      if (ferror(fp))
      {  print("lpx_print_mip: can't write to `%s' - %s", fname,
            strerror(errno));
         goto fail;
      }
      fclose(fp);
      return 0;
fail: if (fp != NULL) fclose(fp);
      return 1;
}

/* eof */
