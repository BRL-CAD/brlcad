/* glpluf.h */

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

#ifndef _GLPLUF_H
#define _GLPLUF_H

#define luf_create            glp_luf_create
#define luf_defrag_sva        glp_luf_defrag_sva
#define luf_enlarge_row       glp_luf_enlarge_row
#define luf_enlarge_col       glp_luf_enlarge_col
#define luf_alloc_wa          glp_luf_alloc_wa
#define luf_free_wa           glp_luf_free_wa
#define luf_decomp            glp_luf_decomp
#define luf_f_solve           glp_luf_f_solve
#define luf_v_solve           glp_luf_v_solve
#define luf_solve             glp_luf_solve
#define luf_delete            glp_luf_delete

/*----------------------------------------------------------------------
-- The structure LUF defines LU-factorization of a square matrix A,
-- which is the following quartet:
--
--    [A] = (F, V, P, Q),                                            (1)
--
-- where F and V are such matrices that
--
--    A = F * V,                                                     (2)
--
-- and P and Q are such permutation matrices that the matrix
--
--    L = P * F * inv(P)                                             (3)
--
-- is lower triangular with unity diagonal, and the matrix
--
--    U = P * V * Q                                                  (4)
--
-- is upper triangular. All the matrices have the order n.
--
-- The matrices F and V are stored in row/column-wise sparse format as
-- row and column linked lists of non-zero elements. Unity elements on
-- the main diagonal of the matrix F are not stored. Pivot elements of
-- the matrix V (that correspond to diagonal elements of the matrix U)
-- are also missing from the row and column lists and stored separately
-- in an ordinary array.
--
-- The permutation matrices P and Q are stored as ordinary arrays using
-- both row- and column-like formats.
--
-- The matrices L and U being completely defined by the matrices F, V,
-- P, and Q are not stored explicitly.
--
-- It can easily be shown that the factorization (1)-(3) is a version of
-- LU-factorization. Indeed, from (3) and (4) it follows that
--
--    F = inv(P) * L * P,
--
--    V = inv(P) * U * inv(Q),
--
-- and substitution into (2) gives
--
--    A = F * V = inv(P) * L * U * inv(Q).
--
-- For more details see the program documentation. */

typedef struct LUF LUF;
typedef struct LUF_WA LUF_WA;

