/*
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
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
	stk = (union tree **)rt_malloc( stklen*sizeof( union tree * ), "Initstack: stk" );
	if( stk == NULL )
	{
		rt_log( "Cannot allocate stack space\n" );
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
			rt_log( "Cannot reallocate stack space\n" );
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
