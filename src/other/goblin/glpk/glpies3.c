/* glpies3.c */

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
#include "glpies.h"
#include "glplib.h"

/*----------------------------------------------------------------------
-- ies_get_node_level - get node depth level.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- int ies_get_node_level(IESTREE *tree, IESNODE *node);
--
-- *Returns*
--
-- The routine returns the depth level of the specified node problem.
-- The root node problem has the depth level 0. */

int ies_get_node_level(IESTREE *tree, IESNODE *node)
{     insist(tree == tree);
      return node->level;
}

/*----------------------------------------------------------------------
-- ies_get_node_count - get node reference count.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- int ies_get_node_count(IESTREE *tree, IESNODE *node);
--
-- *Returns*
--
-- The routine returns the reference count for the specified node
-- problem. If the count is negative, the node problem is active and
-- therefore has no child node problems. Non-negative count means the
-- node problem is inactive, and the count is number of its existing
-- child node problems. */

int ies_get_node_count(IESTREE *tree, IESNODE *node)
{     insist(tree == tree);
      return node->count;
}

/*----------------------------------------------------------------------
-- ies_get_next_node - get pointer to next node.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESNODE *ies_get_next_node(IESTREE *tree, IESNODE *node);
--
-- *Returns*
--
-- If the parameter node is NULL, the routine returns a pointer to the
-- first node, which was created before any other nodes (it is always
-- the root node, by the way), or NULL, if the tree is empty.
-- Otherwise, the parameter node should specify some node of the tree,
-- in which case the routine returns a pointer to the next node, which
-- was created later than the specified node, or NULL, if there is no
-- next node in the tree. */

IESNODE *ies_get_next_node(IESTREE *tree, IESNODE *node)
{     if (node == NULL)
         node = tree->root_node;
      else
         node = node->next;
      return node;
}

/*----------------------------------------------------------------------
-- ies_get_prev_node - get pointer to previous node.
--
-- *Returns*
--
-- #include "glpies.h"
-- IESNODE *ies_get_prev_node(IESTREE *tree, IESNODE *node);
--
-- *Description*
--
-- If the parameter node is NULL, the routine returns a pointer to the
-- last node, which was created after any other nodes, or NULL, if the
-- tree is empty. Otherwise, the parameter node should specify some
-- node of the tree, in which case the routine returns a pointer to the
-- previous node, which was created earlier than the specified node, or
-- NULL, if there is no previous node in the tree. */

IESNODE *ies_get_prev_node(IESTREE *tree, IESNODE *node)
{     if (node == NULL)
         node = tree->last_node;
      else
         node = node->prev;
      return node;
}

/*----------------------------------------------------------------------
-- ies_get_this_node - get pointer to current node.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESNODE *ies_get_this_node(IESTREE *tree);
--
-- *Returns*
--
-- The routine ies_get_this_node returns a pointer to the current node
-- problem or NULL, if the current node problem doesn't exist. */

IESNODE *ies_get_this_node(IESTREE *tree)
{     IESNODE *node = tree->this_node;
      return node;
}

/*----------------------------------------------------------------------
-- ies_set_node_link - store node problem specific information.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_node_link(IESTREE *tree, IESNODE *node, void *link);
--
-- *Description*
--
-- The routine ies_set_node_link stores information specified by the
-- pointer link to the specified node problem. */

void ies_set_node_link(IESTREE *tree, IESNODE *node, void *link)
{     insist(tree == tree);
      node->link = link;
      return;
}

/*----------------------------------------------------------------------
-- ies_get_node_link - retrieve node problem specific information.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void *ies_get_node_link(IESTREE *tree, IESNODE *node);
--
-- *Returns*
--
-- The routine ies_get_node_link returns information previously stored
-- to the specified node problem. */

void *ies_get_node_link(IESTREE *tree, IESNODE *node)
{     insist(tree == tree);
      return node->link;
}