struct LUF
{     /* LU-factorization of a square matrix */
      int n;
      /* order of the matrices A, F, V, P, Q */
      int valid;
      /* if this flag is not set, the factorization is invalid */
      /*--------------------------------------------------------------*/
      /* matrix F in row-wise format */
      int *fr_ptr; /* int fr_ptr[1+n]; */
      /* fr_ptr[0] is not used;
         fr_ptr[i], i = 1, ..., n, is a pointer to the first element of
         the i-th row in the sparse vector area */
      int *fr_len; /* int fr_len[1+n]; */
      /* fr_len[0] is not used;
         fr_len[i], i = 1, ..., n, is number of elements in the i-th
         row (except unity diagonal element) */
      /*--------------------------------------------------------------*/
      /* matrix F in column-wise format */
      int *fc_ptr; /* int fc_ptr[1+n]; */
      /* fc_ptr[0] is not used;
         fc_ptr[j], j = 1, ..., n, is a pointer to the first element of
         the j-th column in the sparse vector area */
      int *fc_len; /* int fc_len[1+n]; */
      /* fc_len[0] is not used;
         fc_len[j], j = 1, ..., n, is number of elements in the j-th
         column (except unity diagonal element) */
      /*--------------------------------------------------------------*/
      /* matrix V in row-wise format */
      int *vr_ptr; /* int vr_ptr[1+n]; */
      /* vr_ptr[0] is not used;
         vr_ptr[i], i = 1, ..., n, is a pointer to the first element of
         the i-th row in the sparse vector area */
      int *vr_len; /* int vr_len[1+n]; */
      /* vr_len[0] is not used;
         vr_len[i], i = 1, ..., n, is number of elements in the i-th
         row (except pivot element) */
      int *vr_cap; /* int vr_cap[1+n]; */
      /* vr_cap[0] is not used;
         vr_cap[i], i = 1, ..., n, is capacity of the i-th row, i.e.
         maximal number of elements, which can be stored there without
         relocating the row, vr_cap[i] >= vr_len[i] */
      double *vr_piv; /* double vr_piv[1+n]; */
      /* vr_piv[0] is not used;
         vr_piv[p], p = 1, ..., n, is the pivot element v[p,q], which
         corresponds to a diagonal element of the matrix U = P*V*Q */
      /*--------------------------------------------------------------*/
      /* matrix V in column-wise format */
      int *vc_ptr; /* int vc_ptr[1+n]; */
      /* vc_ptr[0] is not used;
         vc_ptr[j], j = 1, ..., n, is a pointer to the first element of
         the j-th column in the sparse vector area */
      int *vc_len; /* int vc_len[1+n]; */
      /* vc_len[0] is not used;
         vc_len[j], j = 1, ..., n, is number of elements in the j-th
         column (except pivot element) */
      int *vc_cap; /* int vc_cap[1+n]; */
      /* vc_cap[0] is not used;
         vc_cap[j], j = 1, ..., n, is capacity of the j-th column, i.e.
         maximal number of elements, which can be stored there without
         relocating the column, vc_cap[j] >= vc_len[j] */
      /*--------------------------------------------------------------*/
      /* matrix P */
      int *pp_row; /* int pp_row[1+n]; */
      /* pp_row[0] is not used; pp_row[i] = j means that p[i,j] = 1 */
      int *pp_col; /* int pp_col[1+n]; */
      /* pp_col[0] is not used; pp_col[j] = i means that p[i,j] = 1 */
      /* if i-th row or column of the matrix F corresponds to i'-th row
         or column of the matrix L = P*F*inv(P), or if i-th row of the
         matrix V corresponds to i'-th row of the matrix U = P*V*Q, then
         pp_row[i'] = i and pp_col[i] = i' */
      /*--------------------------------------------------------------*/
      /* matrix Q */
      int *qq_row; /* int qq_row[1+n]; */
      /* qq_row[0] is not used; qq_row[i] = j means that q[i,j] = 1 */
      int *qq_col; /* int qq_col[1+n]; */
      /* qq_col[0] is not used; qq_col[j] = i means that q[i,j] = 1 */
      /* if j-th column of the matrix V corresponds to j'-th column of
         the matrix U = P*V*Q, then qq_row[j] = j' and qq_col[j'] = j */
      /*--------------------------------------------------------------*/
      /* sparse vector area (SVA) is a set of locations intended to
         store sparse vectors that represent rows and columns of the
         matrices F and V; each location is the doublet (ndx, val),
         where ndx is an index and val is a numerical value of a sparse
         vector element; in the whole each sparse vector is a set of
         adjacent locations defined by a pointer to the first element
         and number of elements; these pointer and number are stored in
         the corresponding matrix data structure (see above); the left
         part of SVA is used to store rows and columns of the matrix V,
         the right part is used to store rows and columns of the matrix
         F; between the left and right parts there is the middle part,
         locations of which are free */
      int sv_size;
      /* total size of the sparse vector area, in locations; locations
         are numbered by integers 1, 2, ..., sv_size, and location with
         the number 0 is not used; if it is necessary, the size of SVA
         is automatically increased */
      int sv_beg, sv_end;
      /* SVA partitioning pointers:
         locations 1, ..., sv_beg-1 belong to the left part;
         locations sv_beg, ..., sv_end-1 belong to the middle part;
         locations sv_end, ..., sv_size belong to the right part;
         number of free locations, i.e. locations that belong to the
         middle part, is (sv_end - sv_beg) */
      int *sv_ndx; /* int sv_ndx[1+sv_size]; */
      /* sv_ndx[0] is not used;
         sv_ndx[k], 1 <= k <= sv_size, is the index field of the k-th
         location */
      double *sv_val; /* double sv_val[1+sv_size]; */
      /* sv_val[0] is not used;
         sv_val[k], 1 <= k <= sv_size, is the value field of the k-th
         location */
      /* in order to efficiently defragment the left part of SVA there
         is a double linked list of rows and columns of the matrix V,
         where rows have numbers 1, ..., n, and columns have numbers
         n+1, ..., n+n, due to that each row and column can be uniquely
         identified by one integer; in this list rows and columns are
         ordered by ascending their pointers vr_ptr[i] and vc_ptr[j] */
      int sv_head;
      /* the number of the leftmost row/column */
      int sv_tail;
      /* the number of the rightmost row/column */
      int *sv_prev; /* int sv_prev[1+n+n]; */
      /* sv_prev[k], k = 1, ..., n+n, is the number of a row/column,
         which precedes the k-th row/column */
      int *sv_next; /* int sv_next[1+n+n]; */
      /* sv_next[k], k = 1, ..., n+n, is the number of a row/column,
         which succedes the k-th row/column */
      /*--------------------------------------------------------------*/
      /* working arrays */
      int *flag; /* int flag[1+n]; */
      /* integer working array */
      double *work; /* double work[1+n]; */
      /* floating-point working array */
      /*--------------------------------------------------------------*/
      /* control parameters */
      int new_sva;
      /* new required size of the sparse vector area, in locations; set
         automatically by the factorizing routine */
      double piv_tol;
      /* threshold pivoting tolerance, 0 < piv_tol < 1; element v[i,j]
         of the active submatrix fits to be pivot if it satisfies to the
         stability condition |v[i,j]| >= piv_tol * max|v[i,*]|, i.e. if
         this element is not very small (in absolute value) among other
         elements in the same row; decreasing this parameter involves
         better sparsity at the expense of numerical accuracy and vice
         versa */
      int piv_lim;
      /* maximal allowable number of pivot candidates to be considered;
         if piv_lim pivot candidates have been considered, the pivoting
         routine terminates the search with the best candidate found */
      int suhl;
      /* if this flag is set, the pivoting routine applies a heuristic
         rule proposed by Uwe Suhl: if a column of the active submatrix
         has no eligible pivot candidates (i.e. all its elements don't
         satisfy to the stability condition), the routine excludes such
         column from the futher consideration until it becomes a column
         singleton; in many cases this reduces a time needed for pivot
         searching */
      double eps_tol;
      /* epsilon tolerance; each element of the matrix V with absolute
         value less than eps_tol is replaced by exact zero */
      double max_gro;
      /* maximal allowable growth of elements of the matrix V during
         all the factorization process; if on some elimination step the
         ratio big_v / max_a (see below) becomes greater than max_gro,
         the matrix A is considered as ill-conditioned (it is assumed
         that the tolerance piv_tol has an adequate value) */
      /*--------------------------------------------------------------*/
      /* some statistics */
      int nnz_a;
      /* number of non-zeros in the matrix A */
      int nnz_f;
      /* number of non-zeros in the matrix F (except diagonal elements,
         which are always equal to one and therefore not stored) */
      int nnz_v;
      /* number of non-zeros in the matrix V (except pivot elements,
         which correspond to diagonal elements of the matrix U = P*V*Q
         and which are stored separately in the array vr_piv) */
      double max_a;
      /* largest of absolute values of elements of the matrix A */
      double big_v;
      /* estimated largest of absolute values of elements appeared in
         the active submatrix during all the factorization process */
      int rank;
      /* estimated rank of the matrix A */
};

