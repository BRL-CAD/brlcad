/* glpies2.c */

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
#include <string.h>
#include "glpies.h"
#include "glplib.h"

static int debug = 1;
/* debug mode flag */

static int use_names = 1;
/* if this flag is set, symbolic names assigned to the master rows and
   columns are copied to the LP problem object (for debugging only) */

static int nrs_max = 200;
/* critical number of new rows; if more new rows should be added at
   once, the constraint matrix is constructed from scratch */

static int ncs_max = 200;
/* critical number of new columns; if more new columns should be added
   at once, the constraint matrix is constructed from scratch */

/*----------------------------------------------------------------------
-- ies_default_tagx - determine default status of master row/column.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- int ies_default_tagx(IESITEM *item);
--
-- *Returns*
--
-- The routine returns a default status, which should be assigned to
-- the specified master row/column on adding it to a node problem.
--
-- This routine is auxiliary and not intended for using in application
-- programs. */

int ies_default_tagx(IESITEM *item)
{     int tagx;
      switch (item->what)
      {  case 'R':
            /* row becomes basic */
            tagx = LPX_BS; break;
         case 'C':
            /* column becomes non-basic */
            switch (item->typx)
            {  case LPX_FR:
                  tagx = LPX_NF; break;
               case LPX_LO:
                  tagx = LPX_NL; break;
               case LPX_UP:
                  tagx = LPX_NU; break;
               case LPX_DB:
                  tagx = (fabs(item->lb) <= fabs(item->ub) ? LPX_NL :
                     LPX_NF);
                  break;
               case LPX_FX:
                  tagx = LPX_NS; break;
               default:
                  insist(item != item);
            }
            break;
         default:
            insist(item != item);
      }
      return tagx;
}

/*----------------------------------------------------------------------
-- make_patch_lists - make patch lists for the current node problem.
--
-- This routine makes patch lists for the current node problem, which
-- is assumed to be active, so its patch lists are empty.
--
-- This operation is performed to save all changes in the current node
-- problem when it becomes either non-current or inactive. */

