/* glplpt.h */

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

#ifndef _GLPLPT_H
#define _GLPLPT_H

#include "glpdmp.h"

#define lpt_read_prob         glp_lpt_read_prob
#define lpt_free_prob         glp_lpt_free_prob

typedef struct LPT LPT;
typedef struct LPTROW LPTROW;
typedef struct LPTCOL LPTCOL;
typedef struct LPTLFE LPTLFE;

struct LPT
{     /* LP/MIP problem in CPLEX LP format */
      DMP *pool;
      /* memory pool that holds all relevant data structures */
      char name[16+1];
      /* name of the objective function */
      int sense;
      /* optimization sense indicator: */
#define LPT_MIN '-' /* minimization */
#define LPT_MAX '+' /* maximization */
      LPTLFE *obj;
      /* pointer to a linear form for the objective function */
      int m;
      /* number of rows (constraints) */
      int n;
      /* number of columns (variables) */
      LPTROW *first_row, *last_row;
      /* pointers to the first and the last rows, respectively */
      LPTCOL *first_col, *last_col;
      /* pointers to the first and the last columns, respectively */
};

struct LPTROW
{     /* row (constraint) */
      char name[16+1];
      /* row name */
      int i;
      /* ordinal number of this row (1 <= i <= m) */
      LPTLFE *ptr;
      /* pointer to a linear form for this row */
      int sense;
      /* row sense indicator: */
#define LPT_LE  '<' /* less than or equal to */
#define LPT_GE  '>' /* greater than or equal to */
#define LPT_EQ  '=' /* equal to */
      double rhs;
      /* right-hand side */
      LPTROW *next;
      /* pointer to the next row */
};

struct LPTCOL
{     /* column (variable) */
      char name[16+1];
      /* column name */
      int j;
      /* ordinal number of this column (1 <= j <= n) */
      int kind;
      /* column kind: */
#define LPT_CON 'C' /* continuous variable */
#define LPT_INT 'I' /* general integer variable */
#define LPT_BIN 'B' /* binary variable */
      double lb;
      /* lower bound or -DBL_MAX (with the meaning -inf) */
      double ub;
      /* upper bound or +DBL_MAX (with the meaning +inf) */
      LPTCOL *next;
      /* pointer to the next column */
};

struct LPTLFE
{     /* linear form entry */
      double coef;
      /* coefficient */
      LPTCOL *col;
      /* pointer to a column (variable) */
      LPTLFE *next;
      /* pointer to the next entry */
};

LPT *lpt_read_prob(char *fname);
/* read LP/MIP problem in CPLEX LP format */

void lpt_free_prob(LPT *lpt);
/* free LP/MIP problem data block */

#endif

/* eof */
