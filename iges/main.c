/*
 *			I G E S - G / M A I N . C
 *
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
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "./iges_struct.h"
#include "./iges_types.h"

char eor,eof,card[256],crdate[9],crtime[9];
fastf_t scale,inv_scale,conv_factor;
int units,counter,pstart,dstart,totentities,dirarraylen;
FILE *fd,*fdout;
int reclen,currec,ntypes;
struct iges_directory **dir;
struct reglist *regroot;

char operator[]={
	' ',
	'u',
	'+',
	'-' };

mat_t *identity;
extern int errno;

extern char	version[];

main( argc , argv )
int argc;
char *argv[];
{
	int i;

	if( argc != 3 )
		usage();

	printf( "%s", version+5);
	printf( "Please direct bug reports to <jra@brl.mil>\n\n" );

	ntypes = NTYPES;
	regroot = NULL;

	fd = fopen( *++argv , "r" );	/* open IGES file */
	if( fd == NULL )
	{
		fprintf( stderr , "Cannot open %s\n" , *argv );
		perror( "Conv-igs2g" );
		exit( 1 );
	}

	reclen = Recsize() * sizeof( char ); /* Check length of records */
	if( reclen == 0 )
	{
		fprintf( stderr , "File not in IGES ASCII format\n" );
		exit(1);
	}

	fdout = fopen( *++argv , "w" );	/* open BRLCAD file	*/
	if( fdout == NULL )
	{
		fprintf( stderr , "Cannot open %s\n" , *argv );
		perror( "Conv-igs2g" );
		exit( 1 );
	}

	identity = (mat_t *)malloc( sizeof( mat_t ) );
	for( i=0 ; i<16 ; i++ )
	{
		if( !(i%5) )
			(*identity)[i] = 1.0;
		else
			(*identity)[i] = 0.0;
	}

	Initstack();	/* Initialize node stack */

	Readstart();	/* Read start section */

	Readglobal();	/* Read global section */

	pstart = Findp();	/* Find start of parameter section */

	Makedir();	/* Read drectory section and build a linked list of entries */

	Summary();	/* Print a summary of what is in the IGES file */

	Docolor();	/* Get color info from color definition entities */

	Evalxform();	/* Accumulate the transformation matrices */

	Convinst();	/* Handle Instances */

	Convsolids();	/* Convert solid entities */

	Convsurfs();	/* Convert NURBS */

	Convtree();	/* Convert Boolean Trees */

	Convassem();	/* Convert solid assemblies */

	Makegroup();	/* Make a top level group */

}