static void make_patch_lists(IESTREE *tree)
{     IESNODE *curr, *node;
      IESDIFF *diff, *diff2;
      IESBNDS *bnds;
      IESCOEF *coef;
      IESSTAT *stat;
      int m, n, k;
      void **temp;
      /* get a pointer to the current node */
      curr = tree->this_node;
      /* the current node should exist */
      insist(curr != NULL);
      /* and should be active */
      insist(curr->count < 0);
      /* so its patch lists should be empty */
      insist(curr->del_them == NULL);
      insist(curr->add_them == NULL);
      insist(curr->bnds_patch == NULL);
      insist(curr->coef_patch == NULL);
      insist(curr->stat_patch == NULL);
      /* determine number of rows and number of columns in the current
         node problem */
      m = curr->m, n = curr->n;
      insist(tree->m == m);
      insist(tree->n == n);
      /* make a path from the root node to the current node */
      curr->temp = NULL;
      for (node = curr; node != NULL; node = node->up)
         if (node->up != NULL) node->up->temp = node;
      /* nullify the bind fields for all master rows and columns; below
         these fields will be restored */
      for (k = 1; k <= m+n; k++)
      {  insist(tree->item[k]->bind == (k <= m ? k : k - m));
         tree->item[k]->bind = 0;
      }
      if (debug)
      {  IESITEM *item;
         for (item = tree->first_row; item != NULL; item = item->next)
            insist(item->bind == 0);
         for (item = tree->first_col; item != NULL; item = item->next)
            insist(item->bind == 0);
      }
      /* go downstairs from the root node to the parent of the current
         node in order to mark rows and columns, which are presented in
         the parent node */
      for (node = tree->root_node; node != curr; node = node->temp)
      {  /* unmark deleted rows and columns */
         for (diff = node->del_them; diff != NULL; diff = diff->next)
         {  insist(diff->item->bind != 0);
            diff->item->bind = 0;
         }
         /* and mark added rows and columns */
         for (diff = node->add_them; diff != NULL; diff = diff->next)
         {  insist(diff->item->bind == 0);
            diff->item->bind = 1;
         }
      }
      /* look through rows and columns of the current node problem in
         order to determine which of them are missing in its parent and
         therefore should be included in the add_them patch list */
      for (k = 1; k <= m+n; k++)
      {  if (tree->item[k]->bind == 0)
         {  /* row/column presented in the current node is missing in
               its parent; include it to the add_them patch list */
            diff = dmp_get_atom(tree->diff_pool);
            diff->item = tree->item[k];
            insist(diff->item->count >= 0);
            diff->item->count++;
            diff->next = curr->add_them;
            curr->add_them = diff;
         }
         else
         {  /* row/column presented in the current node is also in its
               parent; unmark it */
            tree->item[k]->bind = 0;
         }
      }
      /* now marked rows and columns are presented in the parent node
         and missing in the current node, so they should be included in
         the del_them patch list */
      for (node = tree->root_node; node != curr; node = node->temp)
      {  for (diff = node->add_them; diff != NULL; diff = diff->next)
         {  if (diff->item->bind != 0)
            {  /* row/column is marked because it is presented in the
                  parent node and missing in the current node; include
                  it in the del_them patch list */
               diff2 = dmp_get_atom(tree->diff_pool);
               diff2->item = diff->item;
               diff2->next = curr->del_them;
               curr->del_them = diff2;
               /* unmark the row/column */
               diff->item->bind = 0;
            }
         }
      }
      /* currently the bind fields for all master row and columns are
         nullified; restore them for the current node problem */
      if (debug)
      {  IESITEM *item;
         for (item = tree->first_row; item != NULL; item = item->next)
            insist(item->bind == 0);
         for (item = tree->first_col; item != NULL; item = item->next)
            insist(item->bind == 0);
      }
      for (k = 1; k <= m+n; k++)
      {  insist(tree->item[k]->bind == 0);
         tree->item[k]->bind = (k <= m ? k : k - m);
      }
      /* end of making the del_them and add_them lists */
      /* allocate working array */
      temp = ucalloc(1+m+n, sizeof(void *));
      /* go downstairs from the root node to the parent of the current
         node and store pointers to patch entries, which would be used
         on restoring types and bounds for the parent node */
      for (k = 1; k <= m+n; k++) temp[k] = NULL;
      for (node = tree->root_node; node != curr; node = node->temp)
      {  for (diff = node->del_them; diff != NULL; diff = diff->next)
         {  if (diff->item->bind != 0)
            {  /* row/column is deleted on some level and added again
                  on a higher level, so its attributes should be set to
                  default as it would be added for the first time */
               k = (diff->item->what == 'R' ? 0 : m) + diff->item->bind;
               insist(1 <= k && k <= m+n);
               temp[k] = NULL;
            }
         }
         for (bnds = node->bnds_patch; bnds != NULL; bnds = bnds->next)
         {  if (bnds->item->bind != 0)
            {  /* row/column is presented in the current node, so store
                  a pointer to the type/bounds patch entry */
               k = (bnds->item->what == 'R' ? 0 : m) + bnds->item->bind;
               insist(1 <= k && k <= m+n);
               temp[k] = bnds;
            }
         }
      }
      /* look through the list of accumulated patch entries and build
         the types/bounds patch list for the current node */
      for (k = 1; k <= m+n; k++)
      {  int typx;
         double lb, ub;
         bnds = temp[k];
         if (bnds == NULL)
         {  /* no patch entry met; use default type and bounds for the
               corresponding master row/column */
            typx = tree->item[k]->typx;
            lb = tree->item[k]->lb;
            ub = tree->item[k]->ub;
         }
         else
         {  /* use type and bounds from the most recent patch entry */
            typx = bnds->typx;
            lb = bnds->lb;
            ub = bnds->ub;
         }
         /* now typx, lb, and ub has the same values as they would have
            in the parent node problem */
         if (!(tree->typx[k] == typx && tree->lb[k] == lb &&
               tree->ub[k] == ub))
         {  /* type and bounds were changed in the current node, so add
               an entry to the types/bounds patch list */
            bnds = dmp_get_atom(tree->bnds_pool);
            bnds->item = tree->item[k];
            bnds->typx = tree->typx[k];
            bnds->lb = tree->lb[k];
            bnds->ub = tree->ub[k];
            bnds->next = curr->bnds_patch;
            curr->bnds_patch = bnds;
         }
      }
      /* end of making the bnds_patch list */
      /* go downstairs from the root node to the parent of the current
         node and store pointers to patch entries, which would be used
         on restoring objective coefficients for the parent node */
      for (k = 0; k <= m+n; k++) temp[k] = NULL;
      for (node = tree->root_node; node != curr; node = node->temp)
      {  for (diff = node->del_them; diff != NULL; diff = diff->next)
         {  if (diff->item->bind != 0)
            {  /* row/column is deleted on some level and added again
                  on a higher level, so its attributes should be set to
                  default as it would be added for the first time */
               k = (diff->item->what == 'R' ? 0 : m) + diff->item->bind;
               insist(1 <= k && k <= m+n);
               temp[k] = NULL;
            }
         }
         for (coef = node->coef_patch; coef != NULL; coef = coef->next)
         {  if (coef->item == NULL)
            {  /* store a pointer to the constant term patch entry */
               temp[0] = coef;
            }
            else if (coef->item->bind != 0)
            {  /* row/column is presented in the current node, so store
                  a pointer to the objective coefficient patch entry */
               k = (coef->item->what == 'R' ? 0 : m) + coef->item->bind;
               insist(1 <= k && k <= m+n);
               temp[k] = coef;
            }
         }
      }
      /* look through the list of accumulated patch entries and build
         the objective coefficients patch list for the current node */
      for (k = 0; k <= m+n; k++)
      {  double cost;
         coef = temp[k];
         if (coef == NULL)
         {  /* no patch entry met; use default coefficient for the
               corresponding master row/column */
            cost = (k == 0 ? 0.0 : tree->item[k]->coef);
         }
         else
         {  /* use a coefficient from the most recent patch entry */
            cost = coef->coef;
         }
         /* now cost has the same value as it would have in the parent
            node problem */
         if (tree->coef[k] != cost)
         {  /* the coefficient was changed in the current node, so add
               an entry to the objective coefficients patch list */
            coef = dmp_get_atom(tree->coef_pool);
            coef->item = (k == 0 ? NULL : tree->item[k]);
            coef->coef = tree->coef[k];
            coef->next = curr->coef_patch;
            curr->coef_patch = coef;
         }
      }
      /* end of making the coef_patch list */
      /* go downstairs from the root node to the parent of the current
         node and store pointers to patch entries, which would be used
         on restoring row/column statuses for the parent node */
      for (k = 1; k <= m+n; k++) temp[k] = NULL;
      for (node = tree->root_node; node != curr; node = node->temp)
      {  for (diff = node->del_them; diff != NULL; diff = diff->next)
         {  if (diff->item->bind != 0)
            {  /* row/column is deleted on some level and added again
                  on a higher level, so its attributes should be set to
                  default as it would be added for the first time */
               k = (diff->item->what == 'R' ? 0 : m) + diff->item->bind;
               insist(1 <= k && k <= m+n);
               temp[k] = NULL;
            }
         }
         for (stat = node->stat_patch; stat != NULL; stat = stat->next)
         {  if (stat->item->bind != 0)
            {  /* row/column is presented in the current node, so store
                  a pointer to the status patch entry */
               k = (stat->item->what == 'R' ? 0 : m) + stat->item->bind;
               insist(1 <= k && k <= m+n);
               temp[k] = stat;
            }
         }
      }
      /* look through the list of accumulated patch entries and build
         the row/column statuses patch list for the current node */
      for (k = 1; k <= m+n; k++)
      {  int tagx;
         stat = temp[k];
         if (stat == NULL)
         {  /* no patch entry met, therefore use default status for the
               corresponding master row/column */
            tagx = ies_default_tagx(tree->item[k]);
         }
         else
         {  /* use a status from the most recent patch entry */
            tagx = stat->tagx;
         }
         /* now tagx has the same value as it would have in the parent
            node problem */
         if (tree->tagx[k] != tagx)
         {  /* the status of row/column was changed, so add an entry to
               the row/column statuses patch list */
            stat = dmp_get_atom(tree->stat_pool);
            stat->item = tree->item[k];
            stat->tagx = tree->tagx[k];
            stat->next = curr->stat_patch;
            curr->stat_patch = stat;
         }
      }
      /* end of making the stat_patch list */
      /* free working array */
      ufree(temp);
      /* and return to the calling program */
      return;
}

/*----------------------------------------------------------------------
-- free_patch_lists - free patch list of specified node problem.
--
-- This routine frees all patch lists of the specified node problem.
--
-- This operation is performed in order to free memory when the active
-- node problem becomes current or when the node problem is deleted. */

