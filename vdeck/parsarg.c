/*
 *			P A R S A R G . C
 *
 *  Author:		Gary S. Moss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <signal.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "externs.h"
#include "./vextern.h"

/*	p a r s A r g ( )
	Parse the command line arguments.
 */
int
parsArg(int argc, char **argv)
{ 	register int	i, c, arg_cnt;
		extern int	optind;

	while( (c = getopt( argc, argv, "d" )) != EOF )
		{
		switch( c )
			{
		case 'd' :
			debug = 1;
			break;
		case '?' :
			return	0;
			}
		}
	if( optind >= argc )
		{
		(void) fprintf( stderr, "Missing name of input file!\n" );
		return	0;
		}
	else
		objfile = argv[optind++];
	if( (dbip = db_open( objfile, "r" )) == DBI_NULL )  {
		perror(objfile);
	    	return( 0 );		/* FAIL */
	}

	arg_list[0] = argv[0]; /* Program name goes in first.	*/
	for( i = optind, arg_cnt = 1; i < argc; i++, arg_cnt++ )
		/* Insert objects.	*/
		arg_list[arg_cnt] = argv[i];
	return	1;
	}
