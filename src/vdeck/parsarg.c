/*                       P A R S A R G . C
 * BRL-CAD
 *
 * Copyright (C) 1986-2005 United States Government as represented by
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
/** @file parsarg.c
 *
 *  Author:		Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <signal.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
