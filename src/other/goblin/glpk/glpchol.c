/* glpchol.c */

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
#include "glpchol.h"
#include "glplib.h"
#include "glpqmd.h"

/*----------------------------------------------------------------------
-- chol_symb - compute Cholesky decomposition A = U'*U; symbolic phase
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- void chol_symb(MAT *A, MAT *U, int head[], int next[], char work[]);
--
-- *Description*
--
-- The chol_symb routine implements symbolic phase of the Cholesky
-- decomposition A = U'*U, where A is a given sparse symmetric positive
-- definite matrix, U is resultant upper triangular matrix.
--
-- So far as the matrix A is symmetric, on entry it should contain only
-- upper triangular part. Only pattern of A is used, i.e. numeric values
-- of elements of the matrix A are ignored (they all are considered as
-- non-zeros). If A and U are different objects, the matrix A remains
-- unchanged on exit.
--
-- Order of the resultant matrix U should be the same as order of the
-- input matrix A. If A and U are different objects, on entry initial
-- contents of the matrix U is ignored. It is allowed that A and U are
-- the same object; in this case the matrix U is built in place of the
-- matrix A. (Note that the routine assigns the value +1.0 to elements
-- of the matrix U, which correspond to the original elements of the
-- matrix A, and the value -1.0 to elements, which appear as the result
-- of symbolic elimination. This may be used for visual estimating of
-- fill-in phenomenon.)
--
-- Auxiliary arrays head, next, and work should contain at least 1+n
-- elements, where n is order of the matrices A and U. If some of these
-- parameters is NULL, the chol_symb routine automatically allocates and
-- frees the corresponding working array(s).
--
-- *Method*
--
-- The chol_symb routine computes the matrix U in row-wise manner. No
-- pivoting is used, i.e. pivots are diagonal elements of the matrix A.
-- Before computing kth row the matrix U is the following:
--
--    1       k         n
-- 1  x x x x x x x x x x
--    . x x x x x x x x x
--    . . x x x x x x x x
--    . . . x x x x x x x
-- k  . . . . * * * * * *
--    . . . . . * * * * *
--    . . . . . . * * * *
--    . . . . . . . * * *
--    . . . . . . . . * *
-- n  . . . . . . . . . *
--
-- where 'x' denotes rows, which have been computed; '*' denotes rows
-- of the input matrix A (initially they are copied to the matrix U),
-- which have been not processed yet.
--
-- It is well known that in order to symbolically compute k-th row of
-- the matrix U it is necessary to merge k-th row of the matrix A (which
-- is placed in k-th row of the matrix U) and each i-th row of U, where
-- u[i,k] is non-zero (such rows are placed above k-th row).
--
-- However, in order to reduce number of rows, which should be merged,
-- the chol_symb routine uses the advanced algorithm proposed in:
--
-- D.J.Rose, R.E.Tarjan, and G.S.Lueker. Algorithmic aspects of vertex
-- elimination on graphs. SIAM J. Comput. 5, 1976, 266-83.
--
-- The authors of the cited paper show that we have the same result if
-- we merge k-th row of the matrix U and such its rows (among rows with
-- numbers 1, 2, ..., k-1), where leftmost non-zero non-diagonal element
-- is placed in k-th column. This feature signficantly reduces number of
-- rows, which should be merged, especially on the final steps, when the
-- computed rows of the matrix U becomes quite dense.
--
-- In order to determine row numbers, which should be merged with k-th
-- row of the matrix U, for a fixed time the routine uses linked lists
-- of row numbers. The location head[k] contains number of the first
-- row, where leftmost non-zero non-diagonal element is placed in k-th
-- column, and the location next[i] contains number of the next row in
-- the list, which has leftmost non-zero non-diagonal element in the
-- same column as i-th row. */

