/* glpmat.c */

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
#include "glplib.h"
#include "glpmat.h"

/*----------------------------------------------------------------------
-- aat_numb - compute matrix product S = A * D * A'; numeric phase
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *aat_numb(MAT *S, MAT *A, double D[], double work[]);
--
-- *Description*
--
-- The aat_numb computes the matrix product S = A * D * A' numerically,
-- assuming that the pattern of the matrix S (i.e. distribution of its
-- non-zero elements) is known. (Symbolic phase of this operation can
-- be performed by means of the aat_symb routine.)
--
-- Input matrix A remains unchanged on exit.
--
-- The array D specifies diagonal matrix D of order n, where n is number
-- of columns of the matrix A. Diagonal elements of D should be placed
-- in locations D[1], D[2], ..., D[n]. If the parameter D is NULL, it is
-- assumed that D is unity matrix.
--
-- The resultant matrix S should not coincide with the matrix A. Order
-- of S should be equal number of rows of A. On entry the distribution
-- of non-zero elements of S should be correct, however, numeric values
-- of elements are not essential (note that the aat_symb routine stores
-- only upper triangular part of S, because it is a symmetric matrix).
-- On exit the matrix S has the same pattern (some its elements may be
-- equal to zero because of numeric cancellation, i.e. this matrix may
-- contain explicit zeros).
--
-- The auxiliary array work should contain at least 1+n elements, where
-- n is number of columns of the matrix A. If this parameter is NULL,
-- the aat_numb routine automatically allocates and frees the working
-- array.
--
-- *Returns*
--
-- The aat_numb routine returns a pointer to the matrix S. */

MAT *aat_numb(MAT *S, MAT *A, double D[], double _work[])
{     ELEM *e, *ee;
      int i, j;
      double *work = _work;
      if (S == A)
         fault("aat_numb: invalid specification of resultant matrix");
      if (!(S->m == S->n && S->m == A->m))
         fault("aat_numb: inconsistent dimension; product undefined");
      if (_work == NULL) work = ucalloc(1+A->n, sizeof(double));
      for (j = 1; j <= A->n; j++) work[j] = 0.0;
      for (i = 1; i <= S->m; i++)
      {  /* work := i-th row of A */
         for (e = A->row[i]; e != NULL; e = e->row) work[e->j] = e->val;
         /* compute i-th row of S */
         for (e = S->row[i]; e != NULL; e = e->row)
         {  /* s[i,j] = a[i,*] * a[j,*] */
            double sum = 0.0;
            if (D == NULL)
            {  for (ee = A->row[e->j]; ee != NULL; ee = ee->row)
                  sum += work[ee->j] * ee->val;
            }
            else
            {  for (ee = A->row[e->j]; ee != NULL; ee = ee->row)
                  sum += work[ee->j] * D[ee->j] * ee->val;
            }
            e->val = sum;
         }
         /* clear working array */
         for (e = A->row[i]; e != NULL; e = e->row) work[e->j] = 0.0;
      }
      if (_work == NULL) ufree(work);
      return S;
}

/*----------------------------------------------------------------------
-- aat_symb - compute matrix product S = A * A'; symbolic phase
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *aat_symb(MAT *S, MAT *A, char work[]);
--
-- *Description*
--
-- The aat_symb computes the matrix product S = A * A' symbolically,
-- i.e. it only determines distribution of non-zeros of the matrix S not
-- computing their numeric values.
--
-- Input matrix A remains unchanged on exit. Numeric values of elements
-- of this matrix are ignored (they all are considered as non-zeros).
--
-- The resultant matrix S should not coincide with the matrix A. Order
-- of S should be equal number of rows of A. On entry the contents of S
-- is ignored. So far as S is symmetric, the routine computes only upper
-- triangular part of the matrix S.
--
-- The auxiliary array work should contain at least 1+m elements, where
-- m is order of S (number of rows of A). If this parameter is NULL, the
-- routine automatically allocates and frees the working array.
--
-- *Returns*
--
-- The aat_symb routine returns a pointer to the matrix S. */

MAT *aat_symb(MAT *S, MAT *A, char _work[])
{     ELEM *e, *ee;
      int i, j, k;
      char *work = _work;
      if (S == A)
         fault("aat_symb: invalid specification of resultant matrix");
      if (!(S->m == S->n && S->m == A->m))
         fault("aat_symb: inconsistent dimension; product undefined");
      clear_mat(S);
      if (_work == NULL) work = ucalloc(1+S->n, sizeof(char));
      for (j = 1; j <= S->n; j++) work[j] = 0;
      for (i = 1; i <= S->m; i++)
      {  /* compute i-th row of S */
         for (e = A->row[i]; e != NULL; e = e->row)
         {  k = e->j;
            for (ee = A->col[k]; ee != NULL; ee = ee->col)
            {  j = ee->i;
               /* a[i,k] != 0 and a[j,k] != 0 therefore s[i,j] != 0 */
               if (i > j) continue; /* skip subdiagonal element */
               if (work[j] == 0) new_elem(S, i, j, 1.0), work[j] = 1;
            }
         }
         /* clear working array */
         for (e = S->row[i]; e != NULL; e = e->row) work[e->j] = 0;
      }
      if (_work == NULL) ufree(work);
      return S;
}

/*----------------------------------------------------------------------
-- append_lines() -- append new rows and columns to sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *append_lines(MAT *A, int dm, int dn);
--
-- *Description*
--
-- The routine append_lines() appends dm empty rows and dn empty columns
-- to the sparse matrix A.
--
-- The new rows have indices m+1, ..., m+dn, where m is the number of
-- rows in the matrix A before addition. Analogously, the new columns
-- have indices n+1, ..., n+dn, where n is the number of columns in the
-- matrix A before addition.
--
-- It is allowed to specifiy dm = 0 or/and dn = 0.
--
-- *Returns*
--
-- The routine append_lines() returns a pointer to the matrix A. */

MAT *append_lines(MAT *A, int dm, int dn)
{     ELEM **ptr;
      int i, j;
      if (!(dm >= 0 && dn >= 0))
         fault("append_lines: dm = %d, dn = %d; invalid parameters",
            dm, dn);
      /* add new rows to the end of the row list */
      if (A->m_max < A->m + dm)
      {  /* enlarge the row list */
         while (A->m_max < A->m + dm)
         {  A->m_max += A->m_max;
            insist(A->m_max > 0);
         }
         ptr = ucalloc(1+A->m_max, sizeof(ELEM *));
         for (i = 1; i <= A->m; i++) ptr[i] = A->row[i];
         ufree(A->row);
         A->row = ptr;
      }
      for (i = 1; i <= dm; i++)
      {  A->m++;
         A->row[A->m] = NULL;
      }
      /* add new columns to the end of the column list */
      if (A->n_max < A->n + dn)
      {  /* enlarge the column list */
         while (A->n_max < A->n + dn)
         {  A->n_max += A->n_max;
            insist(A->n_max > 0);
         }
         ptr = ucalloc(1+A->n_max, sizeof(ELEM *));
         for (j = 1; j <= A->n; j++) ptr[j] = A->col[j];
         ufree(A->col);
         A->col = ptr;
      }
      for (j = 1; j <= dn; j++)
      {  A->n++;
         A->col[A->n] = NULL;
      }
      return A;
}

/*----------------------------------------------------------------------
-- check_mat - check sparse matrix for correctness.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- void check_mat(MAT *A);
--
-- *Description*
--
-- The check_mat routine checks the representation of the sparse matrix
-- A for correctness. In the case of error the routine displays an error
-- message and terminates a program. Should note that the matrix is
-- considered as correct if it has multiplets (i.e. elements that have
-- identical row and column numbers) and/or zeros (i.e. elements that
-- exist and have zero value).
--
-- This operation is extremely ineffecient. */

