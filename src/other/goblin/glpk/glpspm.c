/* glpspm.c */

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
#include "glpspm.h"

/*----------------------------------------------------------------------
-- spm_create - create sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- SPM *spm_create(void);
--
-- *Description*
--
-- The routine spm_create creates a new sparse matrix, which is empty,
-- i.e. has no rows and no columns.
--
-- *Returns*
--
-- The routine returns a pointer to the created matrix. */

SPM *spm_create(void)
{     int m_max = 50, n_max = 100, size = 500;
      SPM *A;
      A = umalloc(sizeof(SPM));
      A->m_max = m_max;
      A->n_max = n_max;
      A->m = A->n = 0;
      A->ptr = ucalloc(1+m_max+n_max, sizeof(int));
      A->len = ucalloc(1+m_max+n_max, sizeof(int));
      A->cap = ucalloc(1+m_max+n_max, sizeof(int));
      A->size = size;
      A->used = 0;
      A->ndx = ucalloc(1+size, sizeof(int));
      A->val = ucalloc(1+size, sizeof(double));
      A->head = A->tail = 0;
      A->prev = ucalloc(1+m_max+n_max, sizeof(int));
      A->next = ucalloc(1+m_max+n_max, sizeof(int));
      return A;
}

/*----------------------------------------------------------------------
-- spm_check_data - check sparse matrix data structure.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_check_data(SPM *A);
--
-- *Description*
--
-- The routine spm_check_data checks a data structure, which represents
-- the sparse matrix A, for correctness.
--
-- This routine is highly inefficient and intended only for debugging
-- purposes. */

void spm_check_data(SPM *A)
{     int m_max = A->m_max;
      int n_max = A->n_max;
      int m = A->m;
      int n = A->n;
      int *ptr = A->ptr;
      int *len = A->len;
      int *cap = A->cap;
      int size = A->size;
      int used = A->used;
      int *ndx = A->ndx;
      double *val = A->val;
      int head = A->head;
      int tail = A->tail;
      int *prev = A->prev;
      int *next = A->next;
      int i, i_beg, i_end, i_ptr, j, j_beg, j_end, j_ptr, k, kk, *flag;
      print("spm_check_data: checking sparse matrix data structure...");
      insist(m_max > 0);
      insist(n_max > 0);
      insist(0 <= m && m <= m_max);
      insist(0 <= n && n <= n_max);
      insist(size > 0);
      insist(0 <= used && used <= size);
      /* check rows */
      flag = ucalloc(1+n, sizeof(int));
      for (j = 1; j <= n; j++) flag[j] = 0;
      for (i = 1; i <= m; i++)
      {  i_beg = ptr[i];
         i_end = i_beg + len[i] - 1;
         insist(1 <= i_beg && i_beg <= i_end+1 && i_end <= used);
         insist(len[i] <= cap[i]);
         for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
         {  j = ndx[i_ptr];
            insist(1 <= j && j <= n);
            insist(!flag[j]);
            flag[j] = 1;
            insist(val[i_ptr] != 0.0);
            j_beg = ptr[m+j];
            j_end = j_beg + len[m+j] - 1;
            for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
               if (ndx[j_ptr] == i) break;
            insist(j_ptr <= j_end);
            insist(val[j_ptr] == val[i_ptr]);
         }
         for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
            flag[ndx[i_ptr]] = 0;
      }
      ufree(flag);
      /* check columns */
      flag = ucalloc(1+m, sizeof(int));
      for (i = 1; i <= m; i++) flag[i] = 0;
      for (j = 1; j <= n; j++)
      {  j_beg = ptr[m+j];
         j_end = j_beg + len[m+j] - 1;
         insist(1 <= j_beg && j_beg <= j_end+1 && j_end <= used);
         insist(len[m+j] <= cap[m+j]);
         for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
         {  i = ndx[j_ptr];
            insist(1 <= i && i <= m);
            insist(!flag[i]);
            flag[i] = 1;
            insist(val[j_ptr] != 0.0);
            i_beg = ptr[i];
            i_end = i_beg + len[i] - 1;
            for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
               if (ndx[i_ptr] == j) break;
            insist(i_ptr <= i_end);
            insist(val[i_ptr] == val[j_ptr]);
         }
         for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
            flag[ndx[j_ptr]] = 0;
      }
      ufree(flag);
      /* check linked list */
      flag = ucalloc(1+m+n, sizeof(int));
      for (k = 1; k <= m+n; k++) flag[k] = 0;
      if (head == 0) insist(tail == 0);
      for (k = head; k != 0; k = next[k])
      {  insist(1 <= k && k <= m+n);
         insist(!flag[k]);
         flag[k] = 1;
         kk = prev[k];
         if (kk == 0)
            insist(head == k);
         else
         {  insist(1 <= kk && kk <= m+n);
            insist(next[kk] == k);
            insist(ptr[kk] + cap[kk] - 1 < ptr[k]);
         }
         if (next[k] == 0) insist(tail == k);
      }
      for (k = 1; k <= m+n; k++) insist(flag[k]);
      ufree(flag);
      return;
}

