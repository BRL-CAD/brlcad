/*
 *  			P I X - F B . C
 *  
 *  Dumb little program to take a frame buffer image and
 *  write a .pix image.
 *  
 *  Mike Muuss, BRL.
 *
 *  $Revision$
 */
#include <stdio.h>

#include "/vld/include/fb.h"

#define MAX_LINE	1024		/* Max pixels/line */
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

struct pixel outline[MAX_LINE];

int inverse = 0;			/* Draw upside-down */

char usage[] = "Usage: fb-pix [-h] [-i] file.pix [width]\n";

main(argc, argv)
int argc;
char **argv;
{
	static int y;
	static int diskfd;
	static int nlines;		/* Square:  nlines, npixels/line */
	static int fbsize;

	if( argc < 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	fbsize = 512;
	nlines = 512;
	if( strcmp( argv[1], "-h" ) == 0 )  {
		fbsize = 1024;
		nlines = 1024;
		argc--; argv++;
	}
	if( strcmp( argv[1], "-i" ) == 0 )  {
		inverse = 1;
		argc--; argv++;
	}
	if( (diskfd = creat( argv[1], 0444 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}
	if( argc >= 3 )
		nlines = atoi(argv[2] );
	if( nlines > 512 )
		fbsetsize(fbsize);

	scanbytes = nlines * 3;

	if( fbopen( NULL, APPEND ) < 0 )  {
		fprintf(stderr,"fbopen failed\n");
		exit(12);
	}

	if( !inverse )  {
		/*  Regular -- draw bottom to top */
		for( y = nlines-1; y >= 0; y-- )  {
			register char *in;
			register struct pixel *out;
			register int i;

			fbread( 0, y, outline, nlines );

			in = scanline;
			out = outline;
			for( i=0; i<nlines; i++ )  {
				*in++ = out->red;
				*in++ = out->green;
				*in++ = out->blue;
				out++;
			}
			if( write( diskfd, (char *)scanline, scanbytes ) != scanbytes )  {
				perror("write");
				exit(1);
			}
		}
	}  else  {
		/*  Inverse -- draw top to bottom */
		for( y=0; y < nlines; y++ )  {
			register char *in;
			register struct pixel *out;
			register int i;

			fbread( 0, y, outline, nlines );

			in = scanline;
			out = outline;
			for( i=0; i<nlines; i++ )  {
				*in++ = out->red;
				*in++ = out->green;
				*in++ = out->blue;
				out++;
			}
			if( write( diskfd, (char *)scanline, scanbytes ) != scanbytes )  {
				perror("write");
				exit(1);
			}
		}
	}
	exit(0);
}
