/* glpavl.c */

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
#include "glpavl.h"
#include "glplib.h"

/*----------------------------------------------------------------------
-- avl_create_tree - create AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- AVLTREE *avl_create_tree(void *info, int (*fcmp)(void *info,
--    void *key1, void *key2));
--
-- *Description*
--
-- The routine avl_create_tree creates an AVL tree, which initially is
-- empty, i.e. has no nodes.
--
-- The parameter info is a transit pointer passed to the routine fcmp.
--
-- The parameter fcmp specifies an entry point to the user-defined key
-- comparison routine. This routine should compare two keys specified
-- by the pointers key1 and key2 and return the result of comparison as
-- follows:
--
-- < 0, if [key1] is less than [key2]
-- = 0, if [key1] and [key2] are identical
-- > 0, if [key1] is greater than [key2]
--
-- where [p] denotes a value, which the pointer p points to.
--
-- If node keys are ordinary (null-terminated) characters strings, the
-- following statement may be used to create an AVL tree:
--
--    ... = avl_create_tree(NULL, avl_strcmp);
--
-- It is allowed to specify the parameter fcmp as NULL. In this case
-- node keys are not used, so searching, inserting, and deleting nodes
-- are possible only by node positions, not by node keys.
--
-- *Returns*
--
-- The routine returns a pointer to the created AVL tree. */

AVLTREE *avl_create_tree(void *info, int (*fcmp)(void *info,
      void *key1, void *key2))
{     AVLTREE *tree;
      tree = umalloc(sizeof(AVLTREE));
      tree->pool = dmp_create_pool(sizeof(AVLNODE));
      tree->info = info;
      tree->fcmp = fcmp;
      tree->size = 0;
      tree->root = NULL;
      tree->height = 0;
      return tree;
}

/*----------------------------------------------------------------------
-- avl_strcmp - compare keys of character string type.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- int avl_strcmp(void *info, void *key1, void *key2);
--
-- *Description*
--
-- The routine avl_strcmp compares two keys [key1] and [key2], which
-- are oridinary (null-terminated) character strings, and the parameter
-- info is ignored.
--
-- This routine is intended to be used as a key comparison routine for
-- AVL trees.
--
-- *Returns*
--
-- The routine returns the result of comparison as follows:
--
-- < 0, if [key1] is lexicographically less than [key2]
-- = 0, if [key1] and [key2] are lexicographically identical
-- > 0, if [key1] is lexicographically greater than [key2]
--
-- where [p] denotes a value (i.e. character string), which the pointer
-- p points to. */

int avl_strcmp(void *info, void *key1, void *key2)
{     insist(info == info);
      return strcmp((char *)key1, (char *)key2);
}

/*----------------------------------------------------------------------
-- avl_insert_by_key - insert new node with given key into AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- AVLNODE *avl_insert_by_key(AVLTREE *tree, void *key);
--
-- *Description*
--
-- The routine avl_insert_by_key creates a new node with the given key
-- and inserts it into the specified AVL tree. Note that this operation
-- needs the key comparison routine to be defined on creating the tree.
--
-- The insertion is performed independently on if there are nodes with
-- the same key in the tree or not. To check if the node with the same
-- key already exists the routine avl_find_by_key should be used.
--
-- It is assumed that a data structure that represents a value, which
-- the parameter key points to, has been allocated yet. This allocation
-- should be valid at least while the corresponding node exists.
--
-- *Returns*
--
-- The routine returns a pointer to the created new node. */

AVLNODE *avl_insert_by_key(AVLTREE *tree, void *key)
{     AVLNODE *p, *q, *r;
      short int flag;
      if (tree->fcmp == NULL)
         fault("avl_insert_by_key: key comparison routine not defined");
      /* find an appropriate point for insertion */
      p = NULL; q = tree->root;
      while (q != NULL)
      {  p = q;
         if (tree->fcmp(tree->info, key, p->key) <= 0)
         {  flag = 0;
            q = p->left;
            p->rank++;
         }
         else
         {  flag = 1;
            q = p->right;
         }
      }
      /* create new node and insert it into the tree */
      r = dmp_get_atom(tree->pool);
      r->key = key; r->type = 0; r->link = NULL;
      r->rank = 1; r->up = p;
      r->flag = (short int)(p == NULL ? 0 : flag);
      r->bal = 0; r->left = NULL; r->right = NULL;
      tree->size++;
      if (p == NULL)
         tree->root = r;
      else
         if (flag == 0) p->left = r; else p->right = r;
      /* go upstairs to the root and correct all subtrees affected by
         insertion */
      while (p != NULL)
      {  if (flag == 0)
         {  /* the height of the left subtree of [p] is increased */
            if (p->bal > 0)
            {  p->bal = 0;
               break;
            }
            if (p->bal < 0)
            {  avl_rotate_subtree(tree, p);
               break;
            }
            p->bal = -1; flag = p->flag; p = p->up;
         }
         else
         {  /* the height of the right subtree of [p] is increased */
            if (p->bal < 0)
            {  p->bal = 0;
               break;
            }
            if (p->bal > 0)
            {  avl_rotate_subtree(tree, p);
               break;
            }
            p->bal = +1; flag = p->flag; p = p->up;
         }
      }
      /* if the root has been reached, the height of the entire tree is
         increased */
      if (p == NULL) tree->height++;
      return r;
}

