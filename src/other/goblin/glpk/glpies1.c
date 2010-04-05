/* glpies1.c */

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

#include <stddef.h>
#include "glpies.h"
#include "glplib.h"

/*----------------------------------------------------------------------
-- ies_create_tree - create implicit enumeration tree.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESTREE *ies_create_tree(void);
--
-- *Description*
--
-- The routine ies_create_tree creates an implicit enumeration tree,
-- which initially is empty and has the empty master set.
--
-- *Returns*
--
-- The routine returns a pointer to the created tree. */

IESTREE *ies_create_tree(void)
{     IESTREE *tree;
      int m_max = 50, n_max = 100;
      tree = umalloc(sizeof(IESTREE));
      tree->item_pool = dmp_create_pool(sizeof(IESITEM));
      tree->str_pool = create_str_pool();
      tree->elem_pool = dmp_create_pool(sizeof(IESELEM));
      tree->nmrs = tree->ndrs = 0;
      tree->first_row = tree->last_row = NULL;
      tree->nmcs = tree->ndcs = 0;
      tree->first_col = tree->last_col = NULL;
      tree->filt_info = NULL, tree->item_filt = NULL;
      tree->item_info = NULL, tree->item_hook = NULL;
      tree->node_pool = dmp_create_pool(sizeof(IESNODE));
      tree->diff_pool = dmp_create_pool(sizeof(IESDIFF));
      tree->bnds_pool = dmp_create_pool(sizeof(IESBNDS));
      tree->coef_pool = dmp_create_pool(sizeof(IESCOEF));
      tree->stat_pool = dmp_create_pool(sizeof(IESSTAT));
      tree->size = 0;
      tree->root_node = tree->last_node = tree->this_node = NULL;
      tree->node_info = NULL, tree->node_hook = NULL;
      tree->m_max = m_max, tree->n_max = n_max;
      tree->m = tree->n = 0;
      tree->item = ucalloc(1+m_max+n_max, sizeof(IESITEM *));
      tree->typx = ucalloc(1+m_max+n_max, sizeof(int));
      tree->lb = ucalloc(1+m_max+n_max, sizeof(double));
      tree->ub = ucalloc(1+m_max+n_max, sizeof(double));
      tree->coef = ucalloc(1+m_max+n_max, sizeof(double));
      tree->tagx = ucalloc(1+m_max+n_max, sizeof(int));
      tree->lp = lpx_create_prob();
      return tree;
}

/*----------------------------------------------------------------------
-- time_to_clean - determine if it is time to clean the master set.
--
-- This routine returns non-zero if number of master items marked as
-- deleted exceeds 10 percents of total number of items in the master
-- set. */

static int time_to_clean(IESTREE *tree) /* not tested */
{     int marked = tree->ndrs + tree->ndcs;
      return (marked > (tree->nmrs + tree->nmcs + marked) / 10);
}

/*----------------------------------------------------------------------
-- ies_add_master_row - add master row to the master set.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESITEM *ies_add_master_row(IESTREE *tree, char *name, int typx,
--    double lb, double ub, double coef, int len, IESITEM *col[],
--    double val[]);
--
-- *Description*
--
-- The routine ies_add_master_row adds a new master row to the master
-- set and assigns the specified attributes to it.
--
-- The parameter name is a symbolic name (1 to 255 graphic characters)
-- assigned to the master row. If this parameter is NULL, no symbolic
-- name is assigned.
--
-- The parameters typx, lb, and ub specify, respectively, default type,
-- lower bound, and upper bound, which should be assigned to the master
-- row as follows:
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
-- ignored.
--
-- The parameter coef specifies default coefficient of the objective
-- function at the corresponding auxiliary variable.
--
-- The parameters len, col, and val specify constraint coefficients,
-- which should be assigned to the master row. Column pointers should
-- be placed in locations col[1], ..., col[len] and should refer to
-- existing master columns, and numerical values of the corresponding
-- constraint coefficients should be placed in locations val[1], ...,
-- val[len]. Zero coefficients and multiplets (i.e. coefficients that
-- refer to the same column) are not allowed.
--
-- Note that attributes assigned to the master row can't be changed in
-- the future. However, adding new master columns may involve appearing
-- new constraint coefficients in existing master rows.
--
-- *Returns*
--
-- The routine returns a pointer to the created master row. */

