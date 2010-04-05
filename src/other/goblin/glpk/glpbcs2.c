/* glpbcs2.c (branch-and-cut framework; framework routines) */

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

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "glpbcs.h"
#include "glplib.h"

/*----------------------------------------------------------------------
-- bcs_get_event - determine current event code.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- int bcs_get_event(BCS *bcs);
--
-- *Returns*
--
-- The routine bcs_get_event returns a code of the current event, due
-- to which the application procedure is being called.
--
-- The parameter bcs is the second parameter passed to the application
-- procedure. It specifies the branch-and-cut workspace. */

int bcs_get_event(BCS *bcs)
{     int event = bcs->event;
      insist(event != BCS_V_UNDEF);
      return event;
}

/*----------------------------------------------------------------------
-- bcs_add_var - add variable to the branch-and-cut workspace.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- BCSVAR *bcs_add_var(BCS *bcs, char *name, int attr, int typx,
--    double lb, double ub, double coef, int len, BCSCON *con[],
--    double val[]);
--
-- *Description*
--
-- The routine bcs_add_var should be used by the application procedure
-- to add a new variable to the branch-and-cut workspace.
--
-- NOTE that in the current version of the branch-and-cut framework
-- variables can be added to the workspace only during initialization
-- phase (BCS_V_INIT). All variables are considered as globally valid
-- and inherited by all subproblems derived from the root subproblem.
--
-- The parameter bcs is the second parameter passed to the application
-- procedure. It specifies the branch-and-cut workspace.
--
-- The parameter name is a symbolic name (1 to 255 graphic characters)
-- assigned to the variable. If this parameter is NULL, no symbolic name
-- is assigned.
--
-- The parameter attr specifies attributes of the variable:
--
-- BCS_CONTIN     continuous variable (default);
--
-- BCS_INTEGER    integer variable;
--
-- BCS_STATIC     static variable (default). Variable marked as static
--                is always included in every subproblem;
--
-- BCS_DYNAMIC    dynamic variable (should have zero lower bound).
--                Variable marked as dynamic is included in subproblem
--                only if it is required by the reduced cost.
--
-- The parameters typx, lb, and ub specify, respectively, type, lower
-- bound, and upper bound of the variable:
--
--     Type          Bounds            Note
--    -------------------------------------------
--    LPX_FR   -inf <  x <  +inf   free variable
--    LPX_LO     lb <= x <  +inf   lower bound
--    LPX_UP   -inf <  x <=  ub    upper bound
--    LPX_DB     lb <= x <=  ub    double bound
--    LPX_FX           x  =  lb    fixed variable
--
-- The parameter coef specifies a coefficient of the objective function
-- at the variable.
--
-- The parameters len, con, and val specify constraint coefficients for
-- the variable. Pointers to constraints should be placed in locations
-- con[1], ..., con[len], and numerical values of the corresponding
-- constraint coefficients should be placed in locations val[1], ...,
-- val[len]. Zero coefficients and multiplets (i.e. coefficients that
-- refer to the same constraint) are not allowed. If the parameter len
-- is zero, the parameters con and val are not used.
--
-- *Returns*
--
-- The routine returns a pointer to the added variable, which should be
-- used in subsequent operations on this variable. */

