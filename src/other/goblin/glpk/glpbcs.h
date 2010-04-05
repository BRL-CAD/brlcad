/* glpbcs.h (branch-and-cut framework) */

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

#ifndef _GLPBCS_H
#define _GLPBCS_H

#include "glpies.h"

#define bcs_driver            glp_bcs_driver
#define bcs_get_event         glp_bcs_get_event
#define bcs_add_var           glp_bcs_add_var
#define bcs_add_con           glp_bcs_add_con
#define bcs_get_next_var      glp_bcs_get_next_var
#define bcs_get_next_con      glp_bcs_get_next_con
#define bcs_set_var_appl      glp_bcs_set_var_appl
#define bcs_set_con_appl      glp_bcs_set_con_appl
#define bcs_get_var_appl      glp_bcs_get_var_appl
#define bcs_get_con_appl      glp_bcs_get_con_appl
#define bcs_get_num_rows      glp_bcs_get_num_rows
#define bcs_get_num_cols      glp_bcs_get_num_cols
#define bcs_get_ith_con       glp_bcs_get_ith_con
#define bcs_get_jth_var       glp_bcs_get_jth_var
#define bcs_get_var_info      glp_bcs_get_var_info
#define bcs_get_con_info      glp_bcs_get_con_info
#define bcs_which_var         glp_bcs_which_var
#define bcs_which_con         glp_bcs_which_con
#define bcs_get_lp_object     glp_bcs_get_lp_object

typedef struct BCS BCS;
typedef struct BCSVAR BCSVAR;
typedef struct BCSCON BCSCON;
typedef struct BCSJOB BCSJOB;

struct BCS
{     /* branch-and-cut workspace */
      IESTREE *tree;
      /* implicit enumeration tree */
      DMP *var_pool;
      /* memory pool for variables */
      DMP *con_pool;
      /* memory pool for constraints */
      DMP *job_pool;
      /* memory pool for subproblems */
      void *info;
      /* transit pointer passed to the application procedure */
      void (*appl)(void *info, BCS *bcs);
      /* event-driven application procedure */
      int event;
      /* current event code: */
#define BCS_V_UNDEF     1000  /* undefined event (never raised) */
#define BCS_V_INIT      1001  /* initializing */
#define BCS_V_GENCON    1002  /* constraints generation required */
#define BCS_V_GENCUT    1003  /* cutting planes generation required */
#define BCS_V_BRANCH    1004  /* branch selection required */
#define BCS_V_BINGO     1005  /* better solution found */
#define BCS_V_DELVAR    1006  /* some variable is being deleted */
#define BCS_V_DELCON    1007  /* some constraint is being deleted */
#define BCS_V_TERM      1008  /* terminating */
      BCSVAR *br_var;
      /* pointer to a variable chosen by the application procedure to
         branch on; NULL means no variable has been chosen */
      BCSVAR *this_var;
      /* pointer returned by the routine bcs_which_var */
      BCSCON *this_con;
      /* pointer returned by the routine bcs_which_con */
      int found;
      /* this flag is set if the solver has found at least one integer
         feasible solution */
      double best;
      /* value of the objective function for the best integer feasible
         solution found by the solver (if the flag found is clear, this
         value is undefined) */
      double t_last;
      /* most recent time when the visual information was displayed */
};

struct BCSVAR
{     /* variable */
      int flag;
      /* flag used to check pointers: */
#define BCS_VAR_FLAG    0x2A564152
      IESITEM *col;
      /* pointer to the corresponding master column; the field link in
         the master column structure points to this structure */
      int attr;
      /* variable attributes: */
#define BCS_CONTIN      0x01  /* continuous */
#define BCS_INTEGER     0x02  /* integer */
#define BCS_STATIC      0x04  /* static */
#define BCS_DYNAMIC     0x08  /* dynamic */
#define BCS_MARKED      0x10  /* marked (for internal purposes) */
#define BCS_NONINT      0x20  /* integer infeasibility */
      int disc;
      /* reserved for application specific information */
      void *link;
      /* reserved for application specific information */
};

struct BCSCON
{     /* constraint */
      int flag;
      /* flag used to check pointers: */
#define BCS_CON_FLAG    0x2A434F4E
      IESITEM *row;
      /* pointer to the corresponding master row; the field link in the
         master row structure points to this structure */
      int attr;
      /* constraint attributes: */
#define BCS_MARKED      0x10  /* marked (for internal purposes) */
      int disc;
      /* reserved for application specific information */
      void *link;
      /* reserved for application specific information */
};

struct BCSJOB
{     /* subproblem */
      int flag;
      /* flag used to check pointers: */
#define BCS_JOB_FLAG    0x2A4A4F42
      IESNODE *node;
      /* pointer to the corresponding node problem; the field link in
         the node problem structure refers to this structure */
};

int bcs_driver(char *name, int dir,
      void *info, void (*appl)(void *info, BCS *bcs));
/* branch-and-cut driver */

int bcs_get_event(BCS *bcs);
/* determine current event code */

BCSVAR *bcs_add_var(BCS *bcs, char *name, int attr, int typx,
      double lb, double ub, double coef, int len, BCSCON *con[],
      double val[]);
/* add variable to the branch-and-cut workspace */

BCSCON *bcs_add_con(BCS *bcs, char *name, int attr, int typx,
      double lb, double ub, int len, BCSVAR *var[], double val[]);
/* add constraint to the branch-and-cut workspace */

BCSVAR *bcs_get_next_var(BCS *bcs, BCSVAR *var);
/* get pointer to the next variable */

BCSCON *bcs_get_next_con(BCS *bcs, BCSCON *con);
/* get pointer to the next constraint */

void bcs_set_var_appl(BCS *bcs, BCSVAR *var, int disc, void *link);
/* store application information to variable */

void bcs_set_con_appl(BCS *bcs, BCSCON *con, int disc, void *link);
/* store application information to constraint */

void bcs_get_var_appl(BCS *bcs, BCSVAR *var, int *disc, void **link);
/* retrieve application information from variable */

void bcs_get_con_appl(BCS *bcs, BCSCON *con, int *disc, void **link);
/* retrieve application information from constraint */

int bcs_get_num_rows(BCS *bcs);
/* determine number of rows */

int bcs_get_num_cols(BCS *bcs);
/* determine number of columns */

BCSCON *bcs_get_ith_con(BCS *bcs, int i);
/* determine pointer to i-th constraint */

BCSVAR *bcs_get_jth_var(BCS *bcs, int j);
/* determine pointer to j-th variable */

void bcs_get_var_info(BCS *bcs, BCSVAR *var, int *tagx, double *vx,
      double *dx);
/* obtain solution information for variable */

void bcs_get_con_info(BCS *bcs, BCSCON *con, int *tagx, double *vx,
      double *dx);
/* obtain solution information for constraint */

BCSVAR *bcs_which_var(BCS *bcs);
/* determine which variable is meant */

BCSCON *bcs_which_con(BCS *bcs);
/* determine which constraint is meant */

LPX *bcs_get_lp_object(BCS *bcs);
/* get pointer to internal LP object */

#endif

/* eof */