/*----------------------------------------------------------------------
-- avl_find_next_node - find next node in AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- AVLNODE *avl_find_next_node(AVLTREE *tree, AVLNODE *node);
--
-- *Description*
--
-- If the parameter node is NULL, the routine avl_find_next_node finds
-- the first node of the specified AVL tree. Otherwise, if the parameter
-- node is not NULL, in which case it should be a valid pointer to some
-- node of the tree, the routine finds the next node, which follows the
-- given node.
--
-- If node keys are used, the next node always has a key, which is not
-- less than a key of the given node.
--
-- *Returns*
--
-- If the parameter node is NULL, the routine returns a pointer to the
-- first node of the tree or NULL, if the tree is empty. Otherwise, if
-- the parameter node points to some node of the tree, the routine
-- returns a pointer to the next node or NULL, if the given node is the
-- last node in the tree. */

AVLNODE *avl_find_next_node(AVLTREE *tree, AVLNODE *node)
{     AVLNODE *p, *q;
      if (tree->root == NULL) return NULL;
      p = node;
      q = (p == NULL ? tree->root : p->right);
      if (q == NULL)
      {  /* go upstairs from the left subtree */
         for (;;)
         {  q = p->up;
            if (q == NULL) break;
            if (p->flag == 0) break;
            p = q;
         }
      }
      else
      {  /* go downstairs into the right subtree */
         for (;;)
         {  p = q->left;
            if (p == NULL) break;
            q = p;
         }
      }
      return q;
}

/*----------------------------------------------------------------------
-- avl_find_prev_node - find previous node in AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- AVLNODE *avl_find_prev_node(AVLTREE *tree, AVLNODE *node);
--
-- *Description*
--
-- If the parameter node is NULL, the routine avl_find_prev_node finds
-- the last node of the specified AVL tree. Otherwise, if the parameter
-- node is not NULL, in which case it should be a valid pointer to some
-- node of the tree, the routine finds the previous node, which precedes
-- the given node.
--
-- If node keys are used, the previous node always has a key, which is
-- not greater than a key of the given node.
--
-- *Returns*
--
-- If the parameter node is NULL, the routine returns a pointer to the
-- last node of the tree or NULL, if the tree is empty. Otherwise, if
-- the parameter node points to some node of the tree, the routine
-- returns a pointer to the previous node or NULL, if the given node is
-- the first node in the tree. */

AVLNODE *avl_find_prev_node(AVLTREE *tree, AVLNODE *node)
{     AVLNODE *p, *q;
      if (tree->root == NULL) return NULL;
      p = node;
      q = (p == NULL ? tree->root : p->left);
      if (q == NULL)
      {  /* go upstairs from the right subtree */
         for (;;)
         {  q = p->up;
            if (q == NULL) break;
            if (p->flag == 1) break;
            p = q;
         }
      }
      else
      {  /* go downstairs into the left subtree */
         for (;;)
         {  p = q->right;
            if (p == NULL) break;
            q = p;
         }
      }
      return q;
}

/*----------------------------------------------------------------------
-- avl_find_by_key - find first node with given key in AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- AVLNODE *avl_find_by_key(AVLTREE *tree, void *key);
--
-- *Description*
--
-- The routine avl_find_by_key finds the first node, which has the
-- given key, in the specified AVL tree. Note that this operation needs
-- the key comparison routine to be defined on creating the tree.
--
-- *Returns*
--
-- The routine returns a pointer to the first node with the given key
-- or NULL, if there is no such node in the tree. */

