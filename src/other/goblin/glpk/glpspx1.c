/* glpspx1.c (basis maintenance routines) */

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

#include <string.h>
#include "glplib.h"
#include "glpspx.h"

/*----------------------------------------------------------------------
-- spx_invert - reinvert the basis matrix.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- int spx_invert(LPX *lp);
--
-- *Description*
--
-- The routine spx_invert computes a factorization of the current basis
-- matrix B specified by the arrays lp->tagx, lp->posx, and lp->indx.
--
-- *Returns*
--
-- The routine returns one of the following codes:
--
-- 0 - the basis matrix has been successfully factorized;
-- 1 - the basis matrix is singular;
-- 2 - the basis matrix is ill-conditioned.
--
-- In the case of non-zero return code the calling program can "repair"
-- the basis matrix replacing linearly dependent columns by appropriate
-- unity columns of auxiliary variable using information stored by the
-- factorizing routine in the object lp->inv (for details see comments
-- in the module GLPINV). */

static int inv_col(void *info, int j, int rn[], double bj[])
{     /* this auxiliary routine returns row indices and numerical
         values of non-zero elements of the j-th column of the current
         basis matrix B, which has to be reinverted */
      LPX *lp = info;
      int m = lp->m;
      int n = lp->n;
      int *aa_ptr = lp->A->ptr;
      int *aa_len = lp->A->len;
      int *sv_ndx = lp->A->ndx;
      double *sv_val = lp->A->val;
      int *indx = lp->indx;
      int k, ptr, len, t;
      insist(1 <= j && j <= m);
      k = indx[j]; /* x[k] = xB[j] */
      insist(1 <= k && k <= m+n);
      if (k <= m)
      {  /* x[k] is auxiliary variable */
         len = 1;
         rn[1] = k;
         bj[1] = +1.0;
      }
      else
      {  /* x[k] is structural variable */
         ptr = aa_ptr[k];
         len = aa_len[k];
         memcpy(&rn[1], &sv_ndx[ptr], len * sizeof(int));
         memcpy(&bj[1], &sv_val[ptr], len * sizeof(double));
         for (t = len; t >= 1; t--) bj[t] = - bj[t];
      }
      return len;
}

int spx_invert(LPX *lp)
{     static double piv_tol[1+3] = { 0.00, 0.10, 0.30, 0.70 };
      int try, ret;
      /* if the invertable form has a wrong size, delete it */
      if (lp->inv != NULL && lp->inv->m != lp->m)
         inv_delete(lp->inv), lp->inv = NULL;
      /* if the invertable form does not exist, create it */
#if 0
      if (lp->inv == NULL) lp->inv = inv_create(lp->m, 100);
#else
      if (lp->inv == NULL) lp->inv = inv_create(lp->m, 50);
#endif
      /* try to factorize the basis matrix */
      for (try = 1; try <= 3; try++)
      {  if (try > 1 && lp->msg_lev >= 3)
            print("spx_invert: trying to factorize the basis using thre"
               "shold tolerance %g", piv_tol[try]);
         lp->inv->luf->piv_tol = piv_tol[try];
         ret = inv_decomp(lp->inv, lp, inv_col);
         if (ret == 0) break;
      }
      /* analyze the return code */
      switch (ret)
      {  case 0:
            /* success */
            lp->b_stat = LPX_B_VALID;
            break;
         case 1:
            if (lp->msg_lev >= 1)
               print("spx_invert: the basis matrix is singular");
            lp->b_stat = LPX_B_UNDEF;
            break;
         case 2:
            if (lp->msg_lev >= 1)
               print("spx_invert: the basis matrix is ill-conditioned");
            lp->b_stat = LPX_B_UNDEF;
            break;
         default:
            insist(ret != ret);
      }
      /* return to the calling program */
      return ret;
}

/*----------------------------------------------------------------------
-- spx_ftran - perform forward transformation (FTRAN).
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_ftran(LPX *lp, double x[], int save);
--
-- *Description*
--
-- The routine spx_ftran performs forward transformation (FTRAN) of the
-- given vector using the factorization of the basis matrix.
--
-- In order to perform FTRAN the routine solves the system B*x' = x,
-- where B is the basis matrix, x' is vector of unknowns (transformed
-- vector that should be computed), x is vector of right-hand sides
-- (input vector that should be transformed).
--
-- On entry the array x should contain components of the vector x in
-- locations x[1], x[2], ..., x[m], where m is the order of the basis
-- matrix. On exit this array will contain components of the vector x'
-- in the same locations.
--
-- The parameter save is a flag. If this flag is set, it means that the
-- input vector x is a column of the non-basic variable, which has been
-- chosen to enter the basis. In this case the routine spx_ftran saves
-- this column (after partial transformation) in order that the routine
-- spx_update could update (recompute) the factorization for an adjacent
-- basis using this partially transformed column. The simplex method
-- routine should call the routine spx_ftran with the save flag set at
-- least once before a subsequent call to the routine spx_update. */

void spx_ftran(LPX *lp, double x[], int save)
{     insist(lp->b_stat == LPX_B_VALID);
      inv_ftran(lp->inv, x, save);
      return;
}

/*----------------------------------------------------------------------
-- spx_btran - perform backward transformation (BTRAN).
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- void spx_btran(LPX *lp, double x[]);
--
-- *Description*
--
-- The routine spx_btran performs backward transformation (BTRAN) of the
-- given vector using the factorization of the basis matrix.
--
-- In order to perform BTRAN the routine solves the system B'*x' = x,
-- where B' is a matrix transposed to the basis matrix B, x' is vector
-- of unknowns (transformed vector that should be computed), x is vector
-- of right-hand sides (input vector that should be transformed).
--
-- On entry the array x should contain components of the vector x in
-- locations x[1], x[2], ..., x[m], where m is the order of the basis
-- matrix. On exit this array will contain components of the vector x'
-- in the same locations. */

void spx_btran(LPX *lp, double x[])
{     insist(lp->b_stat == LPX_B_VALID);
      inv_btran(lp->inv, x);
      return;
}

/*----------------------------------------------------------------------
-- spx_update - update factorization for adjacent basis matrix.
--
-- *Synopsis*
--
-- #include "glpspx.h"
-- int spx_update(LPX *lp, int j);
--
-- *Description*
--
-- The routine spx_update recomputes the factorization, which on entry
-- corresponds to the current basis matrix B, in order that the updated
-- factorization would correspond to the adjacent basis matrix B' that
-- differs from B in the j-th column.
--
-- The new j-th column of the basis matrix is passed implicitly to the
-- routine spx_update. It is assumed that this column was saved before
-- by the routine spx_ftran (see above).
--
-- *Returns*
--
-- Zero return code means that the factorization has been successfully
-- updated. Non-zero return code means that the factorization should be
-- reinverted by the routine spx_invert. */

int spx_update(LPX *lp, int j)
{     int ret;
      insist(1 <= j && j <= lp->m);
      ret = inv_update(lp->inv, j);
      return ret;
}

/* eof */
