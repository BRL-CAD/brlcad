/*                          T R I E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file burst/trie.h
 *
 */

#ifndef INCL_TRIE
#define INCL_TRIE

#ifdef NULL_FUNC
#  undef NULL_FUNC
#endif

#define NULL_FUNC ((Func *) NULL)
#define TRIE_NULL ((Trie *) NULL)

/* Datum for trie leaves.  */
typedef void Func();

/* Trie tree node.  */
typedef union trie Trie;
union trie {
    struct {
	/* Internal nodes: datum is current letter. */
	int t_char;   /* Current letter.  */
	Trie *t_altr; /* Alternate letter node link.  */
	Trie *t_next; /* Next letter node link.  */
    }
    n;
    struct {
	/* Leaf nodes: datum is function ptr.  */
	Func *t_func; /* Function pointer.  */
	Trie *t_altr; /* Alternate letter node link.  */
	Trie *t_next; /* Next letter node link.  */
    }
    l;
};
#define NewTrie(p) if (((p) = (Trie *) malloc(sizeof(Trie))) == TRIE_NULL) {\
	Malloc_Bomb(sizeof(Trie));\
	return TRIE_NULL;\
}
extern Trie *cmd_trie;
#endif /* INCL_TRIE */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