/*----------------------------------------------------------------------
-- ies_get_num_rows - determine number of rows.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- int ies_get_num_rows(IESTREE *tree);
--
-- *Returns*
--
-- The routine ies_get_num_rows returns number of rows in the current
-- node problem, which should exist. */

int ies_get_num_rows(IESTREE *tree)
{     /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_num_rows: current node problem not exist");
      return tree->m;
}

/*----------------------------------------------------------------------
-- ies_get_num_cols - determine number of columns.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- int ies_get_num_cols(IESTREE *tree);
--
-- *Returns*
--
-- The routine ies_get_num_cols returns number of columns in the current
-- node problem, which should exist. */

int ies_get_num_cols(IESTREE *tree)
{     /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_num_cols: current node problem not exist");
      return tree->n;
}

/*----------------------------------------------------------------------
-- ies_get_ith_row - determine pointer to i-th row.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESITEM *ies_get_ith_row(IESTREE *tree, int i);
--
-- *Returns*
--
-- The routine ies_get_ith_row returns a pointer to a master row, which
-- if the i-th row in the current node problem.
--
-- *Note*
--
-- Ordinal numbers are assigned to master rows on reviving the node
-- problem in some unpredictable manner. Moreover, on reviving the same
-- node problem the next time row ordinal numbers may be changed. */

IESITEM *ies_get_ith_row(IESTREE *tree, int i)
{     /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_ith_row: current node problem not exist");
      /* check the row number */
      if (!(1 <= i && i <= tree->m))
         fault("ies_get_ith_row: i = %d; row number out of range", i);
      /* return a pointer to the i-th row */
      return tree->item[i];
}

/*----------------------------------------------------------------------
-- ies_get_jth_col - determine pointer to j-th column.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESITEM *ies_get_jth_col(IESTREE *tree, int j);
--
-- *Returns*
--
-- The routine ies_get_jth_col returns a pointer to a master column,
-- which is the j-th column in the current node problem.
--
-- *Note*
--
-- Ordinal numbers are assigned to master columns on reviving the node
-- problem in some unpredictable manner. Moreover, on reviving the same
-- node problem the next time column ordinal numbers may be changed. */

IESITEM *ies_get_jth_col(IESTREE *tree, int j)
{     /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_jth_col: current node problem not exist");
      /* check the column number */
      if (!(1 <= j && j <= tree->n))
         fault("ies_get_jth_col: j = %d; column number out of range",
            j);
      /* return a pointer to the j-th column */
      return tree->item[tree->m + j];
}

/*----------------------------------------------------------------------
-- ies_get_row_bind - get ordinal number of master row in LP object.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- int ies_get_row_bind(IESTREE *tree, IESITEM *row);
--
-- *Returns*
--
-- The routine returns the ordinal number of the specified master row
-- in the internal LP problem object for the current node problem, which
-- should exist. If the specified row is not presented in the current
-- node problem, the routine returns zero.
--
-- *Note*
--
-- Ordinal numbers are assigned to master rows on reviving the node
-- problem in some unpredictable manner. Moreover, on reviving the same
-- node problem the next time row ordinal numbers may be changed. */

int ies_get_row_bind(IESTREE *tree, IESITEM *row)
{     /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_row_bind: current node problem not exist");
      /* check if the pointer refers to an existing master row */
      if (!(row->what == 'R' && row->count >= 0))
         fault("ies_get_row_bind: row = %p; invalid row pointer", row);
      return row->bind;
}

/*----------------------------------------------------------------------
-- ies_get_col_bind - get ordinal number of master column in LP object.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- int ies_get_col_bind(IESTREE *tree, IESITEM *col);
--
-- *Returns*
--
-- The routine returns the ordinal number of the specified master column
-- in the internal LP problem object for the current node problem, which
-- should exist. If the specified column is not presented in the current
-- node problem, the routine returns zero.
--
-- *Note*
--
-- Ordinal numbers are assigned to master columns on reviving the node
-- problem in some unpredictable manner. Moreover, on reviving the same
-- node problem the next time column ordinal numbers may be changed. */