AVLNODE *avl_find_by_key(AVLTREE *tree, void *key)
{     AVLNODE *p, *q;
      int c;
      if (tree->fcmp == NULL)
         fault("avl_find_by_key: key comparison routine not defined");
      p = tree->root;
      while (p != NULL)
      {  c = tree->fcmp(tree->info, key, p->key);
         if (c == 0) break;
         p = (c < 0 ? p->left : p->right);
      }
      if (p != NULL) for (;;)
      {  q = avl_find_prev_node(tree, p);
         if (q == NULL) break;
         if (tree->fcmp(tree->info, q->key, p->key) != 0) break;
         p = q;
      }
      return p;
}

/*----------------------------------------------------------------------
-- avl_next_by_key - find next node with same key in AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- AVLNODE *avl_next_by_key(AVLTREE *tree, AVLNODE *node);
--
-- *Description*
--
-- The routine avl_next_by_key find the next node, which has the same
-- key as the specified node, in the specified AVL tree. Note that this
-- operation needs the key comparison routine to be defined on creating
-- the tree.
--
-- *Returns*
--
-- The routine returns a pointer to next node with the same key or NULL,
-- if there is no more nodes with the same key in the tree. */

AVLNODE *avl_next_by_key(AVLTREE *tree, AVLNODE *node)
{     AVLNODE *p;
      if (tree->fcmp == NULL)
         fault("avl_next_by_key: key comparison routine not defined");
      if (node == NULL)
         fault("avl_next_by_key: null node pointer not allowed");
      p = avl_find_next_node(tree, node);
      if (p != NULL)
         if (tree->fcmp(tree->info, node->key, p->key) != 0) p = NULL;
      return p;
}

/*----------------------------------------------------------------------
-- avl_insert_by_pos - insert new node into given position of AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- AVLNODE *avl_insert_by_pos(AVLTREE *tree, int pos);
--
-- *Description*
--
-- The routine avl_insert_by_pos creates a new node and inserts it into
-- the given position pos of the specified AVL tree.
--
-- The position pos should satisfy the condition 1 <= pos <= N+1, where
-- N = tree->size is the size (i.e. number of nodes) of the tree before
-- insertion.
--
-- Let all nodes of the tree be renumbered from 1 to N in the order,
-- in which they are enumerated by the routine avl_find_next_node. Then
-- the routine avl_insert_by_pos places the new node between nodes with
-- the numbers pos-1 and pos (if pos is 1, the new node is placed before
-- the first node). Thus, after insertion the new node will occupy the
-- given position pos.
--
-- It is not recommended to use the routine avl_insert_by_pos together
-- with other AVL routines, which assume keyed nodes (avl_insert_by_key,
-- avl_find_by_key, and avl_next_by_key).
--
-- *Returns*
--
-- The routine returns a pointer to the created new node. */

AVLNODE *avl_insert_by_pos(AVLTREE *tree, int pos)
{     AVLNODE *p, *q, *r;
      short int flag;
      if (!(1 <= pos && pos <= tree->size+1))
         fault ("avl_insert_by_pos: pos = %d; invalid position", pos);
      /* find an appropriate point for insertion */
      p = NULL; q = tree->root;
      while (q != NULL)
      {  p = q;
         if (pos <= p->rank)
         {  flag = 0;
            q = p->left;
            p->rank++;
         }
         else
         {  flag = 1;
            q = p->right;
            pos -= p->rank;
         }
      }
      /* create new node and insert it into the tree */
      r = dmp_get_atom(tree->pool);
      r->key = NULL; r->type = 0; r->link = NULL;
      r->rank = 1; r->up = p;
      r->flag = (short int)(p == NULL ? 0 : flag);
      r->bal = 0; r->left = NULL; r->right = NULL;
      tree->size++;
      if (p == NULL)
         tree->root = r;
      else
         if (flag == 0) p->left = r; else p->right = r;
      /* go upstairs to the root and correct all subtrees affected by
         insertion */
      while (p != NULL)
      {  if (flag == 0)
         {  /* the height of the left subtree of [p] is increased */
            if (p->bal > 0)
            {  p->bal = 0;
               break;
            }
            if (p->bal < 0)
            {  avl_rotate_subtree(tree, p);
               break;
            }
            p->bal = -1; flag = p->flag; p = p->up;
         }
         else
         {  /* the height of the right subtree of [p] is increased */
            if (p->bal < 0)
            {  p->bal = 0;
               break;
            }
            if (p->bal > 0)
            {  avl_rotate_subtree(tree, p);
               break;
            }
            p->bal = +1; flag = p->flag; p = p->up;
         }
      }
      /* if the root has been reached, the height of the entire tree is
         increased */
      if (p == NULL) tree->height++;
      return r;
}