BCSVAR *bcs_add_var(BCS *bcs, char *name, int attr, int typx,
      double lb, double ub, double coef, int len, BCSCON *con[],
      double val[])
{     BCSVAR *var;
      IESITEM **row = (IESITEM **)con;
      int t, temp;
      /* check if the routine is called during initialization phase */
      if (bcs->event != BCS_V_INIT)
         fault("bcs_add_var: attempt to call at improper point");
      /* check variable name */
      if (name != NULL && lpx_check_name(name))
         fault("bcs_add_var: invalid variable name");
      /* check variable attributes */
      temp = BCS_CONTIN | BCS_INTEGER | BCS_STATIC | BCS_DYNAMIC;
      if (attr & ~temp)
         fault("bcs_add_var: attr = 0x%X; invalid attributes", attr);
      temp = BCS_CONTIN | BCS_INTEGER;
      if ((attr & temp) == temp)
         fault("bcs_add_var: attributes BCS_CONTIN and BCS_INTEGER in c"
            "onflict");
      if ((attr & temp) == 0) attr |= BCS_CONTIN; /* default */
      temp = BCS_STATIC | BCS_DYNAMIC;
      if ((attr & temp) == temp)
         fault("bcs_add_var: attributes BCS_STATIC and BCS_DYNAMIC in c"
            "onflict");
      if ((attr & temp) == 0) attr |= BCS_STATIC; /* default */
      /* check variable type */
      if (!(typx == LPX_FR || typx == LPX_LO || typx == LPX_UP ||
            typx == LPX_DB || typx == LPX_FX))
         fault("bcs_add_var: typx = %d; invalid variable type", typx);
      /* integer variable should have integer bounds */
      if (attr & BCS_INTEGER)
      {  if ((typx == LPX_LO || typx == LPX_DB || typx == LPX_FX) &&
            lb != floor(lb + 0.5))
            fault("bcs_add_var: lb = %g; invalid lower bound of integer"
               " variable", lb);
         if ((typx == LPX_UP || typx == LPX_DB) &&
            ub != floor(ub + 0.5))
            fault("bcs_add_var: ub = %g; invalid upper bound of integer"
               " variable", ub);
      }
      /* dynamic variable should have zero lower bound */
      if (attr & BCS_DYNAMIC)
      {  if (!(typx == LPX_LO || typx == LPX_DB))
            fault("bcs_add_var: typx = %d; invalid type of dynamic vari"
               "able", typx);
         if (lb != 0.0)
            fault("bcs_add_var: lb = %g; invalid lower bound od dynamic"
               " variable", lb);
      }
      /* replace temporarily pointers to constraints by pointers to the
         corresponding master rows */
      for (t = 1; t <= len; t++)
      {  if (!(con[t] != NULL && con[t]->flag == BCS_CON_FLAG))
            fault("bcs_add_var: con[%d] = %p; invalid pointer",
               t, con[t]);
         row[t] = con[t]->row;
      }
      /* create new variable */
      var = dmp_get_atom(bcs->var_pool);
      var->flag = BCS_VAR_FLAG;
      var->col = ies_add_master_col(bcs->tree, name, typx, lb, ub, coef,
         len, row, val);
      ies_set_item_link(bcs->tree, var->col, var);
      var->attr = attr;
      var->disc = 0, var->link = NULL;
      /* restore pointers to constraints */
      for (t = 1; t <= len; t++) con[t] = row[t]->link;
      /* mark static variable to include it in the root subproblem */
      if (var->attr & BCS_STATIC) var->attr |= BCS_MARKED;
      /* return to the application procedure */
      return var;
}

/*----------------------------------------------------------------------
-- bcs_add_con - add constraint to the branch-and-cut workspace.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- BCSCON *bcs_add_con(BCS *bcs, char *name, int attr, int typx,
--    double lb, double ub, int len, BCSVAR *var[], double val[]);
--
-- *Description*
--
-- The routine bcs_add_con should be used by the application procedure
-- to add a new constraint to the branch-and-cut workspace.
--
-- NOTE that in the current version of the branch-and-cut framework
-- constraints can be added to the workspace only during initialization
-- phase (BCS_V_INIT) or constraints generation phase (BCS_V_GENCON) or
-- cutting planes generation phase (BCS_V_GENCUT). In the first case
-- the constraints are considered as globally valid and inherited by
-- all subproblems derived from the root subproblem. In the second and
-- third cases constraints are considered as locally valid; they are
-- added to the current subproblem and inherited by subproblems derived
-- from the current subproblem. Actually there is no difference between
-- these cases in the current version.
--
-- The parameter bcs is the second parameter passed to the application
-- procedure. It specifies the branch-and-cut workspace.
--
-- The parameter name is a symbolic name (1 to 255 graphic characters)
-- assigned to the constraint. If this parameter is NULL, no symbolic
-- name is assigned.
--
-- The parameter attr specifies attributes of the constraint. It should
-- be zero in the current version of the branch-and-cut framework.
--
-- The parameters typx, lb, and ub specify, respectively, type, lower
-- bound, and upper bound of the constraint (linear form):
--
--     Type          Bounds            Note
--    -------------------------------------------
--    LPX_FR   -inf <  x <  +inf   free variable
--    LPX_LO     lb <= x <  +inf   lower bound
--    LPX_UP   -inf <  x <=  ub    upper bound
--    LPX_DB     lb <= x <=  ub    double bound
--    LPX_FX           x  =  lb    fixed variable
--
-- where x is an auxiliary variable associated with the constraint.
--
-- The parameters len, var, and val should specify coefficients for the
-- constraint. Pointers to variables should be placed in locations
-- var[1], ..., var[len], and numerical values of the corresponding
-- coefficients should be placed in locations val[1], ..., val[len].
-- Zero coefficients and multiplets (i.e. coefficients that refer to
-- the same constraint) are not allowed. If the parameter len is zero,
-- the parameters var and val are not used.
--
-- *Returns*
--
-- The routine returns a pointer to the added constraint, which should
-- be used in subsequent operations on this constraint. */

