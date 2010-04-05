/* glpies.h */

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

#ifndef _GLPIES_H
#define _GLPIES_H

#include "glplpx.h"

#define ies_create_tree       glp_ies_create_tree
#define ies_add_master_row    glp_ies_add_master_row
#define ies_add_master_col    glp_ies_add_master_col
#define ies_next_master_row   glp_ies_next_master_row
#define ies_next_master_col   glp_ies_next_master_col
#define ies_what_item         glp_ies_what_item
#define ies_set_item_link     glp_ies_set_item_link
#define ies_get_item_link     glp_ies_get_item_link
#define ies_clean_master_set  glp_ies_clean_master_set
#define ies_del_master_row    glp_ies_del_master_row
#define ies_del_master_col    glp_ies_del_master_col
#define ies_set_item_filt     glp_ies_set_item_filt
#define ies_set_item_hook     glp_ies_set_item_hook
#define ies_delete_tree       glp_ies_delete_tree

#define ies_default_tagx      glp_ies_default_tagx
#define ies_create_node       glp_ies_create_node
#define ies_revive_node       glp_ies_revive_node
#define ies_add_rows          glp_ies_add_rows
#define ies_add_cols          glp_ies_add_cols
#define ies_del_items         glp_ies_del_items
#define ies_delete_node       glp_ies_delete_node
#define ies_prune_branch      glp_ies_prune_branch
#define ies_set_node_hook     glp_ies_set_node_hook

#define ies_get_node_level    glp_ies_get_node_level
#define ies_get_node_count    glp_ies_get_node_count
#define ies_get_next_node     glp_ies_get_next_node
#define ies_get_prev_node     glp_ies_get_prev_node
#define ies_get_this_node     glp_ies_get_this_node
#define ies_set_node_link     glp_ies_set_node_link
#define ies_get_node_link     glp_ies_get_node_link
#define ies_get_num_rows      glp_ies_get_num_rows
#define ies_get_num_cols      glp_ies_get_num_cols
#define ies_get_ith_row       glp_ies_get_ith_row
#define ies_get_jth_col       glp_ies_get_jth_col
#define ies_get_row_bind      glp_ies_get_row_bind
#define ies_get_col_bind      glp_ies_get_col_bind
#define ies_get_row_bnds      glp_ies_get_row_bnds
#define ies_get_col_bnds      glp_ies_get_col_bnds
#define ies_get_row_info      glp_ies_get_row_info
#define ies_get_col_info      glp_ies_get_col_info
#define ies_eval_red_cost     glp_ies_eval_red_cost
#define ies_set_row_bnds      glp_ies_set_row_bnds
#define ies_set_col_bnds      glp_ies_set_col_bnds
#define ies_set_obj_c0        glp_ies_set_obj_c0
#define ies_set_row_stat      glp_ies_set_row_stat
#define ies_set_col_stat      glp_ies_set_col_stat
#define ies_get_lp_object     glp_ies_get_lp_object
#define ies_solve_node        glp_ies_solve_node

/*----------------------------------------------------------------------
-- This module implements Implicit Enumeration Suite (IES), which is
-- a k-nary rooted tree, where each node corresponds to some (pure) LP
-- problem.
--
-- All node problems in the tree are based on the same set of so called
-- master rows and master columns. A particular node problem is defined
-- by some subset of the master set.
--
-- The IES program object is mainly intended for implementing implicit
-- enumeration methods like branch-and-bound and branch-and-cut. */

typedef struct IESTREE IESTREE;
typedef struct IESITEM IESITEM;
typedef struct IESELEM IESELEM;
typedef struct IESNODE IESNODE;
typedef struct IESDIFF IESDIFF;
typedef struct IESBNDS IESBNDS;
typedef struct IESCOEF IESCOEF;
typedef struct IESSTAT IESSTAT;