IESITEM *ies_add_master_row(IESTREE *tree, char *name, int typx,
      double lb, double ub, double coef, int len, IESITEM *col[],
      double val[])
{     IESITEM *row;
      IESELEM *elem;
      int t;
      /* clean the master set if it's time to do that */
      if (time_to_clean(tree)) ies_clean_master_set(tree);
      /* create and initialize new master row */
      row = dmp_get_atom(tree->item_pool);
      row->what = 'R';
      if (name == NULL)
         row->name = NULL;
      else
      {  if (lpx_check_name(name))
            fault("ies_add_master_row: invalid row name");
         row->name = create_str(tree->str_pool);
         set_str(row->name, name);
      }
      row->typx = typx;
      switch (typx)
      {  case LPX_FR:
            row->lb = row->ub = 0.0;
            break;
         case LPX_LO:
            row->lb = lb, row->ub = 0.0;
            break;
         case LPX_UP:
            row->lb = 0.0, row->ub = ub;
            break;
         case LPX_DB:
            row->lb = lb, row->ub = ub;
            break;
         case LPX_FX:
            row->lb = row->ub = lb;
            break;
         default:
            fault("ies_add_master_row: typx = %d; invalid row type",
               typx);
      }
      row->coef = coef;
      row->ptr = NULL;
      row->count = 0;
      row->bind = 0;
      row->link = NULL;
      row->prev = tree->last_row;
      row->next = NULL;
      /* add the new master row to the end of the row list */
      tree->nmrs++;
      if (row->prev == NULL)
         tree->first_row = row;
      else
         row->prev->next = row;
      tree->last_row = row;
      /* build the list of the specified constraint coefficients */
      if (!(0 <= len && len <= tree->nmcs))
         fault("ies_add_master_row: len = %d; invalid row length",
            len);
      for (t = 1; t <= len; t++)
      {  /* check if col[t] refers to an existing master column */
         if (!(col[t]->what == 'C' && col[t]->count >= 0))
            fault("ies_add_master_row: col[%d] = %p; invalid column poi"
               "nter", t, col[t]);
         /* check if val[t] is non-zero */
         if (val[t] == 0.0)
            fault("ies_add_master_row: val[%d] = 0; zero coefficient no"
               "t allowed", t);
         /* create new constraint coefficient */
         elem = dmp_get_atom(tree->elem_pool);
         elem->row = row, elem->col = col[t], elem->val = val[t];
         elem->r_next = elem->row->ptr, elem->c_next = elem->col->ptr;
         /* check for multiplets */
         if (elem->c_next != NULL && elem->c_next->row == row)
            fault("ies_add_master_row: col[%d] = %p; duplicate column p"
               "ointer not allowed", t, col[t]);
         /* add the new constraint coefficient to both row and column
            linked lists */
         elem->row->ptr = elem->col->ptr = elem;
      }
      /* return a pointer to the created master row */
      return row;
}

/*----------------------------------------------------------------------
-- ies_add_master_col - add master column to the master set.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESITEM *ies_add_master_col(IESTREE *tree, char *name, int typx,
--    double lb, double ub, double coef, int len, IESITEM *row[],
--    double val[]);
--
-- *Description*
--
-- The routine ies_add_master_col adds a new master column to the master
-- set and assigns the specified attributes to it.
--
-- The parameter name is a symbolic name (1 to 255 graphic characters)
-- assigned to the master column. If this parameter is NULL, no symbolic
-- name is assigned.
--
-- The parameters typx, lb, and ub specify, respectively, default type,
-- lower bound, and upper bound, which should be assigned to the master
-- column as follows:
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
-- ub is ignored.
--
-- The parameter coef specifies default coefficient of the objective
-- function at the corresponding structural variable.
--
-- The parameters len, row, and val, specify constraint coefficients,
-- which should be assigned to the master column. Row pointers should
-- be placed in locations row[1], ..., row[len] and should refer to
-- existing master rows, and numerical values of the corresponding
-- constraint coefficients should be placed in locations val[1], ...,
-- val[len]. Zero coefficients and multiplets (i.e. coefficients that
-- refer to the same row) are not allowed.
--
-- Note that attributes assigned to the master column can't be changed
-- in the future. However, adding new master rows may involve appearing
-- new constraint coefficients in existing master columns.
--
-- *Returns*
--
-- The routine returns a pointer to the created master column. */