void chol_symb(MAT *A, MAT *U, int _head[], int _next[], char _work[])
{     ELEM *e;
      int n = A->m, i, j, k, *head = _head, *next = _next;
      char *work = _work;
      if (!(U->m == n && U->n == n && A->n == n))
         fault("chol_symb: inconsistent dimension");
      /* U := A */
      if (U != A) copy_mat(clear_mat(U), A);
      /* check that the original matrix is upper triangular and assign
         values +1.0 to the original non-zeros (this may be useful for
         visual estimating) */
      for (i = 1; i <= n; i++)
      {  for (e = U->row[i]; e != NULL; e = e->row)
         {  if (e->i > e->j)
               fault("chol_symb: input matrix is not upper triangular");
            e->val = +1.0;
         }
      }
      /* add diagonal elements (only those, which are missing) */
      for (i = 1; i <= n; i++)
      {  for (e = U->row[i]; e != NULL; e = e->row)
            if (e->i == e->j) break;
         if (e == NULL) new_elem(U, i, i, 0.0);
      }
      /* allocate and clear auxiliary arrays */
      if (_head == NULL) head = ucalloc(1+n, sizeof(int));
      if (_next == NULL) next = ucalloc(1+n, sizeof(int));
      if (_work == NULL) work = ucalloc(1+n, sizeof(char));
      for (i = 1; i <= n; i++) head[i] = next[i] = 0, work[i] = 0;
      /* main loop */
      for (k = 1; k <= n; k++)
      {  /* compute k-th row of the final matrix U */
         /* mark existing (original) elements of k-th row */
         for (e = U->row[k]; e != NULL; e = e->row) work[e->j] = 1;
         /* merge those rows of the matrix U, where leftmost non-zero
            non-diagonal element is placed in k-th column */
         for (i = head[k]; i != 0; i = next[i])
         {  insist(i < k);
            for (e = U->row[i]; e != NULL; e = e->row)
            {  /* ignore diagonal elements and elements, which already
                  exist in k-th row */
               if (e->i == e->j || work[e->j]) continue;
               /* create new non-zero in k-th row */
               new_elem(U, k, e->j, -1.0), work[e->j] = 1;
            }
         }
         /* k-th row has been symbolically computed */
         /* clear the array work and determine column index of leftmost
            non-zero non-diagonal element of k-th column */
         j = n+1;
         for (e = U->row[k]; e != NULL; e = e->row)
         {  insist(e->j >= k);
            if (e->i != e->j && j > e->j) j = e->j;
            work[e->j] = 0;
         }
         /* include k-th row into the appropriate linked list */
         if (j <= n) next[k] = head[j], head[j] = k;
      }
      /* free auxiliary arrays */
      if (_head == NULL) ufree(head);
      if (_next == NULL) ufree(next);
      if (_work == NULL) ufree(work);
      /* return to the calling program */
      return;
}