/*----------------------------------------------------------------------
-- spm_realloc - reallocate sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_realloc(SPM *A, int m_max, int n_max);
--
-- *Description*
--
-- The routine spm_realloc reallocates arrays for the sparse matrix A.
--
-- The parameter m_max specifies a new maximal number of rows, and the
-- parameter n_max specifies a new maximal number of columns.
--
-- This routine is intended for auxiliary purposes and should not be
-- used directly. */

void spm_realloc(SPM *A, int m_max, int n_max)
{     int m = A->m;
      int n = A->n;
      int *temp;
      insist(m_max >= m);
      insist(n_max >= n);
#     define reallocate(type, ptr, max_len, len) \
      temp = ucalloc(max_len, sizeof(type)), \
      memcpy(temp, ptr, (len) * sizeof(type)), \
      ufree(ptr), ptr = temp
      reallocate(int, A->ptr, 1+m_max+n_max, 1+m+n);
      reallocate(int, A->len, 1+m_max+n_max, 1+m+n);
      reallocate(int, A->cap, 1+m_max+n_max, 1+m+n);
      reallocate(int, A->prev, 1+m_max+n_max, 1+m+n);
      reallocate(int, A->next, 1+m_max+n_max, 1+m+n);
#     undef reallocate
      A->m_max = m_max;
      A->n_max = n_max;
      return;
}

/*----------------------------------------------------------------------
-- spm_add_rows - add new rows to sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_add_rows(SPM *A, int nrs);
--
-- *Description*
--
-- The routine spm_add_rows adds nrs new (empty) rows to the sparse
-- matrix A. New rows are always added to the end of the row list, so
-- the ordinal numbers of existing rows remain unchanged. */

void spm_add_rows(SPM *A, int nrs)
{     int m = A->m;
      int n = A->n;
      int *ptr = A->ptr;
      int *len = A->len;
      int *cap = A->cap;
      int *prev = A->prev;
      int *next = A->next;
      int m_new, i, k;
      if (nrs < 1)
         fault("spm_add_rows: nrs = %d; invalid parameter", nrs);
      /* determine new number of rows */
      m_new = m + nrs;
      /* reallocate the arrays (if necessary) */
      if (A->m_max < m_new)
      {  int m_max = A->m_max;
         while (m_max < m_new) m_max += m_max;
         spm_realloc(A, m_max, A->n_max);
         ptr = A->ptr;
         len = A->len;
         cap = A->cap;
         prev = A->prev;
         next = A->next;
      }
      /* fix up references to column numbers */
      if (A->head > m) A->head += nrs;
      if (A->tail > m) A->tail += nrs;
      for (k = 1; k <= m+n; k++)
      {  if (prev[k] > m) prev[k] += nrs;
         if (next[k] > m) next[k] += nrs;
      }
      /* free a place between existing rows and columns */
      memmove(&ptr[m_new+1], &ptr[m+1], n * sizeof(int));
      memmove(&len[m_new+1], &len[m+1], n * sizeof(int));
      memmove(&cap[m_new+1], &cap[m+1], n * sizeof(int));
      memmove(&prev[m_new+1], &prev[m+1], n * sizeof(int));
      memmove(&next[m_new+1], &next[m+1], n * sizeof(int));
      /* initialize new rows */
      for (i = m+1; i <= m_new; i++)
      {  /* the new row has no elements */
         ptr[i] = A->used + 1;
         len[i] = cap[i] = 0;
         /* formally the new row is placed in the rightmost position of
            SVA, so add it to the end of the linked list */
         prev[i] = A->tail;
         next[i] = 0;
         if (prev[i] == 0)
            A->head = i;
         else
            next[prev[i]] = i;
         A->tail = i;
      }
      /* set new number of rows */
      A->m = m_new;
      return;
}

/*----------------------------------------------------------------------
-- spm_add_cols - add new columns to sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_add_cols(SPM *A, int ncs);
--
-- *Description*
--
-- The routine spm_add_cols adds ncs new (empty) columns to the sparse
-- matrix A. New columns are always added to the end of the column list,
-- so the ordinal numbers of existing columns remain unchanged. */

void spm_add_cols(SPM *A, int ncs)
{     int m = A->m;
      int n = A->n;
      int *ptr = A->ptr;
      int *len = A->len;
      int *cap = A->cap;
      int *prev = A->prev;
      int *next = A->next;
      int n_new, j;
      if (ncs < 1)
         fault("spm_add_cols: ncs = %d; invalid parameter", ncs);
      /* determine new number of columns */
      n_new = n + ncs;
      /* reallocate the arrays (if necessary) */
      if (A->n_max < n_new)
      {  int n_max = A->n_max;
         while (n_max < n_new) n_max += n_max;
         spm_realloc(A, A->m_max, n_max);
         ptr = A->ptr;
         len = A->len;
         cap = A->cap;
         prev = A->prev;
         next = A->next;
      }
      /* initialize new columns */
      for (j = m+n+1; j <= m+n_new; j++)
      {  /* the new column has no elements */
         ptr[j] = A->used + 1;
         len[j] = cap[j] = 0;
         /* formally the new column is placed in the rightmost position
            of SVA, so add it to the end of the linked list */
         prev[j] = A->tail;
         next[j] = 0;
         if (prev[j] == 0)
            A->head = j;
         else
            next[prev[j]] = j;
         A->tail = j;
      }
      /* set new number of columns */
      A->n = n_new;
      return;
}