struct IESTREE
{     /* implicit enumeration tree */
      /*--------------------------------------------------------------*/
      /* master set */
      DMP *item_pool;
      /* memory pool for IESITEM objects */
      DMP *str_pool;
      /* memory pool for segmented character strings */
      DMP *elem_pool;
      /* memory pool for IESELEM objects */
      int nmrs;
      /* number of master rows (except deleted ones) */
      int ndrs;
      /* number of deleted master rows, which are still in the set */
      IESITEM *first_row;
      /* pointer to the (chronologically) first master row */
      IESITEM *last_row;
      /* pointer to the (chronologically) last master row */
      int nmcs;
      /* number of master columns (except deleted ones) */
      int ndcs;
      /* number of deleted master columns, which are still in the set */
      IESITEM *first_col;
      /* pointer to the (chronologically) first master column */
      IESITEM *last_col;
      /* pointer to the (chronologically) last master column */
      void *filt_info;
      /* transit pointer passed to the item filter routine */
      int (*item_filt)(void *info, IESITEM *item);
      /* this item filter routine is called when the reference count of
         some master row/column, pointer to which is passed to this
         routine, reaches zero; if this routine returns zero, the item
         is deleted from the master set; otherwise, the item is kept in
         the master set; if this entry point is NULL, it is equivalent
         to a filter routine, which always returns zero */
      void *item_info;
      /* transit pointer passed to the item hook routine */
      void (*item_hook)(void *info, IESITEM *item);
      /* this item hook routine is called when some master row/column,
         pointer to which is passed to this routine, is being deleted
         from the master set */
      /*--------------------------------------------------------------*/
      /* implicit enumeration tree */
      DMP *node_pool;
      /* memory pool for IESNODE objects */
      DMP *diff_pool;
      /* memory pool for IESDIFF objects */
      DMP *bnds_pool;
      /* memory pool for IESBNDS objects */
      DMP *coef_pool;
      /* memory pool for IESCOEF objects */
      DMP *stat_pool;
      /* memory pool for IESSTAT objects */
      int size;
      /* size of the tree (i.e. number of nodes in the tree) */
      IESNODE *root_node;
      /* pointer to the root node; the root node is created before any
         other nodes, so it is also the (chronologically) first in the
         node list */
      IESNODE *last_node;
      /* pointer to a node, which is the (chronologically) last in the
         node list */
      IESNODE *this_node;
      /* pointer to the current node (which may be active as well as
         inactive); NULL means the current node doesn't exist */
      void *node_info;
      /* transit pointer passed to the node hook routine */
      void (*node_hook)(void *info, IESNODE *node);
      /* this node hook routine is called when some node problem,
         pointer to which is passed to this routine, is being deleted
         from the enumeration tree */
      /*--------------------------------------------------------------*/
      /* LP problem instance */
      /* if the field this_node is not NULL, the instance corresponds
         to the current node problem; when the current problem becomes
         undefined and the field this_node is set to NULL, the instance
         is kept unchanged (this allows efficiently reviving another
         node problem in the case it differs from the instance only in
         a few rows and columns, i.e. not from scratch) */
      int m_max;
      /* maximal number of rows */
      int n_max;
      /* maximal number of columns */
      int m;
      /* number of rows in the problem instance */
      int n;
      /* number of columns in the problem instance */
      IESITEM **item; /* IESITEM *item[1+m_max+n_max]; */
      /* item[0] is not used;
         item[i], 1 <= i <= m, is a pointer to the master row, which
         corresponds to the i-th row of the instance;
         item[m+j], 1 <= j <= n, is a pointer to the master column,
         which corresponds to the j-th column of the instance;
         item[k] = NULL means that the corresponding master row/column
         was deleted (this may happen only if the current node problem
         doesn't exist) */
      int *typx; /* int typx[1+m_max+n_max]; */
      /* typx[0] is not used;
         typx[k], 1 <= k <= m+n, is the type of the variable x[k]:
         LPX_FR - free variable  (-inf <  x[k] < +inf)
         LPX_LO - lower bound    (l[k] <= x[k] < +inf)
         LPX_UP - upper bound    (-inf <  x[k] <= u[k])
         LPX_DB - double bound   (l[k] <= x[k] <= u[k])
         LPX_FX - fixed variable (l[k]  = x[k]  = u[k]) */
      double *lb; /* double lb[1+m_max+n_max]; */
      /* lb[0] is not used;
         lb[k], 1 <= k <= m+n, is an lower bound of the variable x[k];
         if x[k] has no lower bound, lb[k] is zero */
      double *ub; /* double ub[1+m_max+n_max]; */
      /* ub[0] is not used;
         ub[k], 1 <= k <= m+n, is an upper bound of the variable x[k];
         if x[k] has no upper bound, ub[k] is zero; if x[k] is of fixed
         type, ub[k] is equal to lb[k] */
      double *coef; /* double coef[1+m_max+n_max]; */
      /* coef[0] is a constant term of the objective function;
         coef[k], 1 <= k <= m+n, is a coefficient of the objective
         function at the variable x[k] (note that auxiliary variables
         also may have non-zero objective coefficients) */
      int *tagx; /* int tagx[1+m_max+n_max]; */
      /* tagx[0] is not used;
         tagx[k], 1 <= k <= m+n, is the status of the variable x[k]:
         LPX_BS - basic variable
         LPX_NL - non-basic variable on lower bound
         LPX_NU - non-basic variable on upper bound
         LPX_NF - non-basic free variable
         LPX_NS - non-basic fixed variable */
      LPX *lp;
      /* the problem instance in a solver specific format */
};