BCSCON *bcs_add_con(BCS *bcs, char *name, int attr, int typx,
      double lb, double ub, int len, BCSVAR *var[], double val[])
{     BCSCON *con;
      IESITEM **col = (IESITEM **)var;
      int t;
      /* check if the routine is called during either initialization
         phase or constraints/cutting planes generation phase */
      if (!(bcs->event == BCS_V_INIT ||
            bcs->event == BCS_V_GENCON ||
            bcs->event == BCS_V_GENCUT))
         fault("bcs_add_con: attempt to call at improper point");
      /* check constraint name */
      if (name != NULL && lpx_check_name(name))
         fault("bcs_add_con: invalid constraint name");
      /* check constraint attributes */
      if (attr != 0)
         fault("bcs_add_con: attr = 0x%X; invalid attributes", attr);
      /* replace temporarily pointers to variables by pointers to the
         corresponding master columns */
      for (t = 1; t <= len; t++)
      {  if (!(var[t] != NULL && var[t]->flag == BCS_VAR_FLAG))
            fault("bcs_add_con: var[%d] = %p; invalid pointer",
               t, var[t]);
         col[t] = var[t]->col;
      }
      /* create new constraint */
      con = dmp_get_atom(bcs->con_pool);
      con->flag = BCS_CON_FLAG;
      con->row = ies_add_master_row(bcs->tree, name, typx, lb, ub, 0.0,
         len, col, val);
      ies_set_item_link(bcs->tree, con->row, con);
      con->attr = attr;
      con->disc = 0, con->link = NULL;
      /* restore pointers to variables */
      for (t = 1; t <= len; t++) var[t] = col[t]->link;
      /* mark constraint to include it in the current subproblem */
      con->attr |= BCS_MARKED;
      /* return to the application procedure */
      return con;
}

/*----------------------------------------------------------------------
-- bcs_get_next_var - get pointer to the next variable.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- BCSVAR *bcs_get_next_var(BCS *bcs, BCSVAR *var);
--
-- *Returns*
--
-- If the parameter var is NULL, the routine returns a pointer to the
-- first variable, which was added into the workspace before any other
-- variables, or NULL, if there are no variables in the workspace.
-- Otherwise the parameter var should specify some variable, in which
-- case the routine returns a pointer to the next variable, which was
-- added into the workspace later than the specified one, or NULL, if
-- there is no next variable in the workspace. */

BCSVAR *bcs_get_next_var(BCS *bcs, BCSVAR *var)
{     IESITEM *col;
      if (!(var == NULL || var->flag == BCS_VAR_FLAG))
         fault("bcs_get_next_var: var = %p; invalid pointer", var);
      col = (var == NULL ? NULL : var->col);
      col = ies_next_master_col(bcs->tree, col);
      return (BCSVAR *)(col == NULL ? NULL : col->link);
}

/*----------------------------------------------------------------------
-- bcs_get_next_con - get pointer to the next constraint.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- BCSCON *bcs_get_next_con(BCS *bcs, BCSCON *con);
--
-- *Returns*
--
-- If the parameter con is NULL, the routine returns a pointer to the
-- first constraint, which was added into the workspace before any other
-- constraints, or NULL, if there are no constraints in the workspace.
-- Otherwise the parameter con should specify some constraint, in which
-- case the routine returns a pointer to the next constraint, which was
-- added into the workspace later than the specified one, or NULL, if
-- there is no next constraint in the workspace. */