int ies_get_col_bind(IESTREE *tree, IESITEM *col)
{     /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_col_bind: current node problem not exist");
      /* check if the pointer refers to an existing master column */
      if (!(col->what == 'C' && col->count >= 0))
         fault("ies_get_col_bind: col = %p; invalid column pointer",
            col);
      return col->bind;
}

/*----------------------------------------------------------------------
-- ies_get_row_bnds - obtain row bounds.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_get_row_bnds(IESTREE *tree, IESITEM *row, int *typx,
--    double *lb, double *ub);
--
-- *Description*
--
-- The routine ies_get_row_bnds stores the type, lower bound and upper
-- bound of the specified row, which this row has in the current node
-- problem, to locations, which the parameters typx, lb, and ub point
-- to, respectively.
--
-- If some of the parameters typx, lb, or ub is NULL, the corresponding
-- value is not stored.
--
-- Types and bounds have the following meaning:
--
--     Type          Bounds            Note
--    -------------------------------------------
--    LPX_FR   -inf <  x <  +inf   free variable
--    LPX_LO     lb <= x <  +inf   lower bound
--    LPX_UP   -inf <  x <=  ub    upper bound
--    LPX_DB     lb <= x <=  ub    double bound
--    LPX_FX           x  =  lb    fixed variable
--
-- where x is the corresponding auxiliary variable.
--
-- If the row has no lower bound, *lb is set to zero. If the row has no
-- upper bound, *ub is set to zero. If the row is of fixed type, *lb and
-- *ub are set to the same value. */

void ies_get_row_bnds(IESTREE *tree, IESITEM *row, int *typx,
      double *lb, double *ub)
{     int i;
      /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_row_bnds: current node problem not exist");
      /* check if the pointer refers to an existing master row */
      if (!(row->what == 'R' && row->count >= 0))
         fault("ies_get_row_bnds: row = %p; invalid row pointer", row);
      /* the specified master row should be presented in the current
         node problem */
      if (row->bind == 0)
         fault("ies_get_row_bnds: row = %p; master row missing in curre"
            "nt node problem");
      /* obtain type and bounds of the row */
      i = row->bind;
      insist(tree->item[i] == row);
      if (typx != NULL) *typx = tree->typx[i];
      if (lb != NULL) *lb = tree->lb[i];
      if (ub != NULL) *ub = tree->ub[i];
      return;
}

/*----------------------------------------------------------------------
-- ies_get_col_bnds - obtain column bounds.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_get_col_bnds(IESTREE *tree, IESITEM *col, int *typx,
--    double *lb, double *ub);
--
-- *Description*
--
-- The routine ies_get_col_bnds stores the type, lower bound and upper
-- bound of the specified column, which this column has in the current
-- node problem, to locations, which the parameters typx, lb, and ub
-- point to, respectively.
--
-- If some of the parameters typx, lb, or ub is NULL, the corresponding
-- value is not stored.
--
-- Types and bounds have the following meaning:
--
--     Type          Bounds            Note
--    -------------------------------------------
--    LPX_FR   -inf <  x <  +inf   free variable
--    LPX_LO     lb <= x <  +inf   lower bound
--    LPX_UP   -inf <  x <=  ub    upper bound
--    LPX_DB     lb <= x <=  ub    double bound
--    LPX_FX           x  =  lb    fixed variable
--
-- where x is the corresponding structural variable.
--
-- If the column has no lower bound, *lb is set to zero. If the column
-- has no upper bound, *ub is set to zero. If the column is of fixed
-- type, *lb and *ub are set to the same value. */

