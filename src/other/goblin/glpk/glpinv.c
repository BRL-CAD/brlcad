/* glpinv.c */

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
#include "glpinv.h"
#include "glplib.h"

/*----------------------------------------------------------------------
-- inv_create - create factorization of the basis matrix.
--
-- *Synopsis*
--
-- #include "glpinv.h"
-- INV *inv_create(int m, int max_upd);
--
-- *Description*
--
-- The routine inv_create creates factorization data structure for the
-- the basis matrix of the order m.
--
-- The parameter max_upd specifies maximal number of updates of the
-- factorization. (This parameter defines maximal number of factors of
-- the matrix H, since each update of the factorization for an adjacent
-- basis matrix gives one factor of the matrix H.) The value 100 may be
-- recommended in most cases.
--
-- Being created the factorization initially corresponds to the unity
-- basis matrix (F = H = V = P0 = P = Q = I, so B = I).
--
-- *Returns*
--
-- The routine returns a pointer to the created data structure. */

INV *inv_create(int m, int max_upd)
{     INV *inv;
      int k;
      if (m < 1)
         fault("inv_create: m = %d; invalid parameter", m);
      if (max_upd < 0)
         fault("inv_create: max_upd = %d; invalid parameter", max_upd);
      inv = umalloc(sizeof(INV));
      inv->m = m;
      inv->valid = 1;
      inv->luf = luf_create(m, 0);
      inv->hh_max = max_upd;
      inv->hh_nfs = 0;
      inv->hh_ndx = ucalloc(1+max_upd, sizeof(int));
      inv->hh_ptr = ucalloc(1+max_upd, sizeof(int));
      inv->hh_len = ucalloc(1+max_upd, sizeof(int));
      inv->p0_row = ucalloc(1+m, sizeof(int));
      inv->p0_col = ucalloc(1+m, sizeof(int));
      for (k = 1; k <= m; k++)
         inv->p0_row[k] = inv->p0_col[k] = k;
      inv->cc_len = -1;
      inv->cc_ndx = ucalloc(1+m, sizeof(int));
      inv->cc_val = ucalloc(1+m, sizeof(double));
      inv->upd_tol = 1e-12;
      inv->nnz_h = 0;
      return inv;
}

/*----------------------------------------------------------------------
-- inv_decomp - compute factorization of the basis matrix.
--
-- *Synopsis*
--
-- #include "glpinv.h"
-- int inv_decomp(INV *inv,
--    void *info, int (*col)(void *info, int j, int rn[], double bj[]));
--
-- *Description*
--
-- The routine inv_decomp computes the factorization of the given basis
-- matrix B (reinverts the basis matrix).
--
-- The parameter inv specifies the factorization data structure built by
-- the routine inv_create.
--
-- The parameter info is a transit pointer passed to the formal routine
-- col (see below).
--
-- The formal routine col specifies the given basis matrix B. In order
-- to obtain j-th column of the matrix B the routine inv_decomp calls
-- the routine col with the parameter j (1 <= j <= m, where m is the
-- order of B). In response the routine col should store row indices and
-- numerical values of non-zero elements of the j-th column of B to the
-- locations rn[1], ..., rn[len] and bj[1], ..., bj[len] respectively,
-- where len is number of non-zeros in the j-th column, which should be
-- returned on exit. Neiter zero nor duplicate elements are allowed.
--
-- *Returns*
--
-- The routine inv_decomp returns one of the following codes:
--
-- 0 - no errors;
-- 1 - the given basis matrix is singular (on some elimination step all
--     elements of the active submatrix are zeros, due to that the pivot
--     can't be chosen);
-- 2 - the given basis matrix is ill-conditioned (on some elimination
--     step too intensive growth of elements of the active submatrix has
--     been detected).
--
-- In case of non-zero return code the factorization becomes invalid.
-- It should not be used in other operations until the cause of failure
-- has been eliminated and the factorization has been recomputed again
-- using the routine inv_decomp. For details of obtaining information
-- needed for repairing the basis matrix see the routine luf_decomp (in
-- the module GLPLUF).
--
-- *Algorithm*
--
-- The routine inv_decomp is an interface to the routine luf_decomp
-- (see the module 'glpluf'), which actually computes LU-factorization
-- of the basis matrix B in the form
--
--    [B] = (F, V, P, Q),
--
-- where F and V are such matrices that
--
--    B = F * V,
--
-- and P and Q are such permutation matrices that the matrix
--
--    L = P * F * inv(P)
--
-- is lower triangular with unity diagonal, and the matrix
--
--    U = P * V * Q
--
-- is upper triangular.
--
-- In order to build the complete representation of the factorization
-- (see the formula (1) in the file 'glpinv.h') the routine inv_decomp
-- just additionally sets H = I and P0 = P. */