BCSCON *bcs_get_next_con(BCS *bcs, BCSCON *con)
{     IESITEM *row;
      if (!(con == NULL || con->flag == BCS_CON_FLAG))
         fault("bcs_get_next_con: con = %p; invalid pointer", con);
      row = (con == NULL ? NULL : con->row);
      row = ies_next_master_row(bcs->tree, row);
      return (BCSCON *)(row == NULL ? NULL : row->link);
}

/*----------------------------------------------------------------------
-- bcs_set_var_appl - store application information to variable.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- void bcs_set_var_appl(BCS *bcs, BCSVAR *var, int disc, void *link);
--
-- *Description*
--
-- The routine bcs_set_var_appl stores application specific information
-- to the specified variable.
--
-- The parameter disc is a discriminant, which allows the application
-- to distinguish between variables of different sorts.
--
-- The parameter link is a pointer to some extended information, which
-- the application may assign to the variable. */

void bcs_set_var_appl(BCS *bcs, BCSVAR *var, int disc, void *link)
{     insist(bcs == bcs);
      if (!(var != NULL && var->flag == BCS_VAR_FLAG))
         fault("bcs_set_var_appl: var = %p; invalid pointer", var);
      var->disc = disc;
      var->link = link;
      return;
}

/*----------------------------------------------------------------------
-- bcs_set_con_appl - store application information to constraint.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- void bcs_set_con_appl(BCS *bcs, BCSCON *con, int disc, void *link);
--
-- *Description*
--
-- The routine bcs_set_con_appl stores application specific information
-- to the specified constraint.
--
-- The parameter disc is a discriminant, which allows the application
-- to distinguish between constraints of different sorts.
--
-- The parameter link is a pointer to some extended information, which
-- the application may assign to the constraint. */

void bcs_set_con_appl(BCS *bcs, BCSCON *con, int disc, void *link)
{     insist(bcs == bcs);
      if (!(con != NULL && con->flag == BCS_CON_FLAG))
         fault("bcs_set_con_appl: con = %p; invalid pointer", con);
      con->disc = disc;
      con->link = link;
      return;
}

/*----------------------------------------------------------------------
-- bcs_get_var_appl - retrieve application information from variable.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- void bcs_get_var_appl(BCS *bcs, BCSVAR *var, int *disc, void **link);
--
-- *Description*
--
-- The routine bcs_get_var_appl retrieves application specific
-- information (previously stored by the routine bcs_set_var_appl) from
-- the specified variable.
--
-- A discriminant value and a pointer to an extended information are
-- stored to the locations, which the parameters disc and link point to,
-- respectively. If some of the parameters disc or link is NULL, the
-- corresponding value is not stored. */

void bcs_get_var_appl(BCS *bcs, BCSVAR *var, int *disc, void **link)
{     insist(bcs == bcs);
      if (!(var != NULL && var->flag == BCS_VAR_FLAG))
         fault("bcs_get_var_appl: var = %p; invalid pointer", var);
      if (disc != NULL) *disc = var->disc;
      if (link != NULL) *link = var->link;
      return;
}

/*----------------------------------------------------------------------
-- bcs_get_con_appl - retrieve application information from constraint.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- void bcs_get_con_appl(BCS *bcs, BCSCON *con, int *disc, void **link);
--
-- *Description*
--
-- The routine bcs_get_con_appl retrieves application specific
-- information (previously stored by the routine bcs_set_con_appl) from
-- the specified constraint.
--
-- A discriminant value and a pointer to an extended information are
-- stored to the locations, which the parameters disc and link point to,
-- respectively. If some of the parameters disc or link is NULL, the
-- corresponding value is not stored. */

void bcs_get_con_appl(BCS *bcs, BCSCON *con, int *disc, void **link)
{     insist(bcs == bcs);
      if (!(con != NULL && con->flag == BCS_CON_FLAG))
         fault("bcs_get_con_appl: con = %p; invalid pointer", con);
      if (disc != NULL) *disc = con->disc;
      if (link != NULL) *link = con->link;
      return;
}

/*----------------------------------------------------------------------
-- bcs_get_num_rows - determine number of rows.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- int bcs_get_num_rows(BCS *bcs);
--
-- *Returns*
--
-- The routine bcs_get_num_rows returns number of rows in the current
-- subproblem, which should exist. */

int bcs_get_num_rows(BCS *bcs)
{     /* the current subproblem should exist */
      if (ies_get_this_node(bcs->tree) == NULL)
         fault("bcs_get_num_rows: current subproblem not exist");
      return ies_get_num_rows(bcs->tree);
}