void ies_get_col_bnds(IESTREE *tree, IESITEM *col, int *typx,
      double *lb, double *ub)
{     int j;
      /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_col_bnds: current node problem not exist");
      /* check if the pointer refers to an existing master column */
      if (!(col->what == 'C' && col->count >= 0))
         fault("ies_get_col_bnds: col = %p; invalid master column point"
            "er", col);
      /* the specified master column should be presented in the current
         node problem */
      if (col->bind == 0)
         fault("ies_get_col_bnds: col = %p; master column missing in cu"
            "rrent node problem");
      /* obtain type and bounds of the column */
      j = tree->m + col->bind;
      insist(tree->item[j] == col);
      if (typx != NULL) *typx = tree->typx[j];
      if (lb != NULL) *lb = tree->lb[j];
      if (ub != NULL) *ub = tree->ub[j];
      return;
}

/*----------------------------------------------------------------------
-- ies_get_row_info - obtain row solution information.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_get_row_info(IESTREE *tree, IESITEM *row, int *tagx,
--    double *vx, double *dx);
--
-- *Description*
--
-- The routine ies_get_row_info stores status, primal and dual values,
-- which the specified row has in a basic solution of the current node
-- problem, to locations, which the parameters tagx, vx, and dx point
-- to, respectively.
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

void ies_get_row_info(IESTREE *tree, IESITEM *row, int *tagx,
      double *vx, double *dx)
{     int i;
      /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_row_info: current node problem not exist");
      /* check if the pointer refers to an existing master row */
      if (!(row->what == 'R' && row->count >= 0))
         fault("ies_get_row_info: row = %p; invalid row pointer", row);
      /* the specified master row should be presented in the current
         node problem */
      if (row->bind == 0)
         fault("ies_get_row_info: row = %p; master row missing in curre"
            "nt node problem");
      /* obtain status and primal and dual values of the row */
      i = row->bind;
      insist(tree->item[i] == row);
      lpx_get_row_info(tree->lp, i, tagx, vx, dx);
      return;
}

/*----------------------------------------------------------------------
-- ies_get_col_info - obtain column solution information.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_get_col_info(IESTREE *tree, IESITEM *col, int *tagx,
--    double *vx, double *dx);
--
-- *Description*
--
-- The routine ies_get_col_info stores status, primal and dual values,
-- which the specified column has in a basic solution of the current
-- node problem, to locations, which the parameters tagx, vx, and dx
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

void ies_get_col_info(IESTREE *tree, IESITEM *col, int *tagx,
      double *vx, double *dx)
{     int j;
      /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_get_col_info: current node problem not exist");
      /* check if the pointer refers to an existing master column */
      if (!(col->what == 'C' && col->count >= 0))
         fault("ies_get_col_info: col = %p; invalid master column point"
            "er", col);
      /* the specified master column should be presented in the current
         node problem */
      if (col->bind == 0)
         fault("ies_get_col_info: col = %p; master column missing in cu"
            "rrent node problem");
      /* obtain status and primal and dual values of the column */
      j = tree->m + col->bind;
      insist(tree->item[j] == col);
      lpx_get_col_info(tree->lp, j - tree->m, tagx, vx, dx);
      return;
}

/*----------------------------------------------------------------------
-- ies_eval_red_cost - compute reduced cost of master column.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- double ies_eval_red_cost(IESTREE *tree, IESITEM *col);
--
-- *Description*
--
-- The routine computes reduced cost of the specified master column.
--
-- If the column is presented in the current node problem, its reduced
-- cost is obtained directly from the problem object. If the column is
-- missing in the current node problem, its reduced cost is computed
-- using simplex multipliers for all rows (as presented as well as not
-- presented in the current node problem).
--
-- *Returns*
--
-- The routine returns reduced cost of the specified master column. */