IESITEM *ies_add_master_col(IESTREE *tree, char *name, int typx,
      double lb, double ub, double coef, int len, IESITEM *row[],
      double val[])
{     IESITEM *col;
      IESELEM *elem;
      int t;
      /* clean the master set if it's time to do that */
      if (time_to_clean(tree)) ies_clean_master_set(tree);
      /* create and initialize new master column */
      col = dmp_get_atom(tree->item_pool);
      col->what = 'C';
      if (name == NULL)
         col->name = NULL;
      else
      {  if (lpx_check_name(name))
            fault("ies_add_master_col: invalid column name");
         col->name = create_str(tree->str_pool);
         set_str(col->name, name);
      }
      col->typx = typx;
      switch (typx)
      {  case LPX_FR:
            col->lb = col->ub = 0.0;
            break;
         case LPX_LO:
            col->lb = lb, col->ub = 0.0;
            break;
         case LPX_UP:
            col->lb = 0.0, col->ub = ub;
            break;
         case LPX_DB:
            col->lb = lb, col->ub = ub;
            break;
         case LPX_FX:
            col->lb = col->ub = lb;
            break;
         default:
            fault("ies_add_master_col: typx = %d; invalid column type",
               typx);
      }
      col->coef = coef;
      col->ptr = NULL;
      col->count = 0;
      col->bind = 0;
      col->link = NULL;
      col->prev = tree->last_col;
      col->next = NULL;
      /* add the new master column to the end of the column list */
      tree->nmcs++;
      if (col->prev == NULL)
         tree->first_col = col;
      else
         col->prev->next = col;
      tree->last_col = col;
      /* build the list of the specified constraint coefficients */
      if (!(0 <= len && len <= tree->nmrs))
         fault("ies_add_master_col: len = %d; invalid column length",
            len);
      for (t = 1; t <= len; t++)
      {  /* check if row[t] refers to an existing master row */
         if (!(row[t]->what == 'R' && row[t]->count >= 0))
            fault("ies_add_master_col: row[%d] = %p; invalid row pointe"
               "r", t, row[t]);
         /* check if val[t] is non-zero */
         if (val[t] == 0.0)
            fault("ies_add_master_col: val[%d] = 0; zero coefficient no"
               "t allowed", t);
         /* create new constraint coefficients */
         elem = dmp_get_atom(tree->elem_pool);
         elem->row = row[t], elem->col = col, elem->val = val[t];
         elem->r_next = elem->row->ptr, elem->c_next = elem->col->ptr;
         /* check for multiplets */
         if (elem->r_next != NULL && elem->r_next->col == col)
            fault("ies_add_master_col: row[%d] = %p; duplicate row poin"
               "ter not allowed", t, row[t]);
         /* add the new constraint coefficient to both row and column
            linked lists */
         elem->row->ptr = elem->col->ptr = elem;
      }
      /* return a pointer to the created master column */
      return col;
}

/*----------------------------------------------------------------------
-- ies_next_master_row - get pointer to the next master row.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESITEM *ies_next_master_row(IESTREE *tree, IESITEM *row);
--
-- *Returns*
--
-- If the parameter row is NULL, the routine returns a pointer to the
-- first master row, which was created before any other master rows, or
-- NULL, if the master set is empty. Otherwise the parameter row should
-- specify some master row, in which case the routine returns a pointer
-- to the next master row, which was created later than the specified
-- row, or NULL, if there is no next row in the master set. */

IESITEM *ies_next_master_row(IESTREE *tree, IESITEM *row)
{     IESITEM *start;
      if (row == NULL)
         start = tree->first_row;
      else
      {  if (!(row->what == 'R' && row->count >= 0))
            fault("ies_next_master_row: row = %p; invalid row pointer",
               row);
         start = row->next;
      }
      for (row = start; row != NULL; row = row->next)
      {  insist(row->what == 'R');
         if (row->count >= 0) break;
      }
      return row;
}

