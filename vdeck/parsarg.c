/*
 *	@(#) parsarg.c			retrieved: 8/13/86 at 08:01:52
 *	@(#) version 2.1		last edit: 6/1/84 at 14:04:47
 *
 *	Written by:	Gary S. Moss
 *			U. S. Army Ballistic Research Laboratory
 *			Aberdeen Proving Ground
 *			Maryland 21005
 *			(301)278-6647 or AV-283-6647
 */
static
char	sccsTag[] = "@(#) parsarg.c	2.1	last edit 6/1/84 at 14:04:47";
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "./vextern.h"

/*	==== p a r s A r g ( )
 *	Parse the command line arguments.
 */
parsArg( argc, argv )	char	*argv[]; {
	register int	i, c, arg_ct;
	extern int	optind;
	extern char	*optarg;

	while( (c = getopt( argc, argv, "d" )) != EOF ) {
		switch( c ) {
		case 'd':
			debug = 1;
			break;
		case '?':
			return	0;
		}
	}
	if( optind >= argc ) {
		(void) fprintf( stderr, "Missing name of input file!\n" );
		return	0;
	} else
	objfile = argv[optind++];
	if( (objfd = open( objfile, 0 )) < 0 ) {
		perror( objfile );
		return	0;
	}

	arg_list[0] = argv[0]; /* Program name goes in first.	*/
	for( i = optind, arg_ct = 1; i < argc; i++, arg_ct++ )
	{ /* Insert objects.	*/
		arg_list[arg_ct] = argv[i];
	}
	return	1;
}
