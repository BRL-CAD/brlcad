/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#if ! defined( lint )
static const char RCSid[] = "@(#) try.c 2.1, modified 12/9/86 at 15:54:34, archive /vld/moss/src/fbed/s.try.c";
#endif

#include "conf.h"

#include <stdio.h>
#include <assert.h>

#include "machine.h"
#include "externs.h"
#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./font.h"
#include "./try.h"
#include "./extern.h"

#define NewTry( p ) \
		if( ((p) = (Try *) malloc( sizeof(Try) )) == TRY_NULL ) \
			{ \
			Malloc_Bomb(); \
			}
int
add_Try( ftbl, name, trypp )
Func_Tab	*ftbl;
register char *name;
register Try	**trypp;
	{	register Try	*curp;
	if( *name == NUL )
		{ /* We are finished, make leaf node. */
		NewTry( *trypp );
		(*trypp)->l.t_altr = (*trypp)->l.t_next = TRY_NULL;
		(*trypp)->l.t_ftbl = ftbl;
		return 1;
		}
	for(	curp = *trypp;
		curp != TRY_NULL && *name != curp->n.t_curr;
		curp = curp->n.t_altr
		)
		;
	if( curp == TRY_NULL )
		{ /* No Match, this level, so create new alternate. */
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
get_Try( name, tryp )
register char *name;
register Try	*tryp;
	{	register Try *curp = NULL; /* initialize to shutup compiler. */
	/* Traverse next links to end of function name. */
	for( ; tryp != TRY_NULL; tryp = tryp->n.t_next )
		{
		curp = tryp;
		if( *name == NUL )
			{ /* End of user-typed name. */
			if( tryp->n.t_altr != TRY_NULL )
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
			for(	;
				tryp != TRY_NULL && *name != tryp->n.t_curr;
				tryp = tryp->n.t_altr
				)
				;
			if( tryp == TRY_NULL )
				/* Non-existant name, truncate bad part.*/
				{
				*name = NUL;
				return FT_NULL;
				}
			else
				name++;
			}
		}
	assert( curp != NULL ); /* can't happen */

	/* Clobber key-stroke, and return it. */
	--name;
	*name = NUL;
	return curp->l.t_ftbl;
	}