/*----------------------------------------------------------------------
-- spm_load_data - load sparse matrix data.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_load_data(SPM *A,
--    void *info, double (*mat)(void *info, int *i, int *j));
--
-- *Description*
--
-- The routine spm_load_data loads elements of the sparse matrix A from
-- a file specified by the formal routine mat. All existing contents of
-- the matrix A is destroyed.
--
-- The parameter info is a transit pointer passed to the formal routine
-- mat (see below).
--
-- The formal routine mat specifies a set of non-zero elements, which
-- should be loaded into the matrix A. The routine spm_load_data calls
-- the routine mat in order to obtain a next non-zero element a[i,j].
-- In response the routine mat should store row and column indices of
-- this next element to the locations *i and *j and return a numerical
-- value of the element. Elements may be enumerated in arbirary order.
-- Note that zero elements and multiplets (i.e. elements with identical
-- row and column indices) are not allowed. If there is no next element,
-- the routine mat should store zero to both locations *i and *j and
-- then "rewind" the file in order to begin enumerating again from the
-- first element. */

void spm_load_data(SPM *A,
      void *info, double (*mat)(void *info, int *i, int *j))
{     int m = A->m;
      int n = A->n;
      int *ptr = A->ptr;
      int *len = A->len;
      int *cap = A->cap;
      int *ndx = A->ndx;
      double *val = A->val;
      int *prev = A->prev;
      int *next = A->next;
      int i, j, k, nnz, loc, i_beg, i_end, i_ptr;
      double aij;
      /* the first pass is used just to determine lengths of rows and
         columns, which are stored in the array cap, and also to count
         non-zero elements */
      nnz = 0;
      for (k = 1; k <= m+n; k++) cap[k] = 0;
      for (;;)
      {  /* get the next non-zero element */
         mat(info, &i, &j);
         /* check for end of file */
         if (i == 0 && j == 0) break;
         /* check row and column indices */
         if (!(1 <= i && i <= m))
            fault("spm_load_data: i = %d; invalid row number", i);
         if (!(1 <= j && j <= n))
            fault("spm_load_data: j = %d; invalid column number", j);
         /* increase non-zero count */
         nnz++;
         /* increase length of the i-th row */
         cap[i]++;
         if (cap[i] > n)
            fault("spm_load_data: i = %d; row too long", i);
         /* increase length of the j-th column */
         cap[m+j]++;
         if (cap[m+j] > m)
            fault("spm_load_data: j = %d; column too long", j);
      }
      /* the sparse vector area should have at least 2*nnz locations */
      if (A->size < nnz + nnz)
      {  /* reallocate SVA */
         ufree(ndx);
         ufree(val);
         A->size = nnz + nnz;
         ndx = A->ndx = ucalloc(1+A->size, sizeof(int));
         val = A->val = ucalloc(1+A->size, sizeof(double));
      }
      /* 2*nnz locations will be used */
      A->used = nnz + nnz;
      /* allocate locations for rows and columns */
      loc = 1;
      for (k = 1; k <= m+n; k++)
      {  ptr[k] = loc;
         len[k] = 0;
         loc += cap[k];
      }
      insist(loc == A->used + 1);
      /* build the row/column linked list (rows and columns follow in
         the natural order: 1, 2, ..., m, m+1, m+2, ..., m+n) */
      if (m + n == 0)
         A->head = A->tail = 0;
      else
      {  A->head = 1, A->tail = m+n;
         for (k = 1; k <= m+n; k++) prev[k] = k-1, next[k] = k+1;
         next[m+n] = 0;
      }
      /* the seconds pass is used to build the row lists */
      for (;;)
      {  /* get the next non-zero element */
         aij = mat(info, &i, &j);
         /* check for end of file */
         if (i == 0 && j == 0) break;
         /* check row and column indices */
         if (!(1 <= i && i <= m))
            fault("spm_load_data: i = %d; invalid row number", i);
         if (!(1 <= j && j <= n))
            fault("spm_load_data: j = %d; invalid column number", j);
         /* check numerical value */
         if (aij == 0.0)
            fault("spm_load_data: i = %d; j = %d; zero element not allo"
               "wed", i, j);
         /* check for overflow of the i-th row */
         if (len[i] == cap[i])
            fault("spm_load_data: i = %d; invalid row pattern", i);
         /* store the element in the i-th row list */
         loc = ptr[i] + (len[i]++);
         ndx[loc] = j, val[loc] = aij;
      }
      /* now the row lists have been built; use them in order to build
         the column lists */
      for (i = 1; i <= m; i++)
      {  /* length and capacity of each row should be the same */
         if (len[i] != cap[i])
            fault("spm_load_data: i = %d; invalid row pattern", i);
         /* scan elements of the i-th row */
         i_beg = ptr[i];
         i_end = i_beg + len[i] - 1;
         for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
         {  j = m + ndx[i_ptr];
            /* check for overflow of the j-th column */
            if (len[j] == cap[j])
               fault("spm_load_data: j = %d; invalid column pattern",
                  j - m);
            /* if there is a duplicate element, it is always placed in
               the end of the j-th column list */
            loc = ptr[j] + (len[j]++);
            if (loc > ptr[j] && ndx[loc-1] == i)
               fault("spm_load_data: i = %d; j = %d; duplicate elements"
                  " not allowed", i, j - m);
            /* store the element in the j-th column list */
            ndx[loc] = i, val[loc] = val[i_ptr];
         }
      }
      /* final check */
      for (j = m+1; j <= m+n; j++)
      {  /* length and capacity of each column should be the same */
         if (len[j] != cap[j])
            fault("spm_load_data: j = %d; invalid pattern", j - m);
      }
#if 0
      spm_check_data(A);
#endif
      return;
}