void check_mat(MAT *A)
{     ELEM *e, *ee;
      int i, j, flag;
      if (!(A->m > 0 && A->n > 0))
         fault("check_mat: invalid dimension");
      for (i = 1; i <= A->m; i++)
      {  flag = 0;
         for (e = A->row[i]; e != NULL; e = e->row)
         {  if (flag && e == A->row[i])
               fault("check_mat: row list has a cycle");
            if (e->i != i)
               fault("check_mat: element has wrong row number");
            flag = 1;
         }
      }
      for (j = 1; j <= A->n; j++)
      {  flag = 0;
         for (e = A->col[j]; e != NULL; e = e->col)
         {  if (flag && e == A->col[j])
               fault("check_mat: column list has a cycle");
            if (e->j != j)
               fault("check_mat: element has wrong column number");
            flag = 1;
         }
      }
      for (i = 1; i <= A->m; i++)
      {  for (e = A->row[i]; e != NULL; e = e->row)
         {  for (ee = A->col[e->j]; ee != NULL; ee = ee->col)
               if (ee == e) break;
            if (ee == NULL)
               fault("check_mat: element not found in column list");
         }
      }
      for (j = 1; j <= A->n; j++)
      {  for (e = A->col[j]; e != NULL; e = e->col)
         {  for (ee = A->row[e->i]; ee != NULL; ee = ee->row)
               if (ee == e) break;
            if (ee == NULL)
               fault("check_mat: element not found in row list");
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- check_mplets - check whether sparse matrix has multiplets.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- ELEM *check_mplets(MAT *A);
--
-- *Description*
--
-- The check_mplets routine checks whether the sparse matrix A has any
-- multiplets, i.e. sets of elements which have identical row and column
-- indices.
--
-- *Returns*
--
-- If the matrix A has no multiplets, the check_mplets routine returns
-- NULL. Otherwise the routine returns a pointer to some element which
-- belongs to some multiplet. */

ELEM *check_mplets(MAT *A)
{     int i;
      sort_mat(A);
      for (i = 1; i <= A->m; i++)
      {  ELEM *e;
         for (e = A->row[i]; e != NULL; e = e->row)
            if (e->row != NULL && e->j == e->row->j) return e;
      }
      return NULL;
}

/*----------------------------------------------------------------------
-- check_per - check permutation matrix for correctness.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- void check_per(PER *P);
--
-- *Description*
--
-- The check_per routine checks the representation of the permutation
-- matrix P for correctness. In the case of error the routine displays
-- an error message and terminates a program. */

void check_per(PER *P)
{     int i;
      if (P->n < 1)
         fault("check_per: invalid order");
      for (i = 1; i <= P->n; i++)
      {  if (!(1 <= P->row[i] && P->row[i] <= P->n
            && P->col[P->row[i]] == i))
            fault("check_per: invalid representation");
      }
      return;
}

/*----------------------------------------------------------------------
-- clear_line - clear row or column of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *clear_line(MAT *A, int k);
--
-- *Description*
--
-- The clear_line routine performs one of the following:
--
-- If k > 0, the routine clears k-th row of the sparse matrix A.
--
-- If k < 0, the routine clears k-th column of the sparse matrix A.
--
-- If k = 0, the routine clears entire matrix A.
--
-- Should note that in any case this operation requires the time O(nz),
-- where nz is number of non-zeros of the matrix A.
--
-- *Returns*
--
-- The clear_line routine returns a pointer to the matrix A. */

MAT *clear_line(MAT *A, int k)
{     ELEM *e, *en; int i, j;
      if (k > 0)
      {  /* clear k-th row of the matrix */
         i = +k;
         if (!(1 <= i && i <= A->m))
            fault("clear_line: row number out of range");
         for (e = A->row[i]; e != NULL; e = en)
         {  en = e->row;
            if (A->col[e->j] == e)
               A->col[e->j] = e->col;
            else
            {  ELEM *f;
               for (f = A->col[e->j]; f != NULL; f = f->col)
                  if (f->col == e) break;
               insist(f != NULL);
               f->col = e->col;
            }
            dmp_free_atom(A->pool, e);
         }
         A->row[i] = NULL;
      }
      else if (k < 0)
      {  /* clear k-th column of the matrix */
         j = -k;
         if (!(1 <= j && j <= A->n))
            fault("clear_line: column number out of range");
         for (e = A->col[j]; e != NULL; e = en)
         {  en = e->col;
            if (A->row[e->i] == e)
               A->row[e->i] = e->row;
            else
            {  ELEM *f;
               for (f = A->row[e->i]; f != NULL; f = f->row)
                  if (f->row == e) break;
               insist(f != NULL);
               f->row = e->row;
            }
            dmp_free_atom(A->pool, e);
         }
         A->col[j] = NULL;
      }
      else /* k == 0 */
      {  /* clear entire matrix */
         clear_mat(A);
      }
      return A;
}

/*----------------------------------------------------------------------
-- clear_lines() -- clear rows and columns of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *clear_lines(MAT *A, int rs[], int cs[]);
--
-- *Description*
--
-- The routine clear_lines() erases elements in the rows and columns of
-- the sparse matrix, which belong to the specified subsets.
--
-- The array rs specifies a subset of rows and should have at least 1+m
-- locations, where m is number of rows in the matrix A. The location
-- rs[0] is not used. If rs[i] is non-zero, the routine erases elements
-- in the i-th row. Otherwise, if rs[i] is zero, the i-th row is not
-- changed.
--
-- The array cs specifies a subset of columns and should have at least
-- 1+n locations, where n is number of columns in the matrix A. The
-- location cs[0] is not used. If cs[j] is non-zero, the routine erases
-- elements in the j-th columns. Otherwise, if cs[j] is zero, the j-th
-- column is not changed.
--
-- It is allowed to specify rs = NULL or cs = NULL, in which case the
-- subset of rows or columns is assumed to be empty.
--
-- *Complexity*
--
-- Independently on how many rows and columns should be nullified the
-- time complexity is O(nz), where nz is the number of elements in the
-- matrix A before the operation.
--
-- *Returns*
--
-- The routine clear_lines() returns a pointer to the matrix A. */

MAT *clear_lines(MAT *A, int rs[], int cs[])
{     ELEM *ptr, *e;
      int m = A->m, n = A->n, i, j;
      /* clear rows */
      if (rs != NULL)
      {  for (i = 1; i <= m; i++)
            if (rs[i]) A->row[i] = NULL;
         for (j = 1; j <= n; j++)
         {  ptr = NULL;
            while (A->col[j] != NULL)
            {  e = A->col[j];
               A->col[j] = e->col;
               if (rs[e->i])
                  dmp_free_atom(A->pool, e);
               else
                  e->col = ptr, ptr = e;
            }
            A->col[j] = ptr;
         }
      }
      /* clear columns */
      if (cs != NULL)
      {  for (j = 1; j <= n; j++)
            if (cs[j]) A->col[j] = NULL;
         for (i = 1; i <= m; i++)
         {  ptr = NULL;
            while (A->row[i] != NULL)
            {  e = A->row[i];
               A->row[i] = e->row;
               if (cs[e->j])
                  dmp_free_atom(A->pool, e);
               else
                  e->row = ptr, ptr = e;
            }
            A->row[i] = ptr;
         }
      }
      return A;
}

/*----------------------------------------------------------------------
-- clear_mat - clear sparse matrix (A := 0).
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *clear_mat(MAT *A);
--
-- *Description*
--
-- The clear_mat routine clears the sparse matrix A, emptying its row
-- and column lists.
--
-- Should note that the memory allocated to elements of the matrix A
-- will not be freed. The routine just returns that memory to the memory
-- pool for further reusage.
--
-- *Returns*
--
-- The clear_mat routine returns a pointer to the matrix A. */

MAT *clear_mat(MAT *A)
{     int i, j;
      dmp_free_all(A->pool);
      for (i = 1; i <= A->m; i++) A->row[i] = NULL;
      for (j = 1; j <= A->n; j++) A->col[j] = NULL;
      return A;
}

/*----------------------------------------------------------------------
-- copy_mat - copy sparse matrix (B := A).
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *copy_mat(MAT *B, MAT *A);
--
-- *Description*
--
-- The copy_mat routine copies the sparse matrix A to the sparse matrix
-- B. Both matrix should have identical number of rows and columns. The
-- matrix A remains unchanged.
--
-- *Returns*
--
-- The copy_mat routine returns a pointer to the matrix B. */

MAT *copy_mat(MAT *B, MAT *A)
{     if (!(A->m == B->m && A->n == B->n))
         fault("copy_mat: inconsistent dimension");
      if (A != B)
      {  ELEM *e;
         int i;
         clear_mat(B);
         for (i = 1; i <= A->m; i++)
         {  for (e = A->row[i]; e != NULL; e = e->row)
               new_elem(B, e->i, e->j, e->val);
         }
      }
      return B;
}

/*----------------------------------------------------------------------
-- copy_per - copy permutation matrix (Q := P).
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- PER *copy_per(PER *Q, PER *P);
--
-- *Description*
--
-- The copy_per routine copies the permutation matrix P to the other
-- permutation matrix Q. Both matrices should have identical order. The
-- matrix P remains unchanged.
--
-- *Returns*
--
-- The copy_per routine returns a pointer to the matrix Q. */

PER *copy_per(PER *Q, PER *P)
{     if (P->n != Q->n)
         fault("copy_per: inconsistent order");
      if (P != Q)
      {  int k;
         for (k = 1; k <= P->n; k++)
            Q->row[k] = P->row[k], Q->col[k] = P->col[k];
      }
      return Q;
}

/*----------------------------------------------------------------------
-- count_nz - count non-zeros of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- int count_nz(MAT *A, int k);
--
-- *Description*
--
-- If k = 0, the count_nz routine counts non-zero elements of entire
-- matrix A.
--
-- If k > 0, the count_nz routine counts non-zero elements in k-th row
-- of the matrix A.
--
-- If k < 0, the count_nz routine counts non-zero elements in k-th
-- column of the matrix A.
--
-- *Returns*
--
-- The count_nz routine returns the counted number of non-zeros. */

int count_nz(MAT *A, int k)
{     ELEM *e;
      int i, j, nz = 0;
      if (k == 0)
      {  /* count non-zeros of entire matrix */
         for (i = 1; i <= A->m; i++)
            for (e = A->row[i]; e != NULL; e = e->row) nz++;
      }
      else if (k > 0)
      {  /* count non-zeros in k-th row */
         i = +k;
         if (!(1 <= i && i <= A->m))
            fault("count_nz: invalid row number");
         for (e = A->row[i]; e != NULL; e = e->row) nz++;
      }
      else
      {  /* count non-zeros in k-th column */
         j = -k;
         if (!(1 <= j && j <= A->n))
            fault("count_nz: invalid column number");
         for (e = A->col[j]; e != NULL; e = e->col) nz++;
      }
      return nz;
}

/*----------------------------------------------------------------------
-- create_mat() -- create sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *create_mat(int m, int n);
--
-- *Description*
--
-- The routine create_mat() creates a real sparse matrix, which has m
-- rows and n columns. Initially the created matrix is empty, i.e. has
-- no elements.
--
-- It is allowed to specify m = 0 and/or n = 0.
--
-- *Returns*
--
-- The routine create_mat() returns a pointer to the created matrix. */

MAT *create_mat(int m, int n)
{     MAT *A;
      int m_max = 100, n_max = 100, i, j;
      if (!(m >= 0 && n >= 0))
         fault("create_mat: m = %d, n = %d; invalid dimension", m, n);
      A = umalloc(sizeof(MAT));
      A->pool = dmp_create_pool(sizeof(ELEM));
      while (m_max < m) m_max += m_max, insist(m_max > 0);
      while (n_max < n) n_max += n_max, insist(n_max > 0);
      A->m_max = m_max;
      A->n_max = n_max;
      A->m = m;
      A->n = n;
      A->row = ucalloc(1+m_max, sizeof(ELEM *));
      A->col = ucalloc(1+n_max, sizeof(ELEM *));
      for (i = 1; i <= m; i++) A->row[i] = NULL;
      for (j = 1; j <= n; j++) A->col[j] = NULL;
      return A;
}

/*----------------------------------------------------------------------
-- create_per - create permutation matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- PER *create_per(int n);
--
-- *Description*
--
-- The create_per routine creates a permutation matrix of order n. The
-- created matrix initially is unity (identity) matrix.
--
-- *Returns*
--
-- The create_per returns a pointer to the created matrix. */

PER *create_per(int n)
{     PER *P;
      int k;
      if (n < 1)
         fault("create_per: invalid order");
      P = umalloc(sizeof(PER));
      P->n = n;
      P->row = ucalloc(1+n, sizeof(int));
      P->col = ucalloc(1+n, sizeof(int));
      for (k = 1; k <= n; k++) P->row[k] = P->col[k] = k;
      return P;
}

/*----------------------------------------------------------------------
-- delete_lines() -- delete rows and columns from sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *delete_lines(MAT *A, int rs[], int cs[]);
--
-- *Description*
--
-- The routine delete_lines() deletes the specified subsets of rows and
-- columns from the sparse matrix A.
--
-- The array rs specifies a subset of rows and should have at least 1+m
-- locations, where m is number of rows in the matrix A before the
-- operation. The location rs[0] is not used. If rs[i] is non-zero, the
-- deletes the i-th row from the matrix. Otherwise, if rs[i] is zero,
-- the i-th row is kept.
--
-- The array cs specifies a subset of columns and should have at least
-- 1+n locations, where n is number of columns in the matrix A before
-- the operation. The location cs[0] is not used. If cs[j] is non-zero,
-- the routine deletes the j-th column from the matrix. Otherwise, if
-- cs[j] is zero, the j-th column is kept.
--
-- It is allowed to specify rs = NULL or cs = NULL, in which case the
-- subset of rows or columns is assumed to be empty.
--
-- *Complexity*
--
-- Independently on how many rows and columns should be deleted the
-- time complexity is O(nz), where nz is the number of elements in the
-- matrix A before the operation.
--
-- *Returns*
--
-- The routine delete_lines() returns a pointer to the matrix A. */

MAT *delete_lines(MAT *A, int rs[], int cs[])
{     ELEM *e;
      int m, n, i, j;
      clear_lines(A, rs, cs);
      /* adjust the row list */
      if (rs != NULL)
      {  m = 0;
         for (i = 1; i <= A->m; i++)
         {  if (!rs[i])
            {  m++;
               A->row[m] = A->row[i];
               for (e = A->row[m]; e != NULL; e = e->row) e->i = m;
            }
         }
         A->m = m;
      }
      /* adjust the column list */
      if (cs != NULL)
      {  n = 0;
         for (j = 1; j <= A->n; j++)
         {  if (!cs[j])
            {  n++;
               A->col[n] = A->col[j];
               for (e = A->col[n]; e != NULL; e = e->col) e->j = n;
            }
         }
         A->n = n;
      }
      return A;
}

/*----------------------------------------------------------------------
-- delete_mat - delete sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- void delete_mat(MAT *A);
--
-- *Description*
--
-- The delete_mat routine deletes the matrix A freeing all memory
-- allocated to this object. */

void delete_mat(MAT *A)
{     dmp_delete_pool(A->pool);
      ufree(A->row);
      ufree(A->col);
      ufree(A);
      return;
}

/*----------------------------------------------------------------------
-- delete_per - delete permutation matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- void delete_per(PER *P);
--
-- *Description*
--
-- The delete_per routine deletes the permutation matrix P freeing all
-- memory allocated to this object. */

void delete_per(PER *P)
{     ufree(P->row);
      ufree(P->col);
      ufree(P);
      return;
}

/*----------------------------------------------------------------------
-- eq_scaling - implicit equilibration scaling.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- void eq_scaling(MAT *A, double R[], double S[], int ord);
--
-- *Description*
--
-- The eq_scaling routine performs implicit equilibration scaling of
-- the matrix R*A*S, where A is the given sparse matrix (that remains
-- unchanged on exit), R and S are the given diagonal scaling matrices.
-- The result of scaling is the matrix R'*A*S', where R' and S' are new
-- scaling matrices computed by the routine, which are placed in the
-- same array. Diagonal elements of the matrix R should be placed in
-- locations R[1], R[2], ..., R[m], where m is number of rows of the
-- matrix A. Diagonal elements of the matrix S should be placed in
-- locations S[1], S[2], ..., S[n], where n is number of columns of the
-- matrix A. Diagonal elements of the matrices R' and S' will be placed
-- in the same manner.
--
-- To perform equilibration scaling the routine divide all elements of
-- each row (column) to the largest absolute value of elements in the
-- corresponding row (column).
--
-- Before a call the matrices R and S should be defined (if the matrix
-- A is unscaled, R and S should be untity matrices). As a result of
-- scaling the routine computes new matrices R' and S', that define the
-- scaled matrix R'*A*S'. (Thus scaling is implicit, because the matrix
-- A remains unchanged.)
--
-- The parameter ord defines the order of scaling:
--
-- if ord = 0, at first rows, then columns;
-- if ord = 1, at first columns, then rows. */

static void scale_rows(MAT *A, double R[], double S[]);
static void scale_cols(MAT *A, double R[], double S[]);

void eq_scaling(MAT *A, double R[], double S[], int ord)
{     if (ord == 0)
      {  scale_rows(A, R, S);
         scale_cols(A, R, S);
      }
      else
      {  scale_cols(A, R, S);
         scale_rows(A, R, S);
      }
      return;
}

static void scale_rows(MAT *A, double R[], double S[])
{     /* this routine performs equilibration scaling of rows */
      ELEM *e;
      int i;
      double big, temp;
      for (i = 1; i <= A->m; i++)
      {  big = 0.0;
         for (e = A->row[i]; e != NULL; e = e->row)
         {  temp = fabs(R[e->i] * e->val * S[e->j]);
            if (big < temp) big = temp;
         }
         if (big != 0.0) R[i] /= big;
      }
      return;
}

static void scale_cols(MAT *A, double R[], double S[])
{     /* this routine performs equilibration scaling of columns */
      ELEM *e;
      int j;
      double big, temp;
      for (j = 1; j <= A->n; j++)
      {  big = 0.0;
         for (e = A->col[j]; e != NULL; e = e->col)
         {  temp = fabs(R[e->i] * e->val * S[e->j]);
            if (big < temp) big = temp;
         }
         if (big != 0.0) S[j] /= big;
      }
      return;
}

/*----------------------------------------------------------------------
-- gm_scaling - implicit geometric mean scaling.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- void gm_scaling(MAT *A, double R[], double S[], int ord, double eps,
--    int itmax);
--
-- *Description*
--
-- The gm_scaling routine performs implicit geometric mean scaling of
-- the matrix R*A*S, where A is the given sparse matrix (that remains
-- unchanged on exit), R and S are the given diagonal scaling matrices.
-- The result of scaling is the matrix R'*A*S', where R' and S' are new
-- scaling matrices computed by the routine, which are placed in the
-- same array. Diagonal elements of the matrix R should be placed in
-- locations R[1], R[2], ..., R[m], where m is number of rows of the
-- matrix A. Diagonal elements of the matrix S should be placed in
-- locations S[1], S[2], ..., S[n], where n is number of columns of the
-- matrix A. Diagonal elements of the matrices R' and S' will be placed
-- in the same manner.
--
-- To perform geometric mean scaling the gm_scaling routine divides all
-- elements of each row (column) by sqrt(beta/alfa), where alfa and beta
-- are, respectively, smallest and largest absolute values of non-zero
-- elements of the corresponding row (column). In order to improve the
-- scaling quality the routine performs row and columns scaling several
-- times.
--
-- Before a call the matrices R and S should be defined (if the matrix
-- A is unscaled, R and S should be untity matrices). As a result of
-- scaling the routine computes new matrices R' and S', that define the
-- scaled matrix R'*A*S'. (Thus scaling is implicit, because the matrix
-- A remains unchanged.)
--
-- The parameter ord defines the order of scaling:
--
-- if ord = 0, at first rows, then columns;
-- if ord = 1, at first columns, then rows.
--
-- The parameter eps > 0 is a criterion, that is used to decide when
-- the scaling process has to stop. The scaling process stops if the
-- condition t[k-1] - t[k] < eps * t[k-1] becomes true, where
-- t[k] = beta[k] / alfa[k] is the "quality" of scaling, alfa[k] and
-- beta[k] are, respectively, smallest and largest abolute values of
-- elements of the current matrix, k is number of iteration. The value
-- eps = 0.01-0.1 may be recommended for most cases.
--
-- The parameter itmax defines maximal number of scaling iterations.
-- Recommended value is itmax = 10-50.
--
-- On each iteration the gm_scaling routine prints the "quality" of
-- scaling (see above). */

static double ratio(MAT *A, double R[], double S[]);

#define scale_rows scale_rows1
#define scale_cols scale_cols1

static void scale_rows(MAT *A, double R[], double S[]);
static void scale_cols(MAT *A, double R[], double S[]);

void gm_scaling(MAT *A, double R[], double S[], int ord, double eps,
      int itmax)
{     int iter;
      double told, tnew;
      told = DBL_MAX;
      for (iter = 1; ; iter++)
      {  tnew = ratio(A, R, S);
         if (iter == 1)
            print("gm_scaling: max / min = %9.3e", tnew);
         if (told - tnew < eps * told || iter > itmax)
         {  print("gm_scaling: max / min = %9.3e", tnew);
            break;
         }
         told = tnew;
         if (ord == 0)
         {  scale_rows(A, R, S);
            scale_cols(A, R, S);
         }
         else
         {  scale_cols(A, R, S);
            scale_rows(A, R, S);
         }
      }
      return;
}

static double ratio(MAT *A, double R[], double S[])
{     /* this routine computes the "quality" of scaling, which is
         beta/alfa, where alfa and beta are, respectively, smallest and
         largest absolute values of elements of the current matrix */
      ELEM *e;
      int i;
      double alfa, beta, temp;
      alfa = DBL_MAX; beta = 0.0;
      for (i = 1; i <= A->m; i++)
      {  for (e = A->row[i]; e != NULL; e = e->row)
         {  temp = fabs(R[e->i] * e->val * S[e->j]);
            if (temp == 0.0) continue;
            if (alfa > temp) alfa = temp;
            if (beta < temp) beta = temp;
         }
      }
      temp = (beta == 0.0 ? 1.0 : beta / alfa);
      return temp;
}

static void scale_rows(MAT *A, double R[], double S[])
{     /* this routine performs geometric mean scaling of rows */
      ELEM *e;
      int i;
      double alfa, beta, temp;
      for (i = 1; i <= A->m; i++)
      {  alfa = DBL_MAX; beta = 0.0;
         for (e = A->row[i]; e != NULL; e = e->row)
         {  temp = fabs(R[e->i] * e->val * S[e->j]);
            if (temp == 0.0) continue;
            if (alfa > temp) alfa = temp;
            if (beta < temp) beta = temp;
         }
         if (beta != 0.0) R[i] /= sqrt(alfa * beta);
      }
      return;
}

static void scale_cols(MAT *A, double R[], double S[])
{     /* this routine performs geometric mean scaling of columns */
      ELEM *e;
      int j;
      double alfa, beta, temp;
      for (j = 1; j <= A->n; j++)
      {  alfa = DBL_MAX; beta = 0.0;
         for (e = A->col[j]; e != NULL; e = e->col)
         {  temp = fabs(R[e->i] * e->val * S[e->j]);
            if (temp == 0.0) continue;
            if (alfa > temp) alfa = temp;
            if (beta < temp) beta = temp;
         }
         if (beta != 0.0) S[j] /= sqrt(alfa * beta);
      }
      return;
}

/*----------------------------------------------------------------------
-- inv_per - invert permutation matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- PER *inv_per(PER *P);
--
-- *Description*
--
-- The inv_per routine inverts the permutation matrix P.
--
-- *Returns*
--
-- The inv_per routine returns a pointer to the matrix P. */

PER *inv_per(PER *P)
{     int *ptr;
      ptr = P->row, P->row = P->col, P->col = ptr;
      return P;
}

/*----------------------------------------------------------------------
-- iper_vec - permute vector elements in inverse order (y := P' * x).
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *iper_vec(double y[], PER *P, double x[]);
--
-- *Description*
--
-- The iper_vec routine computes the product y := P' * x, where P' is
-- a permutation matrix transposed (inverted) to the given permutation
-- matrix P, x and y are dense vectors. The matrix P and the vector x
-- remain unchanged. The input array x should contain vector elements
-- in x[1], x[2], ..., x[n], where n is the order of the matrix P. The
-- output array y will contain vector elements in the same locations.
-- The arrays x and y should not overlap each other.
--
-- *Returns*
--
-- The iper_vec routine returns a pointer to the array y. */

double *iper_vec(double y[], PER *P, double x[])
{     int j;
      for (j = 1; j <= P->n; j++) y[j] = x[P->col[j]];
      return y;
}

/*----------------------------------------------------------------------
-- l_solve - solve lower triangular system L*x = b.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *l_solve(MAT *L, double x[]);
--
-- *Description*
--
-- The l_solve routine solves the system L*x = b, where L is lower
-- triangular matrix, x is dense vector of unknowns, b is dense vector
-- of right-hand sides. The matrix L should be non-singular, i.e. all
-- its diagonal elements should be non-zeros. Before a call the array x
-- should contain elements of the vector b in locations x[1], x[2], ...,
-- x[n], where n is the order of the system. After a call this array
-- will contain the vector x in the same locations.
--
-- *Returns*
--
-- The l_solve routine returns a pointer to the array x. */

double *l_solve(MAT *L, double x[])
{     ELEM *e;
      int flag = 1, i;
      double piv;
      if (L->m != L->n)
         fault("l_solve: matrix is not square");
      for (i = 1; i <= L->m; i++)
      {  /* flag = 1 means that x[1] = ... = x[i-1] = 0; therefore if
            b[i] = 0 then x[i] = 0 */
         if (flag && x[i] == 0.0) continue;
         piv = 0.0;
         for (e = L->row[i]; e != NULL; e = e->row)
         {  if (e->j > i)
               fault("l_solve: matrix is not lower triangular");
            if (e->j == i)
               piv = e->val;
            else
               x[i] -= e->val * x[e->j];
         }
         if (piv == 0.0)
            fault("l_solve: matrix is singular");
         x[i] /= piv;
         if (x[i] != 0.0) flag = 0;
      }
      return x;
}

/*----------------------------------------------------------------------
-- lt_solve - solve transposed lower triangular system L'*x = b.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *lt_solve(MAT *L, double x[]);
--
-- *Description*
--
-- The lt_solve routine solves the system L'*x = b, where L' is a matrix
-- transposed to the given lower triangular matrix L, x is dense vector
-- of unknowns, b is dense vector of right-hand sides. The matrix L
-- should be non-singular, i.e. all its diagonal elements should be
-- non-zeros. Before a call the array x should contain elements of the
-- vector b in locations x[1], x[2], ..., x[n], where n is the order of
-- the system. After a call this array will contain the vector x in the
-- same locations.
--
-- *Returns*
--
-- The lt_solve routine returns a pointer to the array x. */

double *lt_solve(MAT *L, double x[])
{     ELEM *e;
      int flag = 1, i;
      double piv;
      if (L->m != L->n)
         fault("lt_solve: matrix is not square");
      for (i = L->m; i >= 1; i--)
      {  /* flag = 1 means that x[i+1] = ... = x[n] = 0; therefore if
            b[i] = 0 then x[i] = 0 */
         if (flag && x[i] == 0.0) continue;
         piv = 0.0;
         for (e = L->col[i]; e != NULL; e = e->col)
         {  if (e->i < i)
               fault("lt_solve: matrix is not lower triangular");
            if (e->i == i)
               piv = e->val;
            else
               x[i] -= e->val * x[e->i];
         }
         if (piv == 0.0)
            fault("lt_solve: matrix is singular");
         x[i] /= piv;
         if (x[i] != 0.0) flag = 0;
      }
      return x;
}

/*----------------------------------------------------------------------
-- mat_per - permute matrix columns (A := A * P)
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *mat_per(MAT *A, PER *P, void *work[]);
--
-- *Description*
--
-- The mat_per routine computes the product A := A * P, where A is a
-- sparse matrix of general kind, P is a permutation matrix. The matrix
-- P remains unchanged. The auxiliary array work should have at least
-- 1+n elements, where n is the number of columns of the matrix A (if
-- this parameter is NULL, the routine automatically allocates and frees
-- the working array).
--
-- *Returns*
--
-- The mat_per returns a pointer to the matrix A. */

MAT *mat_per(MAT *A, PER *P, void *_work[])
{     int j;
      void **work = _work;
      if (A->n != P->n)
         fault("mat_per: product undefined");
      if (_work == NULL) work = ucalloc(1+A->n, sizeof(void *));
      for (j = 1; j <= A->n; j++) work[j] = A->col[j];
      for (j = 1; j <= A->n; j++)
      {  ELEM *e;
         A->col[j] = work[P->col[j]];
         for (e = A->col[j]; e != NULL; e = e->col) e->j = j;
      }
      if (_work == NULL) ufree(work);
      return A;
}

/*----------------------------------------------------------------------
-- mat_vec - multiply matrix on vector (y := A * x)
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *mat_vec(double y[], MAT *A, double x[]);
--
-- *Description*
--
-- The mat_vec routine computes the product y := A * x, where A is a
-- sparse matrix of general kind, x and y are dense vectors. The matrix
-- A and the vector x remain unchanged. The input array x should contain
-- vector elements in x[1], x[2], ..., x[n], where n is the number of
-- columns of the matrix A. The output array y will contain vector
-- elements in y[1], y[2], ..., y[m], where m is the number of rows of
-- the matrix A. The arrays x and y should not overlap each other.
--
-- *Returns*
--
-- The mat_vec routine returns a pointer to the array y. */

double *mat_vec(double y[], MAT *A, double x[])
{     int i, j;
      for (i = 1; i <= A->m; i++) y[i] = 0.0;
      for (j = 1; j <= A->n; j++)
      {  double t = x[j];
         if (t != 0.0)
         {  ELEM *e;
            for (e = A->col[j]; e != NULL; e = e->col)
               y[e->i] += e->val * t;
         }
      }
      return y;
}

/*----------------------------------------------------------------------
-- mprd_numb - multiply matrices (C := A * B); numeric phase
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *mprd_numb(MAT *C, MAT *A, MAT *B, double _work[]);
--
-- *Description*
--
-- The mprd_numb routine computes the product C = A * B numerically,
-- assuming that the pattern of the matrix C (i.e. distribution of its
-- non-zero elements) is known. (Symbolic phase of this operation can
-- be performed by means of the mprd_symb routine.)
--
-- Input matrices A and B remain unchanged on exit. Number of columns of
-- the matrix A should be equal to number of rows of the matrix B.
--
-- The resultant matrix C should not coincide neither with matrix A nor
-- with matrix B. Number of rows of C should be equal number of rows of
-- A, number of columns of C should be equal number of columns of B. On
-- entry the distribution of non-zero elements of the matrix C should be
-- correct, however, numeric values of elements are not essential. On
-- exit the matrix C has the same pattern (note that some elements may
-- be equal to zero as result of numeric cancellation, i.e. the matrix C
-- may contain explicit zeros).
--
-- The auxiliary array work should contain at least 1+n elements, where
-- n is number of columns of the matrix A. If this parameter is NULL,
-- the mprd_numb routine automatically allocates and frees the working
-- array.
--
-- *Returns*
--
-- The mprd_symb routine returns a pointer to the matrix C. */

MAT *mprd_numb(MAT *C, MAT *A, MAT *B, double _work[])
{     ELEM *e, *ee;
      int i, j;
      double *work = _work;
      if (C == A || C == B)
         fault("mprd_numb: invalid specification of resultant matrix");
      if (!(C->m == A->m && A->n == B->m && B->n == C->n))
         fault("mprd_numb: inconsistent dimension; product undefined");
      if (_work == NULL) work = ucalloc(1+A->n, sizeof(double));
      for (j = 1; j <= A->n; j++) work[j] = 0;
      for (i = 1; i <= C->m; i++)
      {  /* work := i-th row of A */
         for (e = A->row[i]; e != NULL; e = e->row) work[e->j] = e->val;
         /* compute i-th row of C */
         for (e = C->row[i]; e != NULL; e = e->row)
         {  /* c[i,j] = a[i,*] * b[*,j] */
            double sum = 0.0;
            for (ee = B->col[e->j]; ee != NULL; ee = ee->col)
               sum += work[ee->i] * ee->val;
            e->val = sum;
         }
         /* clear working array */
         for (e = A->row[i]; e != NULL; e = e->row) work[e->j] = 0.0;
      }
      if (_work == NULL) ufree(work);
      return C;
}

/*----------------------------------------------------------------------
-- mprd_symb - multiply matrices (C := A * B); symbolic phase
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *mprd_symb(MAT *C, MAT *A, MAT *B, char work[]);
--
-- *Description*
--
-- The mprd_symb routine computes the product C = A * B symbolically,
-- i.e. it only determines distribution of non-zeros of the matrix C not
-- computing their numeric values.
--
-- Input matrices A and B remain unchanged on exit. Number of columns of
-- the matrix A should be equal to number of rows of the matrix B.
-- Numeric values of elements of these matrices are ignored (i.e. they
-- all are considered as non-zeros).
--
-- The resultant matrix C should not coincide neither with matrix A nor
-- with matrix B. Number of rows of C should be equal number of rows of
-- A, number of columns of C should be equal number of columns of B. On
-- entry the contents of the matrix C is ignored.
--
-- The auxiliary array work should contain at least 1+n elements, where
-- n is number of columns of the matrix C. If this parameter is NULL,
-- the mprd_symb routine automatically allocates and frees the working
-- array.
--
-- *Returns*
--
-- The mprd_symb routine returns a pointer to the matrix C. */

MAT *mprd_symb(MAT *C, MAT *A, MAT *B, char _work[])
{     ELEM *e, *ee;
      int i, j, k;
      char *work = _work;
      if (C == A || C == B)
         fault("mprd_symb: invalid specification of resultant matrix");
      if (!(C->m == A->m && A->n == B->m && B->n == C->n))
         fault("mprd_symb: inconsistent dimension; product undefined");
      clear_mat(C);
      if (_work == NULL) work = ucalloc(1+C->n, sizeof(char));
      for (j = 1; j <= C->n; j++) work[j] = 0;
      for (i = 1; i <= C->m; i++)
      {  /* compute i-th row of C */
         for (e = A->row[i]; e != NULL; e = e->row)
         {  k = e->j;
            for (ee = B->row[k]; ee != NULL; ee = ee->row)
            {  j = ee->j;
               /* a[i,k] != 0 and b[k,j] != 0 therefore c[i,j] != 0 */
               if (work[j] == 0) new_elem(C, i, j, 1.0), work[j] = 1;
            }
         }
         /* clear working array */
         for (e = C->row[i]; e != NULL; e = e->row) work[e->j] = 0;
      }
      if (_work == NULL) ufree(work);
      return C;
}

/*----------------------------------------------------------------------
-- new_elem - create new element of matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- ELEM *new_elem(MAT *A, int i, int j, double val);
--
-- *Description*
--
-- The new_elem routine creates a new element of the matrix A. This
-- element is placed on ith row and jth column of the matrix, and the
-- value val is assigned to the new element.
--
-- Note that the new_elem routine performs no check whether such
-- element already exists. The routine just inserts the new element to
-- the corresponding row and column linked lists, therefore it may
-- happen that the matrix will have multiplets (i.e. elements that have
-- identical row and column numbers) and/or zeros (i.e. elements that
-- exist and have zero value).
--
-- *Returns*
--
-- The new_elem routine returns a pointer to the created element. */

ELEM *new_elem(MAT *A, int i, int j, double val)
{     ELEM *e;
      if (!(1 <= i && i <= A->m && 1 <= j && j <= A->n))
         fault("new_elem: row or column number out of range");
      e = dmp_get_atom(A->pool);
      e->i = i; e->j = j; e->val = val;
      e->row = A->row[i]; e->col = A->col[j];
      A->row[i] = A->col[j] = e;
      return e;
}

/*----------------------------------------------------------------------
-- per_mat - permute matrix rows (A := P * A).
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *per_mat(PER *P, MAT *A, void *work[]);
--
-- *Description*
--
-- The per_mat routine computes the product A := P * A, where P is a
-- permutation matrix, A is a sparse matrix of general kind. The matrix
-- P remains unchanged. The auxiliary array work should have at least
-- 1+m elements, where m is the number of rows of the matrix A (if this
-- parameter is NULL, the routine automatically allocates and frees the
-- working array).
--
-- *Returns*
--
-- The per_mat returns a pointer to the matrix A. */

MAT *per_mat(PER *P, MAT *A, void *_work[])
{     int i;
      void **work = _work;
      if (P->n != A->m)
         fault("per_mat: product undefined");
      if (_work == NULL) work = ucalloc(1+A->m, sizeof(void *));
      for (i = 1; i <= A->m; i++) work[i] = A->row[i];
      for (i = 1; i <= A->m; i++)
      {  ELEM *e;
         A->row[i] = work[P->row[i]];
         for (e = A->row[i]; e != NULL; e = e->row) e->i = i;
      }
      if (_work == NULL) ufree(work);
      return A;
}

/*----------------------------------------------------------------------
-- per_sym - permute symmetric matrix (A := P*A*P').
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *per_sym(PER *P, MAT *A, void *work[]);
--
-- *Description*
--
-- The per_sym routine computes the product A := P*A*P', where P is a
-- permutation matrix, A is a symmetric matrix specified only by upper
-- triangular part; on exit the matrix A also contains upper triangle.
-- The matrix P remains unchanged on exit. The auxiliary array work
-- should have at least 1+n locations, where n is order of the matrix A
-- (if this parameter is NULL, the routine automatically allocates and
-- frees the working array).
--
-- *Returns*
--
-- The per_sym routine returns a pointer to the matrix A. */

MAT *per_sym(PER *P, MAT *A, void *work[])
{     ELEM *e;
      int i, j;
      if (!(P->n == A->m && P->n == A->n))
         fault("per_sym: product undefined");
      /* compute P*A*P' as if A would be upper triangular */
      per_mat(P, A, work);
      inv_per(P);
      mat_per(A, P, work);
      inv_per(P);
      /* now some elements of A may be in the lower triangular part, so
         they should be removed to the corresponding symmetric positions
         of the upper triangular part */
      for (j = 1; j <= A->n; j++) A->col[j] = NULL;
      for (i = 1; i <= A->m; i++)
      {  for (e = A->row[i]; e != NULL; e = e->row)
         {  j = e->j;
            if (i > j) e->i = j, e->j = i;
            e->col = A->col[e->j], A->col[e->j] = e;
         }
      }
      /* restore correct row lists */
      for (i = 1; i <= A->m; i++) A->row[i] = NULL;
      for (j = 1; j <= A->n; j++)
      {  for (e = A->col[j]; e != NULL; e = e->col)
            e->row = A->row[e->i], A->row[e->i] = e;
      }
      return A;
}

/*----------------------------------------------------------------------
-- per_vec - permute vector elements (y := P * x).
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *per_vec(double y[], PER *P, double x[]);
--
-- *Description*
--
-- The per_vec routine computes the product y := P * x, where P is a
-- permutation matrix, x and y are dense vectors. The matrix P and the
-- vector x remain unchanged. The input array x should contain vector
-- elements in x[1], x[2], ..., x[n], where n is the order of the matrix
-- P. The output array y will contain vector elements in the same array
-- locations. The arrays x and y should not overlap each other.
--
-- *Returns*
--
-- The per_vec routine returns a pointer to the array y. */

double *per_vec(double y[], PER *P, double x[])
{     int i;
      for (i = 1; i <= P->n; i++) y[i] = x[P->row[i]];
      return y;
}

/*----------------------------------------------------------------------
-- reset_per - reset permutation matrix (P := I).
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- PER *reset_per(PER *P);
--
-- *Description*
--
-- The reset_per routine resets the permutation matrix P, so the matrix
-- P becomes unity matrix.
--
-- *Returns*
--
-- The reset_per routine returns a pointer to the matrix P. */

PER *reset_per(PER *P)
{     int k;
      for (k = 1; k <= P->n; k++) P->row[k] = P->col[k] = k;
      return P;
}

/*----------------------------------------------------------------------
-- scrape_mat - remove tiny elements from sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- int scrape_mat(MAT *A, double eps);
--
-- *Description*
--
-- The scrape_mat routine removes from sparse matrix A all elements,
-- whose magnitudes are not greater than eps.
--
-- If the parameter eps is equal to 0.0, only zero elements will be
-- removed from the matrix.
--
-- *Returns*
--
-- The scrape_mat routine returns total number of elements which have
-- been removed from the matrix. */

int scrape_mat(MAT *A, double eps)
{     ELEM *e, *ee;
      int i, j, count = 0;
      /* nullify all tiny elements */
      for (i = 1; i <= A->m; i++)
      {  for (e = A->row[i]; e != NULL; e = e->row)
            if (fabs(e->val) < eps) e->val = 0.0;
      }
      /* remove zero elements from row linked lists */
      for (i = 1; i <= A->m; i++)
      {  ee = NULL;
         while (A->row[i] != NULL)
         {  e = A->row[i], A->row[i] = e->row;
            if (e->val == 0.0)
               count++;
            else
               e->row = ee, ee = e;
         }
         A->row[i] = ee;
      }
      /* remove zero elements from column linked lists */
      for (j = 1; j <= A->n; j++)
      {  ee = NULL;
         while (A->col[j] != NULL)
         {  e = A->col[j], A->col[j] = e->col;
            if (e->val == 0.0)
               dmp_free_atom(A->pool, e);
            else
               e->col = ee, ee = e;
         }
         A->col[j] = ee;
      }
      return count;
}

/*----------------------------------------------------------------------
-- sort_mat - ordering row and column lists of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *sort_mat(MAT *A);
--
-- *Description*
--
-- The sort_mat routine sorts row and column linked lists of the matrix
-- A, so in row lists all elements follow in the order of increasing
-- column numbers, and in column lists all elements follow in the order
-- of increasing row numbers.
--
-- *Returns*
--
-- The sort_mat routine returns a pointer to the matrix A. */

MAT *sort_mat(MAT *A)
{     ELEM *e;
      int i, j;
      /* sort elements in row lists */
      for (i = 1; i <= A->m; i++) A->row[i] = NULL;
      for (j = A->n; j >= 1; j--)
         for (e = A->col[j]; e != NULL; e = e->col)
            e->row = A->row[e->i], A->row[e->i] = e;
      /* sort elements in column lists */
      for (j = 1; j <= A->n; j++) A->col[j] = NULL;
      for (i = A->m; i >= 1; i--)
         for (e = A->row[i]; e != NULL; e = e->row)
            e->col = A->col[e->j], A->col[e->j] = e;
      return A;
}

/*----------------------------------------------------------------------
-- submatrix - copy submatrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *submatrix(MAT *B, MAT *A, int i1, int i2, int j1, int j2);
--
-- *Description*
--
-- The submatrix routine copies a submatrix of the sparse matrix A to
-- the sparse matrix B. The copied submatrix consists of rows i1 to i2
-- and column j1 to j2 of the matrix A. The matrix B should have m rows
-- and n columns, where m = i2 - i1 + 1 and n = j2 - j1 + 1. The matrix
-- A remains unchanged.
--
-- *Returns*
--
-- The submatrix routine returns a pointer to the matrix B. */

MAT *submatrix(MAT *B, MAT *A, int i1, int i2, int j1, int j2)
{     ELEM *e;
      int i, j;
      if (!(1 <= i1 && i1 <= i2 && i2 <= A->m))
         fault("submatrix: invalid row numbers");
      if (!(1 <= j1 && j1 <= j2 && j2 <= A->n))
         fault("submatrix: invalid column numbers");
      if (!(B->m == i2 - i1 + 1 && B->n == j2 - j1 + 1))
         fault("submatrix: invalid dimension of target matrix");
      clear_mat(B);
      for (i = i1; i <= i2; i++)
      {  for (e = A->row[i]; e != NULL; e = e->row)
         {  j = e->j;
            if (j1 <= j && j <= j2)
               new_elem(B, i - i1 + 1, j - j1 + 1, e->val);
         }
      }
      return B;
}

/*----------------------------------------------------------------------
-- sum_mplets - sum multiplets of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- int sum_mplets(MAT *A, double eps);
--
-- *Description*
--
-- The sum_mplets routine sums multuplets (i.e. elements which have
-- identical row and column indices) of the sparse matrix A in order to
-- remove all multiplets from the matrix replacing them by their sums.
--
-- The parameter eps is used to remove all zero and tiny elements which
-- might appear in the matrix as the result of summation. If eps = 0.0,
-- only zero elements (including multiplets) are removed.
--
-- *Returns*
--
-- The sum_mplets routine returns total number of elements which have
-- been removed from the matrix. */

int sum_mplets(MAT *A, double eps)
{     int i;
      sort_mat(A);
      for (i = 1; i <= A->m; i++)
      {  ELEM *ee = NULL, *e;
         /* ee points to the first element of multiplet; e points to
            the current element */
         for (e = A->row[i]; e != NULL; e = e->row)
         {  if (ee == NULL || ee->j < e->j)
               ee = e;
            else
            {  insist(ee != e && ee->j == e->j);
               ee->val += e->val, e->val = 0.0;
            }
         }
      }
      /* remove all zero and tiny elements */
      return scrape_mat(A, eps);
}

/*----------------------------------------------------------------------
-- sym_vec - multiply symmetric matrix on vector (y := A * x).
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *sym_vec(double y[], MAT *A, double x[]);
--
-- *Description*
--
-- The sym_vec routine computes the product y := A * x, where A is a
-- symmetric sparse matrix, x and y are dense vectors. The matrix A
-- should contain only upper triangular part. This matrix and the vector
-- x remain unchanged on exit. The input array x should contain vector
-- elements in x[1], x[2], ..., x[n], where n is order of A. The output
-- vector y will contain vector elements in y[1], y[2], ..., y[n]. The
-- arrays x and y should not overlap each other.
--
-- *Returns*
--
-- The sym_vec routine returns a pointer to the array y. */

double *sym_vec(double y[], MAT *A, double x[])
{     int i, j;
      if (A->m != A->n)
         fault("sym_vec: matrix is not square");
      for (i = 1; i <= A->m; i++) y[i] = 0.0;
      for (j = 1; j <= A->n; j++)
      {  double t = x[j];
         if (t != 0.0)
         {  ELEM *e;
            for (e = A->col[j]; e != NULL; e = e->col)
            {  if (e->i > e->j)
                  fault("sym_vec: matrix is not upper triangular");
               y[e->i] += e->val * t;
            }
            for (e = A->row[j]; e != NULL; e = e->row)
               if (e->i != e->j) y[e->j] += e->val * t;
         }
      }
      return y;
}

/*----------------------------------------------------------------------
-- test_mat_d - create test sparse matrix of D(n,c) class.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *test_mat_d(int n, int c);
--
-- *Description*
--
-- The test_mat_d routine creates a test sparse matrix of D(n,c) class
-- as described in the book: Ole 0sterby, Zahari Zlatev. Direct Methods
-- for Sparse Matrices. Springer-Verlag, 1983.
--
-- Matrix of D(n,c) class is a square matrix of order n. It has unit
-- diagonal, three codiagonals above main diagonal in the distance of c,
-- which are cyclically continued below main diagonal, and a triangle of
-- 10x10 elements in the upper right corner.
--
-- It is necessary that n >= 14 and 1 <= c <= n-13.
--
-- *Returns*
--
-- The test_mat_d routine returns a pointer to the created matrix. */

MAT *test_mat_d(int n, int c)
{     MAT *A;
      int i, j;
      if (!(n >= 14 && 1 <= c && c <= n-13))
         fault("test_mat_d: invalid parameters");
      A = create_mat(n, n);
      for (i = 1; i <= n; i++)
         new_elem(A, i, i, 1.0);
      for (i = 1; i <= n-c; i++)
         new_elem(A, i, i+c, (double)(i+1));
      for (i = n-c+1; i <= n; i++)
         new_elem(A, i, i-n+c, (double)(i+1));
      for (i = 1; i <= n-c-1; i++)
         new_elem(A, i, i+c+1, (double)(-i));
      for (i = n-c; i <= n; i++)
         new_elem(A, i, i-n+c+1, (double)(-i));
      for (i = 1; i <= n-c-2; i++)
         new_elem(A, i, i+c+2, 16.0);
      for (i = n-c-1; i <= n; i++)
         new_elem(A, i, i-n+c+2, 16.0);
      for (j = 1; j <= 10; j++)
      {  for (i = 1; i <= 11-j; i++)
            new_elem(A, i, n-11+i+j, 100.0*(double)j);
      }
      return A;
}

/*----------------------------------------------------------------------
-- test_mat_e - create test sparse matrix of E(n,c) class.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *test_mat_e(int n, int c);
--
-- *Description*
--
-- The test_mat_e routine creates a test sparse matrix of E(n,c) class
-- as described in the book: Ole 0sterby, Zahari Zlatev. Direct Methods
-- for Sparse Matrices. Springer-Verlag, 1983.
--
-- Matrix of E(n,c) class is a symmetric positive definite matrix of
-- order n. It has the number 4 on its main diagonal and the number -1
-- on its four codiagonals, two of which neighbour with main diagonal
-- and two others are away from main diagonal in the distance of c.
--
-- It is necessary that n >= 3 and 2 <= c <= n-1.
--
-- *Returns*
--
-- The test_mat_e routine returns a pointer to the created matrix. */

MAT *test_mat_e(int n, int c)
{     MAT *A;
      int i;
      if (!(n >= 3 && 2 <= c && c <= n-1))
         fault("test_mat_e: invalid parameters");
      A = create_mat(n, n);
      for (i = 1; i <= n; i++)
         new_elem(A, i, i, 4.0);
      for (i = 1; i <= n-1; i++)
      {  new_elem(A, i, i+1, -1.0);
         new_elem(A, i+1, i, -1.0);
      }
      for (i = 1; i <= n-c; i++)
      {  new_elem(A, i, i+c, -1.0);
         new_elem(A, i+c, i, -1.0);
      }
      return A;
}

/*----------------------------------------------------------------------
-- tmat_vec - multiply transposed matrix on vector (y := A' * x).
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *tmat_vec(double y[], MAT *A, double x[]);
--
-- *Description*
--
-- The tmat_vec routine computes the product y := A' * x, where A' is a
-- matrix transposed to the given sparse matrix A of general kind, x and
-- y are dense vectors. The matrix A and the vector x remains unchanged.
-- The input array x should contain vector elements in x[1], x[2], ...,
-- x[m], where m is the number of rows of the matrix A. The output array
-- y will contain vector elements in y[1], y[2], ..., y[n], where n is
-- the number of columns of the matrix A. The arrays x and y should not
-- overlap each other.
--
-- *Returns*
--
-- The tmat_vec routine returns a pointer to the array y. */

double *tmat_vec(double y[], MAT *A, double x[])
{     int i, j;
      for (j = 1; j <= A->n; j++) y[j] = 0.0;
      for (i = 1; i <= A->m; i++)
      {  double t = x[i];
         if (t != 0.0)
         {  ELEM *e;
            for (e = A->row[i]; e != NULL; e = e->row)
               y[e->j] += e->val * t;
         }
      }
      return y;
}

/*----------------------------------------------------------------------
-- trn_mat - transpose sparse matrix.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- MAT *trn_mat(MAT *A);
--
-- *Description*
--
-- The trn_mat routine transposes the matrix A.
--
-- *Returns*
--
-- The trn_mat routine returns a pointer to the matrix A. */

MAT *trn_mat(MAT *A)
{     ELEM **ptr, *e, *x;
      int i, t;
      t = A->m_max, A->m_max = A->n_max, A->n_max = t;
      t = A->m, A->m = A->n, A->n = t;
      ptr = A->row, A->row = A->col, A->col = ptr;
      for (i = 1; i <= A->m; i++)
      {  for (e = A->row[i]; e != NULL; e = e->row)
         {  t = e->i, e->i = e->j, e->j = t;
            x = e->row, e->row = e->col, e->col = x;
         }
      }
      return A;
}

/*----------------------------------------------------------------------
-- u_solve - solve upper triangular system U*x = b.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *u_solve(MAT *U, double x[]);
--
-- *Description*
--
-- The u_solve routine solves the system U*x = b, where U is upper
-- triangular matrix, x is dense vectors of unknowns, b is dense vector
-- of right-hand sides. The matrix U should be non-singular, i.e. all
-- its diagonal elements should be non-zeros. Before a call the array x
-- should contain elements of the vector b in locations x[1], x[2], ...,
-- x[n], where n is the order of the system. After a call this array
-- will contain the vector x in the same locations.
--
-- *Returns*
--
-- The u_solve routine returns a pointer to the array x. */

double *u_solve(MAT *U, double x[])
{     ELEM *e;
      int flag = 1, i;
      double piv;
      if (U->m != U->n)
         fault("u_solve: matrix is not square");
      for (i = U->m; i >= 1; i--)
      {  /* flag = 1 means that x[i+1] = ... = x[n] = 0; therefore if
            b[i] = 0 then x[i] = 0 */
         if (flag && x[i] == 0.0) continue;
         piv = 0.0;
         for (e = U->row[i]; e != NULL; e = e->row)
         {  if (e->j < i)
               fault("u_solve: matrix is not upper triangular");
            if (e->j == i)
               piv = e->val;
            else
               x[i] -= e->val * x[e->j];
         }
         if (piv == 0.0)
            fault("u_solve: matrix is singular");
         x[i] /= piv;
         if (x[i] != 0.0) flag = 0;
      }
      return x;
}

/*----------------------------------------------------------------------
-- ut_solve - solve transposed upper triangular system U'*x = b.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *ut_solve(MAT *U, double x[]);
--
-- *Description*
--
-- The ut_solve routine solves the system U'*x = b, where U' is a matrix
-- transposed to the given upper triangular matrix U, x is dense vector
-- of unknowns, b is dense vector of right-hand sides. The matrix U
-- should be non-singular, i.e. all its diagonal elements should be
-- non-zeros. Before a call the array x should contain elements of the
-- vector b in locations x[1], x[2], ..., x[n], where n is the order of
-- the system. After a call this array will contain the vector x in the
-- same locations.
--
-- *Returns*
--
-- The ut_solve routine returns a pointer to the array x. */

double *ut_solve(MAT *U, double x[])
{     ELEM *e;
      int flag = 1, i;
      double piv;
      if (U->m != U->n)
         fault("ut_solve: matrix is not square");
      for (i = 1; i <= U->m; i++)
      {  /* flag = 1 means that x[1] = ... = x[i-1] = 0; therefore if
            b[i] = 0 then x[i] = 0 */
         if (flag && x[i] == 0.0) continue;
         piv = 0.0;
         for (e = U->col[i]; e != NULL; e = e->col)
         {  if (e->i > i)
               fault("ut_solve: matrix is not upper triangular");
            if (e->i == i)
               piv = e->val;
            else
               x[i] -= e->val * x[e->i];
         }
         if (piv == 0.0)
            fault("ut_solve: matrix is singular");
         x[i] /= piv;
         if (x[i] != 0.0) flag = 0;
      }
      return x;
}

/*----------------------------------------------------------------------
-- v_solve - solve implicit upper triangular system V*x = b.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *v_solve(PER *P, MAT *V, PER *Q, double x[], double work[]);
--
-- *Description*
--
-- The v_solve routine solves the system V*x = b, where U = P*V*Q is
-- upper triangular matrix, V is the given sparse matrix, P and Q are
-- permutation matrices, x is dense vector of unknowns, b is dense
-- vector of right-hand sides. The matrix U should be non-singular, i.e.
-- all its diagonal elements should be non-zeros. On entry the array x
-- should contain elements of the vector b in locations x[1], x[2], ...,
-- x[n], where n is the order of the system. On exit this array will
-- contain the vector x in the same locations. The auxiliary array work
-- should have at least 1+n elements, where n is order of the system
-- (if this parameter is NULL, the routine automatically allocates and
-- frees the working array).
--
-- *Returns*
--
-- The v_solve routine returns a pointer to the array x. */

double *v_solve(PER *P, MAT *V, PER *Q, double x[], double _work[])
{     ELEM *e;
      int i, j, flag = 1;
      double uii, *work = _work;
      if (V->m != V->n)
         fault("v_solve: matrix is not square");
      if (!(P->n == V->m && Q->n == V->m))
         fault("v_solve: permutation matrices have invalid order");
      if (_work == NULL) work = ucalloc(1+V->m, sizeof(double));
      /* compute the vector b~ = P*b */
      for (i = 1; i <= V->m; i++) work[i] = x[i];
      per_vec(x, P, work);
      /* solve the system U*x~ = b~, where U = P*V*Q */
      for (i = V->m; i >= 1; i--)
      {  /* flag = 1 means that x~[i+1] = ... = x~[n] = 0; therefore if
            b~[i] = 0 then x~[i] = 0 */
         if (flag && x[i] == 0.0) continue;
         uii = 0.0;
         for (e = V->row[P->row[i]]; e != NULL; e = e->row)
         {  j = Q->row[e->j];
            if (j < i)
               fault("v_solve: matrix P*V*Q is not upper triangular");
            if (j == i)
               uii = e->val;
            else
               x[i] -= e->val * x[j];
         }
         if (uii == 0.0)
            fault("v_solve: matrix is singular");
         x[i] /= uii;
         if (x[i] != 0.0) flag = 0;
      }
      /* compute the vector x = Q*x~ */
      for (i = 1; i <= V->m; i++) work[i] = x[i];
      per_vec(x, Q, work);
      if (_work == NULL) ufree(work);
      return x;
}

/*----------------------------------------------------------------------
-- vt_solve - solve implicit transposed upper triangular system V'*x =b.
--
-- *Synopsis*
--
-- #include "glpmat.h"
-- double *vt_solve(PER *P, MAT *V, PER *Q, double x[], double work[]);
--
-- *Description*
--
-- The vt_solve routine solves the system V'*x = b, where V' is a
-- matrix transposed to the given sparse matrix V, U = P*V*Q is upper
-- triangular matrix, P and Q are permutation matrices, x is dense
-- vector of unknowns, b is dense vector of right-hand sides. The matrix
-- U should be non-singular, i.e. all its diagonal elements should be
-- non-zeros. On entry the array x should contain elements of the vector
-- b in locations x[1], x[2], ..., x[n]. On exit this array will contain
-- the vector x in the same locations. The auxiliary array work should
-- have at least 1+n elements, where n is order of the system (if this
-- parameter is NULL, the routine automatically allocates and frees the
-- working array).
--
-- *Returns*
--
-- The vt_solve routine returns a pointer to the array x. */

double *vt_solve(PER *P, MAT *V, PER *Q, double x[], double _work[])
{     ELEM *e;
      int i, j, flag = 1;
      double uii, *work = _work;
      if (V->m != V->n)
         fault("vt_solve: matrix is not square");
      if (!(P->n == V->m && Q->n == V->m))
         fault("vt_solve: permutation matrices have invalid order");
      if (_work == NULL) work = ucalloc(1+V->m, sizeof(double));
      /* compute the vector b~ = inv(Q)*b */
      for (i = 1; i <= V->m; i++) work[i] = x[i];
      iper_vec(x, Q, work);
      /* solve the system U'*x~ = b~, where U' is a matrix transposed
         to U = P*V*Q */
      for (i = 1; i <= V->m; i++)
      {  /* flag = 1 means that x~[1] = ... = x~[i-1] = 0; therefore if
            b~[i] = 0 then x~[i] = 0 */
         if (flag && x[i] == 0.0) continue;
         uii = 0.0;
         for (e = V->col[Q->col[i]]; e != NULL; e = e->col)
         {  j = P->col[e->i];
            if (j > i)
               fault("vt_solve: matrix P*V*Q is not upper triangular");
            if (j == i)
               uii = e->val;
            else
               x[i] -= e->val * x[j];
         }
         if (uii == 0.0)
            fault("vt_solve: matrix is singular");
         x[i] /= uii;
         if (x[i] != 0.0) flag = 0;
      }
      /* compute the vector x = inv(P)*x~ */
      for (i = 1; i <= V->m; i++) work[i] = x[i];
      iper_vec(x, P, work);
      if (_work == NULL) ufree(work);
      return x;
}

/* eof */