double ies_eval_red_cost(IESTREE *tree, IESITEM *col)
{     double dj;
      /* the current node problem should exist */
      if (tree->this_node == NULL)
         fault("ies_eval_red_cost: current node problem not exist");
      /* check if the pointer refers to an existing master column */
      if (!(col->what == 'C' && col->count >= 0))
         fault("ies_eval_red_cost: col = %p; invalid master column poin"
            "ter", col);
      /* obtain or compute reduced cost */
      if (col->bind != 0)
      {  int j;
         /* the column is presented in the current node problem; obtain
            its reduced cost from the problem object */
         j = tree->m + col->bind;
         insist(tree->item[j] == col);
         lpx_get_col_info(tree->lp, j - tree->m, NULL, NULL, &dj);
      }
      else
      {  /* the column is missing in the current node problem; compute
            its reduced cost using simplex multipliers */
         IESITEM *row;
         IESELEM *elem;
         int i;
         double dx, pi;
         dj = col->coef;
         for (elem = col->ptr; elem != NULL; elem = elem->c_next)
         {  row = elem->row;
            if (row->bind != 0)
            {  /* the row is presented in the current node problem;
                  obtain its dual value and compute the corresponding
                  simplex multiplier */
               i = row->bind;
               insist(tree->item[i] == row);
               lpx_get_row_info(tree->lp, i, NULL, NULL, &dx);
               pi = lpx_get_row_coef(tree->lp, i) - dx;
            }
            else
            {  /* the row is missing in the current node problem, so it
                  should be considered as basic with zero dual value */
               dx = 0.0;
               pi = row->coef - dx;
               /* in the branch-and-cut framework it is not allowed to
                  have rows with non-zero objective coefficients, since
                  locally valid rows might involve wrong results */
               insist(pi == 0.0);
               continue;
            }
            /* sum up the next term of the reduced cost */
            dj += pi * elem->val;
         }
      }
      return dj;
}

/*----------------------------------------------------------------------
-- ies_set_row_bnds - set (change) row bounds.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_row_bnds(IESTREE *tree, IESITEM *row, int typx,
--    double lb, double ub);
--
-- *Description*
--
-- The routine ies_set_row_bnds sets (changes) type and bounds of the
-- specified row. New type and bounds are valid only for the current
-- node problem, i.e. changes are local.
--
-- The current node problem should exist and be active. The specified
-- row should be presented in the current node problem.
--
-- Parameters typx, lb, and ub specify respectively the type, the lower
-- bound, and the upper bound, which should be set for the row:
--
--     Type          Bounds            Note
--    -------------------------------------------
--    LPX_FR   -inf <  x <  +inf   free variable
--    LPX_LO     lb <= x <  +inf   lower bound
--    LPX_UP   -inf <  x <=  ub    upper bound
--    LPX_DB     lb <= x <=  ub    double bound
--    LPX_FX           x  =  lb    fixed variable
--
-- where x is the corresponding auxiliary variable.
--
-- If the row has no lower bound, the parameter lb is ignored. If the
-- row has no upper bound, the parameter ub is ignored. If the row is
-- of fixed type, the parameter lb is used, and the parameter ub is
-- ignored. */

void ies_set_row_bnds(IESTREE *tree, IESITEM *row, int typx, double lb,
      double ub)
{     IESNODE *node;
      int i;
      /* get a pointer to the current node problem */
      node = tree->this_node;
      /* the current node problem should exist */
      if (node == NULL)
         fault("ies_set_row_bnds: current node problem not exist");
      /* and should be active */
      if (node->count >= 0)
         fault("ies_set_row_bnds: attempt to modify inactive node probl"
            "em");
      /* check if the pointer refers to an existing master row */
      if (!(row->what == 'R' && row->count >= 0))
         fault("ies_set_row_bnds: row = %p; invalid master row pointer",
            row);
      /* the specified master row should be presented in the current
         node problem */
      if (row->bind == 0)
         fault("ies_set_row_bnds: row = %p; master row missing in curre"
            "nt node problem");
      /* set new type and bounds */
      i = row->bind;
      insist(tree->item[i] == row);
      tree->typx[i] = typx;
      switch (typx)
      {  case LPX_FR:
            tree->lb[i] = tree->ub[i] = 0.0;
            break;
         case LPX_LO:
            tree->lb[i] = lb, tree->ub[i] = 0.0;
            break;
         case LPX_UP:
            tree->lb[i] = 0.0, tree->ub[i] = ub;
            break;
         case LPX_DB:
            tree->lb[i] = lb, tree->ub[i] = ub;
            break;
         case LPX_FX:
            tree->lb[i] = tree->ub[i] = lb;
            break;
         default:
            fault("ies_set_row_bnds: typx = %d; invalid row type",
               typx);
      }
      /* change the row status (if necessary) */
      if (tree->tagx[i] != LPX_BS)
      {  int tagx = ies_default_tagx(row);
         if (tree->tagx[i] != tagx) tree->tagx[i] = tagx;
      }
      /* copy changes to the problem object */
      lpx_set_row_bnds(tree->lp, i, tree->typx[i], tree->lb[i],
         tree->ub[i]);
      lpx_set_row_stat(tree->lp, i, tree->tagx[i]);
      return;
}

