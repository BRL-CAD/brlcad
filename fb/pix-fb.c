/*
 *  			P I X - F B . C
 *  
 *  Dumb little program to take bottom-up pixel files and
 *  send them to a framebuffer.
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

char usage[] = "Usage: pix-fb [-h] file.pix [width] [fr_offset] [fr_count]\n";

main(argc, argv)
int argc;
char **argv;
{
	static int y;
	static int infd;
	static int nlines;		/* Square:  nlines, npixels/line */
	static int frame_offset;
	static int frame_count;
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
	if( (infd = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}
	if( argc >= 3 )
		nlines = atoi(argv[2] );
	frame_offset = 0;
	if( argc >= 4 )
		frame_offset = atoi(argv[3]);
	frame_count = 1;
	if( argc >= 5 )
		frame_count = atoi(argv[4]);
	if( nlines > 512 )
		fbsetsize(fbsize);

	scanbytes = nlines * 3;

	(void)lseek( infd, (long)frame_offset*scanbytes*nlines, 0 );

	if( fbopen( NULL, CREATE ) < 0 )
		exit(12);
	fbclear();
	fbzoom( fbsize==nlines? 0 : fbsize/nlines,
		fbsize==nlines? 0 : fbsize/nlines );
	fbwindow( nlines/2, nlines/2 );		/* center of view */

	while(frame_count-- > 0)  {
		for( y = nlines-1; y >= 0; y-- )  {
			register char *in;
			register struct pixel *out;
			register int i;

			if( read( infd, (char *)scanline, scanbytes ) != scanbytes )
				exit(0);

			in = scanline;
			out = outline;
			for( i=0; i<nlines; i++ )  {
				out->red = *in++;
				out->green = *in++;
				out->blue = *in++;
				(out++)->spare = 0;
			}
			fbwrite( 0, y, outline, nlines );
		}
	}
	exit(0);
}
