/*
 *			P I X B U S T U P . C
 *
 *	Take concatenated .pix files, and write them into individual files.
 *	Mostly a holdover from the days when RT wrote animations into
 *	one huge file, but still occasionally useful.
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
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"

static int scanbytes;			/* # of bytes of scanline */

unsigned char *in1;

static int nlines;		/* Number of input lines */
static int pix_line;		/* Number of pixels/line */

char usage[] = 
"Usage: pixbustup basename width [image_offset] [first_number] <input.pix\n";

int infd;

int
main(argc, argv)
int argc;
char **argv;
{
	int image_offset;
	int framenumber;
	char *basename;
	char name[128];

	if( argc < 3 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	basename = argv[1];
	nlines = atoi(argv[2] );

	pix_line = nlines;	/* Square pictures */
	scanbytes = nlines * pix_line * 3;
	in1 = (unsigned char  *) malloc( scanbytes );

	if( argc == 4 )  {
		image_offset = atoi(argv[3]);
		lseek(0, image_offset*scanbytes, 0);
	}
	if( argc == 5 )
		framenumber = atoi(argv[4]);
	else
		framenumber = 0;

	for( ; ; framenumber++ )  {
		int fd;

		if( read( 0, in1, scanbytes ) != scanbytes )
			break;
		sprintf(name, "%s.%d", basename, framenumber);
		if( (fd=creat(name,0444))<0 )  {
			perror(name);
			continue;
		}
		if( write( fd, in1, scanbytes ) != scanbytes ) {
			perror("write");
		}
		(void)close(fd);
		printf("wrote %s\n", name);
	}
	exit(0);
}
