/*
 *  			A S C 2 P I X . C
 *  
 *  Author -
 *	Michael John Muuss
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
	auto int r,g,b;

	ip = line;
	while( !feof( stdin ) )  {
		scanf( "%x %x %x", &r, &g, &b );
		*ip++ = r;
		*ip++ = g;
		*ip++ = b;
		if( ip >= &line[512*3] )  {
			ip = line;
			write( 1, line, sizeof(line) );
		}
	}
}
