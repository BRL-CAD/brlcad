/* glpspm.h */

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

#ifndef _GLPSPM_H
#define _GLPSPM_H

#define spm_create            glp_spm_create
#define spm_check_data        glp_spm_check_data
#define spm_realloc           glp_spm_realloc
#define spm_add_rows          glp_spm_add_rows
#define spm_add_cols          glp_spm_add_cols
#define spm_load_data         glp_spm_load_data
#define spm_defrag_sva        glp_spm_defrag_sva
#define spm_enlarge_cap       glp_spm_enlarge_cap
#define spm_set_row           glp_spm_set_row
#define spm_set_col           glp_spm_set_col
#define spm_clear_rows        glp_spm_clear_rows
#define spm_clear_cols        glp_spm_clear_cols
#define spm_del_rows          glp_spm_del_rows
#define spm_del_cols          glp_spm_del_cols
#define spm_delete            glp_spm_delete

typedef struct SPM SPM;

struct SPM
{     /* sparse matrix in row/column-wise format */
      int m_max;
      /* maximal number of rows (if necessary, this parameter can be
         automatically changed) */
      int n_max;
      /* maximal number of columns (if necessary, this parameter can be
         automatically changed) */
      int m;
      /* current number of rows, 0 <= m <= m_max */
      int n;
      /* current number of columns, 0 <= n <= n_max */
      int *ptr; /* int ptr[1+m_max+n_max]; */
      /* ptr[0] is not used;
         ptr[i], 1 <= i <= m, is a pointer to the first element of the
         i-th row in the sparse vector area;
         ptr[m+j], 1 <= j <= n, is a pointer to the first element of the
         j-th column in the sparse vector area */
      int *len; /* int len[1+m_max+n_max]; */
      /* len[0] is not used;
         len[i], 1 <= i <= m, is number of (non-zero) elements in the
         i-th row, 0 <= len[i] <= n;
         len[m+j], 1 <= j <= n, is number of (non-zero) elements in the
         j-th column, 0 <= len[m+j] <= m */
      int *cap; /* int cap[1+m_max+n_max]; */
      /* cap[0] is not used;
         cap[i], 1 <= i <= m, is capacity of the i-th row; this is
         number of locations, which can be used without relocating the
         row, cap[i] >= len[i];
         cap[m+j], 1 <= j <= n, is capacity of the j-th column; this is
         number of locations, which can be used without relocating the
         column, cap[m+j] >= len[m+j] */
      /*--------------------------------------------------------------*/
      /* sparse vector area (SVA) is a set of locations intended to
         store sparse vectors that represent rows and columns of the
         matrix; each location is a doublet (ndx, val), where ndx is an
         index and val is a numerical value of sparse vector element;
         in the whole each sparse vector is a set of adjacent locations
         defined by a pointer to the first element ptr[*] and number of
         its non-zero elements len[*] */
      int size;
      /* current size of SVA, in locations; locations are numbered by
         integers 1, 2, ..., size (the location number 0 is not used);
         if necessary, this parameter can be automatically changed */
      int used;
      /* locations that have the numbers 1, 2, ..., used are used by
         rows and columns of the matrix (this extent may contain holes
         because of fragmentation);
         locations that have the numbers used+1, used+2, ..., size are
         currently free;
         total number of free locations is (size - used) */
      int *ndx; /* int ndx[1+size]; */
      /* ndx[0] is not used;
         ndx[k] is the index field of the k-th location */
      double *val; /* double val[1+size]; */
      /* val[0] is not used;
         val[k] is the value field of the k-th location */
      /*--------------------------------------------------------------*/
      /* in order to efficiently defragment SVA a doubly linked list of
         rows and columns is used, where rows have numbers 1, ..., m,
         and column have numbers m+1, ..., m+n, so each row and column
         can be uniquely identified by only one integer; in this linked
         rows and columns are ordered by ascending their pointers */
      int head;
      /* the number of the leftmost (in SVA) row/column */
      int tail;
      /* the number of the rightmost (in SVA) row/column */
      int *prev; /* int prev[1+m_max+n_max]; */
      /* prev[k], 1 <= k <= m+n, is the number of a row/column, which
         precedes (in SVA) the k-th row/column */
      int *next; /* int next[1+m_max+n_max]; */
      /* next[k], 1 <= k <= m+n, is the number of a row/column, which
         succedes (in SVA) the k-th row/column */
};

SPM *spm_create(void);
/* create sparse matrix */

void spm_check_data(SPM *A);
/* check sparse matrix data structure */

void spm_realloc(SPM *A, int m_max, int n_max);
/* reallocate sparse matrix */

void spm_add_rows(SPM *A, int nrs);
/* add new rows to sparse matrix */

void spm_add_cols(SPM *A, int ncs);
/* add new columns to sparse matrix */

void spm_load_data(SPM *A,
      void *info, double (*mat)(void *info, int *i, int *j));
/* load sparse matrix data */

void spm_defrag_sva(SPM *A);
/* defragment the sparse vector area */

int spm_enlarge_cap(SPM *A, int k, int new_cap);
/* enlarge capacity of row or column */

void spm_set_row(SPM *A, int i, int len, int ndx[], double val[],
      double R[], double S[]);
/* set (replace) row of sparse matrix */

void spm_set_col(SPM *A, int j, int len, int ndx[], double val[],
      double R[], double S[]);
/* set (replace) column of sparse matrix */

void spm_clear_rows(SPM *A, int mark[]);
/* clear marked rows of sparse matrix */

void spm_clear_cols(SPM *A, int mark[]);
/* clear marked columns of sparse matrix */

void spm_del_rows(SPM *A, int mark[]);
/* delete marked rows of sparse matrix */

void spm_del_cols(SPM *A, int mark[]);
/* delete marked columns of sparse matrix */

void spm_delete(SPM *A);
/* delete sparse matrix */

#endif

/* eof */
