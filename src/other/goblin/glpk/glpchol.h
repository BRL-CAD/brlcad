/* glpchol.h */

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

#ifndef _GLPCHOL_H
#define _GLPCHOL_H

#include "glpmat.h"

#define chol_numb             glp_chol_numb
#define chol_symb             glp_chol_symb
#define create_adat           glp_create_adat
#define create_chol           glp_create_chol
#define decomp_adat           glp_decomp_adat
#define decomp_chol           glp_decomp_chol
#define delete_adat           glp_delete_adat
#define delete_chol           glp_delete_chol
#define min_deg               glp_min_deg
#define solve_adat            glp_solve_adat
#define solve_chol            glp_solve_chol

extern void chol_symb(MAT *A, MAT *U, int head[], int next[],
      char work[]);
/* compute Cholesky decomposition A = U'*U; symbolic phase */

extern int chol_numb(MAT *A, MAT *U, void *head[], double work[]);
/* compute Cholesky decomposition A = U'*U; numeric phase */

extern void min_deg(MAT *A, PER *P);
/* minimum degree ordering */

typedef struct CHOL CHOL;

struct CHOL
{     /* Cholesky factorization of symmetric positive definite matrix
         in the form A = P'*U'*U*P */
      int n;
      /* order of matrices */
      PER *P;
      /* permutation matrix (that determines A~ = P*A*P' for which the
         factorization is actually computed) */
      MAT *U;
      /* upper triangular matrix (U'*U = A~ = P*A*P') */
      int sing;
      /* number of processed non-positive or very small diagonal
         elements of the matrix U during numeric factorization (for
         details see comments to the chol_numb routine) */
};

extern CHOL *create_chol(MAT *A);
/* create Cholesky factorization */

extern void decomp_chol(CHOL *chol, MAT *A);
/* compute Cholesky factorization */

extern void solve_chol(CHOL *chol, double x[], double work[]);
/* solving linear system using Cholesky factorization */

extern void delete_chol(CHOL *chol);
/* delete Cholesky factorization */

typedef struct ADAT ADAT;

struct ADAT
{     /* Cholesky factorization of symmetric positive definite matrix
         S = A*D*A', where A is rectangular matrix (of full rank), D is
         diagonal matrix; no dense columns processing */
      int m;
      /* number of rows of A */
      int n;
      /* number of columns of A, order of D */
      MAT *S;
      /* S = A*D*A' (only upper triangular part) */
      CHOL *chol;
      /* Cholesky factorization of S */
};

extern ADAT *create_adat(MAT *A);
/* create Cholesky factorization for S = A*D*A' */

extern void decomp_adat(ADAT *adat, MAT *A, double D[]);
/* compute Cholesky factorization for S = A*D*A' */

extern void solve_adat(ADAT *adat, double x[], double work[]);
/* solve system A*D*A'*x = b using Cholesky factorization */

extern void delete_adat(ADAT *adat);
/* delete Cholesky factorization for S = A*D*A' */

#endif

/* eof */