static void free_patch_lists(IESTREE *tree, IESNODE *node)
{     while (node->del_them != NULL)
      {  IESDIFF *diff = node->del_them;
         node->del_them = diff->next;
         dmp_free_atom(tree->diff_pool, diff);
      }
      while (node->add_them != NULL)
      {  IESDIFF *diff = node->add_them;
         /* deleting node may involve deleting some master items, which
            are not referenced in any other node problems; in this case
            diff->item is set to NULL */
         if (diff->item != NULL) diff->item->count--;
         node->add_them = diff->next;
         dmp_free_atom(tree->diff_pool, diff);
      }
      while (node->bnds_patch != NULL)
      {  IESBNDS *bnds = node->bnds_patch;
         node->bnds_patch = bnds->next;
         dmp_free_atom(tree->bnds_pool, bnds);
      }
      while (node->coef_patch != NULL)
      {  IESCOEF *coef = node->coef_patch;
         node->coef_patch = coef->next;
         dmp_free_atom(tree->coef_pool, coef);
      }
      while (node->stat_patch != NULL)
      {  IESSTAT *stat = node->stat_patch;
         node->stat_patch = stat->next;
         dmp_free_atom(tree->stat_pool, stat);
      }
      return;
}

/*----------------------------------------------------------------------
-- ies_create_node - create new node problem.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- IESNODE *ies_create_node(IESTREE *tree, IESNODE *parent);
--
-- *Description*
--
-- The routine ies_create_node creates a new node problem and inserts
-- it in the implicit enumeration tree.
--
-- The parameter parent should specify an existing node, to which the
-- new node is attached. The parent node can be in any state, i.e. it
-- can be active, non-active, current, or non-current. If the parent
-- node was active, it becomes non-active (so further modifications of
-- this node problem are not allowed). If the parent node was current,
-- it remains current. Note that arbitrary number (not necessarily two)
-- of child nodes can be created for the same parent node.
--
-- In the special case when the parameter parent is NULL, the routine
-- creates the root node. The root node should be created only once and
-- before creating any other nodes.
--
-- Being created the new node problem inherits all attributes of its
-- parent, so initially it is equivalent to the parent node problem
-- (except the root node problem, which is equivalent to an empty LP).
-- Since the created node problem becomes active, it can be modified.
--
-- *Returns*
--
-- The routine returns a pointer to the created node. */

IESNODE *ies_create_node(IESTREE *tree, IESNODE *parent)
{     IESNODE *node;
      /* the only root node can be created */
      if (parent == NULL && tree->root_node != NULL)
         fault("ies_create_node: root node already exists");
      /* if the parent node problem is active, deactivate it */
      if (parent != NULL && parent->count < 0)
      {  /* if the parent node problem being active is also current,
            store its patch lists */
         if (tree->this_node == parent) make_patch_lists(tree);
         /* now the parent node problem can be marked as inactive */
         parent->count = 0;
      }
      /* create and initialize new node */
      node = dmp_get_atom(tree->node_pool);
      node->up = parent;
      node->level = (parent == NULL ? 0 : parent->level + 1);
      node->count = -1;
      node->m = (parent == NULL ? 0 : parent->m);
      node->n = (parent == NULL ? 0 : parent->n);
      node->link = NULL;
      node->prev = tree->last_node;
      node->next = NULL;
      node->temp = NULL;
      node->del_them = NULL;
      node->add_them = NULL;
      node->bnds_patch = NULL;
      node->coef_patch = NULL;
      node->stat_patch = NULL;
      /* insert the new node into the tree */
      tree->size++;
      if (node->prev == NULL)
         tree->root_node = node;
      else
         node->prev->next = node;
      tree->last_node = node;
      /* and attach the new node to its parent */
      if (parent != NULL) parent->count++;
      /* return a pointer to the created node */
      return node;
}

/*----------------------------------------------------------------------
-- get_next_elem - get next element of the constraint matrix.
--
-- This routine obtains a next element of the constraint matrix from
-- the master set and returns its indices and numerical value. */

struct info
{     /* transit parameters passed to the routine next_elem */
      IESTREE *tree;
      /* pointer to the main data structure */
      IESITEM *row;
      /* pointer to the current master row */
      IESELEM *elem;
      /* pointer to the current element of the current master row */
};

static double get_next_elem(void *_info, int *i, int *j)
{     struct info *info = _info;
      double val;
      /* obtain a next element placed in the row and the column, which
         both are presented in the problem instance */
loop: if (info->elem == NULL)
      {  /* the current row has been exceeded; take the next row */
next:    if (info->row == NULL)
            info->row = info->tree->first_row;
         else
            info->row = info->row->next;
         /* if there is no next row, signal end-of-file */
         if (info->row == NULL)
         {  *i = *j = 0;
            return 0.0;
         }
         /* skip the row not presented in the problem instance */
         if (info->row->bind == 0) goto next;
         /* take the first element of the row */
         info->elem = info->row->ptr;
         /* skip the empty row */
         if (info->elem == NULL) goto next;
      }
      /* skip the element, which is placed in a column not presented in
         the problem instance */
      if (info->elem->col->bind == 0)
      {  info->elem = info->elem->r_next;
         goto loop;
      }
      /* store indices and numerical value of the current element */
      *i = info->elem->row->bind;
      *j = info->elem->col->bind;
      val = info->elem->val;
      /* and get ready for obtaining the next element */
      info->elem = info->elem->r_next;
      return val;
}

/*----------------------------------------------------------------------
-- load_matrix - load constraint mastrix from scratch.
--
-- This routine loads the constraint matrix for the problem instance
-- from scratch. */

static void load_matrix(IESTREE *tree)
{     struct info info;
      int k;
      /* clear the existing contents of the constraint matrix */
      for (k = 1; k <= tree->m+tree->n; k++)
      {  if (k <= tree->m)
            lpx_mark_row(tree->lp, k, k);
         else
            lpx_mark_col(tree->lp, k - tree->m, k);
      }
      lpx_clear_mat(tree->lp);
      lpx_unmark_all(tree->lp);
      /* and load the new contents of the matrix in row-wise manner */
      info.tree = tree;
      info.row = NULL;
      info.elem = NULL;
      lpx_load_mat(tree->lp, &info, get_next_elem);
      return;
}

/*----------------------------------------------------------------------
-- ies_revive_node - make specified node problem current.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_revive_node(IESTREE *tree, IESNODE *node);
--
-- *Description*
--
-- The routine ies_revive_node makes the specified node problem (which
-- can be active as well as inactive) to be the current node problem.
--
-- The same node problem can be revived several times. However, should
-- note that the order, in which its rows and columns are placed in the
-- problem object at a particular reincarnation, may be different.
--
-- In the special case when the parameter node is NULL the routine just
-- makes the current node problem to be non-current. */