/*----------------------------------------------------------------------
-- ies_next_master_col - get pointer to the next master column.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESITEM *ies_next_master_col(IESTREE *tree, IESITEM *col);
--
-- *Returns*
--
-- If the parameter col is NULL, the routine returns a pointer to the
-- first master column, which was created before any other master
-- columns, or NULL, if the master set is empty. Otherwise the parameter
-- col should specify some master column, in which case the routine
-- returns a pointer to the next master column, which was created later
-- than the specified column, or NULL, if there is no next column in the
-- master set. */

IESITEM *ies_next_master_col(IESTREE *tree, IESITEM *col)
{     IESITEM *start;
      if (col == NULL)
         start = tree->first_col;
      else
      {  if (!(col->what == 'C' && col->count >= 0))
            fault("ies_next_master_col: col = %p; invalid column pointe"
               "r", col);
         start = col->next;
      }
      for (col = start; col != NULL; col = col->next)
      {  insist(col->what == 'C');
         if (col->count >= 0) break;
      }
      return col;
}

/*----------------------------------------------------------------------
-- ies_what_item - determine of what sort master item is.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- int ies_what_item(IESTREE *tree, IESITEM *item);
--
-- *Returns*
--
-- The routine ies_what_item returns the code, which determines of what
-- sort the specified master item is:
--
-- 'R', if the specified item is a master row;
-- 'C', if the specified item is a master column. */

int ies_what_item(IESTREE *tree, IESITEM *item)
{     insist(tree == tree);
      if (item->count < 0)
         fault("ies_what_item: item = %p; invalid item pointer", item);
      return item->what;
}

/*----------------------------------------------------------------------
-- ies_set_item_link - store master item specific information.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_item_link(IESTREE *tree, IESITEM *item, void *link);
--
-- *Description*
--
-- The routine ies_set_item_link stores information specified by the
-- pointer link to the specified master item. */

void ies_set_item_link(IESTREE *tree, IESITEM *item, void *link)
{     insist(tree == tree);
      if (item->count < 0)
         fault("ies_set_item_link: item = %p; invalid item pointer",
            item);
      item->link = link;
      return;
}

/*----------------------------------------------------------------------
-- ies_get_item_link - retrieve master item specific information.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void *ies_get_item_link(IESTREE *tree, IESITEM *item);
--
-- *Returns*
--
-- The routine ies_get_item_link returns information previously stored
-- to the specified master item. */

void *ies_get_item_link(IESTREE *tree, IESITEM *item)
{     insist(tree == tree);
      if (item->count < 0)
         fault("ies_get_item_link: item = %p; invalid item pointer",
            item);
      return item->link;
}

/*----------------------------------------------------------------------
-- ies_clean_master_set - clean the master set.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_clean_master_set(IESTREE *tree);
--
-- *Description*
--
-- The routine ies_clean_master_set physically removes master rows and
-- master columns, which are marked as deleted, from the master set.
--
-- This routine is called automatically from other routines every time
-- when necessary. Thus, although this is allowed, there is no need to
-- call it explicitly from the application program. */