/*----------------------------------------------------------------------
-- bcs_get_num_cols - determine number of columns.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- int bcs_get_num_cols(BCS *bcs);
--
-- *Returns*
--
-- The routine bcs_get_num_cols returns number of columns in the current
-- subproblem, which should exist. */

int bcs_get_num_cols(BCS *bcs)
{     /* the current subproblem should exist */
      if (ies_get_this_node(bcs->tree) == NULL)
         fault("bcs_get_num_cols: current subproblem not exist");
      return ies_get_num_cols(bcs->tree);
}

/*----------------------------------------------------------------------
-- bcs_get_ith_con - determine pointer to i-th constraint.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- BCSCON *bcs_get_ith_con(BCS *bcs, int i);
--
-- *Returns*
--
-- The routine bcs_get_ith_con returns a pointer to a constraint, which
-- corresponds to the i-th row in the current subproblem.
--
-- *Note*
--
-- Due to dynamic nature of subproblems the correspondence between
-- constraints and rows may be changed in the next reincarnation of the
-- current subproblem. In particular, some constraints may enter the
-- current subproblem, and some may leave it. */

BCSCON *bcs_get_ith_con(BCS *bcs, int i)
{     IESITEM *row;
      /* the current subproblem should exist */
      if (ies_get_this_node(bcs->tree) == NULL)
         fault("bcs_get_ith_con: current subproblem not exist");
      /* check the row number */
      if (!(1 <= i && i <= ies_get_num_rows(bcs->tree)))
         fault("bcs_get_ith_con: i = %d; row number out of range", i);
      /* obtain a pointer to the i-th row */
      row = ies_get_ith_row(bcs->tree, i);
      insist(ies_what_item(bcs->tree, row) == 'R');
      /* return a pointer to the corresponding constraint */
      return (BCSCON *)row->link;
}

/*----------------------------------------------------------------------
-- bcs_get_jth_var - determine pointer to j-th variable.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- BCSVAR *bcs_get_jth_var(BCS *bcs, int j);
--
-- *Returns*
--
-- The routine bcs_get_jth_var returns a pointer to a variable, which
-- corresponds to the j-th column in the current subproblem.
--
-- *Note*
--
-- Due to dynamic nature of subproblems the correspondence between
-- variables and columns may be changed in the next reincarnation of
-- the current subproblem. In particular, some variables may enter the
-- current subproblem, and some may leave it. */

BCSVAR *bcs_get_jth_var(BCS *bcs, int j)
{     IESITEM *col;
      /* the current subproblem should exist */
      if (ies_get_this_node(bcs->tree) == NULL)
         fault("bcs_get_jth_var: current subproblem not exist");
      /* check the column number */
      if (!(1 <= j && j <= ies_get_num_cols(bcs->tree)))
         fault("bcs_get_jth_var: j = %d; column number out of range",
            j);
      /* obtain a pointer to the j-th column */
      col = ies_get_jth_col(bcs->tree, j);
      insist(ies_what_item(bcs->tree, col) == 'C');
      /* return a pointer to the corresponding variable */
      return (BCSVAR *)col->link;
}

/*----------------------------------------------------------------------
-- bcs_get_var_info - obtain solution information for variable.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- void bcs_get_var_info(BCS *bcs, BCSVAR *var, int *tagx, double *vx,
--    double *dx);
--
-- *Description*
--
-- The routine bcs_get_var_info stores status, primal and dual values,
-- which the specified variable has in a basic solution of the current
-- subproblem, to locations, which the parameters tagx, vx, and dx
-- point to, respectively.
--
-- The status code has the following meaning:
--
-- LPX_BS - basic variable;
-- LPX_NL - non-basic variable on its lower bound;
-- LPX_NU - non-basic variable on its upper bound;
-- LPX_NF - non-basic free (unbounded) variable;
-- LPX_NS - non-basic fixed variable.
--
-- If some of pointers tagx, vx, or dx is NULL, the corresponding value
-- is not stored. */