/*----------------------------------------------------------------------
-- chol_numb - compute Cholesky decomposition A = U'*U; numeric phase
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- int chol_numb(MAT *A, MAT *U, void *head[], double work[]);
--
-- *Description*
--
-- The chol_numb routine implements numeric phase of the Cholesky
-- decomposition A = U'*U, where A is a given sparse symmetric positive
-- definite matrix, U is resultant upper triangular matrix, assuming
-- that the pattern of the matrix U (i.e. distribution of its non-zero
-- elements) is known. (Symbolic phase of this operation can be
-- performed by means of the chol_symb routine.)
--
-- So far as the matrix A is symmetric, on entry it should contain only
-- upper triangular part. This matrix remains unchanged on exit.
--
-- The resultant matrix U should not coincide with the matrix A. Order
-- of U should be equal to order of A. On entry distribution of non-zero
-- elements of U should be correct, however, numeric values of elements
-- are not essential. On exit the matrix U has the same pattern (some
-- its elements may be equal to zero because of numeric cancellation,
-- i.e. this matrix may contain explicit zeros).
--
-- Auxiliary arrays head and work should contain at least 1+n elements,
-- where n is order of the matrices A and U. If some of these parameters
-- is NULL, the chol_numb routine automatically allocates and frees the
-- corresponding working array(s).
--
-- *Returns*
--
-- The routine returns number of non-positive or very small diagonal
-- elements of the matrix U, which were replaced by huge positive number
-- (see the method description below). Zero return code means that the
-- input matrix is not very ill-conditioned.
--
-- *Method*
--
-- The chol_numb routine computes the matrix U in row-wise manner (see
-- also the description of the chol_symb routine). Before computing k-th
-- row the matrix U is the following:
--
--    1       k         n
-- 1  + + + + x x x x x x
--    . + + + x x x x x x
--    . . + + x x x x x x
--    . . . + x x x x x x
-- k  . . . . * * * * * *
--    . . . . . * * * * *
--    . . . . . . * * * *
--    . . . . . . . * * *
--    . . . . . . . . * *
-- n  . . . . . . . . . *
--
-- where '+' and 'x' denotes rows, which have been computed; '*' denotes
-- rows, which have been not processed yet.
--
-- It is well known that in order to numerically compute k-th row of the
-- matrix U it is necessary to copy to that row k-th row of the original
-- matrix A and subtract from it each i-th row of the matrix U (where
-- i = 1, 2, ..., k-1), multiplied by gaussian multiplier u[i,k] (if, of
-- course, u[i,k] is non-zero). Since U is upper triangular, we should
-- skip those elements of each i-th row, which are placed to the right
-- of the main diagonal.
--
-- In order to avoid scanning skipped elements of i-th row each time
-- when the row is processed, the routine uses ordered representation
-- of the matrix U, i.e. all elements of each row of U are ordered in
-- increasing their column indices (the routine automatically performs
-- ordering, so on entry the matrix U may be unordered). On k-th step
-- of the main loop the location head[i] (i = 1, 2, ..., k-1) points to
-- the sublist of elements of i-th row, whose column indices are not
-- less than k, because only these elements are needed in subsequent
-- operations (elements that are removed from sublists are marked on
-- the figure above by '+'; active elements that belong to sublists yet
-- are marked by '*'). Immediately after i-th row has been subtracted
-- from k-th row (i.e. u[i,k] != 0) the routine shifts the pointer
-- head[i] to the next row element, that automatically removes u[i,k]
-- from the sublist. This technique allows significantly improving the
-- performance.
--
-- If the input matrix A (which is assumed to be positive definite) is
-- close to semidefinite and therefore ill-conditioned, it may happen
-- that on some elimination step diagonal element u[k,k] is non-positive
-- or very small. So far as the routine doesn't use pivoting, it can't
-- permute rows and columns of U in order to choose more appropriate
-- diagonal element. Therefore the routine uses a technique proposed in
-- the paper:
--
-- S.J.Wright. The Cholesky factorization in interior-point and barrier
-- methods. Preprint MCS-P600-0596, Mathematics and Computer Science
-- Division, Argonne National Laboratory, Argonne, Ill., May 1996.
--
-- The routine just replaces non-positive or very small pivot element
-- u[k,k] by huge positive number. Such replacement involves the
-- off-diagonal elements in the k-th column of the matrix U to be close
-- to zero and causes the k-th conponent of the solution vector also to
-- be close to zero. Of course, this technique works only in the case
-- when the system A*x = b is consistent. */

int chol_numb(MAT *A, MAT *U, void *_head[], double _work[])
{     ELEM *e, *ee, **head = (ELEM **)_head;
      int n = A->m, i, j, k, ret = 0;
      double big, ukk, *work = _work;
      if (A == U)
         fault("chol_numb: invalid specification of resultant matrix");
      if (!(U->m == n && U->n == n && A->n == n))
         fault("chol_numb: inconsistent dimension");
      /* allocate and clear auxiliary arrays */
      if (_head == NULL) head = ucalloc(1+n, sizeof(ELEM *));
      for (i = 1; i <= n; i++) head[i] = NULL;
      if (_work == NULL) work = ucalloc(1+n, sizeof(double));
      for (j = 1; j <= n; j++) work[j] = 0.0;
      /* element lists of the matrix U should be ordered by increasing
         column indices */
      sort_mat(U);
      /* determine maximal absolute value of diagonal elements of the
         matrix A */
      big = DBL_EPSILON;
      for (i = 1; i <= n; i++)
      {  for (e = A->row[i]; e != NULL; e = e->row)
         {  if (e->i == e->j)
            {  if (big < fabs(e->val)) big = fabs(e->val);
            }
         }
      }
      /* main loop */
      for (k = 1; k <= n; k++)
      {  /* compute k-th row of the matrix U */
         /* work := k-th row of the matrix A */
         for (e = A->row[k]; e != NULL; e = e->row) work[e->j] = e->val;
         /* non-zeros in k-th column of U (except diagonal element)
            determine rows of U, which should be subtracted from k-th
            row */
         for (e = U->col[k]; e != NULL; e = e->col)
         {  i = e->i;
            insist(i <= k);
            if (i == k) continue; /* skip diagonal element u[k,k] */
            /* u[i,k] != 0, therefore it should be the first element in
               the sublist of i-th row */
            insist(head[i] != NULL && head[i]->j == k);
            /* work := work - f * (i-th row of U), where f = u[i,k] is
               gaussian multiplier */
            for (ee = head[i]; ee != NULL; ee = ee->row)
               work[ee->j] -= e->val * ee->val;
            /* remove u[i,k] from the sublist of i-th row */
            head[i] = head[i]->row;
         }
         /* determine pivot element u[k,k] */
         ukk = work[k];
         /* if the matrix A is close to semidefinite and therefore
            ill-conditioned, it may happen that u[k,k] is non-positive
            or very small due to round-off errors; in this case such
            pivot element is just replaced by huge positive number */
         if (ukk < (DBL_EPSILON * DBL_EPSILON) * big)
         {  ret++;
            ukk = work[k] = 0.1 * DBL_MAX;
         }
         /* now the array work contains elements of k-th row of the
            matrix U; however, k-th row should be normalized by dividing
            all its elements on sqrt(u[k,k]) */
         ukk = sqrt(ukk);
         for (e = U->row[k]; e != NULL; e = e->row)
            e->val = work[e->j] / ukk, work[e->j] = 0.0;
         /* if the pattern of U is correct, all elements of the array
            work now should be equal to zero */
         /* initialize the pointer to the sublist of k-th row (each row
            list starts from the diagonal element u[k,k], therefore this
            element should be skipped before) */
         insist(U->row[k] != NULL && U->row[k]->j == k);
         head[k] = U->row[k]->row;
      }
      /* free auxiliary arrays */
      if (_head == NULL) ufree(head);
      if (_work == NULL) ufree(work);
      /* return to the calling program */
      return ret;
}