/*----------------------------------------------------------------------
-- spm_defrag_sva - defragment the sparse vector area.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_defrag_sva(SPM *A);
--
-- *Description*
--
-- The routine spm_defrag_sva defragments the sparse vector area (SVA),
-- which contains sparse rows and columns of the matrix A, in order to
-- gather all unused locations on one continuous extent.
--
-- Due to "garbage collection" this operation may change row and column
-- pointers of the matrix.
--
-- This routine is intended for auxiliary purposes and should not be
-- used directly. */

void spm_defrag_sva(SPM *A)
{     int *ptr = A->ptr;
      int *len = A->len;
      int *cap = A->cap;
      int *ndx = A->ndx;
      double *val = A->val;
      int *next = A->next;
      int beg = 1, k;
      /* skip rows and columns, which needn't to be relocated */
      for (k = A->head; k != 0; k = next[k])
      {  if (ptr[k] != beg) break;
         cap[k] = len[k];
         beg += cap[k];
      }
      /* relocate other rows and columns in order to gather all unused
         locations in one continuous extent */
      for (k = k; k != 0; k = next[k])
      {  memmove(&ndx[beg], &ndx[ptr[k]], len[k] * sizeof(int));
         memmove(&val[beg], &val[ptr[k]], len[k] * sizeof(double));
         ptr[k] = beg;
         cap[k] = len[k];
         beg += cap[k];
      }
      /* set new pointer to the last used location */
      A->used = beg - 1;
      return;
}

/*----------------------------------------------------------------------
-- spm_enlarge_cap - enlarge capacity of row or column.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- int spm_enlarge_cap(SPM *A, int k, int new_cap);
--
-- *Description*
--
-- The routine spm_enlarge_cap allows enlarging capacity of either the
-- i-th row (if k = i) or the j-th column (if k = m+j) of the matrix A
-- to new_cap locations (it is assumed that the current capacity of the
-- row or the column is less than new_cap). If necessary, the routine
-- may defragment the sparse vector area (SVA) or even reallocate it.
--
-- Due to "garbage collection" this operation may change row and column
-- pointers of the matrix.
--
-- This routine is intended for auxiliary purposes and should not be
-- used directly.
--
-- *Returns*
--
-- Non-zero return code means that SVA has been reallocated, therefore
-- the pointers A->ndx and A->val have been changed. */

int spm_enlarge_cap(SPM *A, int k, int new_cap)
{     int m = A->m;
      int n = A->n;
      int *ptr = A->ptr;
      int *len = A->len;
      int *cap = A->cap;
      int *ndx = A->ndx;
      double *val = A->val;
      int *prev = A->prev;
      int *next = A->next;
      int extra, cur, ret = 0;
      insist(1 <= k && k <= m+n);
      insist(cap[k] < new_cap);
      /* if there are less than cap free locations, defragment SVA */
      if (A->size - A->used < new_cap)
      {  spm_defrag_sva(A);
         /* it is reasonable to have extra amount of free locations in
            order to prevent frequent defragmentations in the future */
         extra = m + n + new_cap + 100;
         /* if there are less than extra free locations, reallocate SVA
            in order to increase its size */
         if (A->size - A->used < extra)
         {  while (A->size - A->used < extra) A->size += A->size;
            A->ndx = ucalloc(1+A->size, sizeof(int));
            memmove(&A->ndx[1], &ndx[1], A->used * sizeof(int));
            ufree(ndx), ndx = A->ndx;
            A->val = ucalloc(1+A->size, sizeof(double));
            memmove(&A->val[1], &val[1], A->used * sizeof(double));
            ufree(val), val = A->val;
            /* SVA has been reallocated */
            ret = 1;
         }
      }
      /* remember current capacity of the k-th row/column */
      cur = cap[k];
      /* copy existing elements to the beginning of the free part */
      memmove(&ndx[A->used+1], &ndx[ptr[k]], len[k] * sizeof(int));
      memmove(&val[A->used+1], &val[ptr[k]], len[k] * sizeof(double));
      /* set new pointer and new capacity of the k-th row/column */
      ptr[k] = A->used+1;
      cap[k] = new_cap;
      /* set new pointer to the last used location */
      A->used += new_cap;
      /* now the k-th row/column starts in the rightmost location among
         other rows and columns of the constraint matrix, therefore its
         node should be moved to the end of the row/column list */
      /* remove the k-th row/column from the linked list */
      if (prev[k] == 0)
         A->head = next[k];
      else
      {  /* capacity of the previous row/column can be increased at the
            expense of old locations of the k-th row/column */
         cap[prev[k]] += cur;
         next[prev[k]] = next[k];
      }
      if (next[k] == 0)
         A->tail = prev[k];
      else
         prev[next[k]] = prev[k];
      /* insert the k-th row/column to the end of the linked list */
      prev[k] = A->tail;
      next[k] = 0;
      if (prev[k] == 0)
         A->head = k;
      else
         next[prev[k]] = k;
      A->tail = k;
      return ret;
}