void ies_revive_node(IESTREE *tree, IESNODE *node)
{     IESITEM *item;
      IESDIFF *diff;
      IESBNDS *bnds;
      IESCOEF *coef;
      IESSTAT *stat;
      int i, j, k, m_new, n_new, k_new, nrs, ncs;
      /* check whether the specified node problem is already current;
         if so, do nothing */
      if (tree->this_node == node) goto done;
      /* if the current node problem exists, make it non-current */
      if (tree->this_node != NULL)
      {  /* if the current node is active, store its patch list */
         if (tree->this_node->count < 0) make_patch_lists(tree);
         /* now the current node problem can be made non-current */
         tree->this_node = NULL;
         /* note that the problem instance remains unchanged */
      }
      /* if only the current node problem should be made non-current,
         all is done */
      if (node == NULL) goto done;
      /* set pointer to the new current node problem */
      tree->this_node = node;
      /* currently the problem instance contains some rows and columns
         remaining from some old current node problem; we need to remove
         the rows and columns, which are missing in the new current node
         problem, and add the rows and columns, which being presented in
         the new current node problem are missing in the instance; in
         order to do that we walk from the root node to the new current
         node and set the bind fields for master rows and columns in the
         following way:
         bind = 0       if the master item is presented neither in the
                        instance nor in the current node problem;
         bind > 0       if the master item is presented in both the
                        instance and the current node problem;
         bind < 0       if the master item is presented in the instance
                        and missing in the current node problem;
         bind = INT_MAX if the master item is missing in the instance
                        and presented in the current node problem */
      /* make a path from the root node to the current node */
      tree->this_node->temp = NULL;
      for (node = tree->this_node; node != NULL; node = node->up)
         if (node->up != NULL) node->up->temp = node;
      /* since we start from a dummy empty problem (that precedes the
         root node problem), the bind fields for all rows and columns
         presented in the instance should be made negative */
      for (k = 1; k <= tree->m+tree->n; k++)
      {  item = tree->item[k];
         if (item != NULL)
         {  if (k <= tree->m)
            {  insist(item->what == 'R');
               insist(item->bind == k);
            }
            else
            {  insist(item->what == 'C');
               insist(item->bind == k - tree->m);
            }
            item->bind = - item->bind;
         }
      }
      /* now go downstairs from the root node to the current node and
         modify the bind fields for each visited node problem */
      for (node = tree->root_node; node != NULL; node = node->temp)
      {  for (diff = node->del_them; diff != NULL; diff = diff->next)
         {  item = diff->item;
            insist(item->bind > 0);
            if (item->bind == INT_MAX)
               item->bind = 0;
            else
               item->bind = - item->bind;
         }
         for (diff = node->add_them; diff != NULL; diff = diff->next)
         {  item = diff->item;
            insist(item->bind <= 0);
            if (item->bind == 0)
               item->bind = INT_MAX;
            else
               item->bind = - item->bind;
         }
      }
      /* master items with negative bind field are presented in the
         instance and missing in the current node problem, therefore the
         corresponding rows and columns (and also which have no binding)
         should be deleted from the instance */
      m_new = n_new = 0;
      lpx_unmark_all(tree->lp);
      for (k = 1; k <= tree->m+tree->n; k++)
      {  item = tree->item[k];
         if (item == NULL || item->bind < 0)
         {  /* either this row/column has no binding in a master set
               or it is missing in the current node problem, therefore
               it should be unbound and deleted from the instance */
            if (item != NULL) item->bind = 0;
            if (k <= tree->m)
               lpx_mark_row(tree->lp, k, 1);
            else
               lpx_mark_col(tree->lp, k - tree->m, 1);
         }
         else
         {  /* this row/column is presented in both the instance and in
               the current node problem, therefore it should be kept in
               the instance */
            if (k <= tree->m) m_new++; else n_new++;
            k_new = m_new + n_new;
            tree->item[k_new] = item;
            insist(item->bind == (k <= tree->m ? k : k - tree->m));
            item->bind = (k <= tree->m ? k_new : k_new - m_new);
#if 0 /* copying is not needed due to further re-initialization */
            tree->typx[k_new] = tree->typx[k];
            tree->lb[k_new] = tree->lb[k];
            tree->ub[k_new] = tree->ub[k];
            tree->coef[k_new] = tree->coef[k];
            tree->tagx[k_new] = tree->tagx[k];
#endif
         }
      }
      /* delete the corresponding rows and columns from the LP problem
         object */
      if (!(tree->m == m_new && tree->n == n_new))
         lpx_del_items(tree->lp);
      /* set new size of the problem instance */
      tree->m = m_new, tree->n = n_new;
      insist(lpx_get_num_rows(tree->lp) == tree->m);
      insist(lpx_get_num_cols(tree->lp) == tree->n);
      /* master items marked by the special value INT_MAX in the bind
         field are presented in the current node problem and missing in
         the instance, so they should be added to the instance */
      /* at first we determine number of added rows and columns */
      nrs = ncs = 0;
      for (node = tree->root_node; node != NULL; node = node->temp)
      {  for (diff = node->add_them; diff != NULL; diff = diff->next)
         {  item = diff->item;
            if (item->bind == INT_MAX)
            {  switch (item->what)
               {  case 'R': nrs++; break;
                  case 'C': ncs++; break;
                  default: insist(item != item);
               }
            }
         }
      }
      /* thus, nrs rows and ncs columns should be added; determine new
         size of the instance */
      m_new = tree->m + nrs;
      n_new = tree->n + ncs;
      /* implementation note: if in future versions it will be allowed
         to add rows and columns to non-current active nodes, the next
         two statements should be replaced by the code for reallocating
         the instance's arrays (see ies_add_rows and ies_add_cols) */
      insist(m_new <= tree->m_max);
      insist(n_new <= tree->n_max);
      /* make a free room in the instance's arrays for added rows */
      if (nrs > 0)
      {  memmove(&tree->item[m_new+1], &tree->item[tree->m+1],
            tree->n * sizeof(IESITEM *));
#if 0 /* copying is not needed due to further re-initialization */
         memmove(&tree->typx[m_new+1], &tree->typx[tree->m+1],
            tree->n * sizeof(int));
         memmove(&tree->lb[m_new+1], &tree->lb[tree->m+1],
            tree->n * sizeof(double));
         memmove(&tree->ub[m_new+1], &tree->ub[tree->m+1],
            tree->n * sizeof(double));
         memmove(&tree->coef[m_new+1], &tree->coef[tree->m+1],
            tree->n * sizeof(double));
         memmove(&tree->tagx[m_new+1], &tree->tagx[tree->m+1],
            tree->n * sizeof(int));
#endif
      }
      /* set the bind field for just added rows and columns */
      i = tree->m, j = tree->n;
      for (node = tree->root_node; node != NULL; node = node->temp)
      {  for (diff = node->add_them; diff != NULL; diff = diff->next)
         {  item = diff->item;
            if (item->bind == INT_MAX)
            {  switch (item->what)
               {  case 'R':
                     i++; tree->item[i] = item; item->bind = i;
                     break;
                  case 'C':
                     j++; tree->item[m_new+j] = item; item->bind = j;
                     break;
                  default:
                     insist(item != item);
               }
            }
         }
      }
      insist(i == m_new);
      insist(j == n_new);
      /* add nrs rows to the LP problem object */
      if (nrs > 0)
      {  lpx_add_rows(tree->lp, nrs);
         /* copy symbolic names assigned to the corresponding master
            rows (if required) */
         if (use_names)
         {  for (i = tree->m+1; i <= m_new; i++)
            {  if (tree->item[i]->name != NULL)
               {  char name[255+1];
                  get_str(name, tree->item[i]->name);
                  lpx_set_row_name(tree->lp, i, name);
               }
            }
         }
      }
      /* add ncs columns to the LP problem object */
      if (ncs > 0)
      {  lpx_add_cols(tree->lp, ncs);
         /* copy symbolic names assigned to the corresponding master
            columns (if required) */
         if (use_names)
         {  for (j = m_new+tree->n+1; j <= m_new+n_new; j++)
            {  if (tree->item[j]->name != NULL)
               {  char name[255+1];
                  get_str(name, tree->item[j]->name);
                  lpx_set_col_name(tree->lp, j - m_new, name);
               }
            }
         }
      }
      /* set the final size of the problem instance */
      tree->m = m_new, tree->n = n_new;
      insist(lpx_get_num_rows(tree->lp) == tree->m);
      insist(lpx_get_num_cols(tree->lp) == tree->n);
      insist(tree->this_node->m == tree->m);
      insist(tree->this_node->n == tree->n);
      /* at this point the problem instance should have the same set of
         rows and columns as the current node problem */
      if (debug)
      {  for (item = tree->first_row; item != NULL; item = item->next)
         {  if (item->bind != 0)
            {  insist(1 <= item->bind && item->bind <= tree->m);
               insist(tree->item[item->bind] == item);
            }
         }
         for (item = tree->first_col; item != NULL; item = item->next)
         {  if (item->bind != 0)
            {  insist(1 <= item->bind && item->bind <= tree->n);
               insist(tree->item[tree->m + item->bind] == item);
            }
         }
         for (k = 1; k <= tree->m+tree->n; k++)
         {  item = tree->item[k];
            insist(item != NULL);
            if (k <= tree->m)
            {  insist(item->what == 'R');
               insist(item->bind == k);
            }
            else
            {  insist(item->what == 'C');
               insist(item->bind == k - tree->m);
            }
         }
      }
      /* now we need to set attributes for rows and columns presented
         in the current node problem */
      /* initialize attributes of all rows and columns by their default
         values specified in the corresponding master items */
      tree->coef[0] = 0.0;
      for (k = 1; k <= tree->m+tree->n; k++)
      {  item = tree->item[k];
         tree->typx[k] = item->typx;
         tree->lb[k] = item->lb;
         tree->ub[k] = item->ub;
         tree->coef[k] = item->coef;
         tree->tagx[k] = ies_default_tagx(item);
      }
      /* go downstairs from the root node to the current node and apply
         patch lists specified for each visited node problem */
      for (node = tree->root_node; node != NULL; node = node->temp)
      {  for (diff = node->del_them; diff != NULL; diff = diff->next)
         {  /* row/column is deleted on some level and added again on a
               higher level, so its attributes should be set to default
               values as it would be added for the first time */
            if (diff->item->bind != 0)
            {  /* row/column is presented in the current node */
               k = (diff->item->what == 'R' ? 0 : tree->m) +
                  diff->item->bind;
               insist(1 <= k && k <= tree->m+tree->n);
               item = tree->item[k];
               insist(diff->item == item);
               tree->typx[k] = item->typx;
               tree->lb[k] = item->lb;
               tree->ub[k] = item->ub;
               tree->coef[k] = item->coef;
               tree->tagx[k] = ies_default_tagx(item);
            }
         }
         /* apply the types/bounds patch list */
         for (bnds = node->bnds_patch; bnds != NULL; bnds = bnds->next)
         {  if (bnds->item->bind != 0)
            {  /* row/column is presented in the current node */
               k = (bnds->item->what == 'R' ? 0 : tree->m) +
                  bnds->item->bind;
               insist(1 <= k && k <= tree->m+tree->n);
               insist(tree->item[k] == bnds->item);
               tree->typx[k] = bnds->typx;
               tree->lb[k] = bnds->lb;
               tree->ub[k] = bnds->ub;
            }
         }
         /* apply the objective coefficients patch list */
         for (coef = node->coef_patch; coef != NULL; coef = coef->next)
         {  if (coef->item == NULL)
            {  /* the constant term of the objective function */
               tree->coef[0] = coef->coef;
            }
            else if (coef->item->bind != 0)
            {  /* row/column is presented in the current node */
               k = (coef->item->what == 'R' ? 0 : tree->m) +
                  coef->item->bind;
               insist(1 <= k && k <= tree->m+tree->n);
               insist(tree->item[k] == coef->item);
               tree->coef[k] = coef->coef;
            }
         }
         /* apply the row/column statuses patch list */
         for (stat = node->stat_patch; stat != NULL; stat = stat->next)
         {  if (stat->item->bind != 0)
            {  /* row/column is presented in the current node */
               k = (stat->item->what == 'R' ? 0 : tree->m) +
                  stat->item->bind;
               insist(1 <= k && k <= tree->m+tree->n);
               insist(tree->item[k] == stat->item);
               tree->tagx[k] = stat->tagx;
            }
         }
      }
      /* if the current node problem is active, its patch lists should
         be freed */
      if (tree->this_node->count < 0)
         free_patch_lists(tree, tree->this_node);
      /* copy attributes of rows and columns from the instance to the
         problem object */
      lpx_set_obj_c0(tree->lp, tree->coef[0]);
      for (i = 1; i <= tree->m; i++)
      {  lpx_set_row_bnds(tree->lp, i, tree->typx[i],
            tree->lb[i], tree->ub[i]);
         lpx_set_row_coef(tree->lp, i, tree->coef[i]);
         lpx_set_row_stat(tree->lp, i, tree->tagx[i]);
      }
      for (j = tree->m+1; j <= tree->m+tree->n; j++)
      {  lpx_set_col_bnds(tree->lp, j - tree->m, tree->typx[j],
            tree->lb[j], tree->ub[j]);
         lpx_set_col_coef(tree->lp, j - tree->m, tree->coef[j]);
         lpx_set_col_stat(tree->lp, j - tree->m, tree->tagx[j]);
      }
      /* finally we have to fill the last nrs rows and ncs columns of
         the constraint matrix; these rows and columns were just added
         to the instance, so currently they are empty */
      if (nrs <= nrs_max && ncs <= ncs_max)
      {  /* number of added rows and columns is not very big, so they
            can be filled directly using the routines lpx_set_mat_row
            and lpx_set_mat_col */
         /* on filling the last nrs rows constraint coefficients in the
            last ncs columns are skipped, since they are entered to the
            matrix on filling the last ncs columns:
               * * * * * * c c c
               * * * * * * c c c
               * * * * * * c c c
               * * * * * * c c c
               r r r r r r c c c
               r r r r r r c c c
            where '*' are existing elements of the constraint matrix,
            'r' are coefficients entered to the matrix on filling the
            last nrs rows, 'c' are coefficients entered to the matrix
            on filling the last ncs columns */
         IESELEM *e;
         int len, *ndx;
         double *val;
         if (nrs > 0)
         {  /* fill the last nrs rows of the constraint matrix */
            ndx = ucalloc(1+tree->n-ncs, sizeof(int));
            val = ucalloc(1+tree->n-ncs, sizeof(double));
            for (i = (tree->m - nrs) + 1; i <= tree->m; i++)
            {  item = tree->item[i];
               insist(item->what == 'R');
               insist(item->bind == i);
               len = 0;
               for (e = item->ptr; e != NULL; e = e->r_next)
               {  j = e->col->bind;
                  if (j != 0 && j <= tree->n-ncs)
                     len++, ndx[len] = j, val[len] = e->val;
               }
               insist(len <= tree->n-ncs);
               lpx_set_mat_row(tree->lp, i, len, ndx, val);
            }
            ufree(ndx);
            ufree(val);
         }
         if (ncs > 0)
         {  /* fill the last ncs columns of the constraint matrix */
            ndx = ucalloc(1+tree->m, sizeof(int));
            val = ucalloc(1+tree->m, sizeof(double));
            for (j = (tree->n - ncs) + 1; j <= tree->n; j++)
            {  item = tree->item[tree->m+j];
               insist(item->what == 'C');
               insist(item->bind == j);
               len = 0;
               for (e = item->ptr; e != NULL; e = e->c_next)
               {  i = e->row->bind;
                  if (i != 0)
                     len++, ndx[len] = i, val[len] = e->val;
               }
               insist(len <= tree->m);
               lpx_set_mat_col(tree->lp, j, len, ndx, val);
            }
            ufree(ndx);
            ufree(val);
         }
      }
      else
      {  /* number of added rows and columns is big enough, so it is
            more efficient (for the current version of lpx routines) to
            construct the constraint matrix from scratch */
         load_matrix(tree);
      }
done: /* return to the calling program */
      return;
}