/*----------------------------------------------------------------------
-- min_deg - minimum degree ordering.
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- void min_deg(MAT *A, PER *P);
--
-- *Description*
--
-- The min_deg routine uses minimum degree ordering in order to find
-- such permutation matrix P for the given sparse symmetric (positive
-- definite) matrix A, which minimizes number of non-zeros of the upper
-- triangular matrix U for Cholesky factorization P*A*P' = U'*U.
--
-- So far as the matrix A is symmetric, on entry it should contain only
-- upper triangular part. Only pattern of A is used, i.e. numeric values
-- of elements of the matrix A are ignored (they all are considered as
-- non-zeros). The matrix A is unchanged on exit.
--
-- Order of the resultant permutation matrix P should be equal to order
-- of the matrix A. On entry the initial contents of P is ignored.
--
-- *Method*
--
-- The min_deg routine is based on some subroutines from the SPARSPAK
-- package described in the book:
--
-- Alan George, Joseph W-H Liu. Computer Solution of Large Sparse
-- Positive Definite Systems. Prentice-Hall, 1981.
--
-- See the acknowledgements in comments to the routines genqmd, qmdrch,
-- qmdqt, qmdupd, and qmdmrg in the module GLPQMD. */

void min_deg(MAT *A, PER *P)
{     ELEM *e;
      int n = A->m, ne, ke, k;
      int *xadj, *adjncy, *deg, *marker, *rchset, *nbrhd, *qsize,
         *qlink, nofsub;
      if (!(A->n == n && P->n == n))
         fault("min_deg: inconsistent dimension");
      /* determine the length of the array adjncy */
      ne = 0;
      for (k = 1; k <= n; k++)
      {  for (e = A->row[k]; e != NULL; e = e->row)
         {  if (e->i > e->j)
               fault("min_deg: input matrix is not upper triangular");
            if (e->i != e->j) ne++;
         }
         for (e = A->col[k]; e != NULL; e = e->col)
            if (e->i != e->j) ne++;
      }
      /* allocate working storage */
      xadj = ucalloc(1+n+1, sizeof(int));
      adjncy = ucalloc(1+ne, sizeof(int));
      deg = ucalloc(1+n, sizeof(int));
      marker = ucalloc(1+n, sizeof(int));
      rchset = ucalloc(1+n, sizeof(int));
      nbrhd = ucalloc(1+n, sizeof(int));
      qsize = ucalloc(1+n, sizeof(int));
      qlink = ucalloc(1+n, sizeof(int));
      /* build adjacency structure (xadj, adjncy) */
      ke = 1;
      for (k = 1; k <= n; k++)
      {  xadj[k] = ke;
         for (e = A->row[k]; e != NULL; e = e->row)
            if (e->i != e->j) adjncy[ke] = e->j, ke++;
         for (e = A->col[k]; e != NULL; e = e->col)
            if (e->i != e->j) adjncy[ke] = e->i, ke++;
      }
      xadj[n+1] = ke;
      /* call the main minimimum degree ordering routine */
      genqmd(&n, xadj, adjncy, P->row, P->col, deg, marker, rchset,
         nbrhd, qsize, qlink, &nofsub);
      /* check the found permutation matrix P for correctness */
      check_per(P);
      /* free working storage */
      ufree(xadj);
      ufree(adjncy);
      ufree(deg);
      ufree(marker);
      ufree(rchset);
      ufree(nbrhd);
      ufree(qsize);
      ufree(qlink);
      /* return to the calling program */
      return;
}

