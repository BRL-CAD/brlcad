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

char usage[] = "Usage: pix-ik [-h] file.pix [width] [fr_offset] [fr_count]\n";

int
main(argc, argv)
int argc;
char **argv;
{
	static int y;
	static int infd;
	static int nlines;		/* Square:  nlines, npixels/line */
	static int frame_offset;
	static int frame_count;

	if( argc < 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	nlines = 512;
	if( strcmp( argv[1], "-h" ) == 0 )  {
		nlines = 1024;
		argc--; argv++;
		ikhires = 1;
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
		ikhires = 1;

	scanbytes = nlines * 3;

	(void)lseek( infd, (long)frame_offset*scanbytes*nlines, 0 );

	ikopen();
	ikclear();

	while(frame_count-- > 0)  {
		for( y = nlines-1; y >= 0; y-- )  {
			register char *in, *out;
			register int i;

			if( read( infd, (char *)scanline, scanbytes ) != scanbytes )
				exit(0);

			in = scanline;
			out = outline;
			for( i=0; i<nlines; i++ )  {
				*out++ = *in++;
				*out++ = *in++;
				*out++ = *in++;
				*out++ = 0;
			}
			clustwrite( outline, y, nlines );
		}
	}
	exit(0);
}
