/*                         S T A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
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
 */
/** @file stack.c
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#include "./iges_struct.h"

#define	STKBLK	100	/* Allocation block size */

static union tree **stk;
static int jtop,stklen;

void
Initstack()
{

	jtop = (-1);
	stklen = STKBLK;
	stk = (union tree **)bu_malloc( stklen*sizeof( union tree * ), "Initstack: stk" );
	if( stk == NULL )
	{
		bu_log( "Cannot allocate stack space\n" );
		perror( "Initstack" );
		exit( 1 );
	}
}

/*  This function pushes a pointer onto the stack. */

void
Push(ptr)
union tree *ptr;
{

	jtop++;
	if( jtop == stklen )
	{
		stklen += STKBLK;
		stk = (union tree **)rt_realloc( (char *)stk , stklen*sizeof( union tree *),
			"Push: stk" );
		if( stk == NULL )
		{
			bu_log( "Cannot reallocate stack space\n" );
			perror( "Push" );
			exit( 1 );
		}
	}
	stk[jtop] = ptr;
}


/*  This function pops the top of the stack. */


union tree *
Pop()
{
	union tree *ptr;

	if( jtop == (-1) )
		ptr=NULL;
	else
	{
		ptr = stk[jtop];
		jtop--;
	}

	return(ptr);
}

void
Freestack()
{
	jtop = (-1);
	return;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
