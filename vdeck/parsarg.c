/*
	SCCS id:	@(#) parsarg.c	2.3
	Last edit: 	12/20/85 at 19:03:51
	Retrieved: 	8/13/86 at 08:02:09
	SCCS archive:	/m/cad/vdeck/RCS/s.parsarg.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) parsarg.c	2.3	last edit 12/20/85 at 19:03:51";
#endif
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "./vextern.h"

/*	p a r s A r g ( )
	Parse the command line arguments.
 */
parsArg( argc, argv )	char	*argv[];
	{ 	register int	i, c, arg_cnt;
		extern int	optind;
		extern char	*optarg;

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
	if( (objfd = open( objfile, 0 )) < 0 )
		{
		perror( objfile );
		return	0;
		}

	arg_list[0] = argv[0]; /* Program name goes in first.	*/
	for( i = optind, arg_cnt = 1; i < argc; i++, arg_cnt++ )
		/* Insert objects.	*/
		arg_list[arg_cnt] = argv[i];
	return	1;
	}