void bcs_get_var_info(BCS *bcs, BCSVAR *var, int *tagx, double *vx,
      double *dx)
{     /* the current subproblem should exist */
      if (ies_get_this_node(bcs->tree) == NULL)
         fault("bcs_get_var_info: current subproblem not exist");
      /* check pointer to variable */
      if (!(var != NULL && var->flag == BCS_VAR_FLAG))
         fault("bcs_get_var_info: var = %p; invalid pointer", var);
      /* the specified variable should be presented in the current
         subproblem */
      if (ies_get_col_bind(bcs->tree, var->col) == 0)
         fault("bcs_get_var_info: var = %p; variable missing in current"
            " subproblem", var);
      /* obtain status and primal and dual value of the variable */
      ies_get_col_info(bcs->tree, var->col, tagx, vx, dx);
      return;
}

/*----------------------------------------------------------------------
-- bcs_get_con_info - obtain solution information for constraint.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- void bcs_get_con_info(BCS *bcs, BCSCON *con, int *tagx, double *vx,
--    double *dx);
--
-- *Description*
--
-- The routine bcs_get_con_info stores status, primal and dual values,
-- which the specified constraint (more exactly, an auxiliary variable
-- for the corresponding constraint) has in a basic solution of the
-- current subproblem, to locations, which the parameters tagx, vx, and
-- dx point to, respectively.
--
-- The status code has the following meaning:
--
-- LPX_BS - non-active constraint;
-- LPX_NL - active inequality constraint on its lower bound;
-- LPX_NU - active inequiality constraint on its upper bound;
-- LPX_NF - free linear form, whose auxiliary variable is non-basic;
-- LPX_NS - active equiality constraint.
--
-- If some of pointers tagx, vx, or dx is NULL, the corresponding value
-- is not stored. */

void bcs_get_con_info(BCS *bcs, BCSCON *con, int *tagx, double *vx,
      double *dx)
{     /* the current subproblem should exist */
      if (ies_get_this_node(bcs->tree) == NULL)
         fault("bcs_get_con_info: current subproblem not exist");
      /* check pointer to constraint */
      if (!(con != NULL && con->flag == BCS_CON_FLAG))
         fault("bcs_get_con_info: con = %p; invalid pointer", con);
      /* the specified constraint should be presented in the current
         subproblem */
      if (ies_get_row_bind(bcs->tree, con->row) == 0)
         fault("bcs_get_con_info: con = %p; constraint missing in curre"
            "nt subproblem", con);
      /* obtain status and primal and dual value of the constraint */
      ies_get_row_info(bcs->tree, con->row, tagx, vx, dx);
      return;
}

/*----------------------------------------------------------------------
-- bcs_which_var - determine which variable is meant.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- BCSVAR *bcs_which_var(BCS *bcs);
--
-- *Returns*
--
-- The routine bcs_which_var returns a pointer to a variable, if the
-- current event is related to this variable. */

BCSVAR *bcs_which_var(BCS *bcs)
{     if (bcs->event != BCS_V_DELVAR)
         fault("bcs_which_var: attempt to call at improper point");
      return bcs->this_var;
}

/*----------------------------------------------------------------------
-- bcs_which_con - determine which constraint is meant.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- BCSCON *bcs_which_con(BCS *bcs);
--
-- *Returns*
--
-- The routine bcs_which_con returns a pointer to a constraint, if the
-- current event is related to this constraint. */

BCSCON *bcs_which_con(BCS *bcs)
{     if (bcs->event != BCS_V_DELCON)
         fault("bcs_which_con: attempt to call at improper point");
      return bcs->this_con;
}

/*----------------------------------------------------------------------
-- bcs_get_lp_object - get pointer to internal LP object.
--
-- *Synopsis*
--
-- #include "glpbcs.h"
-- LPX *bcs_get_lp_object(BCS *bcs);
--
-- *Returns*
--
-- The routine bcs_get_lp_object returns a pointer to the internal LP
-- object for the specified branch-and-cut workspace.
--
-- The internal LP object is a representation of the linear programming
-- problem, which corresponds to the current subproblem.
--
-- The internal LP object *must not* be modified directly by using any
-- API routines with the prefix 'lpx'. All necessary modifications of
-- the current subproblem should be performed only by using the 'bcs'
-- routines. However, the 'lpx' API routines can be used for obtaining
-- information from the LP object. */

LPX *bcs_get_lp_object(BCS *bcs)
{     LPX *lp = ies_get_lp_object(bcs->tree);
      return lp;
}

/* eof */