struct IESITEM
{     /* master row/column */
      int what;
      /* this field determines what this item is:
         'R' - master row
         'C' - master column */
      STR *name;
      /* row/column name (1 to 255 chars) or NULL, if row/column has no
         assigned name */
      int typx;
      /* default row/column type */
      double lb;
      /* default lower bound */
      double ub;
      /* default upper bound */
      double coef;
      /* default coefficient in the objective function */
      IESELEM *ptr;
      /* pointer to the list of non-zero constraint coefficients, which
         belong to this row/column */
      int count;
      /* count of references to this row/column from the add_them patch
         lists (see below); row/column can be deleted only if its count
         is zero and it is not used in the current problem; negative
         count means that this row/column is deleted (physical deletion
         is performed automatically at appropriate moments) */
      int bind;
      /* in the case of row bind = i (1 <= i <= tree->m) means this
         item is referenced as the i-th row of the problem instance and
         therefore tree->item[i] is a pointer to this item;
         in the case of column bind = j (1 <= j <= tree->n) means this
         item is referenced as the j-th column of the problem instance
         and therefore tree->item[tree->m+j] is a pointer to this item;
         bind = 0 means that this master row/column is not used in the
         problem instance */
      void *link;
      /* reserved for row/column specific information */
      IESITEM *prev;
      /* pointer to the (chronologically) previous master row/column */
      IESITEM *next;
      /* pointer to the (chronologically) next master row/column */
};

struct IESELEM
{     /* constraint coefficient */
      IESITEM *row;
      /* pointer to the master row, which this element belongs to */
      IESITEM *col;
      /* pointer to the master column, which this element belongs to */
      double val;
      /* numerical (non-zero) value of this element */
      IESELEM *r_next;
      /* pointer to the next element in the same master row */
      IESELEM *c_next;
      /* pointer to the next element in the same master column */
};

