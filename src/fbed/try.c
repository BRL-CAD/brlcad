/*                           T R Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
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
 */
/** @file try.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./try.h"
#include "./extern.h"

#define NewTry( p ) \
		if ( ((p) = (Try *) malloc( sizeof(Try) )) == TRY_NULL ) \
			{ \
			Malloc_Bomb(); \
			}
int
add_Try(Func_Tab* ftbl, const char* name, Try** trypp)
{
    Try	*curp;
    if ( *name == NUL )
    {
	/* We are finished, make leaf node. */
	NewTry( *trypp );
	(*trypp)->l.t_altr = (*trypp)->l.t_next = TRY_NULL;
	(*trypp)->l.t_ftbl = ftbl;
	return 1;
    }
    for (	curp = *trypp;
		curp != TRY_NULL && *name != curp->n.t_curr;
		curp = curp->n.t_altr
	)
	;
    if ( curp == TRY_NULL )
    {
	/* No Match, this level, so create new alternate. */
	curp = *trypp;
	NewTry( *trypp );
	(*trypp)->n.t_altr = curp;
	(*trypp)->n.t_curr = *name;
	(*trypp)->n.t_next = TRY_NULL;
	add_Try( ftbl, ++name, &(*trypp)->n.t_next );
    }
    else
	/* Found matching character. */
	add_Try( ftbl, ++name, &curp->n.t_next );
    return 1;
}

Func_Tab *
get_Try(char *name, Try *tryp)
{
    Try *curp = NULL; /* initialize to shutup compiler. */
    /* Traverse next links to end of function name. */
    for (; tryp != TRY_NULL; tryp = tryp->n.t_next )
    {
	curp = tryp;
	if ( *name == NUL )
	{
	    /* End of user-typed name. */
	    if ( tryp->n.t_altr != TRY_NULL )
		/* Ambiguous at this point. */
		return FT_NULL;
	    else	  /* Complete next character. */
	    {
		*name++ = tryp->n.t_curr;
		*name = NUL;
	    }
	}
	else	/* Not at end of user-typed name yet, traverse
		   alternate list to find current letter.
		*/
	{
	    for (	;
			tryp != TRY_NULL && *name != tryp->n.t_curr;
			tryp = tryp->n.t_altr
		)
		;
	    if ( tryp == TRY_NULL )
		/* Non-existant name, truncate bad part.*/
	    {
		*name = NUL;
		return FT_NULL;
	    }
	    else
		name++;
	}
    }

    /* Clobber key-stroke, and return it. */
    --name;
    *name = NUL;
    return curp->l.t_ftbl;
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