/*----------------------------------------------------------------------
-- realloc_arrays - reallocate the problem instance's arrays.
--
-- This routine reallocates arrays used to keep information about the
-- problem instance.
--
-- The parameter m_max specifies a new maximal number of rows, and the
-- parameter n_max specifies a new maximal number of columns. */

static void realloc_arrays(IESTREE *tree, int m_max, int n_max)
{     int m = tree->m;
      int n = tree->n;
      void *temp;
      insist(m_max >= m);
      insist(n_max >= n);
#     define reallocate(type, ptr, max_len, len) \
      temp = ucalloc(max_len, sizeof(type)), \
      memcpy(temp, ptr, (len) * sizeof(type)), \
      ufree(ptr), ptr = temp
      reallocate(IESITEM *, tree->item, 1+m_max+n_max, 1+m+n);
      reallocate(int, tree->typx, 1+m_max+n_max, 1+m+n);
      reallocate(double, tree->lb, 1+m_max+n_max, 1+m+n);
      reallocate(double, tree->ub, 1+m_max+n_max, 1+m+n);
      reallocate(double, tree->coef, 1+m_max+n_max, 1+m+n);
      reallocate(int, tree->tagx, 1+m_max+n_max, 1+m+n);
#     undef reallocate
      tree->m_max = m_max;
      tree->n_max = n_max;
      return;
}

