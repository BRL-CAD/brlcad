/* glpdmp.h */

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

#ifndef _GLPDMP_H
#define _GLPDMP_H

#define dmp_create_pool       glp_dmp_create_pool
#define dmp_get_atom          glp_dmp_get_atom
#define dmp_free_atom         glp_dmp_free_atom
#define dmp_get_atomv         glp_dmp_get_atomv
#define dmp_free_all          glp_dmp_free_all
#define dmp_delete_pool       glp_dmp_delete_pool

typedef struct DMP DMP;

struct DMP
{     /* dynamic memory pool (set of atoms) */
      int size;
      /* size of each atom, in bytes (1 <= size <= 256); if size is 0,
         atoms may have different sizes */
      void *avail;
      /* pointer to the linked list of free atoms (not used in the case
         of variable-sized pools) */
      void *link;
      /* pointer to the linked list of allocated blocks (it points to
         the most recently allocated block) */
      int used;
      /* number of bytes used in the most recently allocated block */
      void *stock;
      /* pointer to the linked list of free blocks */
      int count;
      /* total number of allocated atoms */
};

#define DMP_BLK_SIZE 8000
/* the size of memory blocks, in bytes, allocated for dynamic memory
   pools (all pools use memory blocks of the same size) */

DMP *dmp_create_pool(int size);
/* create dynamic memory pool */

void *dmp_get_atom(DMP *pool);
/* obtain free atom from fixed-sized memory pool */

void dmp_free_atom(DMP *pool, void *atom);
/* return specified atom to fixed-sized memory pool */

void *dmp_get_atomv(DMP *pool, int size);
/* obtain free atom from variable-sized memory pool */

void dmp_free_all(DMP *pool);
/* return all atoms to dynamic memory pool */

void dmp_delete_pool(DMP *pool);
/* delete dynamic memory pool */

#endif

/* eof */