/*----------------------------------------------------------------------
-- spm_set_row - set (replace) row of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_set_row(SPM *A, int i, int len, int ndx[], double val[],
--    double R[], double S[]);
--
-- *Description*
--
-- The routine spm_set_row clears the i-th row of the sparse matrix A
-- and then stores a new set of non-zero elements in this row.
--
-- Column indices and numerical values of new non-zero elements stored
-- in the i-th row should be placed in locations ndx[1], ..., ndx[len]
-- and val[1], ..., val[len] respectively, where 0 <= len <= n is the
-- new length of the i-th row, n is number of columns.
--
-- Note that zero elements and multiplets (i.e. elements with identical
-- row indices) are not allowed.
--
-- The arrays R and S specify, respectively, diagonal matrices R and S
-- used to scale rows and columns of the matrix A. Diagonal elements of
-- the matrix R should be placed in R[1], ..., R[m]; diagonal elements
-- of the matrix S should be placed in S[1], ..., S[n].
--
-- If a[i,j] is a new non-zero element specified in the arrays ndx and
-- val, actually the routine stores the scaled value R[i]*a[i,j]*S[j].
--
-- If the pointer R or/and S is NULL, the corresponding scaling matrix
-- is considered as the unity matrix, i.e. no scaling is applied. */

void spm_set_row(SPM *A, int i, int len, int ndx[], double val[],
      double R[], double S[])
{     int m = A->m;
      int n = A->n;
      int *A_ptr = A->ptr;
      int *A_len = A->len;
      int *A_cap = A->cap;
      int *A_ndx = A->ndx;
      double *A_val = A->val;
      int i_beg, i_end, i_ptr, j, j_beg, j_end, j_ptr, t;
      double aij;
      if (!(1 <= i && i <= m))
         fault("spm_set_row: i = %d; row number out of range", i);
      if (!(0 <= len && len <= n))
         fault("spm_set_row: len = %d; invalid row length", len);
      /* remove elements of the i-th row from the corresponding column
         lists (this inefficient operation can be avoided, if the i-th
         row was made empty before) */
      i_beg = A_ptr[i];
      i_end = i_beg + A_len[i] - 1;
      for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
      {  /* find a[i,j] in the j-th column */
         j = m + A_ndx[i_ptr];
         j_beg = A_ptr[j];
         j_end = j_beg + A_len[j] - 1;
         for (j_ptr = j_beg; A_ndx[j_ptr] != i; j_ptr++) /* nop */;
         insist(j_ptr <= j_end);
         /* remove a[i,j] from the j-th column list */
         A_ndx[j_ptr] = A_ndx[j_end];
         A_val[j_ptr] = A_val[j_end];
         A_len[j]--;
      }
      /* remove all elements from the i-th row list */
      A_len[i] = 0;
      /* at least len free locations are needed in the i-th row */
      if (A_cap[i] < len)
      {  /* enlarge capacity of the i-th row */
         if (spm_enlarge_cap(A, i, len))
         {  /* SVA has been reallocated */
            A_ndx = A->ndx;
            A_val = A->val;
         }
      }
      /* add new elements to the i-th row list */
      for (t = 1, i_ptr = A_ptr[i]; t <= len; t++, i_ptr++)
      {  j = ndx[t];
         if (!(1 <= j && j <= n))
            fault("spm_set_row: ndx[%d] = %d; column index out of range"
               , t, ndx[t]);
         aij = val[t];
         if (aij == 0.0)
            fault("spm_set_row: val[%d] = 0; zero coefficient not allow"
               "ed", t);
         A_ndx[i_ptr] = j;
         A_val[i_ptr] =
            (R == NULL ? 1.0 : R[i]) * aij * (S == NULL ? 1.0 : S[j]);
      }
      A_len[i] = len;
      /* add new elements to the corresponding column lists */
      for (t = 0; t < len; t++)
      {  i_ptr = A_ptr[i] + t;
         j = m + A_ndx[i_ptr];
         aij = A_val[i_ptr];
         /* check if there is an element with the same row index in the
            j-th column (if such element exists, it is the last element
            in the column list) */
         j_beg = A_ptr[j];
         j_end = j_beg + A_len[j] - 1;
         if (j_beg <= j_end && A_ndx[j_end] == i)
            fault("spm_set_row: j = %d; duplicate column indices not al"
               "lowed", j-m);
         /* at least one free location is needed in the j-th column */
         if (A_cap[j] < A_len[j] + 1)
         {  /* enlarge capacity of the j-th column */
            if (spm_enlarge_cap(A, j, A_len[j] + 10))
            {  /* SVA has been reallocated */
               A_ndx = A->ndx;
               A_val = A->val;
            }
         }
         /* add new a[i,j] to the end of the j-th column list */
         j_ptr = A_ptr[j] + (A_len[j]++);
         A_ndx[j_ptr] = i;
         A_val[j_ptr] = aij;
      }
      return;
}

