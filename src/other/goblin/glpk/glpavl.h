/* glpavl.h */

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

#ifndef _GLPAVL_H
#define _GLPAVL_H

#include "glpdmp.h"

#define avl_create_tree       glp_avl_create_tree
#define avl_strcmp            glp_avl_strcmp
#define avl_insert_by_key     glp_avl_insert_by_key
#define avl_find_next_node    glp_avl_find_next_node
#define avl_find_prev_node    glp_avl_find_prev_node
#define avl_find_by_key       glp_avl_find_by_key
#define avl_next_by_key       glp_avl_next_by_key
#define avl_insert_by_pos     glp_avl_insert_by_pos
#define avl_find_by_pos       glp_avl_find_by_pos
#define avl_delete_node       glp_avl_delete_node
#define avl_rotate_subtree    glp_avl_rotate_subtree
#define avl_delete_tree       glp_avl_delete_tree

typedef struct AVLTREE AVLTREE;
typedef struct AVLNODE AVLNODE;

struct AVLTREE
{     /* AVL tree (Adelson-Velsky & Landis binary search tree) */
      DMP *pool;
      /* memory pool for allocating nodes */
      void *info;
      /* transit pointer passed to the routine fcmp */
      int (*fcmp)(void *info, void *key1, void *key2);
      /* user-defined key comparison routine; if this field is NULL,
         ordering by keys is not used */
      int size;
      /* size of the tree = total number of nodes */
      AVLNODE *root;
      /* pointer to the root node */
      int height;
      /* height of the tree */
};

struct  AVLNODE
{     /* node of AVL tree */
      void *key;
      /* pointer to node key (data structure for representing keys is
         supplied by the user) */
      int rank;
      /* node rank = relative position of the node in its own subtree =
         number of nodes in the left subtree plus one */
      int type;
      /* reserved for application specific information */
      void *link;
      /* reserved for application specific information */
      AVLNODE *up;
      /* pointer to the parent node */
      short int flag;
      /* node flag:
         0 - this node is the left child of its parent (or this node is
             the root of the tree and has no parent)
         1 - this node is the right child of its parent */
      short int bal;
      /* node balance = the difference between heights of the right and
         left subtrees:
         -1 - the left subtree is higher than the right one;
          0 - the left and right subtrees have the same height;
         +1 - the left subtree is lower than the right one */
      AVLNODE *left;
      /* pointer to the root of the left subtree */
      AVLNODE *right;
      /* pointer to the root of the right subtree */
};

AVLTREE *avl_create_tree(void *info, int (*fcmp)(void *info,
      void *key1, void *key2));
/* create AVL tree */

int avl_strcmp(void *info, void *key1, void *key2);
/* compare keys of character string type */

AVLNODE *avl_insert_by_key(AVLTREE *tree, void *key);
/* insert new node with given key into AVL tree */

AVLNODE *avl_find_next_node(AVLTREE *tree, AVLNODE *node);
/* find next node in AVL tree */

AVLNODE *avl_find_prev_node(AVLTREE *tree, AVLNODE *node);
/* find previous node in AVL tree */

AVLNODE *avl_find_by_key(AVLTREE *tree, void *key);
/* find first node with given key in AVL tree */

AVLNODE *avl_next_by_key(AVLTREE *tree, AVLNODE *node);
/* find next node with same key in AVL tree */

AVLNODE *avl_insert_by_pos(AVLTREE *tree, int pos);
/* insert new node into given position of AVL tree */

AVLNODE *avl_find_by_pos(AVLTREE *tree, int pos);
/* find node placed in given position of AVL tree */

void avl_delete_node(AVLTREE *tree, AVLNODE *node);
/* delete specified node from AVL tree */

AVLNODE *avl_rotate_subtree(AVLTREE *tree, AVLNODE *node);
/* restore balance of AVL subtree */

void avl_delete_tree(AVLTREE *tree);
/* delete AVL tree */

#endif

/* eof */
