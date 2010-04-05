/* glpmip.h */

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

#ifndef _GLPMIP_H
#define _GLPMIP_H

#include "glpies.h"

#define mip_create_tree       glp_mip_create_tree
#define mip_driver            glp_mip_driver
#define mip_delete_tree       glp_mip_delete_tree

typedef struct MIPTREE MIPTREE;
typedef struct MIPROW MIPROW;
typedef struct MIPCOL MIPCOL;
typedef struct MIPNODE MIPNODE;

struct MIPTREE
{     /* MIP solver workspace */
      DMP *row_pool;
      /* memory pool for rows (constraints) */
      DMP *col_pool;
      /* memory pool for columns (variables) */
      DMP *node_pool;
      /* memory pool for nodes (subproblems) */
      /*--------------------------------------------------------------*/
      /* original problem information */
      int orig_m;
      /* number of rows in the original problem */
      int orig_n;
      /* number of columns in the original problem */
      int dir;
      /* optimization direction:
         LPX_MIN - minimization
         LPX_MAX - maximization */
      /*--------------------------------------------------------------*/
      /* application interface */
      void *info;
      /* transit pointer passed to the application procedure */
      void (*appl)(void *info, MIPTREE *tree);
      /* event-driven application procedure */
      int event;
      /* current event code: */
#define MIP_V_UNDEF     1100  /* undefined event (never raised) */
#define MIP_V_SELECT    1101  /* subproblem selection required */
#define MIP_V_BEGSUB    1102  /* subproblem processing begins */
#define MIP_V_BEGLP     1103  /* LP optimization begins */
#define MIP_V_ENDLP     1104  /* LP optimization ended */
#define MIP_V_REJECT    1105  /* subproblem rejected */
#define MIP_V_BINGO     1106  /* better MIP solution found */
#define MIP_V_BRANCH    1107  /* branching variable required */
#define MIP_V_ENDSUB    1108  /* subproblem processing ended */
      /*--------------------------------------------------------------*/
      /* MIP solver segment */
      IESTREE *tree;
      /* implicit enumeration tree */
      MIPNODE *glob;
      /* pointer to a subproblem, which is the highest-leveled common
         ancestor of all active and the current subproblems (initially
         it is the root subproblem); an optimal value of the objective
         for LP relaxation of this subproblem is the global lower (in
         case of minimization) or upper (in case of maximization) bound
         for the original MIP problem */
      MIPNODE *curr;
      /* pointer to the current subproblem being solved; NULL means the
         current subproblem is undefined (not selected yet) */
      int m_max;
      /* current length of the array row (if necessary, this parameter
         is automatically increased) */
      MIPROW **row; /* MIPROW *row[1+m_max]; */
      /* array of pointers to rows of the current subproblem;
         row[1], ..., row[m] are pointers to rows, which are presented
         in the current subproblem, where m = mip_get_num_rows;
         row[m+1], ..., row[m+nrs] are pointers to rows, which will be
         added to the current subproblem before re-optimization;
         row[m+nrs+1], ..., row[m_max] are free entries */
      MIPCOL **col; /* MIPCOL *col[1+n]; */
      /* array of pointers to columns of the current subproblem, where
         n = mip_get_num_cols = orig_n */
      int reopt;
      /* this count is reset to zero when a new subproblem is selected
         from the active list and then increased by one each time when
         the subproblem is re-optimized */
      int e_code;
      /* exit code returned by the lpx_simplex routine after solving LP
         relaxation of the current subproblem */
      int better;
      /* this flag is set if optimal solution of the LP relaxation of
         the current subproblem is better than the best known integer
         feasible solution; otherwise this flag is clear */
      int unsat;
      /* number of columns (structural variables) of integer kind that
         have fractional values in the basic solution of LP relaxation
         of the current subproblem */
      double ii_sum;
      /* the sum of integer infeasibilites for the basic solution of LP
         relaxation of the current subproblem */
      int *del; /* int del[1+m_max]; */
      /* if the flag del[i] (1 <= i <= m, where m = mip_get_num_rows)
         is set, i-th row will be removed from the current subproblem
         before re-optimization */
      int nrs;
      /* number of additional rows, which will be added to the current
         subproblem before re-optimization; pointers to these new rows
         are stored in locations row[m+1], ..., row[m+nrs] */
      int j_br;
      /* number of column (structural variable) chosen to branch on */
      int heir;
      /* this flag specifies which subproblem created as the result of
         branching on the column j_br should be selected next:
         0 - selected using a backtracking heuristic
         1 - down branch (subproblem, where the branching variable has
             a new, decreased upper bound)
         2 - up branch (subproblem, where the branching variable has a
             new, increased lower bound) */
      /*--------------------------------------------------------------*/
      /* best known solution */
      int found;
      /* this flag is set if the solver has found some integer feasible
         solution; otherwise this flag is clear */
      double *best; /* double best[1+orig_m+orig_n]; */
      /* best[0] is a numerical value of the objective function;
         best[1], ..., best[orig_m] are numerical values of auxiliary
         variables (rows) of the original problem;
         best[orig_m+1], ..., best[orig_m+orig_n] are numerical values
         of structural variables (columns) of the original problem;
         if the flag found is not set, all components of the array best
         are undefined */
      /*--------------------------------------------------------------*/
      /* control parameters and statistics */
      int msg_lev;
      /* level of messages output by the solver:
         0 - no output
         1 - error messages only
         2 - normal output
         3 - full output (includes informational messages) */
      double tol_int;
      /* relative tolerance used to check if the current basic solution
         is integer feasible */
      double tol_obj;
      /* relative tolerance used to check if a value of the objective
         function is not better than in the best known integer feasible
         solution */
      int it_lim;
      /* simplex iterations limit; if this value is positive, it is
         decreased by one each time when one simplex iteration has been
         performed, and reaching zero value signals the solver to stop
         the search; negative value means no iterations limit */
      int sn_lim;
      /* solved subproblems limit; if this value is positive, it is
         decreased by one each time when one active subroblem has been
         solved, and reaching zero value signals the solver to stop the
         search; negative value means no subproblems limit */
      double tm_lim;
      /* searching time limit, in seconds; if this value is positive,
         it is decreased each time when one complete round of the
         search is performed by the amount of time spent for the round,
         and reaching zero value signals the solver to stop the search;
         negative value means no time limit */
      double out_frq;
      /* output frequency, in seconds; this parameter specifies how
         frequently the solver sends information about progress of the
         search to the standard output */
      int an_cnt;
      /* number of subproblems in the active list */
      int sn_cnt;
      /* number of solved subproblems (including those ones which were
         rejected not being actually solved); some of them can still be
         in the tree if they have active descendants; at the end of the
         search this count is total number of explored subproblems */
      double tm_last;
      /* most recent time, at which visual information was displayed */
};

