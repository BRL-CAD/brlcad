/* glpdmp.c */

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
#include "glpdmp.h"
#include "glplib.h"

/*----------------------------------------------------------------------
-- dmp_create_pool - create dynamic memory pool.
--
-- *Synopsis*
--
-- #include "glpdmp.h"
-- DMP *dmp_create_pool(int size);
--
-- *Description*
--
-- The routine dmp_create_pool creates a dynamic memory pool.
--
-- If the parameter size is positive, the fixed-sized memory pool is
-- created. This means that all atoms in the memory pool have the same
-- size (in bytes). The size should be in the range 1 to 256 bytes.
--
-- If the parameter size is zero, the variable-sized memory pool is
-- created. This means that atoms in the memory pool may have different
-- sizes.
--
-- *Returns*
--
-- The routine returns a pointer to the created dynamic memory pool. */

DMP *dmp_create_pool(int size)
{     DMP *pool;
      if (!(0 <= size && size <= 256))
         fault("dmp_create_pool: size = %d; invalid atom size", size);
      pool = umalloc(sizeof(DMP));
      pool->size = size;
      pool->avail = NULL;
      pool->link = NULL;
      pool->used = 0;
      pool->stock = NULL;
      pool->count = 0;
      return pool;
}

/*----------------------------------------------------------------------
-- dmp_get_atom - obtain free atom from fixed-sized memory pool.
--
-- *Synopsis*
--
-- #include "glpdmp.h"
-- void *dmp_get_atom(DMP *pool);
--
-- *Description*
--
-- The routine dmp_get_atom obtains a free atom (which is continuous
-- space of memory) from the specified fixed-sized dynamic memory pool.
--
-- The atom size, in bytes, should be specified on creating the pool by
-- the routine dmp_create_pool.
--
-- Note that the free atom returned by the routine initially contains
-- arbitrary data (not binary zeros).
--
-- *Returns*
--
-- The routine returns a pointer to the free atom. */

void *dmp_get_atom(DMP *pool)
{     int size = pool->size;
      void *atom, *blk;
      if (size == 0)
         fault("dmp_get_atom: pool = %p; attempt to obtain atom from va"
            "riable-sized pool", pool);
      /* if there is a free atom, just pull it from the list */
      atom = pool->avail;
      if (atom != NULL)
      {  pool->avail = *(void **)atom;
         goto skip;
      }
      /* free atom list is empty; if the most recently allocated block
         doesn't exist or hasn't enough space, a new block is needed */
      if (pool->link == NULL || pool->used + size > DMP_BLK_SIZE)
      {  /* pull a new block from the list of free blocks, or if this
            list is empty, ask the control program for the block */
         blk = pool->stock;
         if (blk != NULL)
            pool->stock = *(void **)blk;
         else
            blk = umalloc(DMP_BLK_SIZE);
         /* the new block becomes the most recently allocated block */
         *(void **)blk = pool->link;
         pool->link = blk;
         /* now only a few bytes in the beginning of the new block are
            used to keep a pointer to the previously allocated block */
         pool->used = align_datasize(sizeof(void *));
      }
      /* the most recently allocated block exists and has enough space
         to allocate an atom */
      atom = (void *)((char *)pool->link + pool->used);
      pool->used += (size >= sizeof(void *) ? size : sizeof(void *));
skip: pool->count++;
#if 1
      memset(atom, '?', size);
#endif
      return atom;
}

/*----------------------------------------------------------------------
-- dmp_free_atom - return specified atom to fixed-sized memory pool.
--
-- *Synopsis*
--
-- #include "glpdmp.h"
-- void dmp_free_atom(DMP *pool, void *atom);
--
-- *Description*
--
-- The routine dmp_free_atom frees the atom, which the parameter atom
-- points to, returning it to the specified fixed-sized dynamic memory
-- pool.
--
-- Note that the atom should be returned only to the pool, from which
-- it was obtained by the routine dmp_get_atom. */

