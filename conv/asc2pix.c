/*
 *  			A S C 2 P I X . C
 *
 *  Convert ASCII (hex) pixel files to the binary form.
 *  For portable images.
 *  Can also be used for .bw files, and random file conversion.
 *  White space in the file is ignored.
 *  The input is processed as a byte stream, and need not have a multiple
 *  of three bytes.
 *  
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#include <stdlib.h>

int lmap[256];		/* Map HEX ASCII to binary in left nybble */
int rmap[256];		/* Map HEX ASCII to binary in right nybble */

unsigned char line[256];

int
main(void)
{
	register int	a, b;
	register int	i;

	/* Init map */
	for(i=0;i<256;i++) rmap[i] = -1;		/* Unused entries */
	for(i=0; i<10; i++)  rmap['0'+i] = i;
	for(i=10; i<16; i++)  rmap['A'-10+i] = i;
	for(i=10; i<16; i++)  rmap['a'-10+i] = i;
	for(i=0;i<256;i++) {
		if( rmap[i] >= 0 )
			lmap[i] = rmap[i]<<4;
		else
			lmap[i] = -1;
	}

	for(;;)  {
		do {
			if( (a = getchar()) == EOF || a > 255 )  goto out;
		} while( (i = lmap[a]) < 0 );

		if( (b = getchar()) == EOF || b > 255 )  {
			fprintf(stderr,"asc2pix: unexpected EOF in middle of hex number\n");
			return 1;
		}

		if( (b = rmap[b]) < 0 )  {
			fprintf(stderr,"asc2pix: illegal hex code in file, aborting\n");
			return 1;
		}

		putc( (i | b), stdout );
	}
out:
	fflush(stdout);
	exit(0);
}