/*----------------------------------------------------------------------
-- ies_add_rows - add master rows to the current node problem.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_add_rows(IESTREE *tree, int nrs, IESITEM *row[]);
--
-- *Description*
--
-- The routine ies_add_rows includes nrs master rows, which are
-- specified by the pointers row[1], ..., row[nrs], in the current node
-- problem.
--
-- The current node problem should exist and should be active. All the
-- specified master rows should exist and should be not already used in
-- in the node problem.
--
-- Being included in the node problem each new row inherits all
-- attributes specified on creating the corresponding master row: type,
-- lower and upper bounds, objective coefficient, status in basic
-- solution, and constraint coefficients. If necessary, these default
-- attributes can be locally changed for a particular node problem. */

void ies_add_rows(IESTREE *tree, int nrs, IESITEM *row[])
{     IESNODE *node;
      IESITEM *item;
      int m_new, i, ii;
      /* get a pointer to the current node problem */
      node = tree->this_node;
      /* the current node problem should exist */
      if (node == NULL)
         fault("ies_add_rows: current node problem not exist");
      /* and should be active */
      if (node->count >= 0)
         fault("ies_add_rows: attempt to modify inactive node problem");
      /* determine new number of rows in the problem */
      if (nrs < 1)
         fault("ies_add_rows: nrs = %d; invalid parameter", nrs);
      m_new = tree->m + nrs;
      /* reallocate the problem instance's arrays (if necessary) */
      if (tree->m_max < m_new)
      {  int m_max = tree->m_max;
         while (m_max < m_new) m_max += m_max;
         realloc_arrays(tree, m_max, tree->n_max);
      }
      /* make a room for new rows */
      memmove(&tree->item[m_new+1], &tree->item[tree->m+1],
         tree->n * sizeof(IESITEM *));
      memmove(&tree->typx[m_new+1], &tree->typx[tree->m+1],
         tree->n * sizeof(int));
      memmove(&tree->lb[m_new+1], &tree->lb[tree->m+1],
         tree->n * sizeof(double));
      memmove(&tree->ub[m_new+1], &tree->ub[tree->m+1],
         tree->n * sizeof(double));
      memmove(&tree->coef[m_new+1], &tree->coef[tree->m+1],
         tree->n * sizeof(double));
      memmove(&tree->tagx[m_new+1], &tree->tagx[tree->m+1],
         tree->n * sizeof(int));
      /* add the same number of rows to the problem object */
      lpx_add_rows(tree->lp, nrs);
      /* set default attributes specified for the corresponding master
         rows */
      for (i = tree->m+1, ii = nrs; ii >= 1; i++, ii--)
      {  item = row[ii];
         /* check if the pointer refers to an existing master row */
         if (!(item->what == 'R' && item->count >= 0))
            fault("ies_add_rows: row[%d] = %p; invalid master row point"
               "er", ii, item);
         /* check if the master row is already included */
         if (item->bind != 0)
            fault("ies_add_rows: row[%d] = %p; master row already inclu"
               "ded", ii, item);
         /* set default attributes */
         tree->item[i] = item;
         item->bind = i;
         tree->typx[i] = item->typx;
         tree->lb[i] = item->lb;
         tree->ub[i] = item->ub;
         tree->coef[i] = item->coef;
         tree->tagx[i] = ies_default_tagx(item);
         /* and copy them to the problem object */
         if (use_names && item->name != NULL)
         {  char name[255+1];
            get_str(name, item->name);
            lpx_set_row_name(tree->lp, i, name);
         }
         lpx_set_row_bnds(tree->lp, i, tree->typx[i], tree->lb[i],
            tree->ub[i]);
         lpx_set_row_coef(tree->lp, i, tree->coef[i]);
         lpx_set_row_stat(tree->lp, i, tree->tagx[i]);
      }
      /* set new number of rows */
      tree->m = node->m = m_new;
      /* finally we have to fill the last nrs rows of the constraint
         matrix; these rows were just added, so they are empty */
      if (nrs <= nrs_max)
      {  /* there are not so many new rows, so they can be filled
            directly using the routine lpx_set_mat_row */
         IESELEM *e;
         int len, *ndx;
         double *val;
         ndx = ucalloc(1+tree->n, sizeof(int));
         val = ucalloc(1+tree->n, sizeof(double));
         for (i = (tree->m - nrs) + 1; i <= tree->m; i++)
         {  item = tree->item[i];
            len = 0;
            for (e = item->ptr; e != NULL; e = e->r_next)
            {  if (e->col->bind != 0)
               {  len++;
                  insist(len <= tree->n);
                  ndx[len] = e->col->bind;
                  val[len] = e->val;
               }
            }
            lpx_set_mat_row(tree->lp, i, len, ndx, val);
         }
         ufree(ndx);
         ufree(val);
      }
      else
      {  /* number of added rows is big enough; it is reasonable to
            construct the constraint matrix from scratch */
         load_matrix(tree);
      }
      return;
}

