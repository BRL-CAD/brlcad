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

/*		Display a boolean tree		*/

#include <stdio.h>
#include <strings.h>
#include "./iges_struct.h"
#include "./iges_extern.h"

#define	STKBLK	100	/* Allocation block size */

extern int errno;

/* Some junk for this routines private node stack */
struct node **sstk;
int sjtop,sstklen;

/* some junk for this routines private character string stack */
char **stk;
int jtop,stklen;

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
			printf( "Error in Showtree: Popped a null pointer\n" );
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
			tmp = (char *)malloc( strlen( opa ) + strlen( opb ) + 6 );
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
			printf( "%s\n" , Apop() ); /* print the result */

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

Initastack()
{
	int i;

	jtop = (-1);
	stklen = STKBLK;
	stk = (char **)malloc( stklen*sizeof( char * ) );
	if( stk == NULL )
	{
		fprintf( stderr , "Cannot allocate stack space\n" );
		perror( "Initastack" );
		exit( 1 );
	}
	for( i=0 ; i<stklen ; i++ )
		stk[i] = NULL;
}

/*  This function pushes a pointer onto the stack. */

Apush(ptr)
char *ptr;
{
	int i;

	jtop++;
	if( jtop == stklen )
	{
		stklen += STKBLK;
		stk = (char **)realloc( stk , stklen*sizeof( char *) );
		if( stk == NULL )
		{
			fprintf( stderr , "Cannot reallocate stack space\n" );
			perror( "Apush" );
			exit( 1 );
		}
		for( i=jtop ; i<stklen ; i++ )
			stk[i] = NULL;
	}
	stk[jtop] = ptr;
}


/*  This function pops the top of the stack. */


char *Apop()
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
Afreestack()
{

	jtop = (-1);
	stklen = 0;
	free( stk );
	return;
}


/* The following routines are stack routines for 'struct node' */
Initsstack() /* initialize the stack */
{

	sjtop = (-1);
	sstklen = STKBLK;
	sstk = (struct node **)malloc( sstklen*sizeof( struct node * ) );
	if( sstk == NULL )
	{
		fprintf( stderr , "Cannot allocate stack space\n" );
		perror( "Initsstack" );
		exit( 1 );
	}
}

/*  This function pushes a pointer onto the stack. */

Spush(ptr)
struct node *ptr;
{

	sjtop++;
	if( sjtop == sstklen )
	{
		sstklen += STKBLK;
		sstk = (struct node **)realloc( sstk , sstklen*sizeof( struct node *) );
		if( sstk == NULL )
		{
			fprintf( stderr , "Cannot reallocate stack space\n" );
			perror( "Spush" );
			exit( 1 );
		}
	}
	sstk[sjtop] = ptr;
}


/*  This function pops the top of the stack. */


struct node *Spop()
{
	struct node *ptr;

	if( sjtop == (-1) )
		ptr=NULL;
	else
	{
		ptr = sstk[sjtop];
		sjtop--;
	}

	return(ptr);
}

/* free memory associated with the stack, but not the pointed to nodes */
Sfreestack()
{
	sjtop = (-1);
	free( sstk );
	return;
}