struct MIPROW
{     /* row (constraint) */
      int i;
      /* ordinal number of this row */
      IESITEM *row;
      /* pointer to the corresponding master row; the field link in the
         master row structure refers to this structure */
      MIPNODE *node;
      /* pointer to the subproblem, which was the current when this row
         was added */
};

struct MIPCOL
{     /* column (variable) */
      int j;
      /* ordinal number of this column */
      IESITEM *col;
      /* pointer to the corresponding master column; the field link in
         the master column structure refers to this structure */
      int intvar;
      /* if this flag is set, the variable is of integer kind; if this
         flag is clear, the variable is of continuous kind */
      int infeas;
      /* if this flag is set, the variable is of integer kind and its
         value in a basic solution of the current subproblem is integer
         infeasible (fractional); otherwise this flag is clear */
};

struct MIPNODE
{     /* subproblem */
      IESNODE *node;
      /* pointer to the corresponding node problem; the field link in
         the node problem structure refers to this structure */
      double lp_obj;
      /* optimal value of the objective function for LP relaxation of
         this subproblem; if this subproblem is in the active list and
         therefore never solved yet, lp_obj is an estimation of the
         objective function, which is a lower (in case of minimization)
         or an upper (in case of maximization) bound of a true optimal
         value; the estimation can be obtained in different ways, for
         example, it can be just lp_obj from the parent subproblem */
      double temp;
      /* reserved for internal needs */
};

/* exit codes returned by the routine mip_driver: */
#define MIP_E_OK        1200  /* the search is completed */
#define MIP_E_ITLIM     1201  /* iterations limit exhausted */
#define MIP_E_SNLIM     1202  /* subproblems limit exhausted */
#define MIP_E_TMLIM     1203  /* time limit exhausted */
#define MIP_E_ERROR     1204  /* can't solve LP relaxation */

MIPTREE *mip_create_tree(LPX *mip,
      void *info, void appl(void *info, MIPTREE *tree));
/* create MIP solver wprkspace */

int mip_driver(MIPTREE *tree);
/* branch-and-bound driver */

void mip_delete_tree(MIPTREE *tree);
/* delete MIP solver workspace */

#endif

/* eof */