struct IESNODE
{     /* node problem */
      IESNODE *up;
      /* pointer to the parent node; NULL means that this node is the
         root of the tree */
      int level;
      /* this node level (the root node has the level 0) */
      int count;
      /* if count < 0, this node is active, and therefore has no child
         nodes; if count >= 0, this node is inactive, and in this case
         count is number of its child nodes */
      int m;
      /* number of rows in this node problem */
      int n;
      /* number of columns in this node problem */
      void *link;
      /* reserved for node specific information */
      IESNODE *prev;
      /* pointer to the (chronologically) previous node */
      IESNODE *next;
      /* pointer to the (chronologically) next node */
      void *temp;
      /* auxiliary pointer */
      /*--------------------------------------------------------------*/
      /* patch lists; these lists contain information, which should be
         applied to the parent node problem (or to an empty problem if
         this node is the root of the tree) in order to construct this
         node problem; since active nodes are modfiable, if the current
         node is active, all its patch lists are temporarily empty */
      IESDIFF *del_them;
      /* pointer to the list of master rows/columns, which are presented
         in the parent node problem and missing in this node problem, so
         on reviving this node problem they should be removed */
      IESDIFF *add_them;
      /* pointer to the list of master rows/columns, which are missing
         in the parent node problem and presented in this node problem,
         so on reviving this node problem they should be added */
      IESBNDS *bnds_patch;
      /* pointer to the list of master rows/columns, which are presented
         in this node problem and whose bounds should be changed */
      IESCOEF *coef_patch;
      /* pointer to the list of master rows/columns, which are presented
         in this node problem and whose objective coefficients should be
         changed */
      IESSTAT *stat_patch;
      /* pointer to the list of master rows/columns, which are presented
         in this node problem and whose status should be changed */
};

struct IESDIFF
{     /* addition/deletion patch entry */
      IESITEM *item;
      /* pointer to master row/column */
      IESDIFF *next;
      /* pointer to the next patch entry */
};

struct IESBNDS
{     /* type/bounds patch entry */
      IESITEM *item;
      /* pointer to master row/column */
      int typx;
      /* new row/column type */
      double lb;
      /* new lower bound */
      double ub;
      /* new upper bound */
      IESBNDS *next;
      /* pointer to the next patch entry */
};

struct IESCOEF
{     /* objective coefficient patch entry */
      IESITEM *item;
      /* pointer to master row/column; NULL means this entry changes
         the constant term of the objective function */
      double coef;
      /* new objective coefficient or the constant term */
      IESCOEF *next;
      /* pointer to the next patch entry */
};

struct IESSTAT
{     /* status patch entry */
      IESITEM *item;
      /* pointer to master row/column */
      int tagx;
      /* new row/column status */
      IESSTAT *next;
      /* pointer to the next patch entry */
};

/* routines in glpies1.c ---------------------------------------------*/

IESTREE *ies_create_tree(void);
/* create implicit enumeration tree */

IESITEM *ies_add_master_row(IESTREE *tree, char *name, int typx,
      double lb, double ub, double coef, int len, IESITEM *col[],
      double val[]);
/* add master row to the master set */

IESITEM *ies_add_master_col(IESTREE *tree, char *name, int typx,
      double lb, double ub, double coef, int len, IESITEM *row[],
      double val[]);
/* add master column to the master set */

IESITEM *ies_next_master_row(IESTREE *tree, IESITEM *row);
/* get pointer to the next master row */

IESITEM *ies_next_master_col(IESTREE *tree, IESITEM *col);
/* get pointer to the next master column */

int ies_what_item(IESTREE *tree, IESITEM *item);
/* determine of what sort master item is */

void ies_set_item_link(IESTREE *tree, IESITEM *item, void *link);
/* store master item specific information */

void *ies_get_item_link(IESTREE *tree, IESITEM *item);
/* retrieve master item specific information */

void ies_clean_master_set(IESTREE *tree);
/* clean the master set */

void ies_del_master_row(IESTREE *tree, IESITEM *row);
/* delete master row from the master set */

void ies_del_master_col(IESTREE *tree, IESITEM *col);
/* delete master column from the master set */

void ies_set_item_filt(IESTREE *tree,
      void *info, int (*filt)(void *info, IESITEM *item));
/* install item filter routine */

void ies_set_item_hook(IESTREE *tree,
      void *info, void (*hook)(void *info, IESITEM *item));
/* install item hook routine */

