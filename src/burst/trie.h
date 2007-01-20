/*                          T R I E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file trie.h
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066

	$Header$ (BRL)
 */
#ifndef INCL_TRIE
#define INCL_TRIE

#ifdef FUNC_NULL
#undef FUNC_NULL
#endif

#define FUNC_NULL	((Func *) NULL)
#define TRIE_NULL	((Trie *) NULL)

/* Datum for trie leaves.  */
typedef void Func();

/* Trie tree node.  */
typedef union trie Trie;
union trie
        {
        struct  /* Internal nodes: datum is current letter. */
                {
                int t_char;   /* Current letter.  */
                Trie *t_altr; /* Alternate letter node link.  */
                Trie *t_next; /* Next letter node link.  */
                }
        n;
        struct  /* Leaf nodes: datum is function ptr.  */
                {
                Func *t_func; /* Function pointer.  */
                Trie *t_altr; /* Alternate letter node link.  */
                Trie *t_next; /* Next letter node link.  */
                }
        l;
        };
#define NewTrie( p ) \
		if( ((p) = (Trie *) malloc( sizeof(Trie) )) == TRIE_NULL )\
			{\
			Malloc_Bomb(sizeof(Trie));\
			return	TRIE_NULL;\
			}
extern Trie *cmd_trie;
#endif /* INCL_TRIE */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