/*----------------------------------------------------------------------
-- ies_set_col_bnds - set (change) column bounds.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_col_bnds(IESTREE *tree, IESITEM *col, int typx,
--    double lb, double ub);
--
-- *Description*
--
-- The routine ies_set_col_bnds sets (changes) type and bounds of the
-- specified column. New type and bounds are valid only for the current
-- node problem, i.e. changes are local.
--
-- The current node problem should exist and be active. The specified
-- column should be presented in the current node problem.
--
-- Parameters typx, lb, and ub specify respectively the type, the lower
-- bound, and the upper bound, which should be set for the column:
--
--     Type          Bounds            Note
--    -------------------------------------------
--    LPX_FR   -inf <  x <  +inf   free variable
--    LPX_LO     lb <= x <  +inf   lower bound
--    LPX_UP   -inf <  x <=  ub    upper bound
--    LPX_DB     lb <= x <=  ub    double bound
--    LPX_FX           x  =  lb    fixed variable
--
-- where x is the corresponding structural variable.
--
-- If the column has no lower bound, the parameter lb is ignored. If
-- the column has no upper bound, the parameter ub is ignored. If the
-- column is of fixed type, the parameter lb is used, and the parameter
-- ub is ignored. */

void ies_set_col_bnds(IESTREE *tree, IESITEM *col, int typx, double lb,
      double ub)
{     IESNODE *node;
      int j;
      /* get a pointer to the current node problem */
      node = tree->this_node;
      /* the current node problem should exist */
      if (node == NULL)
         fault("ies_set_col_bnds: current node problem not exist");
      /* and should be active */
      if (node->count >= 0)
         fault("ies_set_col_bnds: attempt to modify inactive node probl"
            "em");
      /* check if the pointer refers to an existing master column */
      if (!(col->what == 'C' && col->count >= 0))
         fault("ies_set_col_bnds: col = %p; invalid master column point"
            "er", col);
      /* the specified master column should be presented in the current
         node problem */
      if (col->bind == 0)
         fault("ies_set_col_bnds: col = %p; master column missing in cu"
            "rrent node problem");
      /* set new type and bounds */
      j = tree->m + col->bind;
      insist(tree->item[j] == col);
      tree->typx[j] = typx;
      switch (typx)
      {  case LPX_FR:
            tree->lb[j] = tree->ub[j] = 0.0;
            break;
         case LPX_LO:
            tree->lb[j] = lb, tree->ub[j] = 0.0;
            break;
         case LPX_UP:
            tree->lb[j] = 0.0, tree->ub[j] = ub;
            break;
         case LPX_DB:
            tree->lb[j] = lb, tree->ub[j] = ub;
            break;
         case LPX_FX:
            tree->lb[j] = tree->ub[j] = lb;
            break;
         default:
            fault("ies_set_col_bnds: typx = %d; invalid column type",
               typx);
      }
      /* change the column status (if necessary) */
      if (tree->tagx[j] != LPX_BS)
      {  int tagx = ies_default_tagx(col);
         if (tree->tagx[j] != tagx) tree->tagx[j] = tagx;
      }
      /* copy changes to the problem object */
      lpx_set_col_bnds(tree->lp, j - tree->m, tree->typx[j],
         tree->lb[j], tree->ub[j]);
      lpx_set_col_stat(tree->lp, j - tree->m, tree->tagx[j]);
      return;
}