/*----------------------------------------------------------------------
-- avl_find_by_pos - find node placed in given position of AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- AVLNODE *avl_find_by_pos(AVLTREE *tree, int pos);
--
-- *Description*
--
-- The routine avl_find_by_pos finds a node, which is placed in the
-- given position pos of the specified AVL tree.
--
-- The position pos should satisfy the condition 1 <= pos <= N, where
-- N = tree->size is the size (i.e. number of nodes) of the tree.
--
-- Do not use this routine for visiting all nodes - it is inefficient.
-- Use the routines avl_find_next_node and avl_find_prev_node instead.
--
-- *Returns*
--
-- The routine returns a pointer to the node, which is placed in the
-- given position. */

AVLNODE *avl_find_by_pos(AVLTREE *tree, int pos)
{     AVLNODE *p;
      if (!(1 <= pos && pos <= tree->size))
         fault("avl_find_by_pos: pos = %d; invalid position", pos);
      p = tree->root;
      for (;;)
      {  insist(p != NULL);
         if (pos == p->rank) break;
         if (pos < p->rank)
            p = p->left;
         else
         {  pos -= p->rank;
            p = p->right;
         }
      }
      return p;
}

/*----------------------------------------------------------------------
-- avl_delete_node - delete specified node from AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- void avl_delete_node(AVLTREE *tree, AVLNODE *node);
--
-- *Description*
--
-- The routine avl_delete_node deletes the specified node from the
-- specified AVL tree. This routine keeps the order of nodes and can be
-- used independently on whether nodes have keys or not.
--
-- Note that exactly the specified node is deleted, so all references
-- to other nodes from external data structures remain valid. */

void avl_delete_node(AVLTREE *tree, AVLNODE *node)
{     AVLNODE *f, *p, *q, *r, *s, *x, *y;
      short int flag;
      if (node == NULL)
         fault("avl_delete_node: null node pointer not allowed");
      p = node;
      /* if both subtrees of the specified node are non-empty, the node
         should be interchanged with the next one, at least one subtree
         of which is always empty */
      if (p->left == NULL || p->right == NULL) goto skip;
      f = p->up; q = p->left;
      r = avl_find_next_node(tree, p); s = r->right;
      if (p->right == r)
      {  if (f == NULL)
            tree->root = r;
         else
            if (p->flag == 0) f->left = r; else f->right = r;
         r->rank = p->rank; r->up = f;
         r->flag = p->flag; r->bal = p->bal;
         r->left = q; r->right = p;
         q->up = r;
         p->rank = 1; p->up = r; p->flag = 1;
         p->bal = (short int)(s == NULL ? 0 : +1);
         p->left = NULL; p->right = s;
         if (s != NULL) s->up = p;
      }
      else
      {  x = p->right; y = r->up;
         if (f == NULL)
            tree->root = r;
         else
            if (p->flag == 0) f->left = r; else f->right = r;
         r->rank = p->rank; r->up = f;
         r->flag = p->flag; r->bal = p->bal;
         r->left = q; r->right = x;
         q->up = r; x->up = r; y->left = p;
         p->rank = 1; p->up = y; p->flag = 0;
         p->bal = (short int)(s == NULL ? 0 : +1);
         p->left = NULL; p->right = s;
         if (s != NULL) s->up = p;
      }
skip: /* now the specified node [p] has at least one empty subtree;
         go upstairs to the root and adjust the rank field of all nodes
         affected by deletion */
      q = p; f = q->up;
      while (f != NULL)
      {  if (q->flag == 0) f->rank--;
         q = f; f = q->up;
      }
      /* delete the specified node from the tree */
      f = p->up; flag = p->flag;
      q = p->left != NULL ? p->left : p->right;
      if (f == NULL)
         tree->root = q;
      else
         if (flag == 0) f->left = q; else f->right = q;
      if (q != NULL) q->up = f, q->flag = flag;
      tree->size--;
      /* go upstairs to the root and correct all subtrees affected by
         deletion */
      while (f != NULL)
      {  if (flag == 0)
         {  /* the height of the left subtree of [f] is decreased */
            if (f->bal == 0)
            {  f->bal = +1;
               break;
            }
            if (f->bal < 0)
               f->bal = 0;
            else
            {  f = avl_rotate_subtree(tree, f);
               if (f->bal < 0) break;
            }
            flag = f->flag; f = f->up;
         }
         else
         {  /* the height of the right subtree of [f] is decreased */
            if (f->bal == 0)
            {  f->bal = -1;
               break;
            }
            if (f->bal > 0)
               f->bal = 0;
            else
            {  f = avl_rotate_subtree(tree, f);
               if (f->bal > 0) break;
            }
            flag = f->flag; f = f->up;
         }
      }
      /* if the root has been reached, the height of the entire tree is
         decreased */
      if (f == NULL) tree->height--;
      /* returns the deleted node to the memory pool */
      dmp_free_atom(tree->pool, p);
      return;
}