void ies_clean_master_set(IESTREE *tree)
{     IESITEM *item, *prev;
      IESELEM *elem, *temp;
      /* walk through the list of master rows */
      prev = NULL, item = tree->first_row;
      while (item != NULL)
      {  insist(item->what == 'R');
         if (item->count < 0)
         {  /* the master row is marked as deleted */
            /* delete all elements of this master row */
            while (item->ptr != NULL)
            {  elem = item->ptr;
               item->ptr = elem->r_next;
               dmp_free_atom(tree->elem_pool, elem);
            }
            /* remove this row from the master set */
            if (prev == NULL)
               tree->first_row = item->next;
            else
               prev->next = item->next;
            /* and delete it */
            item->what = '?';
            insist(item->name == NULL);
            dmp_free_atom(tree->item_pool, item);
            /* go to the next master row */
            item = (prev == NULL ? tree->first_row : prev->next);
         }
         else
         {  /* the master row stays in the master set */
            /* delete elements of this row, which are placed in marked
               master columns */
            temp = NULL;
            while (item->ptr != NULL)
            {  elem = item->ptr;
               item->ptr = elem->r_next;
               if (elem->col->count < 0)
                  dmp_free_atom(tree->elem_pool, elem);
               else
                  elem->r_next = temp, temp = elem;
            }
            item->ptr = temp;
            /* go to the next master row */
            prev = item, item = item->next;
         }
      }
      /* now all marked master rows have been deleted */
      tree->ndrs = 0;
      tree->last_row = prev;
      /* walk through the list of master columns */
      prev = NULL, item = tree->first_col;
      while (item != NULL)
      {  insist(item->what == 'C');
         if (item->count < 0)
         {  /* the master column is marked as deleted */
            /* remove this column from the master set */
            if (prev == NULL)
               tree->first_col = item->next;
            else
               prev->next = item->next;
            /* and delete it */
            item->what = '?';
            insist(item->name == NULL);
            dmp_free_atom(tree->item_pool, item);
            /* go to the next master column */
            item = (prev == NULL ? tree->first_col : prev->next);
         }
         else
         {  /* the master column stays in the master set */
            /* the list of its elements will be built below */
            item->ptr = NULL;
            /* go to the next master column */
            prev = item, item = item->next;
         }
      }
      /* now all marked master columns have been deleted */
      tree->ndcs = 0;
      tree->last_col = prev;
      /* walk through the list of remaining master rows and build lists
         of elements for remaining master columns */
      for (item = tree->first_row; item != NULL; item = item->next)
      {  for (elem = item->ptr; elem != NULL; elem = elem->r_next)
         {  insist(elem->col->what == 'C');
            elem->c_next = elem->col->ptr;
            elem->col->ptr = elem;
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- ies_del_master_row - delete master row from the master set.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_del_master_row(IESTREE *tree, IESITEM *row);
--
-- *Description*
--
-- The routine ies_del_master_row deletes the specified master row,
-- which should exist and should not be used in any node problem, from
-- the master set.
--
-- May note that this routine just marks the specified master row as
-- deleted (physical deletion is performed automatically at appropriate
-- moments), however no further references to this item are allowed as
-- if it would be physically deleted by this routine. */

void ies_del_master_row(IESTREE *tree, IESITEM *row)
{     /* check if the pointer refers to an existing master row */
      if (!(row->what == 'R' && row->count >= 0))
         fault("ies_del_master_row: row = %p; invalid row pointer",
            row);
      /* deletion is allowed only if the master row has zero count and
         also is not used in the current node problem */
      if (row->count > 0 || tree->this_node != NULL && row->bind != 0)
         fault("ies_del_master_row: row = %p; attempt to delete row use"
            "d in some node problem(s)", row);
      /* call the item hook routine */
      if (tree->item_hook != NULL)
         tree->item_hook(tree->item_info, row);
      /* unbind the master row, if necessary */
      if (row->bind != 0)
      {  insist(1 <= row->bind && row->bind <= tree->m);
         insist(tree->item[row->bind] == row);
         tree->item[row->bind] = NULL;
         row->bind = 0;
      }
      /* delete symbolic name assigned to the master row */
      if (row->name != NULL)
      {  delete_str(row->name);
         row->name = NULL;
      }
      /* mark the master row as deleted */
      tree->nmrs--, tree->ndrs++, row->count = -1;
      /* clean the master set if it's time to do that */
      if (time_to_clean(tree)) ies_clean_master_set(tree);
      return;
}

/*----------------------------------------------------------------------
-- ies_del_master_col - delete master column from the master set.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_del_master_col(IESTREE *tree, IESITEM *col);
--
-- *Description*
--
-- The routine ies_del_master_col deletes the specified master column,
-- which should exist and should not be used in any node problem, from
-- the master set.
--
-- May note that this routine just marks the specified master column as
-- deleted (physical deletion is performed automatically at appropriate
-- moments), however no further references to this item are allowed as
-- if it would be physically deleted by this routine. */

void ies_del_master_col(IESTREE *tree, IESITEM *col)
{     /* check if the pointer refers to an existing master column */
      if (!(col->what == 'C' && col->count >= 0))
         fault("ies_del_master_col: col = %p; invalid column pointer",
            col);
      /* deletion is allowed only if the master column has zero count
         and also is not used in the current node problem */
      if (col->count > 0 || tree->this_node != NULL && col->bind != 0)
         fault("ies_del_master_col: col = %p; attempt to delete column "
            "used in some node problem(s)", col);
      /* call the item hook routine */
      if (tree->item_hook != NULL)
         tree->item_hook(tree->item_info, col);
      /* unbind the master column, if necessary */
      if (col->bind != 0)
      {  insist(1 <= col->bind && col->bind <= tree->n);
         insist(tree->item[tree->m+col->bind] == col);
         tree->item[tree->m+col->bind] = NULL;
         col->bind = 0;
      }
      /* delete symbolic name assigned to the master column */
      if (col->name != NULL)
      {  delete_str(col->name);
         col->name = NULL;
      }
      /* mark the master column as deleted */
      tree->nmcs--, tree->ndcs++, col->count = -1;
      /* clean the master set if it's time to do that */
      if (time_to_clean(tree)) ies_clean_master_set(tree);
      return;
}

/*----------------------------------------------------------------------
-- ies_set_item_filt - install item filter routine.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_item_filt(IESTREE *tree,
--    void *info, int (*filt)(void *info, IESITEM *item));
--
-- *Description*
--
-- The routine ies_set_item_filt installs the item filter routine for
-- the specified implicit enumeration tree.
--
-- The parameter info is a transit pointer, which is passed to the item
-- filter routine.
--
-- The item filter routine is specified by its entry point filt. This
-- routine is called every time when the reference count of some master
-- row/column, pointer to which is passed to the routine, reaches zero.
-- If this filter routine returns zero, the master item is deleted from
-- the master set. Otherwise, if this routine returns non-zero, the item
-- is kept in the master set. In the latter case it is assumed that the
-- item will be deleted explicitly (if necessary).
--
-- If the parameter filt is NULL, default behavior is restored, as if
-- there were a filter routine, which always returns zero. */

void ies_set_item_filt(IESTREE *tree,
      void *info, int (*filt)(void *info, IESITEM *item))
{     tree->filt_info = info;
      tree->item_filt = filt;
      return;
}

/*----------------------------------------------------------------------
-- ies_set_item_hook - install item hook routine.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_item_hook(IESTREE *tree,
--    void *info, void (*hook)(void *info, IESITEM *item));
--
-- *Description*
--
-- The routine ies_set_item_hook installs the item hook routine for the
-- specified implicit enumeration tree.
--
-- The parameter info is a transit pointer, which is passed to the item
-- hook routine.
--
-- The item hook routine is specified by its entry point hook. This
-- routine is called when some master item, pointer to which is passed
-- to the routine, is being deleted from the master set. */

void ies_set_item_hook(IESTREE *tree,
      void *info, void (*hook)(void *info, IESITEM *item))
{     tree->item_info = info;
      tree->item_hook = hook;
      return;
}

/*----------------------------------------------------------------------
-- ies_delete_tree - delete implicit enumeration tree.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_delete_tree(IESTREE *tree);
--
-- *Description*
--
-- The routine ies_delete_tree deletes the implicit enumeration tree,
-- which the parameter tree points to, and frees all memory allocated
-- to this object. */

void ies_delete_tree(IESTREE *tree)
{     dmp_delete_pool(tree->item_pool);
      dmp_delete_pool(tree->str_pool);
      dmp_delete_pool(tree->elem_pool);
      dmp_delete_pool(tree->node_pool);
      dmp_delete_pool(tree->diff_pool);
      dmp_delete_pool(tree->bnds_pool);
      dmp_delete_pool(tree->coef_pool);
      dmp_delete_pool(tree->stat_pool);
      ufree(tree->item);
      ufree(tree->typx);
      ufree(tree->lb);
      ufree(tree->ub);
      ufree(tree->coef);
      ufree(tree->tagx);
      lpx_delete_prob(tree->lp);
      ufree(tree);
      return;
}

/* eof */
