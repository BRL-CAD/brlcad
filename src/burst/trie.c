/*                          T R I E . C
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
/** @file burst/trie.c
 *
 */

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <signal.h>

#include "bu.h"

#include "./burst.h"
#include "./trie.h"
#include "./ascii.h"
#include "./extern.h"


static Func *matchTrie();

/*
  Trie *addTrie(char *name, Trie **triepp)

  Insert the name in the trie specified by triepp, if it
  doesn't already exist there.
  Return pointer to leaf node associated with name.
*/
Trie *
addTrie(char *name, Trie **triepp)
{
    Trie *curp;
    if (*name == NUL) {
	/* End of name, see if name already exists. */
	if (*triepp == TRIE_NULL) {
	    /* Name does not exist, make leaf node. */
	    NewTrie(*triepp);
	    (*triepp)->l.t_altr = (*triepp)->l.t_next
		= TRIE_NULL;
	    (*triepp)->l.t_func = NULL_FUNC;
	} else
	    if ((*triepp)->n.t_next != TRIE_NULL)
		/* Name is subset of another name. */
		return addTrie(name, &(*triepp)->n.t_altr);
	/* else -- Name already inserted, so do nothing. */
	return *triepp;
    }
    /* Find matching letter, this level.  */
    for (curp = *triepp;
	 curp != TRIE_NULL && *name != curp->n.t_char;
	 curp = curp->n.t_altr
	)
	;
    if (curp == TRIE_NULL) {
	/* No Match, this level, so create new alternate. */
	curp = *triepp;
	NewTrie(*triepp);
	(*triepp)->n.t_altr = curp;
	(*triepp)->n.t_char = *name;
	(*triepp)->n.t_next = TRIE_NULL;
	return addTrie(++name, &(*triepp)->n.t_next);
    } else
	/* Found matching character. */
	return addTrie(++name, &curp->n.t_next);
}


Func *
getTrie(char *name, Trie *triep)
{
    Trie *curp = NULL;
    assert(triep != TRIE_NULL);

    /* Traverse next links to end of region name. */
    for (; triep != TRIE_NULL; triep = triep->n.t_next) {
	curp = triep;
	if (*name == NUL) {
	    /* End of user-typed name. */
	    if (triep->n.t_altr != TRIE_NULL)
		/* Ambiguous at this point. */
		return NULL_FUNC;
	    else {
		/* Complete next character. */
		*name++ = triep->n.t_char;
		*name = NUL;
	    }
	} else
	    if (*name == '*')
		return matchTrie(triep);
	    else	/* Not at end of user-typed name yet, traverse
			   alternate list to find current letter.
			*/ {
		for (;
		     triep != TRIE_NULL
			 &&	*name != triep->n.t_char;
		     triep = triep->n.t_altr
		    )
		    ;
		if (triep == TRIE_NULL) {
		    /* Non-existant name, truncate bad part. */
		    *name = NUL;
		    return NULL_FUNC;
		} else
		    name++;
	    }
    }
    /* Clobber key-stroke, and return it. */
    --name;
    *name = NUL;
    assert(curp != TRIE_NULL);
    return curp->l.t_func;
}


#define MAX_TRIE_LEVEL (32*16)

static Func *
matchTrie(Trie *triep)
{
    Func *func;
    if (triep == TRIE_NULL)
	func = NULL_FUNC;
    else
	if (triep->n.t_altr != TRIE_NULL)
	    func = NULL_FUNC;	/* Ambiguous root, no match.  */
	else
	    if (triep->n.t_next == TRIE_NULL)
		func = triep->l.t_func;	/* At leaf node, return datum.  */
	    else				/* Keep going to leaf.  */
		func = matchTrie(triep->n.t_next);
    return func;
}


void
prntTrie(Trie *triep, int level)
{
    Trie *tp = triep;
    static char name_buf[MAX_TRIE_LEVEL+1], *namep;
    if (tp == TRIE_NULL)
	return;
    if (tp->n.t_altr != TRIE_NULL)
	prntTrie(tp->n.t_altr, level);
    if (level == 0)
	namep = name_buf;
    *namep = tp->n.t_char;
    if (tp->n.t_next == TRIE_NULL) {
	/* At end of name, so print it out. */
	*namep = NUL;
	brst_log("%s\n", name_buf);
    } else {
	namep++;
	prntTrie(tp->n.t_next, level+1);
	namep--;
    }
    return;
}


int
writeTrie(Trie *triep, int level, FILE *fp)
{
    Trie *tp = triep;
    static char name_buf[MAX_TRIE_LEVEL+1], *namep;
    if (tp == TRIE_NULL)
	return 1;
    if (tp->n.t_altr != TRIE_NULL)
	(void) writeTrie(tp->n.t_altr, level, fp);
    if (level == 0)
	namep = name_buf;
    *namep = tp->n.t_char;
    if (tp->n.t_next == TRIE_NULL) {
	/* At end of name, so print it out. */
	*namep = NUL;
	(void) fprintf(fp, "%s\n", name_buf);
    } else {
	namep++;
	(void) writeTrie(tp->n.t_next, level+1, fp);
	namep--;
    }
    return 1;
}


int
readTrie(FILE *fp, Trie **triepp)
{
    static char name_buf[MAX_TRIE_LEVEL+1];
    while (bu_fgets(name_buf, MAX_TRIE_LEVEL, fp) != NULL) {
	name_buf[strlen(name_buf)-1] = '\0'; /* Clobber new-line. */
	(void) addTrie(name_buf, triepp);
    }
    return 1;
}


void
ring_Bell()
{
    (void) putchar(BEL);
    return;
}


char *
char_To_String(int i)
{
    static char buf[4];
    if (i >= SP && i < DEL) {
	buf[0] = i;
	buf[1] = NUL;
    } else
	if (i >= NUL && i < SP) {
	    buf[0] = '^';
	    buf[1] = i + 64;
	    buf[2] = NUL;
	} else
	    if (i == DEL)
		return "DL";
	    else
		return "EOF";
    return buf;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