/*----------------------------------------------------------------------
-- avl_rotate_subtree - restore balance of AVL subtree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- AVLNODE *avl_rotate_subtree(AVLTREE *tree, AVLNODE *node);
--
-- The routine avl_rotate_subtree restores balance of the subtree, the
-- root of which is the specified node.
--
-- This routine is auxiliary and not intended for independent usage.
--
-- *Returns*
--
-- The routine returns a pointer to a new root node of the subtree. */

AVLNODE *avl_rotate_subtree(AVLTREE *tree, AVLNODE *node)
{     AVLNODE *f, *p, *q, *r, *x, *y;
      insist(node != NULL);
      p = node;
      if (p->bal < 0)
      {  /* perform negative (left) rotation */
         f = p->up; q = p->left; r = q->right;
         if (q->bal <= 0)
         {  /* perform single negative rotation */
            if (f == NULL)
               tree->root = q;
            else
               if (p->flag == 0) f->left = q; else f->right = q;
            p->rank -= q->rank;
            q->up = f; q->flag = p->flag; q->bal++; q->right = p;
            p->up = q; p->flag = 1;
            p->bal = (short int)(-q->bal); p->left = r;
            if (r != NULL) r->up = p, r->flag = 0;
            node = q;
         }
         else
         {  /* perform double negative rotation */
            x = r->left; y = r->right;
            if (f == NULL)
               tree->root = r;
            else
               if (p->flag == 0) f->left = r; else f->right = r;
            p->rank -= (q->rank + r->rank);
            r->rank += q->rank;
            p->bal = (short int)(r->bal >= 0 ? 0 : +1);
            q->bal = (short int)(r->bal <= 0 ? 0 : -1);
            r->up = f; r->flag = p->flag; r->bal = 0;
            r->left = q; r->right = p;
            p->up = r; p->flag = 1; p->left = y;
            q->up = r; q->flag = 0; q->right = x;
            if (x != NULL) x->up = q, x->flag = 1;
            if (y != NULL) y->up = p, y->flag = 0;
            node = r;
         }
      }
      else
      {  /* perform positive (right) rotation */
         f = p->up; q = p->right; r = q->left;
         if (q->bal >= 0)
         {  /* perform single positive rotation */
            if (f == NULL)
               tree->root = q;
            else
               if (p->flag == 0) f->left = q; else f->right = q;
            q->rank += p->rank;
            q->up = f; q->flag = p->flag; q->bal--; q->left = p;
            p->up = q; p->flag = 0;
            p->bal = (short int)(-q->bal); p->right = r;
            if (r != NULL) r->up = p, r->flag = 1;
            node = q;
         }
         else
         {  /* perform double positive rotation */
            x = r->left; y = r->right;
            if (f == NULL)
               tree->root = r;
            else
               if (p->flag == 0) f->left = r; else f->right = r;
            q->rank -= r->rank;
            r->rank += p->rank;
            p->bal = (short int)(r->bal <= 0 ? 0 : -1);
            q->bal = (short int)(r->bal >= 0 ? 0 : +1);
            r->up = f; r->flag = p->flag; r->bal = 0;
            r->left = p; r->right = q;
            p->up = r; p->flag = 0; p->right = x;
            q->up = r; q->flag = 1; q->left = y;
            if (x != NULL) x->up = p, x->flag = 1;
            if (y != NULL) y->up = q, y->flag = 0;
            node = r;
         }
      }
      return node;
}

/*----------------------------------------------------------------------
-- avl_delete_tree - delete AVL tree.
--
-- *Synopsis*
--
-- #include "glpavl.h"
-- void avl_delete_tree(AVLTREE *tree);
--
-- *Description*
--
-- The routine avl_delete_tree deletes the specified AVL tree and frees
-- all the memory allocated to this object. */

void avl_delete_tree(AVLTREE *tree)
{     dmp_delete_pool(tree->pool);
      ufree(tree);
      return;
}

/* eof */
