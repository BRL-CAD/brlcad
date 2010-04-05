/* glpinv.h */

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

#ifndef _GLPINV_H
#define _GLPINV_H

#include "glpluf.h"

#define inv_create            glp_inv_create
#define inv_decomp            glp_inv_decomp
#define inv_h_solve           glp_inv_h_solve
#define inv_ftran             glp_inv_ftran
#define inv_btran             glp_inv_btran
#define inv_update            glp_inv_update
#define inv_delete            glp_inv_delete

/*----------------------------------------------------------------------
-- The structure INV defines an invertable form of the basis matrix B,
-- which is based on LU-factorization and is the following sextet:
--
--    [B] = (F, H, V, P0, P, Q),                                     (1)
--
-- where F, H, and V are such matrices that
--
--    B = F * H * V,                                                 (2)
--
-- and P0, P, and Q are such permutation matrices that the matrix
--
--    L = P0 * F * inv(P0)                                           (3)
--
-- is lower triangular with unity diagonal, and the matrix
--
--    U = P * V * Q                                                  (4)
--
-- is upper triangular. All the matrices have the same order m, which
-- is the order of the basis matrix B.
--
-- The matrices F, V, P, and Q are stored in the structure LUF (see the
-- section GLPLUF), which is a member of the structure INV.
--
-- The matrix H is stored in the form of eta file using row-like format
-- as follows:
--
--    H = H[1] * H[2] * ... * H[nfs],                                (5)
--
-- where H[k], k = 1, 2, ..., nfs, is a row-like factor, which differs
-- from the unity matrix only by one row, nfs is current number of row-
-- like factors. After the factorization has been built for some given
-- basis matrix B the matrix H has no factors and thus it is the unity
-- matrix. Then each time when the factorization is recomputed for an
-- adjacent basis matrix, the next factor H[k], k = 1, 2, ... is built
-- and added to the end of the eta file H.
--
-- Being sparse vectors non-trivial rows of the factors H[k] are stored
-- in the right part of the sparse vector area (SVA) in the same manner
-- as rows and columns of the matrix F.
--
-- For more details see the program documentation. */

typedef struct INV INV;

struct INV
{     /* invertable (factorized) form of the basis matrix */
      int m;
      /* order of the matrices B, F, H, V, P0, P, Q */
      int valid;
      /* if this flag is not set, the invertable form is invalid and
         can't be updated nor used in ftran and btran operations */
      LUF *luf;
      /* LU-factorization (holds the matrices F, V, P, Q) */
      /*--------------------------------------------------------------*/
      /* matrix H in the form of eta file */
      int hh_max;
      /* maximal number of row-like factors (that limits maximal number
         of updates of the factorization) */
      int hh_nfs;
      /* current number of row-like factors (0 <= hh_nfs <= hh_max) */
      int *hh_ndx; /* int hh_ndx[1+hh_max]; */
      /* hh_ndx[0] is not used;
         hh_ndx[k], k = 1, ..., nfs, is number of a non-trivial row of
         the factor H[k] */
      int *hh_ptr; /* int hh_ptr[1+hh_max]; */
      /* hh_ptr[0] is not used;
         hh_ptr[k], k = 1, ..., nfs, is a pointer to the first element
         of the non-trivial row of the factor H[k] in the sparse vector
         area */
      int *hh_len; /* int hh_len[1+hh_max]; */
      /* hh_len[0] is not used;
         hh_len[k], k = 1, ..., nfs, is total number of elements in the
         non-trivial row of the factor H[k] */
      /*--------------------------------------------------------------*/
      /* matrix P0 */
      int *p0_row; /* int p0_row[1+n]; */
      /* p0_row[0] is not used; p0_row[i] = j means that p0[i,j] = 1 */
      int *p0_col; /* int p0_col[1+n]; */
      /* p0_col[0] is not used; p0_col[j] = i means that p0[i,j] = 1 */
      /* if i-th row or column of the matrix F corresponds to i'-th row
         or column of the matrix L = P0*F*inv(P0), then p0_row[i'] = i
         and p0_col[i] = i' */
      /*--------------------------------------------------------------*/
      /* partially transformed column is inv(F*H)*B[j], where B[j] is a
         new column, which will replace the existing j-th column of the
         basis matrix */
      int cc_len;
      /* number of (non-zero) elements in the partially transformed
         column; if cc_len < 0, the column has been not prepared yet */
      int *cc_ndx; /* int cc_ndx[1+m]; */
      /* cc_ndx[0] is not used;
         cc_ndx[k], k = 1, ..., cc_len, is a row index of the partially
         transformed column element */
      double *cc_val; /* double cc_val[1+m]; */
      /* cc_val[0] is not used;
         cc_val[k], k = 1, ..., cc_len, is a numerical (non-zero) value
         of the column element */
      /*--------------------------------------------------------------*/
      /* control parameters */
      double upd_tol;
      /* update tolerance; if on updating the factorization absolute
         value of some diagonal element of the matrix U = P*V*Q is less
         than upd_tol, the factorization is considered as inaccurate */
      /*--------------------------------------------------------------*/
      /* some statistics */
      int nnz_h;
      /* current number of non-zeros in all factors of the matrix H */
};

INV *inv_create(int m, int max_upd);
/* create factorization of the basis matrix */

int inv_decomp(INV *inv,
      void *info, int (*col)(void *info, int j, int rn[], double bj[]));
/* compute factorization of the basis matrix */

void inv_h_solve(INV *inv, int tr, double x[]);
/* solve system H*x = b or H'*x = b */

void inv_ftran(INV *inv, double x[], int save);
/* perform forward transformation (FTRAN) */

void inv_btran(INV *inv, double x[]);
/* perform backward transformation (BTRAN) */

int inv_update(INV *inv, int j);
/* update factorization for adjacent basis matrix */

void inv_delete(INV *inv);
/* delete factorization of the basis matrix */

#endif

/* eof */
