/*
 *  			P I X 2 A S C . C
 *  
 *  Author -
 *  	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

unsigned char line[512*3];		/* R, G, B per pixel */

main()
{
	register unsigned char *ip;

	while( read( 0, line, sizeof(line) ) == sizeof(line) )  {
		for( ip = line; ip < &line[512*3]; ip+=3 )
			printf("%2.2x %2.2x %2.2x\n", ip[0], ip[1], ip[2]);
	}
	exit(0);
}