/*----------------------------------------------------------------------
-- spm_set_col - set (replace) column of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_set_col(SPM *A, int j, int len, int ndx[], double val[],
--    double R[], double S[]);
--
-- *Description*
--
-- The routine spm_set_col clears the j-th column of the sparse matrix
-- A and then stores a new set of non-zero elements in this column.
--
-- Row indices and numerical values of new non-zero elements stored in
-- the j-th column should be placed in locations ndx[1], ..., ndx[len]
-- and val[1], ..., val[len] respectively, where 0 <= len <= m is the
-- new length of the j-th column, m is number of rows.
--
-- Note that zero elements and multiplets (i.e. elements with identical
-- row indices) are not allowed.
--
-- The arrays R and S specify, respectively, diagonal matrices R and S
-- used to scale rows and columns of the matrix A. Diagonal elements of
-- the matrix R should be placed in R[1], ..., R[m]; diagonal elements
-- of the matrix S should be placed in S[1], ..., S[n].
--
-- If a[i,j] is a new non-zero element specified in the arrays ndx and
-- val, actually the routine stores the scaled value R[i]*a[i,j]*S[j].
--
-- If the pointer R or/and S is NULL, the corresponding scaling matrix
-- is considered as the unity matrix, i.e. no scaling is applied. */

void spm_set_col(SPM *A, int j, int len, int ndx[], double val[],
      double R[], double S[])
{     int m = A->m;
      int n = A->n;
      int *A_ptr = A->ptr;
      int *A_len = A->len;
      int *A_cap = A->cap;
      int *A_ndx = A->ndx;
      double *A_val = A->val;
      int i, i_beg, i_end, i_ptr, j_beg, j_end, j_ptr, t;
      double aij;
      if (!(1 <= j && j <= n))
         fault("spm_set_col: j = %d; column number out of range", j);
      if (!(0 <= len && len <= m))
         fault("spm_set_col: len = %d; invalid column length", len);
      /* remove elements of the j-th column from the corresponding row
         lists (this inefficient operation can be avoided, if the j-th
         column was made empty before) */
      j_beg = A_ptr[m+j];
      j_end = j_beg + A_len[m+j] - 1;
      for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
      {  /* find a[i,j] in the i-th row */
         i = A_ndx[j_ptr];
         i_beg = A_ptr[i];
         i_end = i_beg + A_len[i] - 1;
         for (i_ptr = i_beg; A_ndx[i_ptr] != j; i_ptr++) /* nop */;
         insist(i_ptr <= i_end);
         /* remove a[i,j] from the i-th row list */
         A_ndx[i_ptr] = A_ndx[i_end];
         A_val[i_ptr] = A_val[i_end];
         A_len[i]--;
      }
      /* remove all elements from the j-th column list */
      A_len[m+j] = 0;
      /* at least len free locations are needed in the j-th column */
      if (A_cap[m+j] < len)
      {  /* enlarge capacity of the j-th column */
         if (spm_enlarge_cap(A, m+j, len))
         {  /* SVA has been reallocated */
            A_ndx = A->ndx;
            A_val = A->val;
         }
      }
      /* add new elements to the j-th column list */
      for (t = 1, j_ptr = A_ptr[m+j]; t <= len; t++, j_ptr++)
      {  i = ndx[t];
         if (!(1 <= i && i <= m))
            fault("spm_set_col: ndx[%d] = %d; row index out of range",
               t, ndx[t]);
         aij = val[t];
         if (aij == 0.0)
            fault("spm_set_col: val[%d] = 0; zero coefficient not allow"
               "ed", t);
         A_ndx[j_ptr] = i;
         A_val[j_ptr] =
            (R == NULL ? 1.0 : R[i]) * aij * (S == NULL ? 1.0 : S[j]);
      }
      A_len[m+j] = len;
      /* add new elements to the corresponding row lists */
      for (t = 0; t < len; t++)
      {  j_ptr = A_ptr[m+j] + t;
         i = A_ndx[j_ptr];
         aij = A_val[j_ptr];
         /* check if there is an element with the same column index in
            the i-th row (if such element exists, it is the last element
            in the row list) */
         i_beg = A_ptr[i];
         i_end = i_beg + A_len[i] - 1;
         if (i_beg <= i_end && A_ndx[i_end] == j)
            fault("spm_set_col: i = %d; duplicate row indices not allow"
               "ed", i);
         /* at least one free location is needed in the i-th row */
         if (A_cap[i] < A_len[i] + 1)
         {  /* enlarge capacity of the i-th row */
            if (spm_enlarge_cap(A, i, A_len[i] + 10))
            {  /* SVA has been reallocated */
               A_ndx = A->ndx;
               A_val = A->val;
            }
         }
         /* add new a[i,j] to the end of the i-th row list */
         i_ptr = A_ptr[i] + (A_len[i]++);
         A_ndx[i_ptr] = j;
         A_val[i_ptr] = aij;
      }
      return;
}

