/* glpmps.h */

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

#ifndef _GLPMPS_H
#define _GLPMPS_H

#define load_mps              glp_load_mps
#define free_mps              glp_free_mps

typedef struct MPS MPS;
typedef struct MPSROW MPSROW;
typedef struct MPSCOL MPSCOL;
typedef struct MPSCQE MPSCQE;
typedef struct MPSBND MPSBND;
typedef struct MPSBQE MPSBQE;
typedef struct MPSQFE MPSQFE;

struct MPS
{     /* linear programming model in MPS format */
      DMP *pool;
      /* memory pool holding all relevant data structures */
      char *name;
      /* model name */
      int n_row;
      /* number of rows */
      int n_col;
      /* number of columns */
      int n_rhs;
      /* number of right-hand side constraint vectors */
      int n_rng;
      /* number of range vectors */
      int n_bnd;
      /* number of bound vectors */
      MPSROW **row; /* MPSROW *row[1+n_row]; */
      /* ordered list of rows */
      MPSCOL **col; /* MPSCOL *col[1+n_col]; */
      /* ordered list of columns */
      MPSCOL **rhs; /* MPSCOL *rhs[1+n_rhs]; */
      /* ordered list of right-hand side constraint vectors */
      MPSCOL **rng; /* MPSCOL *rng[1+n_rng]; */
      /* ordered list of range vectors */
      MPSBND **bnd; /* MPSBND *bnd[1+n_bnd]; */
      /* ordered list of bound vectors */
      MPSQFE *quad;
      /* pointer to the linked list of quadratic form elements; these
         elements specify quadratic part of the objective function */
};

struct MPSROW
{     /* row (constraint) */
      char *name;
      /* row name */
      char type[2+1];
      /* row type:
         "N"  - no constraint
         "G"  - greater than or equal to
         "L"  - less than or equal to
         "E"  - equality */
};

struct MPSCOL
{     /* column (structural variable), right-hand side constraint
         vector, or range vector */
      char *name;
      /* column or vector name */
      int flag;
      /* column flag:
         0 - continuous variable
         1 - integer variable (between INTORG and INTEND markers) */
      MPSCQE *ptr;
      /* pointer to the linked list of column or vector elements */
};

struct MPSCQE
{     /* column or vector element */
      int ind;
      /* row number */
      double val;
      /* element value */
      MPSCQE *next;
      /* pointer to the next element of the same column or vector (in
         this list elements follow in the same order as in the original
         MPS file) */
};

struct MPSBND
{     /* bound vector */
      char *name;
      /* vector name */
      MPSBQE *ptr;
      /* pointer to the linked list of vector elements */
};

struct MPSBQE
{     /* bound vector element */
      char type[2+1];
      /* bound type:
         "LO" - lower bound
         "UP" - upper bound
         "FX" - fixed value
         "FR" - free variable
         "MI" - no lower bound (lower bound is -inf)
         "PL" - no upper bound (upper bound is +inf)
         "UI" - upper integer
         "BV" - binary variable */
      int ind;
      /* column number */
      double val;
      /* element value */
      MPSBQE *next;
      /* pointer to the next element of the same bound vector (in this
         list elements follow in the same order as in the original MPS
         file) */
};

struct MPSQFE
{     /* quadratic form element */
      int ind1;
      /* first column number */
      int ind2;
      /* second column number */
      double val;
      /* element value */
      MPSQFE *next;
      /* pointer to the next element (in this list elements follow in
         the same order as in the original MPS file) */
};

extern MPS *load_mps(char *fname);
/* load linear programming model in MPS format */

extern void free_mps(MPS *mps);
/* free linear programming model in MPS format */

#endif

/* eof */
