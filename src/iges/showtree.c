/*                      S H O W T R E E . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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
/** @file showtree.c
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

/*		Display a boolean tree		*/

#include "./iges_struct.h"
#include "./iges_extern.h"

#define	STKBLK	100	/* Allocation block size */

static void Initastack(),Apush();
static void Initsstack(),Spush();
static struct node *Spop();
static void Afreestack(),Sfreestack();
static char *Apop();

/* Some junk for this routines private node stack */
struct node **sstk_p;
int sjtop,sstklen;

/* some junk for this routines private character string stack */
char **stk;
int jtop,stklen;

void
Showtree( root )
struct node *root;
{
	struct node *ptr,*Spop();
	char *opa,*opb,*tmp,*Apop(),oper[4];

	strcpy( oper , "   " );

	/* initialize both stacks */
	Initastack();
	Initsstack();

	ptr = root;
	while( 1 )
	{
		while( ptr != NULL )
		{
			Spush( ptr );
			ptr = ptr->left;
		}
		ptr = Spop();

		if( ptr == NULL )
		{
			bu_log( "Error in Showtree: Popped a null pointer\n" );
			Afreestack();
			Sfreestack();
			return;
		}

		if( ptr->op < 0 ) /* this is an operand, push it's name */
			Apush( dir[-(1+ptr->op)/2]->name );
		else	/* this is an operator */
		{
			/* Pop the names of the operands */
			opb = Apop();
			opa = Apop();

			/* construct the character string (opa ptr->op opb) */
			tmp = (char *)bu_malloc( strlen( opa ) + strlen( opb ) + 6, "Showtree: tmp" );
			if( ptr->parent != NULL )
				strcpy( tmp , "(" );
			else
				*tmp = '\0';
			strcat( tmp , opa );
			oper[1] = operator[ptr->op];
			strcat( tmp , oper );
			strcat( tmp , opb );
			if( ptr->parent != NULL )
				strcat( tmp , ")" );

			/* push the character string representing the result */
			Apush( tmp );
		}

		if( ptr == root )	/* done! */
		{
			bu_log( "%s\n" , Apop() ); /* print the result */

			/* free some memory */
			Afreestack();
			Sfreestack();
			return;
		}

		if( ptr != ptr->parent->right )
			ptr = ptr->parent->right;
		else
			ptr = NULL;
	}
}

/* The following are stack routines for character strings */

static void
Initastack()
{
	int i;

	jtop = (-1);
	stklen = STKBLK;
	stk = (char **)bu_malloc( stklen*sizeof( char * ), "Initastack: stk" );
	if( stk == NULL )
	{
		bu_log( "Cannot allocate stack space\n" );
		perror( "Initastack" );
		exit( 1 );
	}
	for( i=0 ; i<stklen ; i++ )
		stk[i] = NULL;
}

/*  This function pushes a pointer onto the stack. */

static void
Apush(ptr)
char *ptr;
{
	int i;

	jtop++;
	if( jtop == stklen )
	{
		stklen += STKBLK;
		stk = (char **)rt_realloc( (char *)stk , stklen*sizeof( char *), "Apush: stk" );
		if( stk == NULL )
		{
			bu_log( "Cannot reallocate stack space\n" );
			perror( "Apush" );
			exit( 1 );
		}
		for( i=jtop ; i<stklen ; i++ )
			stk[i] = NULL;
	}
	stk[jtop] = ptr;
}


/*  This function pops the top of the stack. */


static char *
Apop()
{
	char *ptr;

	if( jtop == (-1) )
		ptr=NULL;
	else
	{
		ptr = stk[jtop];
		jtop--;
	}

	return(ptr);
}

/* Free the memory associated with the stack */
static void
Afreestack()
{

	jtop = (-1);
	stklen = 0;
	bu_free( (char *)stk, "Afreestack: stk" );
	return;
}


/* The following routines are stack routines for 'struct node' */
static void
Initsstack() /* initialize the stack */
{

	sjtop = (-1);
	sstklen = STKBLK;
	sstk_p = (struct node **)bu_malloc( sstklen*sizeof( struct node * ), "Initsstack: sstk_p" );
	if( sstk_p == NULL )
	{
		bu_log( "Cannot allocate stack space\n" );
		perror( "Initsstack" );
		exit( 1 );
	}
}

/*  This function pushes a pointer onto the stack. */

static void
Spush(ptr)
struct node *ptr;
{

	sjtop++;
	if( sjtop == sstklen )
	{
		sstklen += STKBLK;
		sstk_p = (struct node **)rt_realloc( (char *)sstk_p , sstklen*sizeof( struct node *), "Spush: sstk_p" );
		if( sstk_p == NULL )
		{
			bu_log( "Cannot reallocate stack space\n" );
			perror( "Spush" );
			exit( 1 );
		}
	}
	sstk_p[sjtop] = ptr;
}


/*  This function pops the top of the stack. */


static struct node *
Spop()
{
	struct node *ptr;

	if( sjtop == (-1) )
		ptr=NULL;
	else
	{
		ptr = sstk_p[sjtop];
		sjtop--;
	}

	return(ptr);
}

/* free memory associated with the stack, but not the pointed to nodes */
static void
Sfreestack()
{
	sjtop = (-1);
	bu_free( (char *)sstk_p, "Sfreestack: sstk_p" );
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