/*----------------------------------------------------------------------
-- ies_set_obj_c0 - set (change) constant term of the obj. function.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_obj_c0(IESTREE *tree, double c0);
--
-- *Description*
--
-- The routine ies_set_obj_c0 sets (changes) the constant term of the
-- objective function for the current node problem, which should exist
-- and be active. */

void ies_set_obj_c0(IESTREE *tree, double c0)
{     IESNODE *node;
      /* get a pointer to the current node problem */
      node = tree->this_node;
      /* the current node problem should exist */
      if (node == NULL)
         fault("ies_set_obj_c0: current node problem not exist");
      /* and should be active */
      if (node->count >= 0)
         fault("ies_set_obj_c0: attempt to modify inactive node problem"
            );
      /* set new constant term of the objective function */
      tree->coef[0] = c0;
      lpx_set_obj_c0(tree->lp, c0);
      return;
}

void ies_set_row_coef(IESTREE *tree, IESITEM *row, double coef);

void ies_set_col_coef(IESTREE *tree, IESITEM *col, double coef);

/*----------------------------------------------------------------------
-- ies_set_row_stat - set (change) row status.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_row_stat(IESTREE *tree, IESITEM *row, int stat);
--
-- *Description*
--
-- The routine ies_set_row_stat sets (changes) status of the specified
-- row in the current node problem.
--
-- The current node problem should exist and be active. The specified
-- row should be presented in the current node problem.
--
-- New status of the row (of the corresponding auxiliary variable) is
-- determined by the parameter stat as follows:
--
-- LPX_BS   - make the row basic (make the constraint inactive);
-- LPX_NL   - make the row non-basic (make the constraint active);
-- LPX_NU   - make the row non-basic and set it to the upper bound; if
--            the row is not double-bounded, this status is equivalent
--            to LPX_NL (only in the case of this routine);
-- LPX_NF   - the same as LPX_NL (only in the case of this routine);
-- LPX_NS   - the same as LPX_NL (only in the case of this routine). */

void ies_set_row_stat(IESTREE *tree, IESITEM *row, int stat)
{     IESNODE *node;
      int i;
      /* get a pointer to the current node problem */
      node = tree->this_node;
      /* the current node problem should exist */
      if (node == NULL)
         fault("ies_set_row_stat: current node problem not exist");
      /* and should be active */
      if (node->count >= 0)
         fault("ies_set_row_stat: attempt to modify inactive node probl"
            "em");
      /* check if the pointer refers to an existing master row */
      if (!(row->what == 'R' && row->count >= 0))
         fault("ies_set_row_stat: row = %p; invalid master row pointer",
            row);
      /* the specified master row should be presented in the current
         node problem */
      if (row->bind == 0)
         fault("ies_set_row_stat: row = %p; master row missing in curre"
            "nt node problem");
      /* set row status */
      i = row->bind;
      insist(tree->item[i] == row);
      if (!(stat == LPX_BS || stat == LPX_NL || stat == LPX_NU ||
            stat == LPX_NF || stat == LPX_NS))
         fault("ies_set_row_stat: stat = %d; invalid status", stat);
      lpx_set_row_stat(tree->lp, i, stat);
      lpx_get_row_info(tree->lp, i, &stat, NULL, NULL);
      tree->tagx[i] = stat;
      return;
}

/*----------------------------------------------------------------------
-- ies_set_col_stat - set (change) column status.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_col_stat(IESTREE *tree, IESITEM *col, int stat);
--
-- *Description*
--
-- The routine ies_set_col_stat sets (changes) status of the specified
-- column in the current node problem.
--
-- The current node problem should exist and be active. The specified
-- column should be presented in the current node problem.
--
-- New status of the column (of the corresponding structural variable)
-- is determined by the parameter stat as follows:
--
-- LPX_BS   - make the column basic;
-- LPX_NL   - make the column non-basic;
-- LPX_NU   - make the column non-basic and set it to the upper bound;
--            if the column is not of double-bounded type, this status
--            is the same as LPX_NL (only in the case of this routine);
-- LPX_NF   - the same as LPX_NL (only in the case of this routine);
-- LPX_NS   - the same as LPX_NL (only in the case of this routine). */