/*----------------------------------------------------------------------
-- ies_add_cols - add master columns to the current node problem.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_add_cols(IESTREE *tree, int ncs, IESITEM *col[]);
--
-- *Description*
--
-- The routine ies_add_cols includes ncs master columns, which are
-- specified by the pointers col[1], ..., col[ncs], in the current node
-- problem.
--
-- The current node problem should exist and should be active. All the
-- specified master columns should exist and should be not already used
-- in the node problem.
--
-- Being included in the node problem each new column inherits all
-- attributes specified on creating the corresponding master column:
-- type, lower and upper bounds, objective coefficient, status in basic
-- solution, and constraint coefficients. If necessary, these default
-- attributes can be locally changed for a particular node problem. */

void ies_add_cols(IESTREE *tree, int ncs, IESITEM *col[])
{     IESNODE *node;
      IESITEM *item;
      int n_new, j, jj;
      /* get a pointer to the current node problem */
      node = tree->this_node;
      /* the current node problem should exist */
      if (node == NULL)
         fault("ies_add_cols: current node problem not exist");
      /* and should be active */
      if (node->count >= 0)
         fault("ies_add_cols: attempt to modify inactive node problem");
      /* determine new number of columns in the problem */
      if (ncs < 1)
         fault("ies_add_cols: ncs = %d; invalid parameter", ncs);
      n_new = tree->n + ncs;
      /* reallocate the problem instance's arrays (if necessary) */
      if (tree->n_max < n_new)
      {  int n_max = tree->n_max;
         while (n_max < n_new) n_max += n_max;
         realloc_arrays(tree, tree->m_max, n_max);
      }
      /* add the same number of columns to the problem object */
      lpx_add_cols(tree->lp, ncs);
      /* set default attributes specified for the corresponding master
         columns */
      for (j = tree->m+tree->n+1, jj = ncs; jj >= 1; j++, jj--)
      {  item = col[jj];
         /* check if the pointer refers to an existing master column */
         if (!(item->what == 'C' && item->count >= 0))
            fault("ies_add_cols: col[%d] = %p; invalid master column po"
               "inter", jj, item);
         /* check if the master column is already included */
         if (item->bind != 0)
            fault("ies_add_cols: col[%d] = %p; master column already in"
               "cluded", jj, item);
         /* set default attributes */
         tree->item[j] = item;
         item->bind = j - tree->m;
         tree->typx[j] = item->typx;
         tree->lb[j] = item->lb;
         tree->ub[j] = item->ub;
         tree->coef[j] = item->coef;
         tree->tagx[j] = ies_default_tagx(item);
         /* and copy them to the problem object */
         if (use_names && item->name != NULL)
         {  char name[255+1];
            get_str(name, item->name);
            lpx_set_col_name(tree->lp, j - tree->m, name);
         }
         lpx_set_col_bnds(tree->lp, j - tree->m, tree->typx[j],
            tree->lb[j], tree->ub[j]);
         lpx_set_col_coef(tree->lp, j - tree->m, tree->coef[j]);
         lpx_set_col_stat(tree->lp, j - tree->m, tree->tagx[j]);
      }
      /* set new number of columns */
      tree->n = node->n = n_new;
      /* finally we have to fill the last ncs columns of the constraint
         matrix; these columns were just added, so they are empty */
      if (ncs <= ncs_max)
      {  /* there are not so many new columns, so they can be filled
            directly using the routine lpx_set_mat_col */
         IESELEM *e;
         int len, *ndx;
         double *val;
         ndx = ucalloc(1+tree->m, sizeof(int));
         val = ucalloc(1+tree->m, sizeof(double));
         for (j = (tree->n - ncs) + 1; j <= tree->n; j++)
         {  item = tree->item[tree->m+j];
            len = 0;
            for (e = item->ptr; e != NULL; e = e->c_next)
            {  if (e->row->bind != 0)
               {  len++;
                  insist(len <= tree->m);
                  ndx[len] = e->row->bind;
                  val[len] = e->val;
               }
            }
            lpx_set_mat_col(tree->lp, j, len, ndx, val);
         }
         ufree(ndx);
         ufree(val);
      }
      else
      {  /* number of added columns is big enough; it is reasonable to
            construct the constraint matrix from scratch */
         load_matrix(tree);
      }
      return;
}

/*----------------------------------------------------------------------
-- ies_del_items - delete rows/columns from the current node problem.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_del_items(IESTREE *tree);
--
-- *Description*
--
-- The routine ies_del_items deletes all marked rows and columns from
-- the current node problem.
--
-- The current node problem must exist and must be active. To mark rows
-- and columns the api routines lpx_mark_row and lpx_mark_col should be
-- used.
--
-- If a row/column being deleted is not used in other node problems, it
-- is also removed from the master set, if the filter routine allows to
-- do that. */