struct LUF_WA
{     /* working area (used only during factorization) */
      double *rs_max; /* double rs_max[1+n]; */
      /* rs_max[0] is not used; rs_max[i], 1 <= i <= n, is used only if
         the i-th row of the matrix V belongs to the active submatrix
         and is the largest of absolute values of elements in this row;
         rs_max[i] < 0.0 means that the largest value is not known yet
         and should be determined by the pivoting routine */
      /*--------------------------------------------------------------*/
      /* in order to efficiently implement Markowitz strategy and Duff
         search technique there are two families {R[0], R[1], ..., R[n]}
         and {C[0], C[1], ..., C[n]}; member R[k] is a set of active
         rows of the matrix V, which have k non-zeros; similarly, member
         C[k] is a set of active columns of the matrix V, which have k
         non-zeros (in the active submatrix); each set R[k] and C[k] is
         implemented as a separate doubly linked list */
      int *rs_head; /* int rs_head[1+n]; */
      /* rs_head[k], 0 <= k <= n, is number of the first active row,
         which has k non-zeros */
      int *rs_prev; /* int rs_prev[1+n]; */
      /* rs_prev[0] is not used; rs_prev[i], 1 <= i <= n, is number of
         the previous active row, which has the same number of non-zeros
         as the i-th row */
      int *rs_next; /* int rs_next[1+n]; */
      /* rs_next[0] is not used; rs_next[i], 1 <= i <= n, is number of
         the next active row, which has the same number of non-zeros as
         the i-th row */
      int *cs_head; /* int cs_head[1+n]; */
      /* cs_head[k], 0 <= k <= n, is number of the first active column,
         which has k non-zeros (in the active submatrix) */
      int *cs_prev; /* int cs_prev[1+n]; */
      /* cs_prev[0] is not used; cs_prev[j], 1 <= j <= n, is number of
         the previous active column, which has the same number of
         non-zeros (in the active submatrix) as the j-th column */
      int *cs_next; /* int cs_next[1+n]; */
      /* cs_next[0] is not used; cs_next[j], 1 <= j <= n, is number of
         the next active column, which has the same number of non-zeros
         (in the active submatrix) as the j-th column */
};

LUF *luf_create(int n, int sv_size);
/* create LU-factorization */

void luf_defrag_sva(LUF *luf);
/* defragment the sparse vector area */

int luf_enlarge_row(LUF *luf, int i, int cap);
/* enlarge row capacity */

int luf_enlarge_col(LUF *luf, int j, int cap);
/* enlarge column capacity */

LUF_WA *luf_alloc_wa(LUF *luf);
/* pre-allocate working area */

void luf_free_wa(LUF_WA *wa);
/* free working area */

int luf_decomp(LUF *luf,
      void *info, int (*col)(void *info, int j, int rn[], double aj[]),
      LUF_WA *wa);
/* compute LU-factorization */

void luf_f_solve(LUF *luf, int tr, double x[]);
/* solve system F*x = b or F'*x = b */

void luf_v_solve(LUF *luf, int tr, double x[]);
/* solve system V*x = b or V'*x = b */

void luf_solve(LUF *luf, int tr, double x[]);
/* solve system A*x = b or A'*x = b */

void luf_delete(LUF *luf);
/* delete LU-factorization */

#endif

/* eof */