void ies_set_col_stat(IESTREE *tree, IESITEM *col, int stat)
{     IESNODE *node;
      int j;
      /* get a pointer to the current node problem */
      node = tree->this_node;
      /* the current node problem should exist */
      if (node == NULL)
         fault("ies_set_col_stat: current node problem not exist");
      /* and should be active */
      if (node->count >= 0)
         fault("ies_set_col_stat: attempt to modify inactive node probl"
            "em");
      /* check if the pointer refers to an existing master column */
      if (!(col->what == 'C' && col->count >= 0))
         fault("ies_set_col_stat: col = %p; invalid master column point"
            "er", col);
      /* the specified master column should be presented in the current
         node problem */
      if (col->bind == 0)
         fault("ies_set_col_stat: col = %p; master column missing in cu"
            "rrent node problem");
      /* set column status */
      j = tree->m + col->bind;
      insist(tree->item[j] == col);
      if (!(stat == LPX_BS || stat == LPX_NL || stat == LPX_NU ||
            stat == LPX_NF || stat == LPX_NS))
         fault("ies_set_col_stat: stat = %d; invalid status", stat);
      lpx_set_col_stat(tree->lp, j - tree->m, stat);
      lpx_get_col_info(tree->lp, j - tree->m, &stat, NULL, NULL);
      tree->tagx[j] = stat;
      return;
}

/*----------------------------------------------------------------------
-- ies_get_lp_object - get pointer to internal LP object.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- LPX *ies_get_lp_object(IESTREE *tree);
--
-- *Returns*
--
-- The routine ies_get_lp_object returns a pointer to the internal LP
-- object for the specified implicit enumeration tree.
--
-- The internal LP object is created once on creating the tree, so the
-- pointer to it remains valid until the tree has been deleted.
--
-- The LP object is a representation of the linear programming problem,
-- which corresponds to the current node problem. If the latter doesn't
-- exist, the LP object corresponds to some undefined linear programming
-- problem (which, nevertheless, is a *valid* problem).
--
-- The internal LP object *must not* be modified directly using any api
-- routines with the prefix 'lpx'. All necessary modifications of the
-- current node problem should be performed using the 'ies' routines.
-- However, the 'lpx' api routines can be used for obtaining information
-- from the LP object. */

LPX *ies_get_lp_object(IESTREE *tree)
{     LPX *lp = tree->lp;
      return lp;
}

/*----------------------------------------------------------------------
-- ies_solve_node - solve the current node problem.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- int ies_solve_node(IESTREE *tree);
--
-- *Description*
--
-- The routine ies_solve_node solves the current node problem, which
-- should exist, using the api routine lpx_simplex (for its description
-- see the document "GLPK Reference Manual".
--
-- *Returns*
--
-- The routine ies_solve_node returns a code reported by the routine
-- lpx_simplex. */

int ies_solve_node(IESTREE *tree)
{     int i, j, ret;
      if (tree->this_node == NULL)
         fault("ies_solve_node: current node problem not exist");
      /* solve the current node problem */
      ret = lpx_simplex(tree->lp);
      /* copy statuses of rows and columns from the internal LP object
         to the problem instance */
      for (i = 1; i <= tree->m; i++)
         lpx_get_row_info(tree->lp, i, &tree->tagx[i], NULL, NULL);
      for (j = 1; j <= tree->n; j++)
         lpx_get_col_info(tree->lp, j, &tree->tagx[tree->m+j], NULL,
            NULL);
      /* return the code reported by the solver routine */
      return ret;
}

/* eof */