/*----------------------------------------------------------------------
-- create_chol - create Cholesky factorization.
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- CHOL *create_chol(MAT *A);
--
-- *Description*
--
-- The create_chol routine creates Cholesky factorization for the given
-- symmetric positive definite matrix A. This operation includes only
-- symbolic phase of factorization, therefore only pattern of A is used,
-- i.e. numeric values of elements of A are ignored (all its elements
-- are considered as non-zeros).
--
-- *Returns*
--
-- The create_chol routine returns a pointer to the data structure that
-- represents Cholesky factorization. */

CHOL *create_chol(MAT *A)
{     CHOL *chol;
      int n = A->m;
      if (A->m != A->n)
         fault("create_chol: invalid matrix");
      chol = umalloc(sizeof(CHOL));
      chol->n = n;
      chol->P = create_per(n);
      chol->U = create_mat(n, n);
      chol->sing = -1;
      /* determine permutation matrix P, which minimizes number of
         non-zeros in the matrix U */
      min_deg(A, chol->P);
      /* compute permuted matrix A~ = P*A*P' */
      per_sym(chol->P, A, NULL);
      /* compute symbolic Cholesky factorization A~ = U'*U */
      chol_symb(A, chol->U, NULL, NULL, NULL);
      /* restore original matrix A = P'*A~*P */
      inv_per(chol->P);
      per_sym(chol->P, A, NULL);
      inv_per(chol->P);
      return chol;
}

/*----------------------------------------------------------------------
-- decomp_chol - compute Cholesky factorization.
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- void decomp_chol(CHOL *chol, MAT *A);
--
-- *Description*
--
-- The decomp_chol routine performs numeric phase of Cholesky
-- factorization for the given symmetric positive definite matrix A.
-- The matrix A passed to the decomp_chol routine should have exactly
-- the same pattern as the matrix passed to the create_chol routine.
--
-- For details see comments to the chol_numb routine. */

void decomp_chol(CHOL *chol, MAT *A)
{     /* compute permuted matrix A~ = P*A*P' */
      per_sym(chol->P, A, NULL);
      /* compute numeric Cholesky factorization A~ = U'*U */
      chol->sing = chol_numb(A, chol->U, NULL, NULL);
      /* restore original matrix A = P'*A~*P */
      inv_per(chol->P);
      per_sym(chol->P, A, NULL);
      inv_per(chol->P);
      return;
}

/*----------------------------------------------------------------------
-- solve_chol - solve linear system using Cholesky factorization.
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- void solve_chol(CHOL *chol, double x[], double work[]);
--
-- *Description*
--
-- The solve_chol routine obtains solution of the linear system A*x = b,
-- where A is symmetric positive definite coefficient matrix, x is dense
-- vector of unknowns, b is dense vector of right-hand sides.
--
-- The parameter chol should specify Cholseky factorization of the
-- coefficient matrix A computed by means of the decomp_chol routine.
-- If the decomp_chol routine detected that A is close to semidefinite
-- (i.e. chol->sing > 0), the system A*x = b should be consistent.
--
-- On entry the array x should contain elements of the vector b in
-- locations x[1], x[2], ..., x[n], where n is order of the system. On
-- exit this array will contain the vector x in the same locations.
--
-- The auxiliary array work should contain at least 1+n elements. If
-- this parameter is NULL, the routine automatically allocates and frees
-- the working array. */

void solve_chol(CHOL *chol, double x[], double _work[])
{     int n = chol->n;
      double *work = _work;
      if (_work == NULL) work = ucalloc(1+n, sizeof(double));
      /* P*A*P' = U'*U; A = P'*U'*U*P; inv(A) = P'*inv(U)*inv(U')*P */
      per_vec(work, chol->P, x);
      ut_solve(chol->U, work);
      u_solve(chol->U, work);
      iper_vec(x, chol->P, work);
      if (_work == NULL) ufree(work);
      return;
}

