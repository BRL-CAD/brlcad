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

unsigned long line[512];

main()
{
	register unsigned long *ip;

	ip = line;
	while( !feof( stdin ) )  {
		scanf( "%x", ip++ );
		if( ip >= &line[512] )  {
			ip = line;
			write( 1, line, sizeof(line) );
			bzero( line, sizeof(line) );
		}
	}
}

#ifdef SYS5
bzero( str, n )
{
	memset( str, '\0', n );
}
#endif