/*----------------------------------------------------------------------
-- spm_clear_rows - clear marked rows of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_clear_rows(SPM *A, int mark[]);
--
-- *Description*
--
-- The routine spm_clear_rows clears (nullifies) marked rows of the
-- sparse matrix A. Row marks should be specified in the array mark: if
-- mark[i] is non-zero, the i-th row is considered as marked, in which
-- case it is cleared by the routine. */

void spm_clear_rows(SPM *A, int mark[])
{     int m = A->m;
      int n = A->n;
      int *ptr = A->ptr;
      int *len = A->len;
      int *ndx = A->ndx;
      double *val = A->val;
      int i, j, j_ptr, j_end;
      /* clear marked rows */
      for (i = 1; i <= m; i++)
         if (mark[i]) len[i] = 0;
      /* remove elements, which belong to the marked rows, from the
         corresponding column lists */
      for (j = m+1; j <= m+n; j++)
      {  j_ptr = ptr[j];
         j_end = j_ptr + len[j] - 1;
         while (j_ptr <= j_end)
         {  i = ndx[j_ptr];
            if (mark[i])
            {  /* remove a[i,j] from the j-th column list */
               ndx[j_ptr] = ndx[j_end];
               val[j_ptr] = val[j_end];
               len[j]--;
               j_end--;
            }
            else
            {  /* keep a[i,j] in the j-th column list */
               j_ptr++;
            }
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- spm_clear_cols - clear marked columns of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_clear_cols(SPM *A, int mark[]);
--
-- *Description*
--
-- The routine spm_clear_cols clears (nullifies) marked columns of the
-- sparse matrix A. Column marks should be specified in the array mark:
-- if mark[j] is non-zero, the j-th column is considered as marked, in
-- which case it is cleared by the routine. */

void spm_clear_cols(SPM *A, int mark[])
{     int m = A->m;
      int n = A->n;
      int *ptr = A->ptr;
      int *len = A->len;
      int *ndx = A->ndx;
      double *val = A->val;
      int i, i_ptr, i_end, j;
      /* clear marked columns */
      for (j = 1; j <= n; j++)
         if (mark[j]) len[m+j] = 0;
      /* remove elements, which belong to the marked columns, from the
         corresponding row lists */
      for (i = 1; i <= m; i++)
      {  i_ptr = ptr[i];
         i_end = i_ptr + len[i] - 1;
         while (i_ptr <= i_end)
         {  j = ndx[i_ptr];
            if (mark[j])
            {  /* remove a[i,j] from the i-th row list */
               ndx[i_ptr] = ndx[i_end];
               val[i_ptr] = val[i_end];
               len[i]--;
               i_end--;
            }
            else
            {  /* keep a[i,j] in the i-th row list */
               i_ptr++;
            }
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- spm_del_rows - delete marked rows of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_del_rows(SPM *A, int mark[]);
--
-- *Description*
--
-- The routine spm_del_rows deletes marked rows of the sparse matrix A.
-- Row marks should be specified in the array mark: if mark[i] is
-- non-zero, the i-th row is considered as marked, in which case it is
-- deleted by the routine. */

void spm_del_rows(SPM *A, int mark[])
{     int m = A->m;
      int n = A->n;
      int *ptr = A->ptr;
      int *len = A->len;
      int *cap = A->cap;
      int *ndx = A->ndx;
      int *prev = A->prev;
      int *next = A->next;
      int i, j, j_beg, j_end, j_ptr, k, m_new, *ord;
      /* clear marked rows */
      spm_clear_rows(A, mark);
      /* and remove them from the row/column linked list */
      for (i = 1; i <= m; i++)
      {  if (mark[i])
         {  if (prev[i] == 0)
               A->head = next[i];
            else
               next[prev[i]] = next[i];
            if (next[i] == 0)
               A->tail = prev[i];
            else
               prev[next[i]] = prev[i];
         }
      }
      /* delete marked rows and determine new ordinal numbers of rows
         that are kept in the matrix */
      ord = ucalloc(1+m, sizeof(int));
      m_new = 0;
      for (i = 1; i <= m; i++)
      {  if (mark[i])
         {  /* the i-th row should be deleted */
            ord[i] = 0;
         }
         else
         {  /* the i-th row should be kept */
            ord[i] = ++m_new;
            ptr[m_new] = ptr[i];
            len[m_new] = len[i];
            cap[m_new] = cap[i];
            prev[m_new] = prev[i];
            next[m_new] = next[i];
         }
      }
      /* fill a gap between remaining rows and columns */
      memmove(&ptr[m_new+1], &ptr[m+1], n * sizeof(int));
      memmove(&len[m_new+1], &len[m+1], n * sizeof(int));
      memmove(&cap[m_new+1], &cap[m+1], n * sizeof(int));
      memmove(&prev[m_new+1], &prev[m+1], n * sizeof(int));
      memmove(&next[m_new+1], &next[m+1], n * sizeof(int));
      /* apply new numbering to row indices in the column lists */
      for (j = m_new+1; j <= m_new+n; j++)
      {  j_beg = ptr[j];
         j_end = j_beg + len[j] - 1;
         for (j_ptr = j_beg; j_ptr <= j_end; j_ptr++)
            ndx[j_ptr] = ord[ndx[j_ptr]];
      }
      /* apply new numbering to the row/column linked list */
#     define new(k) (k <= m ? ord[k] : (k - m) + m_new)
      if (A->head != 0) A->head = new(A->head);
      if (A->tail != 0) A->tail = new(A->tail);
      for (k = 1; k <= m_new+n; k++)
      {  if (prev[k] != 0) prev[k] = new(prev[k]);
         if (next[k] != 0) next[k] = new(next[k]);
      }
#     undef new
      ufree(ord);
      /* set new number of rows */
      A->m = m_new;
      return;
}

/*----------------------------------------------------------------------
-- spm_del_cols - delete marked columns of sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_del_cols(SPM *A, int mark[]);
--
-- *Description*
--
-- The routine spm_del_cols deletes marked columns of the sparse matrix
-- A. Column marks should be specified in the array mark: if mark[j] is
-- non-zero, the j-th column is considered as marked, in which case it
-- is deleted by the routine. */

void spm_del_cols(SPM *A, int mark[])
{     int m = A->m;
      int n = A->n;
      int *ptr = A->ptr;
      int *len = A->len;
      int *cap = A->cap;
      int *ndx = A->ndx;
      int *prev = A->prev;
      int *next = A->next;
      int i, i_beg, i_end, i_ptr, j, k, n_new, *ord;
      /* clear marked columns */
      spm_clear_cols(A, mark);
      /* and remove them from the row/column linked list */
      for (j = 1; j <= n; j++)
      {  if (mark[j])
         {  if (prev[m+j] == 0)
               A->head = next[m+j];
            else
               next[prev[m+j]] = next[m+j];
            if (next[m+j] == 0)
               A->tail = prev[m+j];
            else
               prev[next[m+j]] = prev[m+j];
         }
      }
      /* delete marked columns and determine new ordinal numbers of
         columns that are kept in the matrix */
      ord = ucalloc(1+n, sizeof(int));
      n_new = 0;
      for (j = 1; j <= n; j++)
      {  if (mark[j])
         {  /* the j-th column should be deleted */
            ord[j] = 0;
         }
         else
         {  /* the j-th column should be kept */
            ord[j] = ++n_new;
            ptr[m+n_new] = ptr[m+j];
            len[m+n_new] = len[m+j];
            cap[m+n_new] = cap[m+j];
            prev[m+n_new] = prev[m+j];
            next[m+n_new] = next[m+j];
         }
      }
      /* apply new numbering to column indices in the row lists */
      for (i = 1; i <= m; i++)
      {  i_beg = ptr[i];
         i_end = i_beg + len[i] - 1;
         for (i_ptr = i_beg; i_ptr <= i_end; i_ptr++)
            ndx[i_ptr] = ord[ndx[i_ptr]];
      }
      /* apply new numbering to the row/column linked list */
#     define new(k) (k <= m ? k : ord[k - m] + m)
      if (A->head != 0) A->head = new(A->head);
      if (A->tail != 0) A->tail = new(A->tail);
      for (k = 1; k <= m+n_new; k++)
      {  if (prev[k] != 0) prev[k] = new(prev[k]);
         if (next[k] != 0) next[k] = new(next[k]);
      }
#     undef new
      ufree(ord);
      /* set new number of columns */
      A->n = n_new;
      return;
}

/*----------------------------------------------------------------------
-- spm_delete - delete sparse matrix.
--
-- *Synopsis*
--
-- #include "glpspm.h"
-- void spm_delete(SPM *A);
--
-- *Description*
--
-- The routine spm_delete deletes the sparse matrix A, freeing all the
-- memory allocated to it. */

void spm_delete(SPM *A)
{     ufree(A->ptr);
      ufree(A->len);
      ufree(A->cap);
      ufree(A->ndx);
      ufree(A->val);
      ufree(A->prev);
      ufree(A->next);
      ufree(A);
      return;
}

/* eof */