int inv_decomp(INV *inv,
      void *info, int (*col)(void *info, int j, int rn[], double bj[]))
{     int *pp_row = inv->luf->pp_row;
      int *pp_col = inv->luf->pp_col;
      int *p0_row = inv->p0_row;
      int *p0_col = inv->p0_col;
      int m = inv->m, ret;
      ret = luf_decomp(inv->luf, info, col, NULL);
      if (ret == 0)
      {  /* the matrix B has been successfully factorized */
         inv->valid = 1;
         /* set H = I */
         inv->hh_nfs = 0;
         /* set P0 = P */
         memcpy(&p0_row[1], &pp_row[1], sizeof(int) * m);
         memcpy(&p0_col[1], &pp_col[1], sizeof(int) * m);
         /* invalidate partially transformed column */
         inv->cc_len = -1;
         /* currently the matrix H has no factors */
         inv->nnz_h = 0;
      }
      else
      {  /* the factorization is not valid due to failure */
         inv->valid = 0;
      }
      return ret;
}

/*----------------------------------------------------------------------
-- inv_h_solve - solve system H*x = b or H'*x = b.
--
-- *Synopsis*
--
-- #include "glpinv.h"
-- void inv_h_solve(INV *inv, int tr, double x[]);
--
-- *Description*
--
-- The routine inv_h_solve solves either the system H*x = b (if the
-- flag tr is zero) or the system H'*x = b (if the flag tr is non-zero),
-- where the matrix H is a component of the factorization specified by
-- the parameter inv, H' is a matrix transposed to H.
--
-- On entry the array x should contain elements of the right-hand side
-- vector b in locations x[1], ..., x[m], where m is the order of the
-- matrix H. On exit this array will contain elements of the solution
-- vector x in the same locations. */