void ies_delete_tree(IESTREE *tree);
/* delete implicit enumeration tree */

/* routines in glpies2.c ---------------------------------------------*/

int ies_default_tagx(IESITEM *item);
/* determine default status of master row/column */

IESNODE *ies_create_node(IESTREE *tree, IESNODE *parent);
/* create new node problem */

void ies_revive_node(IESTREE *tree, IESNODE *node);
/* make specified node problem current */

void ies_add_rows(IESTREE *tree, int nrs, IESITEM *row[]);
/* add master rows to the current node problem */

void ies_add_cols(IESTREE *tree, int ncs, IESITEM *col[]);
/* add master columns to the current node problem */

void ies_del_items(IESTREE *tree);
/* delete rows/columns from the current node problem */

void ies_delete_node(IESTREE *tree, IESNODE *node);
/* delete specified node problem */

void ies_prune_branch(IESTREE *tree, IESNODE *node);
/* prune branch of the tree */

void ies_set_node_hook(IESTREE *tree,
      void *info, void (*hook)(void *info, IESNODE *node));
/* install node hook routine */

/* routines in glpies3.c ---------------------------------------------*/

int ies_get_node_level(IESTREE *tree, IESNODE *node);
/* get node depth level */

int ies_get_node_count(IESTREE *tree, IESNODE *node);
/* get node reference count */

IESNODE *ies_get_next_node(IESTREE *tree, IESNODE *node);
/* get pointer to next node */

IESNODE *ies_get_prev_node(IESTREE *tree, IESNODE *node);
/* get pointer to previous node */

IESNODE *ies_get_this_node(IESTREE *tree);
/* get pointer to current node */

void ies_set_node_link(IESTREE *tree, IESNODE *node, void *link);
/* store node problem specific information */

void *ies_get_node_link(IESTREE *tree, IESNODE *node);
/* retrieve node problem specific information */

int ies_get_num_rows(IESTREE *tree);
/* determine number of rows */

int ies_get_num_cols(IESTREE *tree);
/* determine number of columns */

IESITEM *ies_get_ith_row(IESTREE *tree, int i);
/* determine pointer to i-th row */

IESITEM *ies_get_jth_col(IESTREE *tree, int j);
/* determine pointer to j-th column */

int ies_get_row_bind(IESTREE *tree, IESITEM *row);
/* get ordinal number of master row in LP object */

int ies_get_col_bind(IESTREE *tree, IESITEM *col);
/* get ordinal number of master column in LP object */

void ies_get_row_bnds(IESTREE *tree, IESITEM *row, int *typx,
      double *lb, double *ub);
/* obtain row bounds */

void ies_get_col_bnds(IESTREE *tree, IESITEM *col, int *typx,
      double *lb, double *ub);
/* obtain column bounds */

void ies_get_row_info(IESTREE *tree, IESITEM *row, int *tagx,
      double *vx, double *dx);
/* obtain row solution information */

void ies_get_col_info(IESTREE *tree, IESITEM *col, int *tagx,
      double *vx, double *dx);
/* obtain column solution information */

double ies_eval_red_cost(IESTREE *tree, IESITEM *col);
/* compute reduced cost of master column */

void ies_set_row_bnds(IESTREE *tree, IESITEM *row, int typx, double lb,
      double ub);
/* set (change) row bounds */

void ies_set_col_bnds(IESTREE *tree, IESITEM *col, int typx, double lb,
      double ub);
/* set (change) column bounds */

void ies_set_obj_c0(IESTREE *tree, double c0);
/* set (change) constant term of the objective function */

void ies_set_row_stat(IESTREE *tree, IESITEM *row, int stat);
/* set (change) row status */

void ies_set_col_stat(IESTREE *tree, IESITEM *col, int stat);
/* set (change) column status */

LPX *ies_get_lp_object(IESTREE *tree);
/* get pointer to internal LP object */

int ies_solve_node(IESTREE *tree);
/* solve the current node problem */

#endif

/* eof */