void dmp_free_atom(DMP *pool, void *atom)
{     if (pool->size == 0)
         fault("dmp_free_atom: pool = %p; attempt to return atom to var"
            "iable-sized pool", pool);
      if (pool->count == 0)
         fault("dmp_free_atom: pool = %p; pool allocation error", pool);
      /* just push the atom to the list of free atoms */
      *(void **)atom = pool->avail;
      pool->avail = atom;
      pool->count--;
      return;
}

/*----------------------------------------------------------------------
-- dmp_get_atomv - obtain free atom from variable-sized memory pool.
--
-- *Synopsis*
--
-- #include "glpdmp.h"
-- void *dmp_get_atomv(DMP *pool, int size);
--
-- *Description*
--
-- The routine dmp_get_atomv obtains a free atom (which is continuous
-- space of memory) from the specified variable-sized memory pool.
--
-- The atom size, in bytes, should be specified by the parameter size.
-- The size should be in the range 1 to 256 bytes.
--
-- Note that the free atom returned by the routine initially contains
-- arbitrary data (not binary zeros).
--
-- *Returns*
--
-- The routine returns a pointer to the free atom. */

void *dmp_get_atomv(DMP *pool, int size)
{     void *atom, *blk;
      if (pool->size != 0)
         fault("dmp_get_atomv: pool = %p; attempt to obtain atom from f"
            "ixed-sized pool", pool);
      if (!(1 <= size && size <= 256))
         fault("dmp_get_atomv: size = %d; invalid atom size", size);
      /* the actual atom size should be not shorter than sizeof(void *)
         and should be properly aligned */
      if (size < sizeof(void *)) size = sizeof(void *);
      size = align_datasize(size);
      /* if the most recently allocated block doesn't exist or hasn't
         enough space, a new block is needed */
      if (pool->link == NULL || pool->used + size > DMP_BLK_SIZE)
      {  /* pull a new block from the list of free blocks, or if this
            list is empty, ask the control program for the block */
         blk = pool->stock;
         if (blk != NULL)
            pool->stock = *(void **)blk;
         else
            blk = umalloc(DMP_BLK_SIZE);
         /* the new block becomes the most recently allocated block */
         *(void **)blk = pool->link;
         pool->link = blk;
         /* now only a few bytes in the beginning of the new block are
            used to keep a pointer to the previously allocated block */
         pool->used = align_datasize(sizeof(void *));
      }
      /* the most recently allocated block exists and has enough space
         to allocate an atom */
      atom = (void *)((char *)pool->link + pool->used);
      pool->used += size;
      pool->count++;
#if 1
      memset(atom, '?', size);
#endif
      return atom;
}

/*----------------------------------------------------------------------
-- dmp_free_all - return all atoms to dynamic memory pool.
--
-- *Synopsis*
--
-- #include "glpdmp.h"
-- void dmp_free_all(DMP *pool);
--
-- *Description*
--
-- The routine dmp_free_all frees all the atoms, which were obtained
-- from the specified memory pool and which are still in use, returning
-- them to the pool.
--
-- Note that no memory allocated to the pool is returned to the control
-- program. */

void dmp_free_all(DMP *pool)
{     void *blk;
      /* move all allocated blocks to the list of free blocks */
      while (pool->link != NULL)
      {  blk = pool->link;
         pool->link = *(void **)blk;
         *(void **)blk = pool->stock;
         pool->stock = blk;
      }
      pool->avail = NULL;
      pool->used = 0;
      pool->count = 0;
      return;
}

/*----------------------------------------------------------------------
-- dmp_delete_pool - delete dynamic memory pool.
--
-- *Synopsis*
--
-- #include "glpdmp.h"
-- void dmp_delete_pool(DMP *pool);
--
-- *Description*
--
-- The routine dmp_delete_pool deletes the specified dynamic memory
-- pool and frees all the memory allocated to this object. */

void dmp_delete_pool(DMP *pool)
{     void *blk;
      while (pool->link != NULL)
      {  blk = pool->link;
         pool->link = *(void **)blk;
         ufree(blk);
      }
      while (pool->stock != NULL)
      {  blk = pool->stock;
         pool->stock = *(void **)blk;
         ufree(blk);
      }
      ufree(pool);
      return;
}

/* eof */