void inv_h_solve(INV *inv, int tr, double x[])
{     int nfs = inv->hh_nfs;
      int *hh_ndx = inv->hh_ndx;
      int *hh_ptr = inv->hh_ptr;
      int *hh_len = inv->hh_len;
      int *sv_ndx = inv->luf->sv_ndx;
      double *sv_val = inv->luf->sv_val;
      int i, k, beg, end, ptr;
      double temp;
      if (!inv->valid)
         fault("inv_h_solve: the factorization is not valid");
      if (!tr)
      {  /* solve the system H*x = b */
         for (k = 1; k <= nfs; k++)
         {  i = hh_ndx[k];
            temp = x[i];
            beg = hh_ptr[k];
            end = beg + hh_len[k] - 1;
            for (ptr = beg; ptr <= end; ptr++)
               temp -= sv_val[ptr] * x[sv_ndx[ptr]];
            x[i] = temp;
         }
      }
      else
      {  /* solve the system H'*x = b */
         for (k = nfs; k >= 1; k--)
         {  i = hh_ndx[k];
            temp = x[i];
            if (temp == 0.0) continue;
            beg = hh_ptr[k];
            end = beg + hh_len[k] - 1;
            for (ptr = beg; ptr <= end; ptr++)
               x[sv_ndx[ptr]] -= sv_val[ptr] * temp;
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- inv_ftran - perform forward transformation (FTRAN).
--
-- *Synopsis*
--
-- #include "glpinv.h"
-- void inv_ftran(INV *inv, double x[], int save);
--
-- *Description*
--
-- The routine inv_ftran performs forward transformation (FTRAN) of the
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
-- chosen to enter the basis. In this case the routine inv_ftran saves
-- this column (after partial transformation) in order that the routine
-- inv_update could update (recompute) the factorization for an adjacent
-- basis using this partially transformed column. The simplex method
-- routine should call the routine inv_ftran with the save flag set at
-- least once before a subsequent call to the routine inv_update. */

void inv_ftran(INV *inv, double x[], int save)
{     int m = inv->m;
      int *pp_row = inv->luf->pp_row;
      int *pp_col = inv->luf->pp_col;
      double eps_tol = inv->luf->eps_tol;
      int *p0_row = inv->p0_row;
      int *p0_col = inv->p0_col;
      int *cc_ndx = inv->cc_ndx;
      double *cc_val = inv->cc_val;
      int i, len;
      double temp;
      if (!inv->valid)
         fault("inv_ftran: the factorization is not valid");
      /* B = F*H*V, therefore inv(B) = inv(V)*inv(H)*inv(F) */
      inv->luf->pp_row = p0_row;
      inv->luf->pp_col = p0_col;
      luf_f_solve(inv->luf, 0, x);
      inv->luf->pp_row = pp_row;
      inv->luf->pp_col = pp_col;
      inv_h_solve(inv, 0, x);
      /* save partially transformed column (if required) */
      if (save)
      {  len = 0;
         for (i = 1; i <= m; i++)
         {  temp = x[i];
            if (temp == 0.0 || fabs(temp) < eps_tol) continue;
            len++;
            cc_ndx[len] = i;
            cc_val[len] = temp;
         }
         inv->cc_len = len;
      }
      luf_v_solve(inv->luf, 0, x);
      return;
}

/*----------------------------------------------------------------------
-- inv_btran - perform backward transformation (BTRAN).
--
-- *Synopsis*
--
-- #include "glpinv.h"
-- void inv_btran(INV *inv, double x[]);
--
-- *Description*
--
-- The routine inv_btran performs backward transformation (BTRAN) of the
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

void inv_btran(INV *inv, double x[])
{     int *pp_row = inv->luf->pp_row;
      int *pp_col = inv->luf->pp_col;
      int *p0_row = inv->p0_row;
      int *p0_col = inv->p0_col;
      /* B = F*H*V, therefore inv(B') = inv(F')*inv(H')*inv(V') */
      if (!inv->valid)
         fault("inv_btran: the factorization is not valid");
      luf_v_solve(inv->luf, 1, x);
      inv_h_solve(inv, 1, x);
      inv->luf->pp_row = p0_row;
      inv->luf->pp_col = p0_col;
      luf_f_solve(inv->luf, 1, x);
      inv->luf->pp_row = pp_row;
      inv->luf->pp_col = pp_col;
      return;
}

/*----------------------------------------------------------------------
-- inv_update - update factorization for adjacent basis matrix.
--
-- *Synopsis*
--
-- #include "glpinv.h"
-- int inv_update(INV *inv, int j);
--
-- *Description*
--
-- The routine inv_update recomputes the factorization, which on entry
-- corresponds to the current basis matrix B, in order that the updated
-- factorization would correspond to the adjacent basis matrix B' that
-- differs from B in the j-th column.
--
-- The new j-th column of the basis matrix is passed implicitly to the
-- routine inv_update. It is assumed that this column was saved before
-- by the routine inv_ftran (see above).
--
-- *Returns*
--
-- The routine inv_update returns one of the following codes:
--
-- 0 - no errors;
-- 1 - the adjacent basis matrix is structurally singular, since after
--     changing the j-th column of the matrix V by the new column (see
--     the algorithm below) the case k1 > k2 occured;
-- 2 - the factorization is inaccurate, since after transforming the
--     k2-th row of the matrix U = P*V*Q, the diagonal element u[k2,k2]
--     is zero or close to zero;
-- 3 - maximal number of updates has been reached;
-- 4 - overflow of the sparse vector area.
--
-- In case of non-zero return code the factorization becomes invalid.
-- It should not be used until it has been recomputed using the routine
-- inv_decomp.
--
-- *Algorithm*
--
-- The routine inv_update is based on the transformation proposed by
-- Forrest and Tomlin.
--
-- Let the j-th column of the basis matrix B have been replaced by new
-- column B[j]. In order to keep the equality B = F*H*V the j-th column
-- of the matrix V should be replaced by the column inv(F*H)*B[j]. The
-- latter is partially transformed column, which the routine inv_ftran
-- saves on performing forward transformation of B[j].
--
-- From the point of view of the matrix U = P*V*Q, replacement of the
-- j-th column of the matrix V involves replacement of the k1-th column
-- of the matrix U, where k1 is determined by the permutation matrix Q.
-- Thus, the matrix U loses its upper triangular form and becomes the
-- following:
--
--        1   k1       k2   m
--    1   x x * x x x x x x x
--        . x * x x x x x x x
--    k1  . . * x x x x x x x
--        . . * x x x x x x x
--        . . * . x x x x x x
--        . . * . . x x x x x
--        . . * . . . x x x x
--    k2  . . * . . . . x x x
--        . . . . . . . . x x
--    m   . . . . . . . . . x
--
-- where row index k2 corresponds to the lowest non-zero element of the
-- k1-th column.
--
-- Then the routine shifts rows and columns k1+1, k1+2, ..., k2 of the
-- matrix U by one position to the left and upwards and moves k1-th row
-- and k1-th column to the position k2. As the result of such symmetric
-- permutations the matrix U becomes the following:
--
--        1   k1       k2   m
--    1   x x x x x x x * x x
--        . x x x x x x * x x
--    k1  . . x x x x x * x x
--        . . . x x x x * x x
--        . . . . x x x * x x
--        . . . . . x x * x x
--        . . . . . . x * x x
--    k2  . . x x x x x * x x
--        . . . . . . . . x x
--    m   . . . . . . . . . x
--
-- Now the routine performs gaussian elimination in order to eliminate
-- the elements u[k2,k1], u[k2,k1+1], ..., u[k2,k2-1] using the diagonal
-- elements u[k1,k1], u[k1+1,k1+1], ..., u[k2-1,k2-1] as pivots in the
-- same way as described in comments to the routine luf_decomp (see the
-- module 'glpluf'). Note that actually all operations are performed on
-- the matrix V, not on the matrix U. During the elimination process the
-- routine permutes neither rows nor columns, therefore only the k2-th
-- row of the matrix U is changed.
--
-- In order to keep the equality B = F*H*V, each time when the routine
-- applies elementary gaussian transformation to the transformed row of
-- the matrix V (that corresponds to the k2-th row of the matrix U), it
-- also adds a new element (gaussian multiplier) to the current row-like
-- factor of the matrix H, which (factor) corresponds to the transformed
-- row of the matrix V. */

int inv_update(INV *inv, int j)
{     int m = inv->m;
      LUF *luf = inv->luf;
      int *vr_ptr = luf->vr_ptr;
      int *vr_len = luf->vr_len;
      int *vr_cap = luf->vr_cap;
      double *vr_piv = luf->vr_piv;
      int *vc_ptr = luf->vc_ptr;
      int *vc_len = luf->vc_len;
      int *vc_cap = luf->vc_cap;
      int *pp_row = luf->pp_row;
      int *pp_col = luf->pp_col;
      int *qq_row = luf->qq_row;
      int *qq_col = luf->qq_col;
      int *sv_ndx = luf->sv_ndx;
      double *sv_val = luf->sv_val;
      double *work = luf->work;
      double eps_tol = luf->eps_tol;
      int *hh_ndx = inv->hh_ndx;
      int *hh_ptr = inv->hh_ptr;
      int *hh_len = inv->hh_len;
      int cc_len = inv->cc_len;
      int *cc_ndx = inv->cc_ndx;
      double *cc_val = inv->cc_val;
      double upd_tol = inv->upd_tol;
      int ret = 0;
      int i, i_beg, i_end, i_ptr, j_beg, j_end, j_ptr, k, k1, k2, p, q,
         p_beg, p_end, p_ptr, ptr;
      double f, temp;
      if (!inv->valid)
         fault("inv_update: the factorization is not valid");
      if (inv->cc_len < 0)
         fault("inv_update: new column has not been prepared");
      if (!(1 <= j && j <= m))
         fault("inv_update: j = %d; invalid column number", j);
      /* check if a new factor of the matrix H can be created */
      if (inv->hh_nfs == inv->hh_max)
      {  /* maximal number of updates has been reached */
         inv->valid = luf->valid = 0;
         ret = 3;
         goto done;
      }
      /* remove elements of the j-th column from the matrix V */
      j_beg = vc_ptr[j];
      j_end = j_beg + vc_len[j] - 1;
      for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
      {  /* get row index of v[i,j] */
         i = sv_ndx[j_ptr];
         /* find v[i,j] in the i-th row */
         i_beg = vr_ptr[i];
         i_end = i_beg + vr_len[i] - 1;
         for (i_ptr = i_beg; sv_ndx[i_ptr] != j; i_ptr++) /* nop */;
         insist(i_ptr <= i_end);
         /* remove v[i,j] from the i-th row */
         sv_ndx[i_ptr] = sv_ndx[i_end];
         sv_val[i_ptr] = sv_val[i_end];
         vr_len[i]--;
      }
      /* now the j-th column of the matrix V is empty */
      luf->nnz_v -= vc_len[j];
      vc_len[j] = 0;
      /* add elements of the new j-th column to the matrix V; determine
         indices k1 and k2 */
      k1 = qq_row[j];
      k2 = 0;
      for (ptr = 1; ptr <= cc_len; ptr++)
      {  /* get row index of v[i,j] */
         i = cc_ndx[ptr];
         /* at least one unused location is needed in the i-th row */
         if (vr_len[i] + 1 > vr_cap[i])
         {  if (luf_enlarge_row(luf, i, vr_len[i] + 10))
            {  /* overflow of the sparse vector area */
               inv->valid = luf->valid = 0;
               luf->new_sva = luf->sv_size + luf->sv_size;
               ret = 4;
               goto done;
            }
         }
         /* add v[i,j] to the i-th row */
         i_ptr = vr_ptr[i] + vr_len[i];
         sv_ndx[i_ptr] = j;
         sv_val[i_ptr] = cc_val[ptr];
         vr_len[i]++;
         /* adjust the index k2 */
         if (k2 < pp_col[i]) k2 = pp_col[i];
      }
      /* capacity of the j-th column (which is currently empty) should
         be not less than cc_len locations */
      if (vc_cap[j] < cc_len)
      {  if (luf_enlarge_col(luf, j, cc_len))
         {  /* overflow of the sparse vector area */
            inv->valid = luf->valid = 0;
            luf->new_sva = luf->sv_size + luf->sv_size;
            ret = 4;
            goto done;
         }
      }
      /* add elements of the new j-th column to the column list */
      j_ptr = vc_ptr[j];
      memmove(&sv_ndx[j_ptr], &cc_ndx[1], cc_len * sizeof(int));
      memmove(&sv_val[j_ptr], &cc_val[1], cc_len * sizeof(double));
      vc_len[j] = cc_len;
      luf->nnz_v += cc_len;
      /* k1 > k2 means that the diagonal element u[k2,k2] is zero and
         therefore the adjacent basis matrix is structurally singular */
      if (k1 > k2)
      {  inv->valid = luf->valid = 0;
         ret = 1;
         goto done;
      }
      /* perform implicit symmetric permutations of rows and columns of
         the matrix U */
      i = pp_row[k1], j = qq_col[k1];
      for (k = k1; k < k2; k++)
      {  pp_row[k] = pp_row[k+1], pp_col[pp_row[k]] = k;
         qq_col[k] = qq_col[k+1], qq_row[qq_col[k]] = k;
      }
      pp_row[k2] = i, pp_col[i] = k2;
      qq_col[k2] = j, qq_row[j] = k2;
      /* note that now the i-th row of the matrix V is the k2-th row of
         the matrix U; since no pivoting is used, only this row will be
         transformed */
      /* copy elements of the i-th row of the matrix V to the working
         array and remove these elements from the matrix V */
      for (j = 1; j <= m; j++) work[j] = 0.0;
      i_beg = vr_ptr[i];
      i_end = i_beg + vr_len[i] - 1;
      for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
      {  /* get column index of v[i,j] */
         j = sv_ndx[i_ptr];
         /* store v[i,j] to the working array */
         work[j] = sv_val[i_ptr];
         /* find v[i,j] in the j-th column */
         j_beg = vc_ptr[j];
         j_end = j_beg + vc_len[j] - 1;
         for (j_ptr = j_beg; sv_ndx[j_ptr] != i; j_ptr++) /* nop */;
         insist(j_ptr <= j_end);
         /* remove v[i,j] from the j-th column */
         sv_ndx[j_ptr] = sv_ndx[j_end];
         sv_val[j_ptr] = sv_val[j_end];
         vc_len[j]--;
      }
      /* now the i-th row of the matrix V is empty */
      luf->nnz_v -= vr_len[i];
      vr_len[i] = 0;
      /* create the next row-like factor of the matrix H; this factor
         corresponds to the i-th (transformed) row */
      inv->hh_nfs++;
      hh_ndx[inv->hh_nfs] = i;
      /* hh_ptr[] will be set later */
      hh_len[inv->hh_nfs] = 0;
      /* up to (k2 - k1) free locations are needed to add new elements
         to the non-trivial row of the row-like factor */
      if (luf->sv_end - luf->sv_beg < k2 - k1)
      {  luf_defrag_sva(luf);
         if (luf->sv_end - luf->sv_beg < k2 - k1)
         {  /* overflow of the sparse vector area */
            inv->valid = luf->valid = 0;
            luf->new_sva = luf->sv_size + luf->sv_size;
            ret = 4;
            goto done;
         }
      }
      /* eliminate subdiagonal elements of the matrix U */
      for (k = k1; k < k2; k++)
      {  /* v[p,q] = u[k,k] */
         p = pp_row[k], q = qq_col[k];
         /* this is the cruical point, where even tiny non-zeros should
            not be dropped */
         if (work[q] == 0.0) continue;
         /* compute gaussian multiplier f = v[i,q] / v[p,q] */
         f = work[q] / vr_piv[p];
         /* perform gaussian transformation:
            (i-th row) := (i-th row) - f * (p-th row)
            in order to eliminate v[i,q] = u[k2,k] */
         p_beg = vr_ptr[p];
         p_end = p_beg + vr_len[p] - 1;
         for (p_ptr = p_beg; p_ptr <= p_end; p_ptr++)
            work[sv_ndx[p_ptr]] -= f * sv_val[p_ptr];
         /* store new element (gaussian multiplier that corresponds to
            the p-th row) in the current row-like factor */
         luf->sv_end--;
         sv_ndx[luf->sv_end] = p;
         sv_val[luf->sv_end] = f;
         hh_len[inv->hh_nfs]++;
      }
      /* set pointer to the current row-like factor of the matrix H
         (if no elements were added to this factor, it is unity matrix
         and therefore can be discarded) */
      if (hh_len[inv->hh_nfs] == 0)
         inv->hh_nfs--;
      else
      {  hh_ptr[inv->hh_nfs] = luf->sv_end;
         inv->nnz_h += hh_len[inv->hh_nfs];
      }
      /* store new pivot that corresponds to u[k2,k2] */
      vr_piv[i] = work[qq_col[k2]];
      /* check if u[k2,k2] is closer to zero */
      if (fabs(vr_piv[i]) < upd_tol)
      {  /* the factorization should be considered as inaccurate (this
            mainly happens due to excessive round-off errors) */
         inv->valid = luf->valid = 0;
         ret = 2;
         goto done;
      }
      /* new elements of the i-th row of the matrix V (which correspond
         to non-diagonal elements u[k2,k2+1], ..., u[k2,m] of the matrix
         U = P*V*Q) are contained in the working array; add them to the
         matrix V */
      cc_len = 0;
      for (k = k2+1; k <= m; k++)
      {  /* get column index and value of v[i,j] = u[k2,k] */
         j = qq_col[k];
         temp = work[j];
         /* if v[i,j] is close to zero, skip it */
         if (fabs(temp) < eps_tol) continue;
         /* at least one unused location is needed in the j-th column */
         if (vc_len[j] + 1 > vc_cap[j])
         {  if (luf_enlarge_col(luf, j, vc_len[j] + 10))
            {  /* overflow of the sparse vector area */
               inv->valid = luf->valid = 0;
               luf->new_sva = luf->sv_size + luf->sv_size;
               ret = 4;
               goto done;
            }
         }
         /* add v[i,j] to the j-th column */
         j_ptr = vc_ptr[j] + vc_len[j];
         sv_ndx[j_ptr] = i;
         sv_val[j_ptr] = temp;
         vc_len[j]++;
         /* also store v[i,j] to the auxiliary array */
         cc_len++;
         cc_ndx[cc_len] = j;
         cc_val[cc_len] = temp;
      }
      /* capacity of the i-th row (which is currently empty) should be
         not less than cc_len locations */
      if (vr_cap[i] < cc_len)
      {  if (luf_enlarge_row(luf, i, cc_len))
         {  /* overflow of the sparse vector area */
            inv->valid = luf->valid = 0;
            luf->new_sva = luf->sv_size + luf->sv_size;
            ret = 4;
            goto done;
         }
      }
      /* add new elements of the i-th row to the row list */
      i_ptr = vr_ptr[i];
      memmove(&sv_ndx[i_ptr], &cc_ndx[1], cc_len * sizeof(int));
      memmove(&sv_val[i_ptr], &cc_val[1], cc_len * sizeof(double));
      vr_len[i] = cc_len;
      luf->nnz_v += cc_len;
      /* the factorization has been successfully updated */
      inv->cc_len = -1;
done: /* return to the simplex method routine */
      return ret;
}

/*----------------------------------------------------------------------
-- inv_delete - delete factorization of the basis matrix.
--
-- *Synopsis*
--
-- #include "glpinv.h"
-- void inv_delete(INV *inv);
--
-- *Description*
--
-- The routine inv_delete deletes factorization data structure, which
-- the parameter inv points to, freeing all the memory allocated to this
-- object. */

void inv_delete(INV *inv)
{     luf_delete(inv->luf);
      ufree(inv->hh_ndx);
      ufree(inv->hh_ptr);
      ufree(inv->hh_len);
      ufree(inv->p0_row);
      ufree(inv->p0_col);
      ufree(inv->cc_ndx);
      ufree(inv->cc_val);
      ufree(inv);
      return;
}

/* eof */
