/*
 *  			P I X - I K . C
 *  
 *  Dumb little program to take bottom-up pixel files and
 *  send them to the Ikonas.
 *  
 *  Easier than hacking around with RT.
 *  
 *  Mike Muuss, BRL, 05/05/84.
 *
 *  $Revision$
 */
#include <stdio.h>

extern int ikfd;
extern int ikhires;

#define MAX_LINE	1024		/* Max pixels/line */
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

static char outline[MAX_LINE*4];	/* Ikonas pixels */

char usage[] = "Usage: pix-ik [-h] file.pix\n";

main(argc, argv)
int argc;
char **argv;
{
	static int y;
	static int infd;
	static int nlines;

	if( argc < 2 || argc > 3 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	nlines = 512;
	if( argc == 3 )  {
		if( strcmp( argv[1], "-h" ) != 0 )  {
			fprintf(stderr,"%s", usage);
			exit(2);
		}
		ikhires = 1;
		nlines = 1024;
		argc--; argv++;
	}
	if( (infd = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}

	ikopen();
	load_map(1);		/* Standard map: linear */
	ikclear();

	scanbytes = nlines * 3;

	y = nlines-1;
	while( read( infd, (char *)scanline, scanbytes ) == scanbytes )  {
		register char *in, *out;
		register int i;

		in = scanline;
		out = outline;
		for( i=0; i<nlines; i++ )  {
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
			*out++ = 0;
		}
		clustwrite( outline, y, nlines );
		if( --y < 0 )
			break;
	}
}
