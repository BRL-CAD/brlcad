/*
 *  			A S C 2 P I X . C
 *
 *  Convert ASCII (hex) pixel files to the binary form.
 *  For portable images.
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

#include "conf.h"

#include <stdio.h>

int lmap[256];		/* Map HEX ASCII to binary in left nybble */
int rmap[256];		/* Map HEX ASCII to binary in right nybble */

unsigned char line[256];

main()
{
	register unsigned char *cp;
	register int i;

	/* Init map */
	for(i=0; i<10; i++)  rmap['0'+i] = i;
	for(i=10; i<16; i++)  rmap['A'-10+i] = i;
	for(i=10; i<16; i++)  rmap['a'-10+i] = i;
	for(i=0;i<256;i++) lmap[i] = rmap[i]<<4;

	for(;;)  {
		(void)fgets((char *)line, sizeof(line), stdin);
		if( feof(stdin) )  break;
		cp = line;

		/* R */
		i = lmap[*cp++];
		i |= rmap[*cp++];
		putc( i, stdout );

		/* G */
		i = lmap[*cp++];
		i |= rmap[*cp++];
		putc( i, stdout );

		/* B */
		i = lmap[*cp++];
		i |= rmap[*cp++];
		putc( i, stdout );

		/* Ignore rest of line, for now */
	}
	fflush(stdout);
	exit(0);
}