/*----------------------------------------------------------------------
-- delete_chol - delete Cholesky factorization.
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- void delete_chol(CHOL *chol);
--
-- *Description*
--
-- The delete_chol routine deletes the Cholesky factorization, which
-- chol points to, freeing all memory allocated to this object. */

void delete_chol(CHOL *chol)
{     delete_per(chol->P);
      delete_mat(chol->U);
      ufree(chol);
      return;
}

/*----------------------------------------------------------------------
-- create_adat - create Cholesky factorization for S = A*D*A'.
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- ADAT *create_adat(MAT *A);
--
-- *Description*
--
-- The create_adat routine creates Cholesky factorization for symmetric
-- positive definite matrix S = A*D*A', where A is rectangular matrix of
-- full rank, D is diagonal non-singular matrix. This operation include
-- only symbolic phase of factorization, therefore only pattern of A is
-- used, i.e. numeric values of elements of A as well as matrix D are
-- ignored.
--
-- Note that this routine doesn't include dense columns processing.
--
-- *Returns*
--
-- The create_adat routine returns a pointer to the data structure that
-- represents Cholesky factorization. */

ADAT *create_adat(MAT *A)
{     ADAT *adat;
      int m = A->m, n = A->n;
      adat = umalloc(sizeof(ADAT));
      adat->m = m;
      adat->n = n;
      adat->S = create_mat(m, m);
      aat_symb(adat->S, A, NULL);
      adat->chol = create_chol(adat->S);
      return adat;
}

/*----------------------------------------------------------------------
-- decomp_adat - compute Cholesky factorization for S = A*D*A'.
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- void decomp_adat(ADAT *adat, MAT *A, double D[]);
--
-- *Description*
--
-- The decomp_adat routine performs numeric phase of Cholesky
-- factorization for symmetric positive definite matrix S = A*D*A',
-- where A is rectangular matrix of full rank, D is diagonal matrix.
--
-- The matrix A passed to decomp_adat routine should have exactly the
-- same pattern as the matrix passed to create_adat routine. The matrix
-- A remains unchanged on exit.
--
-- The array D specifies diagonal matrix D of order n, where n is number
-- of columns of the matrix A. Diagonal elements of D should be placed
-- in locations D[1], D[2], ..., D[n]. If the parameter D is NULL, it is
-- assumed that D is unity matrix. */

void decomp_adat(ADAT *adat, MAT *A, double D[])
{     aat_numb(adat->S, A, D, NULL);
      decomp_chol(adat->chol, adat->S);
      return;
}

/*----------------------------------------------------------------------
-- solve_adat - solve system A*D*A'*x = b using Cholesky factorization.
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- void solve_adat(ADAT *adat, double x[], double work[]);
--
-- *Description*
--
-- The solve_adat routine obtains solution of the linear system S*x = b,
-- where S = A*D*A' is symmetric positive definite coefficient matrix,
-- A is rectangular matrix of full rank, D is diagonal matrix (assumed
-- to be non-singular), x is dense vector of unknowns, b is dense vector
-- of right-hand sides.
--
-- The parameter adat should specify Cholseky factorization of the
-- coefficient matrix S computed by means of the decomp_adat routine.
--
-- On entry the array x should contain elements of the vector b in
-- locations x[1], x[2], ..., x[m], where m is order of the system. On
-- exit this array will contain the vector x in the same locations.
--
-- The auxiliary array work should contain at least 1+m elements. If
-- this parameter is NULL, the routine automatically allocates and frees
-- the working array. */

void solve_adat(ADAT *adat, double x[], double work[])
{     solve_chol(adat->chol, x, work);
      return;
}

/*----------------------------------------------------------------------
-- delete_adat - delete Cholesky factorization for S = A*D*A'.
--
-- *Synopsis*
--
-- #include "glpchol.h"
-- void delete_adat(ADAT *adat);
--
-- *Description*
--
-- The delete_adat routine deletes the Cholesky factorization, which
-- adat points to, freeing all memory allocated to this object. */

void delete_adat(ADAT *adat)
{     delete_mat(adat->S);
      delete_chol(adat->chol);
      ufree(adat);
      return;
}

/* eof */
