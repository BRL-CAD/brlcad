/*
 *			L O O P . C
 *
 *	Simple program to output integers between "start" and "finish",
 *	inclusive.  Default is an increment of +1 if start < finish or
 *	-1 if start > finish.  User may specify an alternate increment.
 *	There is no attempt to prevent "infinite" loops.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

main(argc, argv)
char **argv;
{
	register int start, finish, incr;
	register int i;

	if( argc < 3 || argc > 4 )  {
		fprintf(stderr, "Usage:  loop start finish [incr]\n");
		exit(9);
	}
	start = atoi( argv[1] );
	finish = atoi( argv[2] );
	if( start > finish )
		incr = -1;
	else
		incr = 1;
	if( argc == 4 )
		incr = atoi( argv[3] );

	if( incr >= 0 )  {
		for( i=start; i<=finish; i += incr )
			printf("%d\n", i);
	} else {
		for( i=start; i>=finish; i += incr )
			printf("%d\n", i);
	}
}