void ies_del_items(IESTREE *tree)
{     IESNODE *node;
      IESITEM *item;
      int m_new, n_new, k, k_new, marked;
      /* get a pointer to the current node problem */
      node = tree->this_node;
      /* the current node problem must exist */
      if (node == NULL)
         fault("ies_del_items: current node problem not exist");
      /* and must be active */
      if (node->count >= 0)
         fault("ies_del_items: attempt to modify inactive node problem")
            ;
      /* look through rows and columns of the current node problem */
      m_new = n_new = 0;
      for (k = 1; k <= tree->m + tree->n; k++)
      {  item = tree->item[k];
         if (k <= tree->m)
            marked = lpx_get_row_mark(tree->lp, k);
         else
            marked = lpx_get_col_mark(tree->lp, k - tree->m);
         if (marked)
         {  /* the row/column should be deleted */
            item->bind = 0;
            /* if the reference count is zero, ask the item filter
               routine to determine whether the item should be deleted
               from the master set right now or not */
            if (item->count == 0 && (tree->item_filt == NULL ||
               tree->item_filt(tree->filt_info, item) == 0))
            {  /* delete the item from the master set */
               switch (item->what)
               {  case 'R': ies_del_master_row(tree, item); break;
                  case 'C': ies_del_master_col(tree, item); break;
                  default: insist(item != item);
               }
            }
         }
         else
         {  /* the row/column should be kept */
            if (k <= tree->m) m_new++; else n_new++;
            k_new = m_new + n_new;
            tree->item[k_new] = item;
            item->bind = (k <= tree->m ? k_new : k_new - m_new);
            tree->typx[k_new] = tree->typx[k];
            tree->lb[k_new] = tree->lb[k];
            tree->ub[k_new] = tree->ub[k];
            tree->coef[k_new] = tree->coef[k];
            tree->tagx[k_new] = tree->tagx[k];
         }
      }
      /* set new numbers of rows and columns */
      tree->m = node->m = m_new;
      tree->n = node->n = n_new;
      /* remove the corresponding rows and columns from the LP problem
         instance */
      lpx_del_items(tree->lp);
      return;
}

/*----------------------------------------------------------------------
-- ies_delete_node - delete specified node problem.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_delete_node(IESTREE *tree, IESNODE *node);
--
-- *Description*
--
-- The routine ies_delete_node deleted the specified node problem from
-- the implicit enumeration tree.
--
-- The deleted node problem can be active as well as inactive, however
-- in the latter case it should have no child node problems. */

void ies_delete_node(IESTREE *tree, IESNODE *node)
{     IESDIFF *diff;
      IESITEM *item;
      /* the specified node problem should have no childs */
      if (node->count > 0)
         fault("ies_delete_node: node = %p; attempt to delete node prob"
            "lem with existing child nodes", node);
      /* call the node hook routine */
      if (tree->node_hook != NULL)
         tree->node_hook(tree->node_info, node);
      /* if the node problem is current, make it non-current */
      if (tree->this_node == node) ies_revive_node(tree, NULL);
      /* walk through the add_them patch list */
      for (diff = node->add_them; diff != NULL; diff = diff->next)
      {  item = diff->item;
         insist(item->count >= 1);
         /* if the reference count is one, being decreased it will be
            zero; so, ask the item filter routine in order to determine
            whether the item should be deleted right now or not */
         if (item->count == 1 && (tree->item_filt == NULL ||
            tree->item_filt(tree->filt_info, item) == 0))
         {  /* delete the item from the master set */
            item->count = 0;
            switch (item->what)
            {  case 'R': ies_del_master_row(tree, item); break;
               case 'C': ies_del_master_col(tree, item); break;
               default: insist(item != item);
            }
            diff->item = NULL;
         }
      }
      /* free all patch lists of the node problem */
      free_patch_lists(tree, node);
      /* decrease the reference count of the parent node */
      if (node->up != NULL)
      {  insist(node->up->count > 0);
         node->up->count--;
      }
      /* remove the node problem from the tree */
      insist(tree->size > 0);
      tree->size--;
      if (node->prev == NULL)
         tree->root_node = node->next;
      else
         node->prev->next = node->next;
      if (node->next  == NULL)
         tree->last_node = node->prev;
      else
         node->next->prev = node->prev;
      /* delete the node problem */
      dmp_free_atom(tree->node_pool, node);
      return;
}

/*----------------------------------------------------------------------
-- ies_prune_branch - prune branch of the tree.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_prune_branch(IESTREE *tree, IESNODE *node);
--
-- *Description*
--
-- The routine ies_prune_branch deletes a branch of the tree starting
-- from the specified node.
--
-- This operation can be understood as recursive deletion of nodes in
-- the following way. At first the routine deletes the specified node
-- and then checks if its parent node has no other child nodes. If so,
-- the routine also deletes the parent node and checks if its parent
-- node (grand-parent of the starting node) has no other child nodes.
-- If so, it also deletes this node, etc.
--
-- The specified node problem, from which the routine starts pruning,
-- can be active as well as inactive, but in the latter case it should
-- have no child node problems.
--
-- This operation is usually performed in the branch-and-bound context
-- after some node problem has been fathomed. */

void ies_prune_branch(IESTREE *tree, IESNODE *node)
{     IESNODE *next;
      /* the starting node problem should have no childs */
      if (node->count > 0)
         fault("ies_prune_branch: node = %p; attempt to delete node pro"
            "blem with existing child nodes", node);
      while (node != NULL)
      {  if (node->count > 0) break;
         next = node->up;
         ies_delete_node(tree, node);
         node = next;
      }
      return;
}

/*----------------------------------------------------------------------
-- ies_set_node_hook - install node hook routine.
--
-- *Synopsis*
--
-- #include "glpies.h"
-- void ies_set_node_hook(IESTREE *tree,
--    void *info, void (*hook)(void *info, IESNODE *node));
--
-- *Description*
--
-- The routine ies_set_node_hook installs the node hook routine for the
-- specified implicit enumeration tree.
--
-- The parameter info is a transit pointer, which is passed to the node
-- hook routine.
--
-- The node hook routine is specified by its entry point hook. This
-- routine is called when some node problem, pointer to which is passed
-- to the routine, is being deleted from the enumeration tree. */

void ies_set_node_hook(IESTREE *tree,
      void *info, void (*hook)(void *info, IESNODE *node))
{     tree->node_info = info;
      tree->node_hook = hook;
      return;
}

/* eof */
