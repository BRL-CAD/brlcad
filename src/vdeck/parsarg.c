/*                       P A R S A R G . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file vdeck/parsarg.c
 *
 */

#include "common.h"

#include <signal.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"

#include "./vextern.h"


/*
 * Parse the command line arguments.
 */
int
parsArg(int argc, char **argv)
{
    int	i, c, arg_cnt;

    while ( (c = bu_getopt( argc, argv, "d" )) != -1 )
    {
	switch ( c )
	{
	    case 'd' :
		debug = 1;
		break;
	    case '?' :
		return	0;
	}
    }
    if ( bu_optind >= argc )
    {
	(void) fprintf( stderr, "Missing name of input file!\n" );
	return	0;
    }
    else
	objfile = argv[bu_optind++];
    if ( (dbip = db_open( objfile, "r" )) == DBI_NULL )  {
	perror(objfile);
	return 0;		/* FAIL */
    }

    arg_list[0] = argv[0]; /* Program name goes in first.	*/
    for ( i = bu_optind, arg_cnt = 1; i < argc; i++, arg_cnt++ )
	/* Insert objects.	*/
	arg_list[arg_cnt] = argv[i];
    return	1;
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
